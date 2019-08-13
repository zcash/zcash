#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sodium.h>

#include "base58.h"
#include "chainparams.h"
#include "key_io.h"
#include "main.h"
#include "primitives/block.h"
#include "random.h"
#include "transaction_builder.h"
#include "utiltest.h"
#include "wallet/wallet.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"

#include <boost/filesystem.hpp>

using ::testing::Return;

extern ZCJoinSplit* params;

ACTION(ThrowLogicError) {
    throw std::logic_error("Boom");
}

class MockWalletDB {
public:
    MOCK_METHOD0(TxnBegin, bool());
    MOCK_METHOD0(TxnCommit, bool());
    MOCK_METHOD0(TxnAbort, bool());

    MOCK_METHOD2(WriteTx, bool(uint256 hash, const CWalletTx& wtx));
    MOCK_METHOD1(WriteWitnessCacheSize, bool(int64_t nWitnessCacheSize));
    MOCK_METHOD1(WriteBestBlock, bool(const CBlockLocator& loc));
};

template void CWallet::SetBestChainINTERNAL<MockWalletDB>(
        MockWalletDB& walletdb, const CBlockLocator& loc);

class TestWallet : public CWallet {
public:
    TestWallet() : CWallet() { }

    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn) {
        return CCryptoKeyStore::EncryptKeys(vMasterKeyIn);
    }

    bool Unlock(const CKeyingMaterial& vMasterKeyIn) {
        return CCryptoKeyStore::Unlock(vMasterKeyIn);
    }

    void IncrementNoteWitnesses(const CBlockIndex* pindex,
                                const CBlock* pblock,
                                SproutMerkleTree& sproutTree,
                                SaplingMerkleTree& saplingTree) {
        CWallet::IncrementNoteWitnesses(pindex, pblock, sproutTree, saplingTree);
    }
    void DecrementNoteWitnesses(const CBlockIndex* pindex) {
        CWallet::DecrementNoteWitnesses(pindex);
    }
    void SetBestChain(MockWalletDB& walletdb, const CBlockLocator& loc) {
        CWallet::SetBestChainINTERNAL(walletdb, loc);
    }
    bool UpdatedNoteData(const CWalletTx& wtxIn, CWalletTx& wtx) {
        return CWallet::UpdatedNoteData(wtxIn, wtx);
    }
    void MarkAffectedTransactionsDirty(const CTransaction& tx) {
        CWallet::MarkAffectedTransactionsDirty(tx);
    }
};

CWalletTx GetValidSproutReceive(const libzcash::SproutSpendingKey& sk, CAmount value, bool randomInputs, int32_t version = 2) {
    return GetValidSproutReceive(*params, sk, value, randomInputs, version);
}

CWalletTx GetInvalidCommitmentSproutReceive(const libzcash::SproutSpendingKey& sk, CAmount value, bool randomInputs, int32_t version = 2) {
    return GetInvalidCommitmentSproutReceive(*params, sk, value, randomInputs, version);
}

libzcash::SproutNote GetSproutNote(const libzcash::SproutSpendingKey& sk,
                       const CTransaction& tx, size_t js, size_t n) {
    return GetSproutNote(*params, sk, tx, js, n);
}

CWalletTx GetValidSproutSpend(const libzcash::SproutSpendingKey& sk,
                        const libzcash::SproutNote& note, CAmount value) {
    return GetValidSproutSpend(*params, sk, note, value);
}

std::vector<SaplingOutPoint> SetSaplingNoteData(CWalletTx& wtx) {
    mapSaplingNoteData_t saplingNoteData;
    SaplingOutPoint saplingOutPoint = {wtx.GetHash(), 0};
    SaplingNoteData saplingNd;
    saplingNoteData[saplingOutPoint] = saplingNd;
    wtx.SetSaplingNoteData(saplingNoteData);
    std::vector<SaplingOutPoint> saplingNotes {saplingOutPoint};
    return saplingNotes;
}

std::pair<JSOutPoint, SaplingOutPoint> CreateValidBlock(TestWallet& wallet,
                            const libzcash::SproutSpendingKey& sk,
                            const CBlockIndex& index,
                            CBlock& block,
                            SproutMerkleTree& sproutTree,
                            SaplingMerkleTree& saplingTree) {
    auto wtx = GetValidSproutReceive(sk, 50, true, 4);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;
    wtx.SetSproutNoteData(noteData);
    auto saplingNotes = SetSaplingNoteData(wtx);
    wallet.AddToWallet(wtx, true, NULL);

    block.vtx.push_back(wtx);
    wallet.IncrementNoteWitnesses(&index, &block, sproutTree, saplingTree);

    return std::make_pair(jsoutpt, saplingNotes[0]);
}

std::pair<uint256, uint256> GetWitnessesAndAnchors(TestWallet& wallet,
                                std::vector<JSOutPoint>& sproutNotes,
                                std::vector<SaplingOutPoint>& saplingNotes,
                                std::vector<boost::optional<SproutWitness>>& sproutWitnesses,
                                std::vector<boost::optional<SaplingWitness>>& saplingWitnesses) {
    sproutWitnesses.clear();
    saplingWitnesses.clear();
    uint256 sproutAnchor;
    uint256 saplingAnchor;
    wallet.GetSproutNoteWitnesses(sproutNotes, sproutWitnesses, sproutAnchor);
    wallet.GetSaplingNoteWitnesses(saplingNotes, saplingWitnesses, saplingAnchor);
    return std::make_pair(sproutAnchor, saplingAnchor);
}

TEST(WalletTests, SetupDatadirLocationRunAsFirstTest) {
    // Get temporary and unique path for file.
    boost::filesystem::path pathTemp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    boost::filesystem::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();
}

TEST(WalletTests, SproutNoteDataSerialisation) {
    auto sk = libzcash::SproutSpendingKey::random();
    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    SproutMerkleTree tree;
    nd.witnesses.push_front(tree.witness());
    noteData[jsoutpt] = nd;

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << noteData;

    mapSproutNoteData_t noteData2;
    ss >> noteData2;

    EXPECT_EQ(noteData, noteData2);
    EXPECT_EQ(noteData[jsoutpt].witnesses, noteData2[jsoutpt].witnesses);
}


TEST(WalletTests, FindUnspentSproutNotes) {
    SelectParams(CBaseChainParams::TESTNET);
    CWallet wallet;
    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteData);
    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier));

    // We currently have an unspent and unconfirmed note in the wallet (depth of -1)
    std::vector<SproutNoteEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 0);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", -1);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    wtx.SetMerkleBranch(block);
    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier));


    // We now have an unspent and confirmed note in the wallet (depth of 1)
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 0);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 1);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 2);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();


    // Let's spend the note.
    auto wtx2 = GetValidSproutSpend(sk, note, 5);
    wallet.AddToWallet(wtx2, true, NULL);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier));

    // Fake-mine a spend transaction
    EXPECT_EQ(0, chainActive.Height());
    CBlock block2;
    block2.vtx.push_back(wtx2);
    block2.hashMerkleRoot = block2.BuildMerkleTree();
    block2.hashPrevBlock = blockHash;
    auto blockHash2 = block2.GetHash();
    CBlockIndex fakeIndex2 {block2};
    mapBlockIndex.insert(std::make_pair(blockHash2, &fakeIndex2));
    fakeIndex2.nHeight = 1;
    chainActive.SetTip(&fakeIndex2);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex2));
    EXPECT_EQ(1, chainActive.Height());

    wtx2.SetMerkleBranch(block2);
    wallet.AddToWallet(wtx2, true, NULL);
    EXPECT_TRUE(wallet.IsSproutSpent(nullifier));

    // The note has been spent.  By default, GetFilteredNotes() ignores spent notes.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 0);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    // Let's include spent notes to retrieve it.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 0, false);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    // The spent note has two confirmations.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 2, false);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    // It does not have 3 confirmations.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 3, false);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();


    // Let's receive a new note
    CWalletTx wtx3;
    {
        auto wtx = GetValidSproutReceive(sk, 20, true);
        auto note = GetSproutNote(sk, wtx, 0, 1);
        auto nullifier = note.nullifier(sk);

        mapSproutNoteData_t noteData;
        JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
        SproutNoteData nd {sk.address(), nullifier};
        noteData[jsoutpt] = nd;

        wtx.SetSproutNoteData(noteData);
        wallet.AddToWallet(wtx, true, NULL);
        EXPECT_FALSE(wallet.IsSproutSpent(nullifier));

        wtx3 = wtx;
    }

    // Fake-mine the new transaction
    EXPECT_EQ(1, chainActive.Height());
    CBlock block3;
    block3.vtx.push_back(wtx3);
    block3.hashMerkleRoot = block3.BuildMerkleTree();
    block3.hashPrevBlock = blockHash2;
    auto blockHash3 = block3.GetHash();
    CBlockIndex fakeIndex3 {block3};
    mapBlockIndex.insert(std::make_pair(blockHash3, &fakeIndex3));
    fakeIndex3.nHeight = 2;
    chainActive.SetTip(&fakeIndex3);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex3));
    EXPECT_EQ(2, chainActive.Height());

    wtx3.SetMerkleBranch(block3);
    wallet.AddToWallet(wtx3, true, NULL);

    // We now have an unspent note which has one confirmation, in addition to our spent note.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 1);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    // Let's return the spent note too.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 1, false);
    EXPECT_EQ(2, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    // Increasing number of confirmations will exclude our new unspent note.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 2, false);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    // If we also ignore spent notes at this depth, we won't find any notes.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, "", 2, true);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);
    mapBlockIndex.erase(blockHash2);
    mapBlockIndex.erase(blockHash3);
}


