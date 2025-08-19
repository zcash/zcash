#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base58.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "fs.h"
#include "key_io.h"
#include "main.h"
#include "primitives/block.h"
#include "random.h"
#include "transaction_builder.h"
#include "gtest/utils.h"
#include "util/test.h"
#include "wallet/wallet.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"

#include <optional>

using ::testing::Return;
using namespace libzcash;

ACTION(ThrowLogicError) {
    throw std::logic_error("Boom");
}

class MockWalletDB {
public:
    MOCK_METHOD0(TxnBegin, bool());
    MOCK_METHOD0(TxnCommit, bool());
    MOCK_METHOD0(TxnAbort, bool());

    MOCK_METHOD1(WriteTx, bool(const CWalletTx& wtx));
    MOCK_METHOD1(WriteOrchardWitnesses, bool(const OrchardWallet& wallet));
    MOCK_METHOD1(WriteWitnessCacheSize, bool(int64_t nWitnessCacheSize));
    MOCK_METHOD1(WriteBestBlock, bool(const CBlockLocator& loc));
};

template void CWallet::SetBestChainINTERNAL<MockWalletDB>(
        MockWalletDB& walletdb, const CBlockLocator& loc);

class TestWallet : public CWallet {
public:
    TestWallet(const CChainParams& params) : CWallet(params) { }

    OrchardWallet& GetOrchardWallet() {
        return orchardWallet;
    }

    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn) {
        return CCryptoKeyStore::EncryptKeys(vMasterKeyIn);
    }

    bool Unlock(const CKeyingMaterial& vMasterKeyIn) {
        return CCryptoKeyStore::Unlock(vMasterKeyIn);
    }

    void IncrementNoteWitnesses(const Consensus::Params& consensus,
                                const CBlockIndex* pindex,
                                const CBlock* pblock,
                                MerkleFrontiers& frontiers,
                                bool performOrchardWalletUpdates) {
        CWallet::IncrementNoteWitnesses(
                consensus, pindex, pblock, frontiers, performOrchardWalletUpdates);
    }


    void DecrementNoteWitnesses(const Consensus::Params& consensus, const CBlockIndex* pindex) {
        CWallet::DecrementNoteWitnesses(consensus, pindex);
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

static std::vector<SaplingOutPoint> SetSaplingNoteData(CWalletTx& wtx, uint32_t idx) {
    mapSaplingNoteData_t saplingNoteData;
    SaplingOutPoint saplingOutPoint = {wtx.GetHash(), idx};
    SaplingNoteData saplingNd;
    saplingNoteData[saplingOutPoint] = saplingNd;
    wtx.SetSaplingNoteData(saplingNoteData);
    std::vector<SaplingOutPoint> saplingNotes {saplingOutPoint};
    return saplingNotes;
}

static std::pair<JSOutPoint, SaplingOutPoint> CreateValidBlock(TestWallet& wallet,
                            const libzcash::SproutSpendingKey& sk,
                            const CBlockIndex& index,
                            CBlock& block,
                            MerkleFrontiers& frontiers) {
    auto wtx = GetValidSproutReceive(sk, 50, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;
    wtx.SetSproutNoteData(noteData);
    auto saplingNotes = SetSaplingNoteData(wtx, 0);
    wallet.LoadWalletTx(wtx);

    block.vtx.push_back(wtx);
    wallet.IncrementNoteWitnesses(Params().GetConsensus(), &index, &block, frontiers, true);

    return std::make_pair(jsoutpt, saplingNotes[0]);
}

static std::pair<uint256, uint256> GetWitnessesAndAnchors(
                                const TestWallet& wallet,
                                const std::vector<JSOutPoint>& sproutNotes,
                                const std::vector<SaplingOutPoint>& saplingNotes,
                                const unsigned int anchorDepth,
                                std::vector<std::optional<SproutWitness>>& sproutWitnesses,
                                std::vector<std::optional<SaplingWitness>>& saplingWitnesses) {
    sproutWitnesses.clear();
    saplingWitnesses.clear();
    uint256 sproutAnchor;
    uint256 saplingAnchor;
    assert(wallet.GetSproutNoteWitnesses(sproutNotes, anchorDepth, sproutWitnesses, sproutAnchor));
    assert(wallet.GetSaplingNoteWitnesses(saplingNotes, anchorDepth, saplingWitnesses, saplingAnchor));
    return std::make_pair(sproutAnchor, saplingAnchor);
}

TEST(WalletTests, SetupDatadirLocationRunAsFirstTest) {
    // Get temporary and unique path for file.
    fs::path pathTemp = fs::temp_directory_path() / fs::unique_path();
    fs::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();
}

TEST(WalletTests, WalletNetworkSerialization) {
    GTEST_SKIP() << "Skipping because it has a race condition and cannot be run in parallel.";
    SelectParams(CBaseChainParams::TESTNET);

    // Get temporary and unique path for file.
    // Note: / operator to append paths
    fs::path pathTemp = fs::temp_directory_path() / fs::unique_path();
    fs::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();

    // create a new testnet wallet and generate a seed
    CWallet wallet(Params(), "wallet.dat");
    wallet.InitLoadWallet(Params(), true);
    wallet.Flush();

    // Stay on TESTNET, make sure wallet can be successfully loaded.
    {
        CWallet restored(Params(), "wallet.dat");
        bool fFirstRunRet;
        EXPECT_EQ(restored.LoadWallet(fFirstRunRet), DB_LOAD_OK);
    }

    // now, switch to mainnet and attempt to restore the wallet
    // using the same wallet.dat
    SelectParams(CBaseChainParams::MAIN);
    CWallet restored(Params(), "wallet.dat");

    // load should fail due to being associated with the wrong network
    bool fFirstRunRet;
    EXPECT_EQ(restored.LoadWallet(fFirstRunRet), DB_WRONG_NETWORK);
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
    auto os = WithVersion(&ss, SAPLING_TX_VERSION | 1 << 31);
    os << noteData;

    mapSproutNoteData_t noteData2;
    os >> noteData2;

    EXPECT_EQ(noteData, noteData2);
    EXPECT_EQ(noteData[jsoutpt].witnesses, noteData2[jsoutpt].witnesses);
}

TEST(WalletTests, FindUnspentSproutNotes) {
    SelectParams(CBaseChainParams::TESTNET);

    CWallet wallet(Params());
    LOCK2(cs_main, wallet.cs_wallet);

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
    wallet.LoadWalletTx(wtx);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier, std::nullopt));

    // We currently have an unspent and unconfirmed note in the wallet (depth of -1)
    std::vector<SproutNoteEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    std::vector<OrchardNoteMetadata> orchardEntries;
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 0);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, -1);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    wtx.SetMerkleBranch(block);
    wallet.LoadWalletTx(wtx);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier, std::nullopt));


    // We now have an unspent and confirmed note in the wallet (depth of 1)
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 0);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 1);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 2);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();


    // Let's spend the note.
    auto wtx2 = GetValidSproutSpend(sk, note, 5);
    wallet.LoadWalletTx(wtx2);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier, std::nullopt));

    // Fake-mine a spend transaction
    EXPECT_EQ(0, chainActive.Height());
    CBlock block2;
    block2.vtx.push_back(wtx2);
    block2.hashMerkleRoot = BlockMerkleRoot(block2);
    block2.hashPrevBlock = blockHash;
    auto blockHash2 = block2.GetHash();
    CBlockIndex fakeIndex2 {block2};
    mapBlockIndex.insert(std::make_pair(blockHash2, &fakeIndex2));
    fakeIndex2.nHeight = 1;
    chainActive.SetTip(&fakeIndex2);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex2));
    EXPECT_EQ(1, chainActive.Height());

    wtx2.SetMerkleBranch(block2);
    wallet.LoadWalletTx(wtx2);
    EXPECT_TRUE(wallet.IsSproutSpent(nullifier, std::nullopt));

    // The note has been spent.  By default, GetFilteredNotes() ignores spent notes.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 0);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    // Let's include spent notes to retrieve it.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 0, INT_MAX, false);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    // The spent note has two confirmations.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 2, INT_MAX, false);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    // It does not have 3 confirmations.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, 3, INT_MAX, false);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();


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
        wallet.LoadWalletTx(wtx);
        EXPECT_FALSE(wallet.IsSproutSpent(nullifier, std::nullopt));

        wtx3 = wtx;
    }

    // Fake-mine the new transaction
    EXPECT_EQ(1, chainActive.Height());
    CBlock block3;
    block3.vtx.push_back(wtx3);
    block3.hashMerkleRoot = BlockMerkleRoot(block3);
    block3.hashPrevBlock = blockHash2;
    auto blockHash3 = block3.GetHash();
    CBlockIndex fakeIndex3 {block3};
    mapBlockIndex.insert(std::make_pair(blockHash3, &fakeIndex3));
    fakeIndex3.nHeight = 2;
    chainActive.SetTip(&fakeIndex3);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex3));
    EXPECT_EQ(2, chainActive.Height());

    wtx3.SetMerkleBranch(block3);
    wallet.LoadWalletTx(wtx3);

    // We now have an unspent note which has one confirmation, in addition to our spent note.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 1);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    // Let's return the spent note too.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 1, INT_MAX, false);
    EXPECT_EQ(2, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    // Increasing number of confirmations will exclude our new unspent note.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 2, INT_MAX, false);
    EXPECT_EQ(1, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();
    // If we also ignore spent notes at this depth, we won't find any notes.
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, 2, INT_MAX, true);
    EXPECT_EQ(0, sproutEntries.size());
    sproutEntries.clear();
    saplingEntries.clear();
    orchardEntries.clear();

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
    LoadProofParameters();

    std::vector<libzcash::Zip212Enabled> zip_212_enabled = {libzcash::Zip212Enabled::BeforeZip212, libzcash::Zip212Enabled::AfterZip212};
    const Consensus::Params& (*activations [])() = {RegtestActivateSapling, RegtestActivateCanopy};
    void (*deactivations [])() = {RegtestDeactivateSapling, RegtestDeactivateCanopy};

    for (int ver = 0; ver < zip_212_enabled.size(); ver++) {
        auto consensusParams = (*activations[ver])();

        TestWallet wallet(Params());
        LOCK(wallet.cs_wallet);

        auto sk = GetTestMasterSaplingSpendingKey();
        auto expsk = sk.expsk;
        auto fvk = expsk.full_viewing_key();
        auto ivk = fvk.in_viewing_key();
        auto pk = sk.ToXFVK().DefaultAddress();

        libzcash::SaplingNote note(pk, 50000, zip_212_enabled[ver]);
        auto cm = note.cmu().value();
        SaplingMerkleTree tree;
        tree.append(cm);
        auto anchor = tree.root();
        auto witness = tree.witness();

        auto nf = note.nullifier(fvk, witness.position());
        ASSERT_TRUE(nf);
        uint256 nullifier = nf.value();

        auto builder = TransactionBuilder(Params(), 1, std::nullopt, anchor);
        builder.AddSaplingSpend(sk, note, witness);
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

        (*deactivations[ver])();
    }
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
    SelectParams(CBaseChainParams::REGTEST);
    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    auto sk = libzcash::SproutSpendingKey::random();
    auto address = sk.address();
    auto dec = ZCNoteDecryption(sk.receiving_key());

    auto wtx = GetInvalidCommitmentSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto hSig = ZCJoinSplit::h_sig(
        wtx.vJoinSplit[0].randomSeed,
        wtx.vJoinSplit[0].nullifiers,
        wtx.joinSplitPubKey);

    ASSERT_THROW(wallet.GetSproutNoteNullifier(
        wtx.vJoinSplit[0],
        address,
        dec,
        hSig, 1), libzcash::note_decryption_failed);
}