TEST(WalletTests, SetSproutNoteAddrsInCWalletTx) {
    auto sk = libzcash::SproutSpendingKey::random();
    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);
    EXPECT_EQ(0, wtx.mapSproutNoteData.size());

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteData);
    EXPECT_EQ(noteData, wtx.mapSproutNoteData);
}

TEST(WalletTests, SetSaplingNoteAddrsInCWalletTx) {
    auto consensusParams = RegtestActivateSapling();

    TestWallet wallet;

    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto fvk = expsk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    auto pk = sk.DefaultAddress();

    libzcash::SaplingNote note(pk, 50000);
    auto cm = note.cm().get();
    SaplingMerkleTree tree;
    tree.append(cm);
    auto anchor = tree.root();
    auto witness = tree.witness();

    auto nf = note.nullifier(fvk, witness.position());
    ASSERT_TRUE(nf);
    uint256 nullifier = nf.get();

    auto builder = TransactionBuilder(consensusParams, 1, expiryDelta);
    builder.AddSaplingSpend(expsk, note, anchor, witness);
    builder.AddSaplingOutput(fvk.ovk, pk, 50000, {});
    builder.SetFee(0);
    auto tx = builder.Build().GetTxOrThrow();

    CWalletTx wtx {&wallet, tx};

    EXPECT_EQ(0, wtx.mapSaplingNoteData.size());
    mapSaplingNoteData_t noteData;

    SaplingOutPoint op {wtx.GetHash(), 0};
    SaplingNoteData nd;
    nd.nullifier = nullifier;
    nd.ivk = ivk;
    nd.witnesses.push_front(witness);
    nd.witnessHeight = 123;
    noteData.insert(std::make_pair(op, nd));

    wtx.SetSaplingNoteData(noteData);
    EXPECT_EQ(noteData, wtx.mapSaplingNoteData);

    // Test individual fields in case equality operator is defined/changed.
    EXPECT_EQ(ivk, wtx.mapSaplingNoteData[op].ivk);
    EXPECT_EQ(nullifier, wtx.mapSaplingNoteData[op].nullifier);
    EXPECT_EQ(nd.witnessHeight, wtx.mapSaplingNoteData[op].witnessHeight);
    EXPECT_TRUE(witness == wtx.mapSaplingNoteData[op].witnesses.front());

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, SetSproutInvalidNoteAddrsInCWalletTx) {
    CWalletTx wtx;
    EXPECT_EQ(0, wtx.mapSproutNoteData.size());

    mapSproutNoteData_t noteData;
    auto sk = libzcash::SproutSpendingKey::random();
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), uint256()};
    noteData[jsoutpt] = nd;

    EXPECT_THROW(wtx.SetSproutNoteData(noteData), std::logic_error);
}

// The following test is the same as SetInvalidSaplingNoteDataInCWalletTx
// TEST(WalletTests, SetSaplingInvalidNoteAddrsInCWalletTx)

// Cannot add note data for an index which does not exist in tx.vShieldedOutput
TEST(WalletTests, SetInvalidSaplingNoteDataInCWalletTx) {
    CWalletTx wtx;
    EXPECT_EQ(0, wtx.mapSaplingNoteData.size());

    mapSaplingNoteData_t noteData;
    SaplingOutPoint op {uint256(), 1};
    SaplingNoteData nd;
    noteData.insert(std::make_pair(op, nd));

    EXPECT_THROW(wtx.SetSaplingNoteData(noteData), std::logic_error);
}

TEST(WalletTests, CheckSproutNoteCommitmentAgainstNotePlaintext) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    auto address = sk.address();
    auto dec = ZCNoteDecryption(sk.receiving_key());

    auto wtx = GetInvalidCommitmentSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto hSig = wtx.vjoinsplit[0].h_sig(
        *params, wtx.joinSplitPubKey);

    ASSERT_THROW(wallet.GetSproutNoteNullifier(
        wtx.vjoinsplit[0],
        address,
        dec,
        hSig, 1), libzcash::note_decryption_failed);
}

TEST(WalletTests, GetSproutNoteNullifier) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    auto address = sk.address();
    auto dec = ZCNoteDecryption(sk.receiving_key());

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto hSig = wtx.vjoinsplit[0].h_sig(
        *params, wtx.joinSplitPubKey);

    auto ret = wallet.GetSproutNoteNullifier(
        wtx.vjoinsplit[0],
        address,
        dec,
        hSig, 1);
    EXPECT_NE(nullifier, ret);

    wallet.AddSproutSpendingKey(sk);

    ret = wallet.GetSproutNoteNullifier(
        wtx.vjoinsplit[0],
        address,
        dec,
        hSig, 1);
    EXPECT_EQ(nullifier, ret);
}

TEST(WalletTests, FindMySaplingNotes) {
    auto consensusParams = RegtestActivateSapling();

    TestWallet wallet;

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto fvk = expsk.full_viewing_key();
    auto pa = sk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Generate transaction
    auto builder = TransactionBuilder(consensusParams, 1, expiryDelta);
    builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
    builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    // No Sapling notes can be found in tx which does not belong to the wallet
    CWalletTx wtx {&wallet, tx};
    ASSERT_FALSE(wallet.HaveSaplingSpendingKey(fvk));
    auto noteMap = wallet.FindMySaplingNotes(wtx).first;
    EXPECT_EQ(0, noteMap.size());

    // Add spending key to wallet, so Sapling notes can be found
    ASSERT_TRUE(wallet.AddSaplingZKey(sk, pa));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk));
    noteMap = wallet.FindMySaplingNotes(wtx).first;
    EXPECT_EQ(2, noteMap.size());

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, FindMySproutNotes) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    auto sk2 = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk2);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto noteMap = wallet.FindMySproutNotes(wtx);
    EXPECT_EQ(0, noteMap.size());

    wallet.AddSproutSpendingKey(sk);

    noteMap = wallet.FindMySproutNotes(wtx);
    EXPECT_EQ(2, noteMap.size());

    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    EXPECT_EQ(1, noteMap.count(jsoutpt));
    EXPECT_EQ(nd, noteMap[jsoutpt]);
}

TEST(WalletTests, FindMySproutNotesInEncryptedWallet) {
    TestWallet wallet;
    uint256 r {GetRandHash()};
    CKeyingMaterial vMasterKey (r.begin(), r.end());

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    ASSERT_TRUE(wallet.EncryptKeys(vMasterKey));

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto noteMap = wallet.FindMySproutNotes(wtx);
    EXPECT_EQ(2, noteMap.size());

    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    EXPECT_EQ(1, noteMap.count(jsoutpt));
    EXPECT_NE(nd, noteMap[jsoutpt]);

    ASSERT_TRUE(wallet.Unlock(vMasterKey));

    noteMap = wallet.FindMySproutNotes(wtx);
    EXPECT_EQ(2, noteMap.size());
    EXPECT_EQ(1, noteMap.count(jsoutpt));
    EXPECT_EQ(nd, noteMap[jsoutpt]);
}

TEST(WalletTests, GetConflictedSproutNotes) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto wtx2 = GetValidSproutSpend(sk, note, 5);
    auto wtx3 = GetValidSproutSpend(sk, note, 10);
    auto hash2 = wtx2.GetHash();
    auto hash3 = wtx3.GetHash();

    // No conflicts for no spends
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());
    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());

    // No conflicts for one spend
    wallet.AddToWallet(wtx2, true, NULL);
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());

    // Conflicts for two spends
    wallet.AddToWallet(wtx3, true, NULL);
    auto c3 = wallet.GetConflicts(hash2);
    EXPECT_EQ(2, c3.size());
    EXPECT_EQ(std::set<uint256>({hash2, hash3}), c3);
}