TEST(WalletTests, GetSproutNoteNullifier) {
    SelectParams(CBaseChainParams::REGTEST);
    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    auto sk = libzcash::SproutSpendingKey::random();
    auto address = sk.address();
    auto dec = ZCNoteDecryption(sk.receiving_key());

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto hSig = ZCJoinSplit::h_sig(
        wtx.vJoinSplit[0].randomSeed,
        wtx.vJoinSplit[0].nullifiers,
        wtx.joinSplitPubKey);

    auto ret = wallet.GetSproutNoteNullifier(
        wtx.vJoinSplit[0],
        address,
        dec,
        hSig, 1);
    EXPECT_NE(nullifier, ret);

    wallet.AddSproutSpendingKey(sk);

    ret = wallet.GetSproutNoteNullifier(
        wtx.vJoinSplit[0],
        address,
        dec,
        hSig, 1);
    EXPECT_EQ(nullifier, ret);
}

TEST(WalletTests, FindMySaplingNotes) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto extfvk = sk.ToXFVK();
    auto pa = extfvk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Generate transaction
    auto builder = TransactionBuilder(Params(), 1, std::nullopt, testNote.tree.root());
    builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
    builder.AddSaplingOutput(extfvk.fvk.ovk, pa, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    // No Sapling notes can be found in tx which does not belong to the wallet
    CWalletTx wtx {&wallet, tx};
    ASSERT_FALSE(wallet.HaveSaplingSpendingKey(extfvk));
    auto noteMap = wallet.FindMySaplingNotes(Params(), wtx, 1).first;
    EXPECT_EQ(0, noteMap.size());

    // Add spending key to wallet, so Sapling notes can be found
    ASSERT_TRUE(wallet.AddSaplingZKey(sk));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk));
    noteMap = wallet.FindMySaplingNotes(Params(), wtx, 1).first;
    EXPECT_EQ(2, noteMap.size());

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, FindMySproutNotes) {
    SelectParams(CBaseChainParams::REGTEST);
    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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
    SelectParams(CBaseChainParams::REGTEST);
    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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
    wallet.LoadWalletTx(wtx);
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());

    // No conflicts for one spend
    wallet.LoadWalletTx(wtx2);
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());

    // Conflicts for two spends
    wallet.LoadWalletTx(wtx3);
    auto c3 = wallet.GetConflicts(hash2);
    EXPECT_EQ(2, c3.size());
    EXPECT_EQ(std::set<uint256>({hash2, hash3}), c3);
}

// Generate note A and spend to create note B, from which we spend to create two conflicting transactions
TEST(WalletTests, GetConflictedSaplingNotes) {
    LoadProofParameters();

    std::vector<libzcash::Zip212Enabled> zip_212_enabled = {libzcash::Zip212Enabled::BeforeZip212, libzcash::Zip212Enabled::AfterZip212};
    const Consensus::Params& (*activations [])() = {RegtestActivateSapling, RegtestActivateCanopy};
    void (*deactivations [])() = {RegtestDeactivateSapling, RegtestDeactivateCanopy};

    for (int ver = 0; ver < zip_212_enabled.size(); ver++) {
        auto consensusParams = (*activations[ver])();

        TestWallet wallet(Params());
        LOCK2(cs_main, wallet.cs_wallet);

        // Generate Sapling address
        auto sk = GetTestMasterSaplingSpendingKey();
        auto expsk = sk.expsk;
        auto extfvk = sk.ToXFVK();
        auto ivk = extfvk.ToIncomingViewingKey();
        auto pk = extfvk.DefaultAddress();

        ASSERT_TRUE(wallet.AddSaplingZKey(sk));
        ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk));

        // Generate note A
        libzcash::SaplingNote note(pk, 50000, zip_212_enabled[ver]);
        auto cm = note.cmu().value();

        MerkleFrontiers frontiers;
        frontiers.sapling.append(cm);
        auto anchor = frontiers.sapling.root();
        auto witness = frontiers.sapling.witness();

        // Generate tx to create output note B
        auto builder = TransactionBuilder(Params(), 1, std::nullopt, anchor);
        builder.AddSaplingSpend(sk, note, witness);
        builder.AddSaplingOutput(extfvk.fvk.ovk, pk, 35000, {});
        auto tx = builder.Build().GetTxOrThrow();
        CWalletTx wtx {&wallet, tx};

        // Fake-mine the transaction
        EXPECT_EQ(-1, chainActive.Height());
        CBlock block;
        block.vtx.push_back(wtx);
        block.hashMerkleRoot = BlockMerkleRoot(block);
        auto blockHash = block.GetHash();
        CBlockIndex fakeIndex {block};
        mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
        chainActive.SetTip(&fakeIndex);
        EXPECT_TRUE(chainActive.Contains(&fakeIndex));
        EXPECT_EQ(0, chainActive.Height());

        // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
        auto saplingNoteData = wallet.FindMySaplingNotes(Params(), wtx,  1).first;
        ASSERT_TRUE(saplingNoteData.size() > 0);
        wtx.SetSaplingNoteData(saplingNoteData);
        wtx.SetMerkleBranch(block);
        wallet.LoadWalletTx(wtx);

        // Simulate receiving new block and ChainTip signal
        wallet.IncrementNoteWitnesses(consensusParams, &fakeIndex, &block, frontiers, true);
        wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

        // Retrieve the updated wtx from wallet
        uint256 hash = wtx.GetHash();
        wtx = wallet.mapWallet[hash];

        // Decrypt output note B
        auto maybe_pt = wtx.DecryptSaplingNote(Params(), SaplingOutPoint(hash, 0));
        ASSERT_EQ(static_cast<bool>(maybe_pt), true);
        auto maybe_note = maybe_pt.value().first.note(ivk);
        ASSERT_EQ(static_cast<bool>(maybe_note), true);
        auto note2 = maybe_note.value();

        SaplingOutPoint sop0(wtx.GetHash(), 0);
        auto spend_note_witness =  wtx.mapSaplingNoteData[sop0].witnesses.front();
        auto maybe_nf = note2.nullifier(extfvk.fvk, spend_note_witness.position());
        ASSERT_EQ(static_cast<bool>(maybe_nf), true);
        auto nullifier2 = maybe_nf.value();

        // Create transaction to spend note B
        auto builder2 = TransactionBuilder(Params(), 2, std::nullopt, spend_note_witness.root());
        builder2.AddSaplingSpend(sk, note2, spend_note_witness);
        builder2.AddSaplingOutput(extfvk.fvk.ovk, pk, 2000, {});
        auto tx2 = builder2.Build().GetTxOrThrow();

        // Create conflicting transaction which also spends note B
        auto builder3 = TransactionBuilder(Params(), 2, std::nullopt, spend_note_witness.root());
        builder3.AddSaplingSpend(sk, note2, spend_note_witness);
        builder3.AddSaplingOutput(extfvk.fvk.ovk, pk, 1999, {});
        auto tx3 = builder3.Build().GetTxOrThrow();

        CWalletTx wtx2 {&wallet, tx2};
        CWalletTx wtx3 {&wallet, tx3};

        auto hash2 = wtx2.GetHash();
        auto hash3 = wtx3.GetHash();

        // No conflicts for no spends (wtx is currently the only transaction in the wallet)
        EXPECT_EQ(0, wallet.GetConflicts(hash2).size());
        EXPECT_EQ(0, wallet.GetConflicts(hash3).size());

        // No conflicts for one spend
        wallet.LoadWalletTx(wtx2);
        EXPECT_EQ(0, wallet.GetConflicts(hash2).size());

        // Conflicts for two spends
        wallet.LoadWalletTx(wtx3);
        auto c3 = wallet.GetConflicts(hash2);
        EXPECT_EQ(2, c3.size());
        EXPECT_EQ(std::set<uint256>({hash2, hash3}), c3);

        // Tear down
        chainActive.SetTip(NULL);
        mapBlockIndex.erase(blockHash);

        (*deactivations[ver])();
    }
}