// Generate note A and spend to create note B, from which we spend to create two conflicting transactions
TEST(WalletTests, GetConflictedSaplingNotes) {
    auto consensusParams = RegtestActivateSapling();

    TestWallet wallet;

    // Generate Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto fvk = expsk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    auto pk = sk.DefaultAddress();

    ASSERT_TRUE(wallet.AddSaplingZKey(sk, pk));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk));

    // Generate note A
    libzcash::SaplingNote note(pk, 50000);
    auto cm = note.cm().get();
    SaplingMerkleTree saplingTree;
    saplingTree.append(cm);
    auto anchor = saplingTree.root();
    auto witness = saplingTree.witness();

    // Generate tx to create output note B
    auto builder = TransactionBuilder(consensusParams, 1, expiryDelta);
    builder.AddSaplingSpend(expsk, note, anchor, witness);
    builder.AddSaplingOutput(fvk.ovk, pk, 35000, {});
    auto tx = builder.Build().GetTxOrThrow();
    CWalletTx wtx {&wallet, tx};

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    SproutMerkleTree sproutTree;
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
    auto saplingNoteData = wallet.FindMySaplingNotes(wtx).first;
    ASSERT_TRUE(saplingNoteData.size() > 0);
    wtx.SetSaplingNoteData(saplingNoteData);
    wtx.SetMerkleBranch(block);
    wallet.AddToWallet(wtx, true, NULL);

    // Simulate receiving new block and ChainTip signal
    wallet.IncrementNoteWitnesses(&fakeIndex, &block, sproutTree, saplingTree);
    wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

    // Retrieve the updated wtx from wallet
    uint256 hash = wtx.GetHash();
    wtx = wallet.mapWallet[hash];

    // Decrypt output note B
    auto maybe_pt = libzcash::SaplingNotePlaintext::decrypt(
            wtx.vShieldedOutput[0].encCiphertext,
            ivk,
            wtx.vShieldedOutput[0].ephemeralKey,
            wtx.vShieldedOutput[0].cm);
    ASSERT_EQ(static_cast<bool>(maybe_pt), true);
    auto maybe_note = maybe_pt.get().note(ivk);
    ASSERT_EQ(static_cast<bool>(maybe_note), true);
    auto note2 = maybe_note.get();

    SaplingOutPoint sop0(wtx.GetHash(), 0);
    auto spend_note_witness =  wtx.mapSaplingNoteData[sop0].witnesses.front();
    auto maybe_nf = note2.nullifier(fvk, spend_note_witness.position());
    ASSERT_EQ(static_cast<bool>(maybe_nf), true);
    auto nullifier2 = maybe_nf.get();

    anchor = saplingTree.root();

    // Create transaction to spend note B
    auto builder2 = TransactionBuilder(consensusParams, 2, expiryDelta);
    builder2.AddSaplingSpend(expsk, note2, anchor, spend_note_witness);
    builder2.AddSaplingOutput(fvk.ovk, pk, 20000, {});
    auto tx2 = builder2.Build().GetTxOrThrow();

    // Create conflicting transaction which also spends note B
    auto builder3 = TransactionBuilder(consensusParams, 2, expiryDelta);
    builder3.AddSaplingSpend(expsk, note2, anchor, spend_note_witness);
    builder3.AddSaplingOutput(fvk.ovk, pk, 19999, {});
    auto tx3 = builder3.Build().GetTxOrThrow();

    CWalletTx wtx2 {&wallet, tx2};
    CWalletTx wtx3 {&wallet, tx3};

    auto hash2 = wtx2.GetHash();
    auto hash3 = wtx3.GetHash();

    // No conflicts for no spends (wtx is currently the only transaction in the wallet)
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());
    EXPECT_EQ(0, wallet.GetConflicts(hash3).size());

    // No conflicts for one spend
    wallet.AddToWallet(wtx2, true, NULL);
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());

    // Conflicts for two spends
    wallet.AddToWallet(wtx3, true, NULL);
    auto c3 = wallet.GetConflicts(hash2);
    EXPECT_EQ(2, c3.size());
    EXPECT_EQ(std::set<uint256>({hash2, hash3}), c3);

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, SproutNullifierIsSpent) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    EXPECT_FALSE(wallet.IsSproutSpent(nullifier));

    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier));

    auto wtx2 = GetValidSproutSpend(sk, note, 5);
    wallet.AddToWallet(wtx2, true, NULL);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx2);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    wtx2.SetMerkleBranch(block);
    wallet.AddToWallet(wtx2, true, NULL);
    EXPECT_TRUE(wallet.IsSproutSpent(nullifier));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);
}

TEST(WalletTests, SaplingNullifierIsSpent) {
    auto consensusParams = RegtestActivateSapling();

    TestWallet wallet;

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto fvk = expsk.full_viewing_key();
    auto pa = sk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Generate transaction
    auto builder = TransactionBuilder(consensusParams, 1, expiryDelta);
    builder.AddSaplingSpend(expsk,  testNote.note, testNote.tree.root(), testNote.tree.witness());
    builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    CWalletTx wtx {&wallet, tx};
    ASSERT_TRUE(wallet.AddSaplingZKey(sk, pa));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk));

    // Manually compute the nullifier based on the known position
    auto nf = testNote.note.nullifier(fvk, testNote.tree.witness().position());
    ASSERT_TRUE(nf);
    uint256 nullifier = nf.get();

    // Verify note has not been spent
    EXPECT_FALSE(wallet.IsSaplingSpent(nullifier));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    wtx.SetMerkleBranch(block);
    wallet.AddToWallet(wtx, true, NULL);

    // Verify note has been spent
    EXPECT_TRUE(wallet.IsSaplingSpent(nullifier));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, NavigateFromSproutNullifierToNote) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteData);

    EXPECT_EQ(0, wallet.mapSproutNullifiersToNotes.count(nullifier));

    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_EQ(1, wallet.mapSproutNullifiersToNotes.count(nullifier));
    EXPECT_EQ(wtx.GetHash(), wallet.mapSproutNullifiersToNotes[nullifier].hash);
    EXPECT_EQ(0, wallet.mapSproutNullifiersToNotes[nullifier].js);
    EXPECT_EQ(1, wallet.mapSproutNullifiersToNotes[nullifier].n);
}

TEST(WalletTests, NavigateFromSaplingNullifierToNote) {
    auto consensusParams = RegtestActivateSapling();

    TestWallet wallet;

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto fvk = expsk.full_viewing_key();
    auto pa = sk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Generate transaction
    auto builder = TransactionBuilder(consensusParams, 1, expiryDelta);
    builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
    builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    CWalletTx wtx {&wallet, tx};
    ASSERT_TRUE(wallet.AddSaplingZKey(sk, pa));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk));

    // Manually compute the nullifier based on the expected position
    auto nf = testNote.note.nullifier(fvk, testNote.tree.witness().position());
    ASSERT_TRUE(nf);
    uint256 nullifier = nf.get();

    // Verify dummy note is unspent
    EXPECT_FALSE(wallet.IsSaplingSpent(nullifier));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    SproutMerkleTree sproutTree;
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
    wtx.SetMerkleBranch(block);
    auto saplingNoteData = wallet.FindMySaplingNotes(wtx).first;
    ASSERT_TRUE(saplingNoteData.size() > 0);
    wtx.SetSaplingNoteData(saplingNoteData);
    wallet.AddToWallet(wtx, true, NULL);

    // Verify dummy note is now spent, as AddToWallet invokes AddToSpends()
    EXPECT_TRUE(wallet.IsSaplingSpent(nullifier));

    // Test invariant: no witnesses means no nullifier.
    EXPECT_EQ(0, wallet.mapSaplingNullifiersToNotes.size());
    for (mapSaplingNoteData_t::value_type &item : wtx.mapSaplingNoteData) {
        SaplingNoteData nd = item.second;
        ASSERT_TRUE(nd.witnesses.empty());
        ASSERT_FALSE(nd.nullifier);
    }

    // Simulate receiving new block and ChainTip signal
    wallet.IncrementNoteWitnesses(&fakeIndex, &block, sproutTree, testNote.tree);
    wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

    // Retrieve the updated wtx from wallet
    uint256 hash = wtx.GetHash();
    wtx = wallet.mapWallet[hash];

    // Verify Sapling nullifiers map to SaplingOutPoints
    EXPECT_EQ(2, wallet.mapSaplingNullifiersToNotes.size());
    for (mapSaplingNoteData_t::value_type &item : wtx.mapSaplingNoteData) {
        SaplingOutPoint op = item.first;
        SaplingNoteData nd = item.second;
        EXPECT_EQ(hash, op.hash);
        EXPECT_EQ(1, nd.witnesses.size());
        ASSERT_TRUE(nd.nullifier);
        auto nf = nd.nullifier.get();
        EXPECT_EQ(1, wallet.mapSaplingNullifiersToNotes.count(nf));
        EXPECT_EQ(op.hash, wallet.mapSaplingNullifiersToNotes[nf].hash);
        EXPECT_EQ(op.n, wallet.mapSaplingNullifiersToNotes[nf].n);
    }

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, SpentSproutNoteIsFromMe) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);
    auto wtx2 = GetValidSproutSpend(sk, note, 5);

    EXPECT_FALSE(wallet.IsFromMe(wtx));
    EXPECT_FALSE(wallet.IsFromMe(wtx2));

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteData);
    EXPECT_FALSE(wallet.IsFromMe(wtx));
    EXPECT_FALSE(wallet.IsFromMe(wtx2));

    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_FALSE(wallet.IsFromMe(wtx));
    EXPECT_TRUE(wallet.IsFromMe(wtx2));
}

// Create note A, spend A to create note B, spend and verify note B is from me.
TEST(WalletTests, SpentSaplingNoteIsFromMe) {
    auto consensusParams = RegtestActivateSapling();

    TestWallet wallet;

    // Generate Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto fvk = expsk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    auto pk = sk.DefaultAddress();

    // Generate Sapling note A
    libzcash::SaplingNote note(pk, 50000);
    auto cm = note.cm().get();
    SaplingMerkleTree saplingTree;
    saplingTree.append(cm);
    auto anchor = saplingTree.root();
    auto witness = saplingTree.witness();

    // Generate transaction, which sends funds to note B
    auto builder = TransactionBuilder(consensusParams, 1, expiryDelta);
    builder.AddSaplingSpend(expsk, note, anchor, witness);
    builder.AddSaplingOutput(fvk.ovk, pk, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    CWalletTx wtx {&wallet, tx};
    ASSERT_TRUE(wallet.AddSaplingZKey(sk, pk));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    SproutMerkleTree sproutTree;
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    auto saplingNoteData = wallet.FindMySaplingNotes(wtx).first;
    ASSERT_TRUE(saplingNoteData.size() > 0);
    wtx.SetSaplingNoteData(saplingNoteData);
    wtx.SetMerkleBranch(block);
    wallet.AddToWallet(wtx, true, NULL);

    // Simulate receiving new block and ChainTip signal.
    // This triggers calculation of nullifiers for notes belonging to this wallet
    // in the output descriptions of wtx.
    wallet.IncrementNoteWitnesses(&fakeIndex, &block, sproutTree, saplingTree);
    wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

    // Retrieve the updated wtx from wallet
    wtx = wallet.mapWallet[wtx.GetHash()];

    // The test wallet never received the fake note which is being spent, so there
    // is no mapping from nullifier to notedata stored in mapSaplingNullifiersToNotes.
    // Therefore the wallet does not know the tx belongs to the wallet.
    EXPECT_FALSE(wallet.IsFromMe(wtx));

    // Manually compute the nullifier and check map entry does not exist
    auto nf = note.nullifier(fvk, witness.position());
    ASSERT_TRUE(nf);
    ASSERT_FALSE(wallet.mapSaplingNullifiersToNotes.count(nf.get()));

    // Decrypt note B
    auto maybe_pt = libzcash::SaplingNotePlaintext::decrypt(
        wtx.vShieldedOutput[0].encCiphertext,
        ivk,
        wtx.vShieldedOutput[0].ephemeralKey,
        wtx.vShieldedOutput[0].cm);
    ASSERT_EQ(static_cast<bool>(maybe_pt), true);
    auto maybe_note = maybe_pt.get().note(ivk);
    ASSERT_EQ(static_cast<bool>(maybe_note), true);
    auto note2 = maybe_note.get();

    // Get witness to retrieve position of note B we want to spend
    SaplingOutPoint sop0(wtx.GetHash(), 0);
    auto spend_note_witness =  wtx.mapSaplingNoteData[sop0].witnesses.front();
    auto maybe_nf = note2.nullifier(fvk, spend_note_witness.position());
    ASSERT_EQ(static_cast<bool>(maybe_nf), true);
    auto nullifier2 = maybe_nf.get();

    // NOTE: Not updating the anchor results in a core dump.  Shouldn't builder just return error?
    // *** Error in `./zcash-gtest': double free or corruption (out): 0x00007ffd8755d990 ***
    anchor = saplingTree.root();

    // Create transaction to spend note B
    auto builder2 = TransactionBuilder(consensusParams, 2, expiryDelta);
    builder2.AddSaplingSpend(expsk, note2, anchor, spend_note_witness);
    builder2.AddSaplingOutput(fvk.ovk, pk, 12500, {});
    auto tx2 = builder2.Build().GetTxOrThrow();
    EXPECT_EQ(tx2.vin.size(), 0);
    EXPECT_EQ(tx2.vout.size(), 0);
    EXPECT_EQ(tx2.vjoinsplit.size(), 0);
    EXPECT_EQ(tx2.vShieldedSpend.size(), 1);
    EXPECT_EQ(tx2.vShieldedOutput.size(), 2);
    EXPECT_EQ(tx2.valueBalance, 10000);

    CWalletTx wtx2 {&wallet, tx2};

    // Fake-mine this tx into the next block
    EXPECT_EQ(0, chainActive.Height());
    CBlock block2;
    block2.vtx.push_back(wtx2);
    block2.hashMerkleRoot = block2.BuildMerkleTree();
    block2.hashPrevBlock = blockHash;
    auto blockHash2 = block2.GetHash();
    CBlockIndex fakeIndex2 {block2};
    mapBlockIndex.insert(std::make_pair(blockHash2, &fakeIndex2));
    fakeIndex2.nHeight = 1;
    chainActive.SetTip(&fakeIndex2);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex2));
    EXPECT_EQ(1, chainActive.Height());

    auto saplingNoteData2 = wallet.FindMySaplingNotes(wtx2).first;
    ASSERT_TRUE(saplingNoteData2.size() > 0);
    wtx2.SetSaplingNoteData(saplingNoteData2);
    wtx2.SetMerkleBranch(block2);
    wallet.AddToWallet(wtx2, true, NULL);

    // Verify note B is spent. AddToWallet invokes AddToSpends which updates mapTxSaplingNullifiers
    EXPECT_TRUE(wallet.IsSaplingSpent(nullifier2));

    // Verify note B belongs to wallet.
    EXPECT_TRUE(wallet.IsFromMe(wtx2));
    ASSERT_TRUE(wallet.mapSaplingNullifiersToNotes.count(nullifier2));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);
    mapBlockIndex.erase(blockHash2);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, CachedWitnessesEmptyChain) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true, 4);
    auto note = GetSproutNote(sk, wtx, 0, 0);
    auto note2 = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);
    auto nullifier2 = note2.nullifier(sk);

    mapSproutNoteData_t sproutNoteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 0};
    JSOutPoint jsoutpt2 {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    SproutNoteData nd2 {sk.address(), nullifier2};
    sproutNoteData[jsoutpt] = nd;
    sproutNoteData[jsoutpt2] = nd2;
    wtx.SetSproutNoteData(sproutNoteData);

    std::vector<JSOutPoint> sproutNotes {jsoutpt, jsoutpt2};
    std::vector<SaplingOutPoint> saplingNotes = SetSaplingNoteData(wtx);

    std::vector<boost::optional<SproutWitness>> sproutWitnesses;
    std::vector<boost::optional<SaplingWitness>> saplingWitnesses;

    ::GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);

    EXPECT_FALSE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_FALSE((bool) saplingWitnesses[0]);

    wallet.AddToWallet(wtx, true, NULL);

    ::GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);

    EXPECT_FALSE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_FALSE((bool) saplingWitnesses[0]);

    CBlock block;
    block.vtx.push_back(wtx);
    CBlockIndex index(block);
    SproutMerkleTree sproutTree;
    SaplingMerkleTree saplingTree;
    wallet.IncrementNoteWitnesses(&index, &block, sproutTree, saplingTree);

    ::GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);

    EXPECT_TRUE((bool) sproutWitnesses[0]);
    EXPECT_TRUE((bool) sproutWitnesses[1]);
    EXPECT_TRUE((bool) saplingWitnesses[0]);

    // Until #1302 is implemented, this should triggger an assertion
    EXPECT_DEATH(wallet.DecrementNoteWitnesses(&index),
                 ".*nWitnessCacheSize > 0.*");
}