TEST(WalletTests, GetConflictedOrchardNotes) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateNU5();
    TestWallet wallet(Params());
    wallet.GenerateNewSeed();

    LOCK2(cs_main, wallet.cs_wallet);

    // Create an account.
    auto ufvk = wallet.GenerateNewUnifiedSpendingKey().first;
    auto fvk = ufvk.GetOrchardKey().value();
    auto ivk = fvk.ToIncomingViewingKey();
    libzcash::diversifier_index_t j(0);
    auto recipient = ivk.Address(j);

    uint256 orchardAnchor;

    // Generate transparent funds
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);

    // Generate a bundle containing output note A.
    auto builder = TransactionBuilder(Params(), 1, orchardAnchor, SaplingMerkleTree::empty_root(), &keystore);
    builder.AddTransparentInput(COutPoint(uint256(), 0), scriptPubKey, 5000);
    builder.AddOrchardOutput(std::nullopt, recipient, 4000, {});
    auto maybeTx = builder.Build();
    EXPECT_TRUE(maybeTx.IsTx());
    if (maybeTx.IsError()) {
        std::cerr << "Failed to build transaction: " << maybeTx.GetError() << std::endl;
        GTEST_FAIL();
    }
    auto tx = maybeTx.GetTxOrThrow();
    CWalletTx wtx {&wallet, tx};

    // Fake-mine the transaction
    MerkleFrontiers frontiers;
    OrchardMerkleFrontier orchardTree;
    orchardTree.AppendBundle(wtx.GetOrchardBundle());

    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    fakeIndex.hashFinalOrchardRoot = orchardTree.root();
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
    auto orchardTxMeta = wallet.GetOrchardWallet().AddNotesIfInvolvingMe(wtx);
    ASSERT_TRUE(orchardTxMeta.has_value());
    EXPECT_FALSE(orchardTxMeta.value().empty());
    wtx.SetOrchardTxMeta(orchardTxMeta.value());
    wtx.SetMerkleBranch(block);
    wallet.LoadWalletTx(wtx);

    // Simulate receiving new block and ChainTip signal
    wallet.IncrementNoteWitnesses(consensusParams, &fakeIndex, &block, frontiers, true);

    // Fetch the Orchard note so we can spend it.
    std::vector<SproutNoteEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    std::vector<OrchardNoteMetadata> orchardEntries;
    wallet.GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, std::nullopt, -1);
    EXPECT_EQ(0, sproutEntries.size());
    EXPECT_EQ(0, saplingEntries.size());
    EXPECT_EQ(1, orchardEntries.size());

    // Generate another recipient
    libzcash::diversifier_index_t j2(1);
    auto recipient2 = ivk.Address(j2);

    // Generate tx to spend note A
    auto builder2 = TransactionBuilder(Params(), 2, orchardTree.root(), SaplingMerkleTree::empty_root());
    auto noteToSpend = std::move(wallet.GetOrchardSpendInfo(orchardEntries, 1, orchardTree.root())[0]);
    builder2.AddOrchardSpend(std::move(noteToSpend.first), std::move(noteToSpend.second));
    auto maybeTx2 = builder2.Build();
    EXPECT_TRUE(maybeTx2.IsTx());
    if (maybeTx2.IsError()) {
        std::cerr << "Failed to build transaction: " << maybeTx2.GetError() << std::endl;
        GTEST_FAIL();
    }
    auto tx2 = maybeTx2.GetTxOrThrow();
    CWalletTx wtx2 {&wallet, tx2};

    // Generate conflicting tx to spend note A
    auto noteToSpend2 = std::move(wallet.GetOrchardSpendInfo(orchardEntries, 1, orchardTree.root())[0]);
    auto builder3 = TransactionBuilder(Params(), 2, orchardTree.root(), SaplingMerkleTree::empty_root());
    builder3.AddOrchardSpend(std::move(noteToSpend2.first), std::move(noteToSpend2.second));
    auto maybeTx3 = builder3.Build();
    EXPECT_TRUE(maybeTx3.IsTx());
    if (maybeTx3.IsError()) {
        std::cerr << "Failed to build transaction: " << maybeTx3.GetError() << std::endl;
        GTEST_FAIL();
    }
    auto tx3 = maybeTx3.GetTxOrThrow();
    CWalletTx wtx3 {&wallet, tx3};

    auto hash2 = wtx2.GetHash();
    auto hash3 = wtx3.GetHash();

    // No conflicts for no spends (wtx is currently the only transaction in the wallet)
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());
    EXPECT_EQ(0, wallet.GetConflicts(hash3).size());

    // No conflicts for one spend
    auto orchardTxMeta2 = wallet.GetOrchardWallet().AddNotesIfInvolvingMe(wtx2);
    ASSERT_TRUE(orchardTxMeta2.has_value());
    EXPECT_FALSE(orchardTxMeta2.value().empty());
    wtx2.SetOrchardTxMeta(orchardTxMeta2.value());
    wallet.LoadWalletTx(wtx2);
    EXPECT_EQ(0, wallet.GetConflicts(hash2).size());
    EXPECT_EQ(0, wallet.GetConflicts(hash3).size());

    // Conflicts for two spends
    auto orchardTxMeta3 = wallet.GetOrchardWallet().AddNotesIfInvolvingMe(wtx3);
    ASSERT_TRUE(orchardTxMeta3.has_value());
    EXPECT_FALSE(orchardTxMeta3.value().empty());
    wtx3.SetOrchardTxMeta(orchardTxMeta3.value());
    wallet.LoadWalletTx(wtx3);
    auto c3 = wallet.GetConflicts(hash2);
    EXPECT_EQ(2, c3.size());
    EXPECT_EQ(std::set<uint256>({hash2, hash3}), c3);

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    RegtestDeactivateNU5();
}

TEST(WalletTests, SproutNullifierIsSpent) {
    SelectParams(CBaseChainParams::REGTEST);
    CWallet wallet(Params());
    LOCK2(cs_main, wallet.cs_wallet);

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    EXPECT_FALSE(wallet.IsSproutSpent(nullifier, std::nullopt));

    wallet.LoadWalletTx(wtx);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier, std::nullopt));

    auto wtx2 = GetValidSproutSpend(sk, note, 5);
    wallet.LoadWalletTx(wtx2);
    EXPECT_FALSE(wallet.IsSproutSpent(nullifier, std::nullopt));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx2);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    wtx2.SetMerkleBranch(block);
    wallet.LoadWalletTx(wtx2);
    EXPECT_TRUE(wallet.IsSproutSpent(nullifier, std::nullopt));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);
}

TEST(WalletTests, SaplingNullifierIsSpent) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();
    TestWallet wallet(Params());
    LOCK2(cs_main, wallet.cs_wallet);

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto extfvk = sk.ToXFVK();
    auto pa = extfvk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Generate transaction
    auto builder = TransactionBuilder(Params(), 1, std::nullopt, testNote.tree.root());
    builder.AddSaplingSpend(sk,  testNote.note, testNote.tree.witness());
    builder.AddSaplingOutput(extfvk.fvk.ovk, pa, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    CWalletTx wtx {&wallet, tx};
    ASSERT_TRUE(wallet.AddSaplingZKey(sk));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk));

    // Manually compute the nullifier based on the known position
    auto nf = testNote.note.nullifier(extfvk.fvk, testNote.tree.witness().position());
    ASSERT_TRUE(nf);
    uint256 nullifier = nf.value();

    // Verify note has not been spent
    EXPECT_FALSE(wallet.IsSaplingSpent(nullifier, std::nullopt));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    wtx.SetMerkleBranch(block);
    wallet.LoadWalletTx(wtx);

    // Verify note has been spent
    EXPECT_TRUE(wallet.IsSaplingSpent(nullifier, std::nullopt));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, NavigateFromSproutNullifierToNote) {
    SelectParams(CBaseChainParams::REGTEST);
    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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

    wallet.LoadWalletTx(wtx);
    EXPECT_EQ(1, wallet.mapSproutNullifiersToNotes.count(nullifier));
    EXPECT_EQ(wtx.GetHash(), wallet.mapSproutNullifiersToNotes[nullifier].hash);
    EXPECT_EQ(0, wallet.mapSproutNullifiersToNotes[nullifier].js);
    EXPECT_EQ(1, wallet.mapSproutNullifiersToNotes[nullifier].n);
}

TEST(WalletTests, NavigateFromSaplingNullifierToNote) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();
    TestWallet wallet(Params());
    LOCK2(cs_main, wallet.cs_wallet);

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto extfvk = sk.ToXFVK();
    auto pa = extfvk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Generate transaction
    auto builder = TransactionBuilder(Params(), 1, std::nullopt, testNote.tree.root());
    builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
    builder.AddSaplingOutput(extfvk.fvk.ovk, pa, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    CWalletTx wtx {&wallet, tx};
    ASSERT_TRUE(wallet.AddSaplingZKey(sk));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk));

    // Manually compute the nullifier based on the expected position
    auto nf = testNote.note.nullifier(extfvk.fvk, testNote.tree.witness().position());
    ASSERT_TRUE(nf);
    uint256 nullifier = nf.value();

    SproutMerkleTree sproutFrontier;
    OrchardMerkleFrontier orchardFrontier;
    MerkleFrontiers frontiers = {
        .sprout = sproutFrontier,
        .sapling = testNote.tree,
        .orchard = orchardFrontier,
    };

    // Verify dummy note is unspent
    EXPECT_FALSE(wallet.IsSaplingSpent(nullifier, std::nullopt));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
    wtx.SetMerkleBranch(block);
    auto saplingNoteData = wallet.FindMySaplingNotes(Params(), wtx, chainActive.Height()).first;
    ASSERT_TRUE(saplingNoteData.size() > 0);
    wtx.SetSaplingNoteData(saplingNoteData);
    wallet.LoadWalletTx(wtx);

    // Verify dummy note is now spent, as AddToWallet invokes AddToSpends()
    EXPECT_TRUE(wallet.IsSaplingSpent(nullifier, std::nullopt));

    // Test invariant: no witnesses means no nullifier.
    EXPECT_EQ(0, wallet.mapSaplingNullifiersToNotes.size());
    for (mapSaplingNoteData_t::value_type &item : wtx.mapSaplingNoteData) {
        SaplingNoteData nd = item.second;
        ASSERT_TRUE(nd.witnesses.empty());
        ASSERT_FALSE(nd.nullifier);
    }

    // Simulate receiving new block and ChainTip signal
    wallet.IncrementNoteWitnesses(consensusParams, &fakeIndex, &block, frontiers, true);
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
        auto nf = nd.nullifier->GetRawBytes();
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
    SelectParams(CBaseChainParams::REGTEST);
    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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

    wallet.LoadWalletTx(wtx);
    EXPECT_FALSE(wallet.IsFromMe(wtx));
    EXPECT_TRUE(wallet.IsFromMe(wtx2));
}

// Create note A, spend A to create note B, spend and verify note B is from me.
TEST(WalletTests, SpentSaplingNoteIsFromMe) {
    LoadProofParameters();

    std::vector<libzcash::Zip212Enabled> zip_212_enabled = {libzcash::Zip212Enabled::BeforeZip212, libzcash::Zip212Enabled::AfterZip212};
    const Consensus::Params& (*activations [])() = {RegtestActivateSapling, RegtestActivateCanopy};
    void (*deactivations [])() = {RegtestDeactivateSapling, RegtestDeactivateCanopy};

    for (int ver = 0; ver < zip_212_enabled.size(); ver++) {
        auto consensusParams = (*activations[ver])();

        TestWallet wallet(Params());
        LOCK2(cs_main, wallet.cs_wallet);

        // Generate Sapling address
        auto sk = GetTestMasterSaplingSpendingKey();
        auto expsk = sk.expsk;
        auto extfvk = sk.ToXFVK();
        auto ivk = extfvk.ToIncomingViewingKey();
        auto pk = extfvk.DefaultAddress();

        // Generate Sapling note A
        libzcash::SaplingNote note(pk, 50000, zip_212_enabled[ver]);
        auto cm = note.cmu().value();
        MerkleFrontiers frontiers;
        frontiers.sapling.append(cm);
        auto anchor = frontiers.sapling.root();
        auto witness = frontiers.sapling.witness();

        // Generate transaction, which sends funds to note B
        auto builder = TransactionBuilder(Params(), 1, std::nullopt, anchor);
        builder.AddSaplingSpend(sk, note, witness);
        builder.AddSaplingOutput(extfvk.fvk.ovk, pk, 25000, {});
        auto tx = builder.Build().GetTxOrThrow();

        CWalletTx wtx {&wallet, tx};
        ASSERT_TRUE(wallet.AddSaplingZKey(sk));
        ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk));

        // Fake-mine the transaction
        EXPECT_EQ(-1, chainActive.Height());
        CBlock block;
        block.vtx.push_back(wtx);
        block.hashMerkleRoot = BlockMerkleRoot(block);
        auto blockHash = block.GetHash();
        CBlockIndex fakeIndex {block};
        mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
        chainActive.SetTip(&fakeIndex);
        EXPECT_TRUE(chainActive.Contains(&fakeIndex));
        EXPECT_EQ(0, chainActive.Height());

        auto saplingNoteData = wallet.FindMySaplingNotes(Params(), wtx, 1).first;
        ASSERT_TRUE(saplingNoteData.size() > 0);
        wtx.SetSaplingNoteData(saplingNoteData);
        wtx.SetMerkleBranch(block);
        wallet.LoadWalletTx(wtx);

        // Simulate receiving new block and ChainTip signal.
        // This triggers calculation of nullifiers for notes belonging to this wallet
        // in the output descriptions of wtx.
        wallet.IncrementNoteWitnesses(consensusParams, &fakeIndex, &block, frontiers, true);
        wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

        // Retrieve the updated wtx from wallet
        wtx = wallet.mapWallet[wtx.GetHash()];

        // The test wallet never received the fake note which is being spent, so there
        // is no mapping from nullifier to notedata stored in mapSaplingNullifiersToNotes.
        // Therefore the wallet does not know the tx belongs to the wallet.
        EXPECT_FALSE(wallet.IsFromMe(wtx));

        // Manually compute the nullifier and check map entry does not exist
        auto nf = note.nullifier(extfvk.fvk, witness.position());
        ASSERT_TRUE(nf);
        ASSERT_FALSE(wallet.mapSaplingNullifiersToNotes.count(nf->GetRawBytes()));

        // Decrypt note B
        auto maybe_pt = wtx.DecryptSaplingNote(Params(), SaplingOutPoint(wtx.GetHash(), 0));
        ASSERT_EQ(static_cast<bool>(maybe_pt), true);
        auto maybe_note = maybe_pt.value().first.note(ivk);
        ASSERT_EQ(static_cast<bool>(maybe_note), true);
        auto note2 = maybe_note.value();

        // Get witness to retrieve position of note B we want to spend
        SaplingOutPoint sop0(wtx.GetHash(), 0);
        auto spend_note_witness =  wtx.mapSaplingNoteData[sop0].witnesses.front();
        auto maybe_nf = note2.nullifier(extfvk.fvk, spend_note_witness.position());
        ASSERT_EQ(static_cast<bool>(maybe_nf), true);
        auto nullifier2 = maybe_nf.value();

        // Create transaction to spend note B
        auto builder2 = TransactionBuilder(Params(), 2, std::nullopt, spend_note_witness.root());
        builder2.AddSaplingSpend(sk, note2, spend_note_witness);
        builder2.AddSaplingOutput(extfvk.fvk.ovk, pk, 12500, {});
        auto tx2 = builder2.Build().GetTxOrThrow();
        EXPECT_EQ(tx2.vin.size(), 0);
        EXPECT_EQ(tx2.vout.size(), 0);
        EXPECT_EQ(tx2.vJoinSplit.size(), 0);
        EXPECT_EQ(tx2.GetSaplingSpendsCount(), 1);
        EXPECT_EQ(tx2.GetSaplingOutputsCount(), 2);
        EXPECT_EQ(tx2.GetValueBalanceSapling(), 1000);

        CWalletTx wtx2 {&wallet, tx2};

        // Fake-mine this tx into the next block
        EXPECT_EQ(0, chainActive.Height());
        CBlock block2;
        block2.vtx.push_back(wtx2);
        block2.hashMerkleRoot = BlockMerkleRoot(block2);
        block2.hashPrevBlock = blockHash;
        auto blockHash2 = block2.GetHash();
        CBlockIndex fakeIndex2 {block2};
        mapBlockIndex.insert(std::make_pair(blockHash2, &fakeIndex2));
        fakeIndex2.nHeight = 1;
        chainActive.SetTip(&fakeIndex2);
        EXPECT_TRUE(chainActive.Contains(&fakeIndex2));
        EXPECT_EQ(1, chainActive.Height());

        auto saplingNoteData2 = wallet.FindMySaplingNotes(Params(), wtx2, 2).first;
        ASSERT_TRUE(saplingNoteData2.size() > 0);
        wtx2.SetSaplingNoteData(saplingNoteData2);
        wtx2.SetMerkleBranch(block2);
        wallet.LoadWalletTx(wtx2);

        // Verify note B is spent. LoadWalletTx invokes AddToSpends which updates mapTxSaplingNullifiers
        EXPECT_TRUE(wallet.IsSaplingSpent(nullifier2, std::nullopt));

        // Verify note B belongs to wallet.
        EXPECT_TRUE(wallet.IsFromMe(wtx2));
        ASSERT_TRUE(wallet.mapSaplingNullifiersToNotes.count(nullifier2.GetRawBytes()));

        // Tear down
        chainActive.SetTip(NULL);
        mapBlockIndex.erase(blockHash);
        mapBlockIndex.erase(blockHash2);

        (*deactivations[ver])();
    }
}