TEST(WalletTests, CachedWitnessesChainTip) {
    TestWallet wallet;
    std::pair<uint256, uint256> anchors1;
    CBlock block1;
    SproutMerkleTree sproutTree;
    SaplingMerkleTree saplingTree;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    {
        // First block (case tested in _empty_chain)
        CBlockIndex index1(block1);
        index1.nHeight = 1;
        auto outpts = CreateValidBlock(wallet, sk, index1, block1, sproutTree, saplingTree);

        // Called to fetch anchor
        std::vector<JSOutPoint> sproutNotes {outpts.first};
        std::vector<SaplingOutPoint> saplingNotes {outpts.second};
        std::vector<boost::optional<SproutWitness>> sproutWitnesses;
        std::vector<boost::optional<SaplingWitness>> saplingWitnesses;

        anchors1 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
        EXPECT_NE(anchors1.first, anchors1.second);
    }

    {
        // Second transaction
        auto wtx = GetValidSproutReceive(sk, 50, true, 4);
        auto note = GetSproutNote(sk, wtx, 0, 1);
        auto nullifier = note.nullifier(sk);

        mapSproutNoteData_t sproutNoteData;
        JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
        SproutNoteData nd {sk.address(), nullifier};
        sproutNoteData[jsoutpt] = nd;
        wtx.SetSproutNoteData(sproutNoteData);
        std::vector<SaplingOutPoint> saplingNotes = SetSaplingNoteData(wtx);
        wallet.AddToWallet(wtx, true, NULL);

        std::vector<JSOutPoint> sproutNotes {jsoutpt};
        std::vector<boost::optional<SproutWitness>> sproutWitnesses;
        std::vector<boost::optional<SaplingWitness>> saplingWitnesses;

        GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);

        // Second block
        CBlock block2;
        block2.hashPrevBlock = block1.GetHash();
        block2.vtx.push_back(wtx);
        CBlockIndex index2(block2);
        index2.nHeight = 2;
        SproutMerkleTree sproutTree2 {sproutTree};
        SaplingMerkleTree saplingTree2 {saplingTree};
        wallet.IncrementNoteWitnesses(&index2, &block2, sproutTree2, saplingTree2);

        auto anchors2 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
        EXPECT_NE(anchors2.first, anchors2.second);

        EXPECT_TRUE((bool) sproutWitnesses[0]);
        EXPECT_TRUE((bool) saplingWitnesses[0]);
        EXPECT_NE(anchors1.first, anchors2.first);
        EXPECT_NE(anchors1.second, anchors2.second);

        // Decrementing should give us the previous anchor
        wallet.DecrementNoteWitnesses(&index2);
        auto anchors3 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);
        // Should not equal first anchor because none of these notes had witnesses
        EXPECT_NE(anchors1.first, anchors3.first);
        EXPECT_NE(anchors1.second, anchors3.second);

        // Re-incrementing with the same block should give the same result
        wallet.IncrementNoteWitnesses(&index2, &block2, sproutTree, saplingTree);
        auto anchors4 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
        EXPECT_NE(anchors4.first, anchors4.second);

        EXPECT_TRUE((bool) sproutWitnesses[0]);
        EXPECT_TRUE((bool) saplingWitnesses[0]);
        EXPECT_EQ(anchors2.first, anchors4.first);
        EXPECT_EQ(anchors2.second, anchors4.second);

        // Incrementing with the same block again should not change the cache
        wallet.IncrementNoteWitnesses(&index2, &block2, sproutTree, saplingTree);
        std::vector<boost::optional<SproutWitness>> sproutWitnesses5;
        std::vector<boost::optional<SaplingWitness>> saplingWitnesses5;

        auto anchors5 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses5, saplingWitnesses5);
        EXPECT_NE(anchors5.first, anchors5.second);

        EXPECT_EQ(sproutWitnesses, sproutWitnesses5);
        EXPECT_EQ(saplingWitnesses, saplingWitnesses5);
        EXPECT_EQ(anchors4.first, anchors5.first);
        EXPECT_EQ(anchors4.second, anchors5.second);
    }
}

TEST(WalletTests, CachedWitnessesDecrementFirst) {
    TestWallet wallet;
    SproutMerkleTree sproutTree;
    SaplingMerkleTree saplingTree;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    {
        // First block (case tested in _empty_chain)
        CBlock block1;
        CBlockIndex index1(block1);
        index1.nHeight = 1;
        CreateValidBlock(wallet, sk, index1, block1, sproutTree, saplingTree);
    }

    std::pair<uint256, uint256> anchors2;
    CBlock block2;
    CBlockIndex index2(block2);

    {
        // Second block (case tested in _chain_tip)
        index2.nHeight = 2;
        auto outpts = CreateValidBlock(wallet, sk, index2, block2, sproutTree, saplingTree);

        // Called to fetch anchor
        std::vector<JSOutPoint> sproutNotes {outpts.first};
        std::vector<SaplingOutPoint> saplingNotes {outpts.second};
        std::vector<boost::optional<SproutWitness>> sproutWitnesses;
        std::vector<boost::optional<SaplingWitness>> saplingWitnesses;
        anchors2 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
    }

{
        // Third transaction - never mined
        auto wtx = GetValidSproutReceive(sk, 20, true, 4);
        auto note = GetSproutNote(sk, wtx, 0, 1);
        auto nullifier = note.nullifier(sk);

        mapSproutNoteData_t noteData;
        JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
        SproutNoteData nd {sk.address(), nullifier};
        noteData[jsoutpt] = nd;
        wtx.SetSproutNoteData(noteData);
        std::vector<SaplingOutPoint> saplingNotes = SetSaplingNoteData(wtx);
        wallet.AddToWallet(wtx, true, NULL);

        std::vector<JSOutPoint> sproutNotes {jsoutpt};
        std::vector<boost::optional<SproutWitness>> sproutWitnesses;
        std::vector<boost::optional<SaplingWitness>> saplingWitnesses;

        auto anchors3 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);

        // Decrementing (before the transaction has ever seen an increment)
        // should give us the previous anchor
        wallet.DecrementNoteWitnesses(&index2);

        auto anchors4 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);
        // Should not equal second anchor because none of these notes had witnesses
        EXPECT_NE(anchors2.first, anchors4.first);
        EXPECT_NE(anchors2.second, anchors4.second);

        // Re-incrementing with the same block should give the same result
        wallet.IncrementNoteWitnesses(&index2, &block2, sproutTree, saplingTree);

        auto anchors5 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);
        EXPECT_EQ(anchors3.first, anchors5.first);
        EXPECT_EQ(anchors3.second, anchors5.second);
    }
}

TEST(WalletTests, CachedWitnessesCleanIndex) {
    TestWallet wallet;
    std::vector<CBlock> blocks;
    std::vector<CBlockIndex> indices;
    std::vector<JSOutPoint> sproutNotes;
    std::vector<SaplingOutPoint> saplingNotes;
    std::vector<uint256> sproutAnchors;
    std::vector<uint256> saplingAnchors;
    SproutMerkleTree sproutTree;
    SproutMerkleTree sproutRiTree = sproutTree;
    SaplingMerkleTree saplingTree;
    SaplingMerkleTree saplingRiTree = saplingTree;
    std::vector<boost::optional<SproutWitness>> sproutWitnesses;
    std::vector<boost::optional<SaplingWitness>> saplingWitnesses;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    // Generate a chain
    size_t numBlocks = WITNESS_CACHE_SIZE + 10;
    blocks.resize(numBlocks);
    indices.resize(numBlocks);
    for (size_t i = 0; i < numBlocks; i++) {
        indices[i].nHeight = i;
        auto oldSproutRoot = sproutTree.root();
        auto oldSaplingRoot = saplingTree.root();
        auto outpts = CreateValidBlock(wallet, sk, indices[i], blocks[i], sproutTree, saplingTree);
        EXPECT_NE(oldSproutRoot, sproutTree.root());
        EXPECT_NE(oldSaplingRoot, saplingTree.root());
        sproutNotes.push_back(outpts.first);
        saplingNotes.push_back(outpts.second);

        auto anchors = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
        for (size_t j = 0; j <= i; j++) {
            EXPECT_TRUE((bool) sproutWitnesses[j]);
            EXPECT_TRUE((bool) saplingWitnesses[j]);
        }
        sproutAnchors.push_back(anchors.first);
        saplingAnchors.push_back(anchors.second);
    }

    // Now pretend we are reindexing: the chain is cleared, and each block is
    // used to increment witnesses again.
    for (size_t i = 0; i < numBlocks; i++) {
        SproutMerkleTree sproutRiPrevTree {sproutRiTree};
        SaplingMerkleTree saplingRiPrevTree {saplingRiTree};
        wallet.IncrementNoteWitnesses(&(indices[i]), &(blocks[i]), sproutRiTree, saplingRiTree);

        auto anchors = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
        for (size_t j = 0; j < numBlocks; j++) {
            EXPECT_TRUE((bool) sproutWitnesses[j]);
            EXPECT_TRUE((bool) saplingWitnesses[j]);
        }
        // Should equal final anchor because witness cache unaffected
        EXPECT_EQ(sproutAnchors.back(), anchors.first);
        EXPECT_EQ(saplingAnchors.back(), anchors.second);

        if ((i == 5) || (i == 50)) {
            // Pretend a reorg happened that was recorded in the block files
            {
                wallet.DecrementNoteWitnesses(&(indices[i]));

                auto anchors = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
                for (size_t j = 0; j < numBlocks; j++) {
                    EXPECT_TRUE((bool) sproutWitnesses[j]);
                    EXPECT_TRUE((bool) saplingWitnesses[j]);
                }
                // Should equal final anchor because witness cache unaffected
                EXPECT_EQ(sproutAnchors.back(), anchors.first);
                EXPECT_EQ(saplingAnchors.back(), anchors.second);
            }

            {
                wallet.IncrementNoteWitnesses(&(indices[i]), &(blocks[i]), sproutRiPrevTree, saplingRiPrevTree);
                auto anchors = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
                for (size_t j = 0; j < numBlocks; j++) {
                    EXPECT_TRUE((bool) sproutWitnesses[j]);
                    EXPECT_TRUE((bool) saplingWitnesses[j]);
                }
                // Should equal final anchor because witness cache unaffected
                EXPECT_EQ(sproutAnchors.back(), anchors.first);
                EXPECT_EQ(saplingAnchors.back(), anchors.second);
            }
        }
    }
}

TEST(WalletTests, ClearNoteWitnessCache) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true, 4);
    auto hash = wtx.GetHash();
    auto note = GetSproutNote(sk, wtx, 0, 0);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 0};
    JSOutPoint jsoutpt2 {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;
    wtx.SetSproutNoteData(noteData);
    auto saplingNotes = SetSaplingNoteData(wtx);

    // Pretend we mined the tx by adding a fake witness
    SproutMerkleTree sproutTree;
    wtx.mapSproutNoteData[jsoutpt].witnesses.push_front(sproutTree.witness());
    wtx.mapSproutNoteData[jsoutpt].witnessHeight = 1;
    wallet.nWitnessCacheSize = 1;

    SaplingMerkleTree saplingTree;
    wtx.mapSaplingNoteData[saplingNotes[0]].witnesses.push_front(saplingTree.witness());
    wtx.mapSaplingNoteData[saplingNotes[0]].witnessHeight = 1;
    wallet.nWitnessCacheSize = 2;

    wallet.AddToWallet(wtx, true, NULL);

    std::vector<JSOutPoint> sproutNotes {jsoutpt, jsoutpt2};
    std::vector<boost::optional<SproutWitness>> sproutWitnesses;
    std::vector<boost::optional<SaplingWitness>> saplingWitnesses;

    // Before clearing, we should have a witness for one note
    GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
    EXPECT_TRUE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_TRUE((bool) saplingWitnesses[0]);
    EXPECT_FALSE((bool) saplingWitnesses[1]);
    EXPECT_EQ(1, wallet.mapWallet[hash].mapSproutNoteData[jsoutpt].witnessHeight);
    EXPECT_EQ(1, wallet.mapWallet[hash].mapSaplingNoteData[saplingNotes[0]].witnessHeight);
    EXPECT_EQ(2, wallet.nWitnessCacheSize);

    // After clearing, we should not have a witness for either note
    wallet.ClearNoteWitnessCache();
    auto anchors2 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
    EXPECT_FALSE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_FALSE((bool) saplingWitnesses[0]);
    EXPECT_FALSE((bool) saplingWitnesses[1]);
    EXPECT_EQ(-1, wallet.mapWallet[hash].mapSproutNoteData[jsoutpt].witnessHeight);
    EXPECT_EQ(-1, wallet.mapWallet[hash].mapSaplingNoteData[saplingNotes[0]].witnessHeight);
    EXPECT_EQ(0, wallet.nWitnessCacheSize);
}

TEST(WalletTests, WriteWitnessCache) {
    TestWallet wallet;
    MockWalletDB walletdb;
    CBlockLocator loc;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;
    wtx.SetSproutNoteData(noteData);
    wallet.AddToWallet(wtx, true, NULL);

    // TxnBegin fails
    EXPECT_CALL(walletdb, TxnBegin())
        .WillOnce(Return(false));
    wallet.SetBestChain(walletdb, loc);
    EXPECT_CALL(walletdb, TxnBegin())
        .WillRepeatedly(Return(true));

    // WriteTx fails
    EXPECT_CALL(walletdb, WriteTx(wtx.GetHash(), wtx))
        .WillOnce(Return(false));
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);

    // WriteTx throws
    EXPECT_CALL(walletdb, WriteTx(wtx.GetHash(), wtx))
        .WillOnce(ThrowLogicError());
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);
    EXPECT_CALL(walletdb, WriteTx(wtx.GetHash(), wtx))
        .WillRepeatedly(Return(true));

    // WriteWitnessCacheSize fails
    EXPECT_CALL(walletdb, WriteWitnessCacheSize(0))
        .WillOnce(Return(false));
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);

    // WriteWitnessCacheSize throws
    EXPECT_CALL(walletdb, WriteWitnessCacheSize(0))
        .WillOnce(ThrowLogicError());
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);
    EXPECT_CALL(walletdb, WriteWitnessCacheSize(0))
        .WillRepeatedly(Return(true));

    // WriteBestBlock fails
    EXPECT_CALL(walletdb, WriteBestBlock(loc))
        .WillOnce(Return(false));
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);

    // WriteBestBlock throws
    EXPECT_CALL(walletdb, WriteBestBlock(loc))
        .WillOnce(ThrowLogicError());
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);
    EXPECT_CALL(walletdb, WriteBestBlock(loc))
        .WillRepeatedly(Return(true));

    // TxCommit fails
    EXPECT_CALL(walletdb, TxnCommit())
        .WillOnce(Return(false));
    wallet.SetBestChain(walletdb, loc);
    EXPECT_CALL(walletdb, TxnCommit())
        .WillRepeatedly(Return(true));

    // Everything succeeds
    wallet.SetBestChain(walletdb, loc);
}