TEST(WalletTests, CachedWitnessesEmptyChain) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
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
    std::vector<SaplingOutPoint> saplingNotes = SetSaplingNoteData(wtx, 0);

    std::vector<std::optional<SproutWitness>> sproutWitnesses;
    std::vector<std::optional<SaplingWitness>> saplingWitnesses;

    ::GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, nAnchorConfirmations, sproutWitnesses, saplingWitnesses);

    EXPECT_FALSE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_FALSE((bool) saplingWitnesses[0]);

    wallet.LoadWalletTx(wtx);

    ::GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, nAnchorConfirmations, sproutWitnesses, saplingWitnesses);

    EXPECT_FALSE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_FALSE((bool) saplingWitnesses[0]);

    CBlock block;
    block.vtx.push_back(wtx);
    CBlockIndex index(block);
    MerkleFrontiers frontiers;
    const auto& params = Params().GetConsensus();
    wallet.IncrementNoteWitnesses(params, &index, &block, frontiers, true);

    // this death will occur because there will not be sufficient Sprout witnesses to reach the
    // default anchor depth
    EXPECT_DEATH(::GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, nAnchorConfirmations, sproutWitnesses, saplingWitnesses),
                 "GetSproutNoteWitnesses");

    // add another block; we still don't have enough witnesses
    {
        CBlock another_block;
        CBlockIndex another_index(another_block);
        another_index.nHeight = 1;
        wallet.IncrementNoteWitnesses(params, &another_index, &another_block, frontiers, true);
    }

    EXPECT_DEATH(::GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, nAnchorConfirmations, sproutWitnesses, saplingWitnesses),
                 "GetSproutNoteWitnesses");

    for (int i = 2; i <= 8; i++) {
        CBlock another_block;
        CBlockIndex another_index(another_block);
        another_index.nHeight = i;
        wallet.IncrementNoteWitnesses(params, &another_index, &another_block, frontiers, true);
    }

    CBlock last_block;
    CBlockIndex last_index(last_block);
    last_index.nHeight = 9;
    wallet.IncrementNoteWitnesses(params, &last_index, &last_block, frontiers, true);

    ::GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, nAnchorConfirmations, sproutWitnesses, saplingWitnesses);

    EXPECT_TRUE((bool) sproutWitnesses[0]);
    EXPECT_TRUE((bool) sproutWitnesses[1]);
    EXPECT_TRUE((bool) saplingWitnesses[0]);

    for (int i = 9; i >= 1; i--) {
        CBlock another_block;
        CBlockIndex another_index(another_block);
        another_index.nHeight = i;
        wallet.DecrementNoteWitnesses(params, &another_index);
    }

    // Until #1302 is implemented, this should trigger an assertion
    EXPECT_DEATH(wallet.DecrementNoteWitnesses(params, &index),
                 ".*nWitnessCacheSize > 0.*");
}

TEST(WalletTests, CachedWitnessesChainTip) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    std::pair<uint256, uint256> anchors1;
    CBlock block1;
    MerkleFrontiers frontiers;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    {
        // First block (case tested in _empty_chain)
        CBlockIndex index1(block1);
        index1.nHeight = 1;
        auto outpts = CreateValidBlock(wallet, sk, index1, block1, frontiers);

        // Called to fetch anchor
        std::vector<JSOutPoint> sproutNotes {outpts.first};
        std::vector<SaplingOutPoint> saplingNotes {outpts.second};
        std::vector<std::optional<SproutWitness>> sproutWitnesses;
        std::vector<std::optional<SaplingWitness>> saplingWitnesses;

        anchors1 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
        EXPECT_NE(anchors1.first, anchors1.second);
    }

    {
        // Second transaction
        auto wtx = GetValidSproutReceive(sk, 50, true);
        auto note = GetSproutNote(sk, wtx, 0, 1);
        auto nullifier = note.nullifier(sk);

        mapSproutNoteData_t sproutNoteData;
        JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
        SproutNoteData nd {sk.address(), nullifier};
        sproutNoteData[jsoutpt] = nd;
        wtx.SetSproutNoteData(sproutNoteData);
        std::vector<SaplingOutPoint> saplingNotes = SetSaplingNoteData(wtx, 0);
        wallet.LoadWalletTx(wtx);

        std::vector<JSOutPoint> sproutNotes {jsoutpt};
        std::vector<std::optional<SproutWitness>> sproutWitnesses;
        std::vector<std::optional<SaplingWitness>> saplingWitnesses;

        GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);

        // Second block
        CBlock block2;
        block2.hashPrevBlock = block1.GetHash();
        block2.vtx.push_back(wtx);
        CBlockIndex index2(block2);
        index2.nHeight = 2;
        MerkleFrontiers frontiers2 = {
            .sprout = frontiers.sprout,
            .sapling = frontiers.sapling,
            .orchard = frontiers.orchard,
        };
        wallet.IncrementNoteWitnesses(Params().GetConsensus(), &index2, &block2, frontiers2, true);

        auto anchors2 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
        EXPECT_NE(anchors2.first, anchors2.second);

        EXPECT_TRUE((bool) sproutWitnesses[0]);
        EXPECT_TRUE((bool) saplingWitnesses[0]);
        EXPECT_NE(anchors1.first, anchors2.first);
        EXPECT_NE(anchors1.second, anchors2.second);

        // Decrementing should give us the previous anchor
        wallet.DecrementNoteWitnesses(Params().GetConsensus(), &index2);
        auto anchors3 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);
        // Should not equal first anchor because none of these notes had witnesses
        EXPECT_NE(anchors1.first, anchors3.first);
        EXPECT_NE(anchors1.second, anchors3.second);

        // Re-incrementing with the same block should give the same result
        wallet.IncrementNoteWitnesses(Params().GetConsensus(), &index2, &block2, frontiers, true);
        auto anchors4 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
        EXPECT_NE(anchors4.first, anchors4.second);

        EXPECT_TRUE((bool) sproutWitnesses[0]);
        EXPECT_TRUE((bool) saplingWitnesses[0]);
        EXPECT_EQ(anchors2.first, anchors4.first);
        EXPECT_EQ(anchors2.second, anchors4.second);

        // Incrementing with the same block again should not change the cache
        wallet.IncrementNoteWitnesses(Params().GetConsensus(), &index2, &block2, frontiers, true);
        std::vector<std::optional<SproutWitness>> sproutWitnesses5;
        std::vector<std::optional<SaplingWitness>> saplingWitnesses5;

        auto anchors5 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses5, saplingWitnesses5);
        EXPECT_NE(anchors5.first, anchors5.second);

        EXPECT_EQ(sproutWitnesses, sproutWitnesses5);
        EXPECT_EQ(saplingWitnesses, saplingWitnesses5);
        EXPECT_EQ(anchors4.first, anchors5.first);
        EXPECT_EQ(anchors4.second, anchors5.second);
    }
}

TEST(WalletTests, CachedWitnessesDecrementFirst) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    MerkleFrontiers frontiers;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    {
        // First block (case tested in _empty_chain)
        CBlock block1;
        CBlockIndex index1(block1);
        index1.nHeight = 1;
        CreateValidBlock(wallet, sk, index1, block1, frontiers);
    }

    std::pair<uint256, uint256> anchors2;
    CBlock block2;
    CBlockIndex index2(block2);

    {
        // Second block (case tested in _chain_tip)
        index2.nHeight = 2;
        auto outpts = CreateValidBlock(wallet, sk, index2, block2, frontiers);

        // Called to fetch anchor
        std::vector<JSOutPoint> sproutNotes {outpts.first};
        std::vector<SaplingOutPoint> saplingNotes {outpts.second};
        std::vector<std::optional<SproutWitness>> sproutWitnesses;
        std::vector<std::optional<SaplingWitness>> saplingWitnesses;
        anchors2 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
    }

{
        // Third transaction - never mined
        auto wtx = GetValidSproutReceive(sk, 20, true);
        auto note = GetSproutNote(sk, wtx, 0, 1);
        auto nullifier = note.nullifier(sk);

        mapSproutNoteData_t noteData;
        JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
        SproutNoteData nd {sk.address(), nullifier};
        noteData[jsoutpt] = nd;
        wtx.SetSproutNoteData(noteData);
        std::vector<SaplingOutPoint> saplingNotes = SetSaplingNoteData(wtx, 0);
        wallet.LoadWalletTx(wtx);

        std::vector<JSOutPoint> sproutNotes {jsoutpt};
        std::vector<std::optional<SproutWitness>> sproutWitnesses;
        std::vector<std::optional<SaplingWitness>> saplingWitnesses;

        auto anchors3 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);

        // Decrementing (before the transaction has ever seen an increment)
        // should give us the previous anchor
        wallet.DecrementNoteWitnesses(Params().GetConsensus(), &index2);

        auto anchors4 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);
        // Should not equal second anchor because none of these notes had witnesses
        EXPECT_NE(anchors2.first, anchors4.first);
        EXPECT_NE(anchors2.second, anchors4.second);

        // Re-incrementing with the same block should give the same result
        wallet.IncrementNoteWitnesses(Params().GetConsensus(), &index2, &block2, frontiers, true);

        auto anchors5 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);

        EXPECT_FALSE((bool) sproutWitnesses[0]);
        EXPECT_FALSE((bool) saplingWitnesses[0]);
        EXPECT_EQ(anchors3.first, anchors5.first);
        EXPECT_EQ(anchors3.second, anchors5.second);
    }
}