TEST(WalletTests, SetBestChainIgnoresTxsWithoutShieldedData) {
    SelectParams(CBaseChainParams::REGTEST);

    TestWallet wallet;
    MockWalletDB walletdb;
    CBlockLocator loc;

    // Set up transparent address
    CKey tsk = AddTestCKeyToKeyStore(wallet);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    // Set up a Sprout address
    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    // Generate a transparent transaction that is ours
    CMutableTransaction t;
    t.vout.resize(1);
    t.vout[0].nValue = 90*CENT;
    t.vout[0].scriptPubKey = scriptPubKey;
    CWalletTx wtxTransparent {nullptr, t};
    wallet.AddToWallet(wtxTransparent, true, nullptr);

    // Generate a Sprout transaction that is ours
    auto wtxSprout = GetValidSproutReceive(sk, 10, true);
    auto noteMap = wallet.FindMySproutNotes(wtxSprout);
    wtxSprout.SetSproutNoteData(noteMap);
    wallet.AddToWallet(wtxSprout, true, nullptr);

    // Generate a Sprout transaction that only involves our transparent address
    auto sk2 = libzcash::SproutSpendingKey::random();
    auto wtxInput = GetValidSproutReceive(sk2, 10, true);
    auto note = GetSproutNote(sk2, wtxInput, 0, 0);
    auto wtxTmp = GetValidSproutSpend(sk2, note, 5);
    CMutableTransaction mtx {wtxTmp};
    mtx.vout[0].scriptPubKey = scriptPubKey;
    CWalletTx wtxSproutTransparent {nullptr, mtx};
    wallet.AddToWallet(wtxSproutTransparent, true, nullptr);

    // Generate a fake Sapling transaction
    CMutableTransaction mtxSapling;
    mtxSapling.fOverwintered = true;
    mtxSapling.nVersion = SAPLING_TX_VERSION;
    mtxSapling.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtxSapling.vShieldedOutput.resize(1);
    mtxSapling.vShieldedOutput[0].cv = libzcash::random_uint256();
    CWalletTx wtxSapling {nullptr, mtxSapling};
    SetSaplingNoteData(wtxSapling);
    wallet.AddToWallet(wtxSapling, true, nullptr);

    // Generate a fake Sapling transaction that would only involve our transparent addresses
    CMutableTransaction mtxSaplingTransparent;
    mtxSaplingTransparent.fOverwintered = true;
    mtxSaplingTransparent.nVersion = SAPLING_TX_VERSION;
    mtxSaplingTransparent.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtxSaplingTransparent.vShieldedOutput.resize(1);
    mtxSaplingTransparent.vShieldedOutput[0].cv = libzcash::random_uint256();
    CWalletTx wtxSaplingTransparent {nullptr, mtxSaplingTransparent};
    wallet.AddToWallet(wtxSaplingTransparent, true, nullptr);

    EXPECT_CALL(walletdb, TxnBegin())
        .WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteTx(wtxTransparent.GetHash(), wtxTransparent))
        .Times(0);
    EXPECT_CALL(walletdb, WriteTx(wtxSprout.GetHash(), wtxSprout))
        .Times(1).WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteTx(wtxSproutTransparent.GetHash(), wtxSproutTransparent))
        .Times(0);
    EXPECT_CALL(walletdb, WriteTx(wtxSapling.GetHash(), wtxSapling))
        .Times(1).WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteTx(wtxSaplingTransparent.GetHash(), wtxSaplingTransparent))
        .Times(0);
    EXPECT_CALL(walletdb, WriteWitnessCacheSize(0))
        .WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteBestBlock(loc))
        .WillOnce(Return(true));
    EXPECT_CALL(walletdb, TxnCommit())
        .WillOnce(Return(true));
    wallet.SetBestChain(walletdb, loc);
}

TEST(WalletTests, UpdateSproutNullifierNoteMap) {
    TestWallet wallet;
    uint256 r {GetRandHash()};
    CKeyingMaterial vMasterKey (r.begin(), r.end());

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    ASSERT_TRUE(wallet.EncryptKeys(vMasterKey));

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    // Pretend that we called FindMySproutNotes while the wallet was locked
    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address()};
    noteData[jsoutpt] = nd;
    wtx.SetSproutNoteData(noteData);

    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_EQ(0, wallet.mapSproutNullifiersToNotes.count(nullifier));

    EXPECT_FALSE(wallet.UpdateNullifierNoteMap());

    ASSERT_TRUE(wallet.Unlock(vMasterKey));

    EXPECT_TRUE(wallet.UpdateNullifierNoteMap());
    EXPECT_EQ(1, wallet.mapSproutNullifiersToNotes.count(nullifier));
    EXPECT_EQ(wtx.GetHash(), wallet.mapSproutNullifiersToNotes[nullifier].hash);
    EXPECT_EQ(0, wallet.mapSproutNullifiersToNotes[nullifier].js);
    EXPECT_EQ(1, wallet.mapSproutNullifiersToNotes[nullifier].n);
}

TEST(WalletTests, UpdatedSproutNoteData) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 0);
    auto note2 = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);
    auto nullifier2 = note2.nullifier(sk);
    auto wtx2 = wtx;

    // First pretend we added the tx to the wallet and
    // we don't have the key for the second note
    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 0};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;
    wtx.SetSproutNoteData(noteData);

    // Pretend we mined the tx by adding a fake witness
    SproutMerkleTree tree;
    wtx.mapSproutNoteData[jsoutpt].witnesses.push_front(tree.witness());
    wtx.mapSproutNoteData[jsoutpt].witnessHeight = 100;

    // Now pretend we added the key for the second note, and
    // the tx was "added" to the wallet again to update it.
    // This happens via the 'z_importkey' RPC method.
    JSOutPoint jsoutpt2 {wtx2.GetHash(), 0, 1};
    SproutNoteData nd2 {sk.address(), nullifier2};
    noteData[jsoutpt2] = nd2;
    wtx2.SetSproutNoteData(noteData);

    // The txs should initially be different
    EXPECT_NE(wtx.mapSproutNoteData, wtx2.mapSproutNoteData);
    EXPECT_EQ(1, wtx.mapSproutNoteData[jsoutpt].witnesses.size());
    EXPECT_EQ(100, wtx.mapSproutNoteData[jsoutpt].witnessHeight);

    // After updating, they should be the same
    EXPECT_TRUE(wallet.UpdatedNoteData(wtx2, wtx));
    EXPECT_EQ(wtx.mapSproutNoteData, wtx2.mapSproutNoteData);
    EXPECT_EQ(1, wtx.mapSproutNoteData[jsoutpt].witnesses.size());
    EXPECT_EQ(100, wtx.mapSproutNoteData[jsoutpt].witnessHeight);
    // TODO: The new note should get witnessed (but maybe not here) (#1350)
}

TEST(WalletTests, UpdatedSaplingNoteData) {
    auto consensusParams = RegtestActivateSapling();

    TestWallet wallet;

    auto m = GetTestMasterSaplingSpendingKey();

    // Generate dummy Sapling address
    auto sk = m.Derive(0);
    auto expsk = sk.expsk;
    auto fvk = expsk.full_viewing_key();
    auto pa = sk.DefaultAddress();

    // Generate dummy recipient Sapling address
    auto sk2 = m.Derive(1);
    auto expsk2 = sk2.expsk;
    auto fvk2 = expsk2.full_viewing_key();
    auto pa2 = sk2.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Generate transaction
    auto builder = TransactionBuilder(consensusParams, 1, expiryDelta);
    builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
    builder.AddSaplingOutput(fvk.ovk, pa2, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    // Wallet contains fvk1 but not fvk2
    CWalletTx wtx {&wallet, tx};
    ASSERT_TRUE(wallet.AddSaplingZKey(sk, pa));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk));
    ASSERT_FALSE(wallet.HaveSaplingSpendingKey(fvk2));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    SproutMerkleTree sproutTree;
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
    auto saplingNoteData = wallet.FindMySaplingNotes(wtx).first;
    ASSERT_TRUE(saplingNoteData.size() == 1); // wallet only has key for change output
    wtx.SetSaplingNoteData(saplingNoteData);
    wtx.SetMerkleBranch(block);
    wallet.AddToWallet(wtx, true, NULL);

    // Simulate receiving new block and ChainTip signal
    wallet.IncrementNoteWitnesses(&fakeIndex, &block, sproutTree, testNote.tree);
    wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

    // Retrieve the updated wtx from wallet
    uint256 hash = wtx.GetHash();
    wtx = wallet.mapWallet[hash];

    // Now lets add key fvk2 so wallet can find the payment note sent to pa2
    ASSERT_TRUE(wallet.AddSaplingZKey(sk2, pa2));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk2));
    CWalletTx wtx2 = wtx;
    auto saplingNoteData2 = wallet.FindMySaplingNotes(wtx2).first;
    ASSERT_TRUE(saplingNoteData2.size() == 2);
    wtx2.SetSaplingNoteData(saplingNoteData2);

    // The payment note has not been witnessed yet, so let's fake the witness.
    SaplingOutPoint sop0(wtx2.GetHash(), 0);
    SaplingOutPoint sop1(wtx2.GetHash(), 1);
    wtx2.mapSaplingNoteData[sop0].witnesses.push_front(testNote.tree.witness());
    wtx2.mapSaplingNoteData[sop0].witnessHeight = 0;

    // The txs are different as wtx is aware of just the change output,
    // whereas wtx2 is aware of both payment and change outputs.
    EXPECT_NE(wtx.mapSaplingNoteData, wtx2.mapSaplingNoteData);
    EXPECT_EQ(1, wtx.mapSaplingNoteData.size());
    EXPECT_EQ(1, wtx.mapSaplingNoteData[sop1].witnesses.size());    // wtx has witness for change

    EXPECT_EQ(2, wtx2.mapSaplingNoteData.size());
    EXPECT_EQ(1, wtx2.mapSaplingNoteData[sop0].witnesses.size());    // wtx2 has fake witness for payment output
    EXPECT_EQ(0, wtx2.mapSaplingNoteData[sop1].witnesses.size());    // wtx2 never had incrementnotewitness called

    // After updating, they should be the same
    EXPECT_TRUE(wallet.UpdatedNoteData(wtx2, wtx));

    // We can't do this:
    // EXPECT_EQ(wtx.mapSaplingNoteData, wtx2.mapSaplingNoteData);
    // because nullifiers (if part of == comparator) have not all been computed
    // Also note that mapwallet[hash] is not updated with the updated wtx.
   // wtx = wallet.mapWallet[hash];

    EXPECT_EQ(2, wtx.mapSaplingNoteData.size());
    EXPECT_EQ(2, wtx2.mapSaplingNoteData.size());
    // wtx copied over the fake witness from wtx2 for the payment output
    EXPECT_EQ(wtx.mapSaplingNoteData[sop0].witnesses.front(), wtx2.mapSaplingNoteData[sop0].witnesses.front());
    // wtx2 never had its change output witnessed even though it has been in wtx
    EXPECT_EQ(0, wtx2.mapSaplingNoteData[sop1].witnesses.size());
    EXPECT_EQ(wtx.mapSaplingNoteData[sop1].witnesses.front(), testNote.tree.witness());

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, MarkAffectedSproutTransactionsDirty) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto hash = wtx.GetHash();
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);
    auto wtx2 = GetValidSproutSpend(sk, note, 5);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {hash, 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteData);
    wallet.AddToWallet(wtx, true, NULL);
    wallet.MarkAffectedTransactionsDirty(wtx);

    // After getting a cached value, the first tx should be clean
    wallet.mapWallet[hash].GetDebit(ISMINE_ALL);
    EXPECT_TRUE(wallet.mapWallet[hash].fDebitCached);

    // After adding the note spend, the first tx should be dirty
    wallet.AddToWallet(wtx2, true, NULL);
    wallet.MarkAffectedTransactionsDirty(wtx2);
    EXPECT_FALSE(wallet.mapWallet[hash].fDebitCached);
}