TEST(WalletTests, CachedWitnessesCleanIndex) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    std::vector<CBlock> blocks;
    std::vector<CBlockIndex> indices;
    std::vector<JSOutPoint> sproutNotes;
    std::vector<SaplingOutPoint> saplingNotes;
    std::vector<uint256> sproutAnchors;
    std::vector<uint256> saplingAnchors;
    MerkleFrontiers frontiers;
    MerkleFrontiers riFrontiers = {
        .sprout = frontiers.sprout,
        .sapling = frontiers.sapling,
        .orchard = frontiers.orchard,
    };
    std::vector<std::optional<SproutWitness>> sproutWitnesses;
    std::vector<std::optional<SaplingWitness>> saplingWitnesses;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    // Generate a chain
    size_t numBlocks = WITNESS_CACHE_SIZE + 10;
    blocks.resize(numBlocks);
    indices.resize(numBlocks);
    for (size_t i = 0; i < numBlocks; i++) {
        indices[i].nHeight = i;
        auto oldSproutRoot = frontiers.sprout.root();
        auto oldSaplingRoot = frontiers.sapling.root();
        auto outpts = CreateValidBlock(wallet, sk, indices[i], blocks[i], frontiers);
        EXPECT_NE(oldSproutRoot, frontiers.sprout.root());
        EXPECT_NE(oldSaplingRoot, frontiers.sapling.root());
        sproutNotes.push_back(outpts.first);
        saplingNotes.push_back(outpts.second);

        auto anchors = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
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
        MerkleFrontiers riPrevFrontiers{riFrontiers};
        wallet.IncrementNoteWitnesses(Params().GetConsensus(), &(indices[i]), &(blocks[i]), riFrontiers, true);

        auto anchors = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
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
                wallet.DecrementNoteWitnesses(Params().GetConsensus(), &(indices[i]));

                auto anchors = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
                for (size_t j = 0; j < numBlocks; j++) {
                    EXPECT_TRUE((bool) sproutWitnesses[j]);
                    EXPECT_TRUE((bool) saplingWitnesses[j]);
                }
                // Should equal final anchor because witness cache unaffected
                EXPECT_EQ(sproutAnchors.back(), anchors.first);
                EXPECT_EQ(saplingAnchors.back(), anchors.second);
            }

            {
                wallet.IncrementNoteWitnesses(Params().GetConsensus(), &(indices[i]), &(blocks[i]), riPrevFrontiers, true);
                auto anchors = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
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
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto hash = wtx.GetHash();
    auto note = GetSproutNote(sk, wtx, 0, 0);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 0};
    JSOutPoint jsoutpt2 {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;
    wtx.SetSproutNoteData(noteData);
    auto saplingNotes = SetSaplingNoteData(wtx, 0);

    // Pretend we mined the tx by adding a fake witness
    SproutMerkleTree sproutTree;
    wtx.mapSproutNoteData[jsoutpt].witnesses.push_front(sproutTree.witness());
    wtx.mapSproutNoteData[jsoutpt].witnessHeight = 1;
    wallet.nWitnessCacheSize = 1;

    SaplingMerkleTree saplingTree;
    wtx.mapSaplingNoteData[saplingNotes[0]].witnesses.push_front(saplingTree.witness());
    wtx.mapSaplingNoteData[saplingNotes[0]].witnessHeight = 1;
    wallet.nWitnessCacheSize = 2;

    wallet.LoadWalletTx(wtx);

    // For Sprout, we have two outputs in the one JSDescription, only one of
    // which is in the wallet.
    std::vector<JSOutPoint> sproutNotes {jsoutpt, jsoutpt2};
    // For Sapling, SetSaplingNoteData() only created a single Sapling output
    // which is in the wallet, so we add a second SaplingOutPoint here to
    // exercise the "note not in wallet" case.
    saplingNotes.emplace_back(wtx.GetHash(), 1);
    ASSERT_EQ(saplingNotes.size(), 2);

    std::vector<std::optional<SproutWitness>> sproutWitnesses;
    std::vector<std::optional<SaplingWitness>> saplingWitnesses;

    // Before clearing, we should have a witness for one note
    GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
    EXPECT_TRUE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_TRUE((bool) saplingWitnesses[0]);
    EXPECT_FALSE((bool) saplingWitnesses[1]);
    EXPECT_EQ(1, wallet.mapWallet[hash].mapSproutNoteData[jsoutpt].witnessHeight);
    EXPECT_EQ(1, wallet.mapWallet[hash].mapSaplingNoteData[saplingNotes[0]].witnessHeight);
    EXPECT_EQ(2, wallet.nWitnessCacheSize);

    // After clearing, we should not have a witness for either note
    wallet.ClearNoteWitnessCache();
    auto anchors2 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, 1, sproutWitnesses, saplingWitnesses);
    EXPECT_FALSE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_FALSE((bool) saplingWitnesses[0]);
    EXPECT_FALSE((bool) saplingWitnesses[1]);
    EXPECT_EQ(-1, wallet.mapWallet[hash].mapSproutNoteData[jsoutpt].witnessHeight);
    EXPECT_EQ(-1, wallet.mapWallet[hash].mapSaplingNoteData[saplingNotes[0]].witnessHeight);
    EXPECT_EQ(0, wallet.nWitnessCacheSize);
}

TEST(WalletTests, WriteWitnessCache) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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
    wallet.LoadWalletTx(wtx);

    // TxnBegin fails
    EXPECT_CALL(walletdb, TxnBegin())
        .WillOnce(Return(false));
    wallet.SetBestChain(walletdb, loc);
    EXPECT_CALL(walletdb, TxnBegin())
        .WillRepeatedly(Return(true));

    // WriteTx fails
    EXPECT_CALL(walletdb, WriteTx(wtx))
        .WillOnce(Return(false));
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);

    // WriteTx throws
    EXPECT_CALL(walletdb, WriteTx(wtx))
        .WillOnce(ThrowLogicError());
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);
    EXPECT_CALL(walletdb, WriteTx(wtx))
        .WillRepeatedly(Return(true));

    // WriteOrchardWitnesses fails
    EXPECT_CALL(walletdb, WriteOrchardWitnesses)
        .WillOnce(Return(false));
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);

    // WriteOrchardWitnesses throws
    EXPECT_CALL(walletdb, WriteOrchardWitnesses)
        .WillOnce(ThrowLogicError());
    EXPECT_CALL(walletdb, TxnAbort())
        .Times(1);
    wallet.SetBestChain(walletdb, loc);
    EXPECT_CALL(walletdb, WriteOrchardWitnesses)
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
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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
    wallet.LoadWalletTx(wtxTransparent);

    // Generate a Sprout transaction that is ours
    auto wtxSprout = GetValidSproutReceive(sk, 10, true);
    auto noteMap = wallet.FindMySproutNotes(wtxSprout);
    wtxSprout.SetSproutNoteData(noteMap);
    wallet.LoadWalletTx(wtxSprout);

    // Generate a Sprout transaction that only involves our transparent address
    auto sk2 = libzcash::SproutSpendingKey::random();
    auto wtxInput = GetValidSproutReceive(sk2, 10, true);
    auto note = GetSproutNote(sk2, wtxInput, 0, 0);
    auto wtxTmp = GetValidSproutSpend(sk2, note, 5);
    CMutableTransaction mtx {wtxTmp};
    mtx.vout[0].scriptPubKey = scriptPubKey;
    mtx.vout[0].nValue = CENT;
    CWalletTx wtxSproutTransparent {nullptr, mtx};
    wallet.LoadWalletTx(wtxSproutTransparent);

    // Generate a fake Sapling transaction
    CMutableTransaction mtxSapling;
    mtxSapling.fOverwintered = true;
    mtxSapling.nVersion = SAPLING_TX_VERSION;
    mtxSapling.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtxSapling.saplingBundle = sapling::test_only_invalid_bundle(0, 1, 0);
    CWalletTx wtxSapling {nullptr, mtxSapling};
    SetSaplingNoteData(wtxSapling, 0);
    wallet.LoadWalletTx(wtxSapling);

    // Generate a fake Sapling transaction that would only involve our transparent addresses
    CMutableTransaction mtxSaplingTransparent;
    mtxSaplingTransparent.fOverwintered = true;
    mtxSaplingTransparent.nVersion = SAPLING_TX_VERSION;
    mtxSaplingTransparent.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtxSaplingTransparent.saplingBundle = sapling::test_only_invalid_bundle(0, 1, 0);
    CWalletTx wtxSaplingTransparent {nullptr, mtxSaplingTransparent};
    wallet.LoadWalletTx(wtxSaplingTransparent);

    EXPECT_CALL(walletdb, TxnBegin())
        .WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteTx(wtxTransparent))
        .Times(0);
    EXPECT_CALL(walletdb, WriteTx(wtxSprout))
        .Times(1).WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteTx(wtxSproutTransparent))
        .Times(0);
    EXPECT_CALL(walletdb, WriteTx(wtxSapling))
        .Times(1).WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteTx(wtxSaplingTransparent))
        .Times(0);
    EXPECT_CALL(walletdb, WriteOrchardWitnesses)
        .WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteWitnessCacheSize(0))
        .WillOnce(Return(true));
    EXPECT_CALL(walletdb, WriteBestBlock(loc))
        .WillOnce(Return(true));
    EXPECT_CALL(walletdb, TxnCommit())
        .WillOnce(Return(true));
    wallet.SetBestChain(walletdb, loc);
}

TEST(WalletTests, UpdateSproutNullifierNoteMap) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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

    wallet.LoadWalletTx(wtx);
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
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();
    TestWallet wallet(Params());
    LOCK2(cs_main, wallet.cs_wallet);

    auto m = GetTestMasterSaplingSpendingKey();

    // Generate dummy Sapling address
    auto sk = m.Derive(0 | HARDENED_KEY_LIMIT);
    auto expsk = sk.expsk;
    auto extfvk = sk.ToXFVK();
    auto pa = extfvk.DefaultAddress();

    // Generate dummy recipient Sapling address
    auto sk2 = m.Derive(1 | HARDENED_KEY_LIMIT);
    auto expsk2 = sk2.expsk;
    auto extfvk2 = sk2.ToXFVK();
    auto pa2 = extfvk2.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Generate transaction
    auto builder = TransactionBuilder(Params(), 1, std::nullopt, testNote.tree.root());
    builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
    builder.AddSaplingOutput(extfvk.fvk.ovk, pa2, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    SproutMerkleTree sproutFrontier;
    OrchardMerkleFrontier orchardFrontier;
    MerkleFrontiers frontiers = {
        .sprout = sproutFrontier,
        .sapling = testNote.tree,
        .orchard = orchardFrontier,
    };

    // Wallet contains extfvk1 but not extfvk2
    CWalletTx wtx {&wallet, tx};
    ASSERT_TRUE(wallet.AddSaplingZKey(sk));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk));
    ASSERT_FALSE(wallet.HaveSaplingSpendingKey(extfvk2));

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
    auto saplingNoteData = wallet.FindMySaplingNotes(Params(), wtx, chainActive.Height()).first;
    ASSERT_EQ(saplingNoteData.size(), 1); // wallet only has key for change output
    SaplingOutPoint sopChange = saplingNoteData.begin()->first;

    wtx.SetSaplingNoteData(saplingNoteData);
    wtx.SetMerkleBranch(block);
    wallet.LoadWalletTx(wtx);

    // Simulate receiving new block and ChainTip signal
    wallet.IncrementNoteWitnesses(consensusParams, &fakeIndex, &block, frontiers, true);
    wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

    // Retrieve the updated wtx from wallet
    uint256 hash = wtx.GetHash();
    wtx = wallet.mapWallet[hash];

    // Now lets add key extfvk2 so wallet can find the payment note sent to pa2
    ASSERT_TRUE(wallet.AddSaplingZKey(sk2));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk2));
    CWalletTx wtx2 = wtx;
    auto saplingNoteData2 = wallet.FindMySaplingNotes(Params(), wtx2, chainActive.Height()).first;
    ASSERT_EQ(saplingNoteData2.size(), 2);

    // Identify the index of the non-change note
    auto sentIndex = sopChange.n == 0 ? 1 : 0;
    wtx2.SetSaplingNoteData(saplingNoteData2);

    // The payment note has not been witnessed yet, so let's fake the witness.
    // The hash of wtx2 is unchanged since it's a copy of wtx, and since wtx's
    // outpoints are in random order, we assign sopNew's index to whichever
    // sopChange didn't use.
    SaplingOutPoint sopNew(wtx2.GetHash(), sentIndex);
    wtx2.mapSaplingNoteData[sopNew].witnesses.push_front(frontiers.sapling.witness());
    wtx2.mapSaplingNoteData[sopNew].witnessHeight = 0;

    // The txs are different as wtx is aware of just the change output,
    // whereas wtx2 is aware of both payment and change outputs.
    EXPECT_NE(wtx.mapSaplingNoteData, wtx2.mapSaplingNoteData);
    EXPECT_EQ(1, wtx.mapSaplingNoteData.size());
    EXPECT_EQ(1, wtx.mapSaplingNoteData[sopChange].witnesses.size());    // wtx has witness for change

    EXPECT_EQ(2, wtx2.mapSaplingNoteData.size());
    EXPECT_EQ(1, wtx2.mapSaplingNoteData[sopNew].witnesses.size());      // wtx2 has fake witness for payment output
    EXPECT_EQ(0, wtx2.mapSaplingNoteData[sopChange].witnesses.size());   // wtx2 never had incrementnotewitness called

    // After updating, they should be the same
    EXPECT_TRUE(wallet.UpdatedNoteData(wtx2, wtx));

    // We can't do this:
    // EXPECT_EQ(wtx.mapSaplingNoteData, wtx2.mapSaplingNoteData);
    // because nullifiers (if part of == comparator) have not all been computed
    // Also note that mapwallet[hash] is not updated with the updated wtx.
    // wtx = wallet.mapWallet[hash];

    EXPECT_EQ(2, wtx2.mapSaplingNoteData.size());
    EXPECT_EQ(0, wtx2.mapSaplingNoteData[sopChange].witnesses.size());

    EXPECT_EQ(2, wtx.mapSaplingNoteData.size());
    // wtx copied over the fake witness from wtx2 for the payment output
    EXPECT_EQ(
        wtx.mapSaplingNoteData[sopNew].witnesses.front(),
        wtx2.mapSaplingNoteData[sopNew].witnesses.front()
    );
    // wtx2 never had its change output witnessed even though it has been in wtx
    EXPECT_EQ(wtx.mapSaplingNoteData[sopChange].witnesses.front().root(), frontiers.sapling.witness().root());
    if (sopChange.n == 1) {
        EXPECT_EQ(wtx.mapSaplingNoteData[sopChange].witnesses.front(), frontiers.sapling.witness());
    } else {
        EXPECT_FALSE(wtx.mapSaplingNoteData[sopChange].witnesses.front() == frontiers.sapling.witness());
    }


    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, MarkAffectedSproutTransactionsDirty) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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
    wallet.LoadWalletTx(wtx);
    wallet.MarkAffectedTransactionsDirty(wtx);

    // After getting a cached value, the first tx should be clean
    wallet.mapWallet[hash].GetDebit(ISMINE_ALL);
    EXPECT_TRUE(wallet.mapWallet[hash].fDebitCached);

    // After adding the note spend, the first tx should be dirty
    wallet.LoadWalletTx(wtx2);
    wallet.MarkAffectedTransactionsDirty(wtx2);
    EXPECT_FALSE(wallet.mapWallet[hash].fDebitCached);
}

TEST(WalletTests, MarkAffectedSaplingTransactionsDirty) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();
    TestWallet wallet(Params());
    LOCK2(cs_main, wallet.cs_wallet);

    // Generate Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto expsk = sk.expsk;
    auto extfvk = sk.ToXFVK();
    auto ivk = extfvk.ToIncomingViewingKey();
    auto pk = extfvk.DefaultAddress();

    ASSERT_TRUE(wallet.AddSaplingZKey(sk));
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk));

    // Set up transparent address
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    // Generate shielding tx from transparent to Sapling
    // 0.0005 t-ZEC in, 0.0004 z-ZEC out, default fee
    auto builder = TransactionBuilder(Params(), 1, std::nullopt, SaplingMerkleTree::empty_root(), &keystore);
    builder.AddTransparentInput(
        COutPoint(uint256S("7777777777777777777777777777777777777777777777777777777777777777"), 0),
        scriptPubKey, 5000);
    builder.AddSaplingOutput(extfvk.fvk.ovk, pk, 4000, {});
    auto tx1 = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx1.vin.size(), 1);
    EXPECT_EQ(tx1.vout.size(), 0);
    EXPECT_EQ(tx1.vJoinSplit.size(), 0);
    EXPECT_EQ(tx1.GetSaplingSpendsCount(), 0);
    EXPECT_EQ(tx1.GetSaplingOutputsCount(), 2);
    EXPECT_EQ(tx1.GetValueBalanceSapling(), -4000);

    CWalletTx wtx {&wallet, tx1};

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    MerkleFrontiers frontiers;
    SproutMerkleTree sproutTree;
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());

    // Simulate SyncTransaction which calls AddToWalletIfInvolvingMe
    auto saplingNoteData = wallet.FindMySaplingNotes(Params(), wtx, chainActive.Height()).first;
    ASSERT_TRUE(saplingNoteData.size() > 0);
    wtx.SetSaplingNoteData(saplingNoteData);
    wtx.SetMerkleBranch(block);
    wallet.LoadWalletTx(wtx);

    // Simulate receiving new block and ChainTip signal
    wallet.IncrementNoteWitnesses(consensusParams, &fakeIndex, &block, frontiers, true);
    wallet.UpdateSaplingNullifierNoteMapForBlock(&block);

    // Retrieve the updated wtx from wallet
    uint256 hash = wtx.GetHash();
    wtx = wallet.mapWallet[hash];

    // Prepare to spend the note that was just created
    auto outpt = SaplingOutPoint(hash, 0);
    auto maybe_pt = wtx.DecryptSaplingNote(Params(), outpt);
    if (!static_cast<bool>(maybe_pt)) {
        outpt = SaplingOutPoint(hash, 1);
        maybe_pt = wtx.DecryptSaplingNote(Params(), outpt);
    }
    ASSERT_EQ(static_cast<bool>(maybe_pt), true);
    auto maybe_note = maybe_pt.value().first.note(ivk);
    ASSERT_EQ(static_cast<bool>(maybe_note), true);
    auto note = maybe_note.value();
    auto anchor = frontiers.sapling.root();
    auto witness = wtx.mapSaplingNoteData[outpt].witnesses.back();
    ASSERT_EQ(anchor, witness.root());

    // Create a Sapling-only transaction
    // 0.0004 z-ZEC in, 0.00025 z-ZEC out, default fee, 0.00005 z-ZEC change
    auto builder2 = TransactionBuilder(Params(), 2, std::nullopt, anchor);
    builder2.AddSaplingSpend(sk, note, witness);
    builder2.AddSaplingOutput(extfvk.fvk.ovk, pk, 2500, {});
    auto tx2 = builder2.Build().GetTxOrThrow();

    EXPECT_EQ(tx2.vin.size(), 0);
    EXPECT_EQ(tx2.vout.size(), 0);
    EXPECT_EQ(tx2.vJoinSplit.size(), 0);
    EXPECT_EQ(tx2.GetSaplingSpendsCount(), 1);
    EXPECT_EQ(tx2.GetSaplingOutputsCount(), 2);
    EXPECT_EQ(tx2.GetValueBalanceSapling(), 1000);

    CWalletTx wtx2 {&wallet, tx2};
    auto hash2 = wtx2.GetHash();

    wallet.MarkAffectedTransactionsDirty(wtx);

    // After getting a cached value, the first tx should be clean
    wallet.mapWallet[hash].GetDebit(ISMINE_ALL);
    EXPECT_TRUE(wallet.mapWallet[hash].fDebitCached);

    // After adding the note spend, the first tx should be dirty
    wallet.LoadWalletTx(wtx2);
    wallet.MarkAffectedTransactionsDirty(wtx2);
    EXPECT_FALSE(wallet.mapWallet[hash].fDebitCached);

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, SproutNoteLocking) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

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