TEST(WalletTests, MarkAffectedSaplingTransactionsDirty) {
    auto consensusParams = RegtestActivateSapling();

    TestWallet wallet;

    // Generate Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto fvk = expsk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    auto pk = sk.DefaultAddress();

    ASSERT_TRUE(wallet.AddSaplingZKey(sk, pk));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk));

    // Set up transparent address
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    // Generate shielding tx from transparent to Sapling
    // 0.0005 t-ZEC in, 0.0004 z-ZEC out, 0.0001 t-ZEC fee
    auto builder = TransactionBuilder(consensusParams, 1, expiryDelta, &keystore);
    builder.AddTransparentInput(COutPoint(), scriptPubKey, 50000);
    builder.AddSaplingOutput(fvk.ovk, pk, 40000, {});
    auto tx1 = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx1.vin.size(), 1);
    EXPECT_EQ(tx1.vout.size(), 0);
    EXPECT_EQ(tx1.vjoinsplit.size(), 0);
    EXPECT_EQ(tx1.vShieldedSpend.size(), 0);
    EXPECT_EQ(tx1.vShieldedOutput.size(), 1);
    EXPECT_EQ(tx1.valueBalance, -40000);

    CWalletTx wtx {&wallet, tx1};

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    SaplingMerkleTree saplingTree;
    SproutMerkleTree sproutTree;
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
    auto saplingNoteData = wallet.FindMySaplingNotes(wtx).first;
    ASSERT_TRUE(saplingNoteData.size() > 0);
    wtx.SetSaplingNoteData(saplingNoteData);
    wtx.SetMerkleBranch(block);
    wallet.AddToWallet(wtx, true, NULL);

    // Simulate receiving new block and ChainTip signal
    wallet.IncrementNoteWitnesses(&fakeIndex, &block, sproutTree, saplingTree);
    wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

    // Retrieve the updated wtx from wallet
    uint256 hash = wtx.GetHash();
    wtx = wallet.mapWallet[hash];

    // Prepare to spend the note that was just created
    auto maybe_pt = libzcash::SaplingNotePlaintext::decrypt(
            tx1.vShieldedOutput[0].encCiphertext, ivk, tx1.vShieldedOutput[0].ephemeralKey, tx1.vShieldedOutput[0].cm);
    ASSERT_EQ(static_cast<bool>(maybe_pt), true);
    auto maybe_note = maybe_pt.get().note(ivk);
    ASSERT_EQ(static_cast<bool>(maybe_note), true);
    auto note = maybe_note.get();
    auto anchor = saplingTree.root();
    auto witness = saplingTree.witness();

    // Create a Sapling-only transaction
    // 0.0004 z-ZEC in, 0.00025 z-ZEC out, 0.0001 t-ZEC fee, 0.00005 z-ZEC change
    auto builder2 = TransactionBuilder(consensusParams, 2, expiryDelta);
    builder2.AddSaplingSpend(expsk, note, anchor, witness);
    builder2.AddSaplingOutput(fvk.ovk, pk, 25000, {});
    auto tx2 = builder2.Build().GetTxOrThrow();

    EXPECT_EQ(tx2.vin.size(), 0);
    EXPECT_EQ(tx2.vout.size(), 0);
    EXPECT_EQ(tx2.vjoinsplit.size(), 0);
    EXPECT_EQ(tx2.vShieldedSpend.size(), 1);
    EXPECT_EQ(tx2.vShieldedOutput.size(), 2);
    EXPECT_EQ(tx2.valueBalance, 10000);

    CWalletTx wtx2 {&wallet, tx2};
    auto hash2 = wtx2.GetHash();

    wallet.MarkAffectedTransactionsDirty(wtx);

    // After getting a cached value, the first tx should be clean
    wallet.mapWallet[hash].GetDebit(ISMINE_ALL);
    EXPECT_TRUE(wallet.mapWallet[hash].fDebitCached);

    // After adding the note spend, the first tx should be dirty
    wallet.AddToWallet(wtx2, true, NULL);
    wallet.MarkAffectedTransactionsDirty(wtx2);
    EXPECT_FALSE(wallet.mapWallet[hash].fDebitCached);

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, SproutNoteLocking) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto wtx2 = GetValidSproutReceive(sk, 10, true);

    JSOutPoint jsoutpt {wtx.GetHash(), 0, 0};
    JSOutPoint jsoutpt2 {wtx2.GetHash(),0, 0};

    // Test selective locking
    wallet.LockNote(jsoutpt);
    EXPECT_TRUE(wallet.IsLockedNote(jsoutpt));
    EXPECT_FALSE(wallet.IsLockedNote(jsoutpt2));

    // Test selective unlocking
    wallet.UnlockNote(jsoutpt);
    EXPECT_FALSE(wallet.IsLockedNote(jsoutpt));

    // Test multiple locking
    wallet.LockNote(jsoutpt);
    wallet.LockNote(jsoutpt2);
    EXPECT_TRUE(wallet.IsLockedNote(jsoutpt));
    EXPECT_TRUE(wallet.IsLockedNote(jsoutpt2));

    // Test unlock all
    wallet.UnlockAllSproutNotes();
    EXPECT_FALSE(wallet.IsLockedNote(jsoutpt));
    EXPECT_FALSE(wallet.IsLockedNote(jsoutpt2));
}

TEST(WalletTests, SaplingNoteLocking) {
    TestWallet wallet;
    SaplingOutPoint sop1 {uint256(), 1};
    SaplingOutPoint sop2 {uint256(), 2};

    // Test selective locking
    wallet.LockNote(sop1);
    EXPECT_TRUE(wallet.IsLockedNote(sop1));
    EXPECT_FALSE(wallet.IsLockedNote(sop2));

    // Test selective unlocking
    wallet.UnlockNote(sop1);
    EXPECT_FALSE(wallet.IsLockedNote(sop1));

    // Test multiple locking
    wallet.LockNote(sop1);
    wallet.LockNote(sop2);
    EXPECT_TRUE(wallet.IsLockedNote(sop1));
    EXPECT_TRUE(wallet.IsLockedNote(sop2));

    // Test list
    auto v = wallet.ListLockedSaplingNotes();
    EXPECT_EQ(v.size(), 2);
    EXPECT_TRUE(std::find(v.begin(), v.end(), sop1) != v.end());
    EXPECT_TRUE(std::find(v.begin(), v.end(), sop2) != v.end());

    // Test unlock all
    wallet.UnlockAllSaplingNotes();
    EXPECT_FALSE(wallet.IsLockedNote(sop1));
    EXPECT_FALSE(wallet.IsLockedNote(sop2));
}