TEST(WalletTests, OrchardNoteLocking) {
    SelectParams(CBaseChainParams::REGTEST);
    TestWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    OrchardOutPoint oop1 {uint256(), 1};
    OrchardOutPoint oop2 {uint256(), 2};

    // Test selective locking
    wallet.LockNote(oop1);
    EXPECT_TRUE(wallet.IsLockedNote(oop1));
    EXPECT_FALSE(wallet.IsLockedNote(oop2));

    // Test selective unlocking
    wallet.UnlockNote(oop1);
    EXPECT_FALSE(wallet.IsLockedNote(oop1));

    // Test multiple locking
    wallet.LockNote(oop1);
    wallet.LockNote(oop2);
    EXPECT_TRUE(wallet.IsLockedNote(oop1));
    EXPECT_TRUE(wallet.IsLockedNote(oop2));

    // Test list
    auto v = wallet.ListLockedOrchardNotes();
    EXPECT_EQ(v.size(), 2);
    EXPECT_TRUE(std::find(v.begin(), v.end(), oop1) != v.end());
    EXPECT_TRUE(std::find(v.begin(), v.end(), oop2) != v.end());

    // Test unlock all
    wallet.UnlockAllOrchardNotes();
    EXPECT_FALSE(wallet.IsLockedNote(oop1));
    EXPECT_FALSE(wallet.IsLockedNote(oop2));
}

TEST(WalletTests, GenerateUnifiedAddress) {
    (void) RegtestActivateSapling();
    TestWallet wallet(Params());

    auto uaResult = wallet.GenerateUnifiedAddress(0, {ReceiverType::P2PKH, ReceiverType::Sapling});

    // If the wallet does not have a mnemonic seed available, it is
    // treated as if the wallet is encrypted.
    EXPECT_FALSE(wallet.IsCrypted());
    EXPECT_FALSE(wallet.GetMnemonicSeed().has_value());
    WalletUAGenerationResult expected = WalletUAGenerationError::WalletEncrypted;
    EXPECT_EQ(uaResult, expected);

    wallet.GenerateNewSeed();
    EXPECT_FALSE(wallet.IsCrypted());
    EXPECT_TRUE(wallet.GetMnemonicSeed().has_value());

    // If the user has not generated a unified spending key,
    // we cannot create an address for the account corresponding
    // to that spending key.
    uaResult = wallet.GenerateUnifiedAddress(0, {ReceiverType::P2PKH, ReceiverType::Sapling});
    expected = WalletUAGenerationError::NoSuchAccount;
    EXPECT_EQ(uaResult, expected);

    // lock required by GenerateNewUnifiedSpendingKey
    LOCK(wallet.cs_wallet);

    // Create an account, then generate an address for that account.
    auto ufvkpair = wallet.GenerateNewUnifiedSpendingKey();
    auto ufvk = ufvkpair.first;
    auto account = ufvkpair.second;
    uaResult = wallet.GenerateUnifiedAddress(account, {ReceiverType::P2PKH, ReceiverType::Sapling});
    auto ua = std::get_if<std::pair<libzcash::UnifiedAddress, libzcash::diversifier_index_t>>(&uaResult);
    EXPECT_NE(ua, nullptr);

    auto uaSaplingReceiver = ua->first.GetSaplingReceiver();
    EXPECT_TRUE(uaSaplingReceiver.has_value());
    EXPECT_EQ(uaSaplingReceiver.value(), ufvk.GetSaplingKey().value().Address(ua->second));

    auto u4r = wallet.FindUnifiedAddressByReceiver(uaSaplingReceiver.value());
    EXPECT_TRUE(u4r.has_value());
    EXPECT_EQ(u4r.value(), ua->first);

    // Explicitly trigger the invalid transparent child index failure
    uaResult = wallet.GenerateUnifiedAddress(
            0,
            {ReceiverType::P2PKH, ReceiverType::Sapling},
            MAX_TRANSPARENT_CHILD_IDX.succ().value());
    expected = UnifiedAddressGenerationError::InvalidTransparentChildIndex;
    EXPECT_EQ(uaResult, expected);

    // Attempt to generate a UA at the maximum transparent child index. This might fail
    // due to this index being invalid for Sapling; if so, it will return an error that
    // the diversifier index is out of range. If it succeeds, we'll attempt to generate
    // the next available diversifier, and this should always fail
    uaResult = wallet.GenerateUnifiedAddress(
            0,
            {ReceiverType::P2PKH, ReceiverType::Sapling},
            MAX_TRANSPARENT_CHILD_IDX);
    ua = std::get_if<std::pair<libzcash::UnifiedAddress, libzcash::diversifier_index_t>>(&uaResult);
    if (ua == nullptr) {
        expected = UnifiedAddressGenerationError::NoAddressForDiversifier;
        EXPECT_EQ(uaResult, expected);
    } else {
        // the previous generation attempt succeeded, so this one should definitely fail.
        uaResult = wallet.GenerateUnifiedAddress(0, {ReceiverType::P2PKH, ReceiverType::Sapling});
        expected = UnifiedAddressGenerationError::InvalidTransparentChildIndex;
        EXPECT_EQ(uaResult, expected);
    }

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(WalletTests, GenerateUnifiedSpendingKeyAddsOrchardAddresses) {
    (void) RegtestActivateSapling();
    TestWallet wallet(Params());
    wallet.GenerateNewSeed();

    // lock required by GenerateNewUnifiedSpendingKey
    LOCK(wallet.cs_wallet);

    // Create an account.
    auto ufvkpair = wallet.GenerateNewUnifiedSpendingKey();
    auto ufvk = ufvkpair.first;
    auto account = ufvkpair.second;
    auto fvk = ufvk.GetOrchardKey();
    EXPECT_TRUE(fvk.has_value());

    // At this point the Orchard wallet should contain the change address, but
    // no other addresses. We detect this by trying to look up their IVKs.
    auto changeAddr = fvk->ToInternalIncomingViewingKey().Address(libzcash::diversifier_index_t{0});
    auto internalIvk = wallet.GetOrchardWallet().GetIncomingViewingKeyForAddress(changeAddr);
    EXPECT_TRUE(internalIvk.has_value());
    EXPECT_EQ(internalIvk.value(), fvk->ToInternalIncomingViewingKey());
    auto externalAddr = fvk->ToIncomingViewingKey().Address(libzcash::diversifier_index_t{0});
    auto externalIvk = wallet.GetOrchardWallet().GetIncomingViewingKeyForAddress(externalAddr);
    EXPECT_FALSE(externalIvk.has_value());

    // Generate an address.
    auto uaResult = wallet.GenerateUnifiedAddress(account, {ReceiverType::Orchard});
    auto ua = std::get_if<std::pair<libzcash::UnifiedAddress, libzcash::diversifier_index_t>>(&uaResult);
    EXPECT_NE(ua, nullptr);

    auto uaOrchardReceiver = ua->first.GetOrchardReceiver();
    EXPECT_TRUE(uaOrchardReceiver.has_value());
    EXPECT_EQ(uaOrchardReceiver.value(), externalAddr);

    // Now we can look up the external IVK.
    externalIvk = wallet.GetOrchardWallet().GetIncomingViewingKeyForAddress(externalAddr);
    EXPECT_TRUE(externalIvk.has_value());
    EXPECT_EQ(externalIvk.value(), fvk->ToIncomingViewingKey());

    // Revert to default
    RegtestDeactivateSapling();
}
