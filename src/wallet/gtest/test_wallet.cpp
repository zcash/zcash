#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sodium.h>

#include "base58.h"
#include "chainparams.h"
#include "main.h"
#include "primitives/block.h"
#include "random.h"
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

CWalletTx GetValidReceive(const libzcash::SproutSpendingKey& sk, CAmount value, bool randomInputs, int32_t version = 2) {
    return GetValidReceive(*params, sk, value, randomInputs, version);
}

libzcash::SproutNote GetNote(const libzcash::SproutSpendingKey& sk,
                       const CTransaction& tx, size_t js, size_t n) {
    return GetNote(*params, sk, tx, js, n);
}

CWalletTx GetValidSpend(const libzcash::SproutSpendingKey& sk,
                        const libzcash::SproutNote& note, CAmount value) {
    return GetValidSpend(*params, sk, note, value);
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
    auto wtx = GetValidReceive(sk, 50, true, 4);
    auto note = GetNote(sk, wtx, 0, 1);
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

TEST(wallet_tests, setup_datadir_location_run_as_first_test) {
    // Get temporary and unique path for file.
    boost::filesystem::path pathTemp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    boost::filesystem::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();
}

TEST(wallet_tests, note_data_serialisation) {
    auto sk = libzcash::SproutSpendingKey::random();
    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
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


TEST(wallet_tests, find_unspent_notes) {
    SelectParams(CBaseChainParams::TESTNET);
    CWallet wallet;
    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteData);
    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_FALSE(wallet.IsSpent(nullifier));

    // We currently have an unspent and unconfirmed note in the wallet (depth of -1)
    std::vector<CSproutNotePlaintextEntry> entries;
    wallet.GetFilteredNotes(entries, "", 0);
    EXPECT_EQ(0, entries.size());
    entries.clear();
    wallet.GetFilteredNotes(entries, "", -1);
    EXPECT_EQ(1, entries.size());
    entries.clear();

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
    EXPECT_FALSE(wallet.IsSpent(nullifier));


    // We now have an unspent and confirmed note in the wallet (depth of 1)
    wallet.GetFilteredNotes(entries, "", 0);
    EXPECT_EQ(1, entries.size());
    entries.clear();
    wallet.GetFilteredNotes(entries, "", 1);
    EXPECT_EQ(1, entries.size());
    entries.clear();
    wallet.GetFilteredNotes(entries, "", 2);
    EXPECT_EQ(0, entries.size());
    entries.clear();


    // Let's spend the note.
    auto wtx2 = GetValidSpend(sk, note, 5);
    wallet.AddToWallet(wtx2, true, NULL);
    EXPECT_FALSE(wallet.IsSpent(nullifier));

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
    EXPECT_TRUE(wallet.IsSpent(nullifier));

    // The note has been spent.  By default, GetFilteredNotes() ignores spent notes.
    wallet.GetFilteredNotes(entries, "", 0);
    EXPECT_EQ(0, entries.size());
    entries.clear();
    // Let's include spent notes to retrieve it.
    wallet.GetFilteredNotes(entries, "", 0, false);
    EXPECT_EQ(1, entries.size());
    entries.clear();
    // The spent note has two confirmations.
    wallet.GetFilteredNotes(entries, "", 2, false);
    EXPECT_EQ(1, entries.size());
    entries.clear();
    // It does not have 3 confirmations.
    wallet.GetFilteredNotes(entries, "", 3, false);
    EXPECT_EQ(0, entries.size());
    entries.clear();


    // Let's receive a new note
    CWalletTx wtx3;
    {
        auto wtx = GetValidReceive(sk, 20, true);
        auto note = GetNote(sk, wtx, 0, 1);
        auto nullifier = note.nullifier(sk);

        mapSproutNoteData_t noteData;
        JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
        SproutNoteData nd {sk.address(), nullifier};
        noteData[jsoutpt] = nd;

        wtx.SetSproutNoteData(noteData);
        wallet.AddToWallet(wtx, true, NULL);
        EXPECT_FALSE(wallet.IsSpent(nullifier));

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
    wallet.GetFilteredNotes(entries, "", 1);
    EXPECT_EQ(1, entries.size());
    entries.clear();
    // Let's return the spent note too.
    wallet.GetFilteredNotes(entries, "", 1, false);
    EXPECT_EQ(2, entries.size());
    entries.clear();
    // Increasing number of confirmations will exclude our new unspent note.
    wallet.GetFilteredNotes(entries, "", 2, false);
    EXPECT_EQ(1, entries.size());
    entries.clear();    
    // If we also ignore spent notes at this depth, we won't find any notes.
    wallet.GetFilteredNotes(entries, "", 2, true);
    EXPECT_EQ(0, entries.size());
    entries.clear(); 

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);
    mapBlockIndex.erase(blockHash2);
    mapBlockIndex.erase(blockHash3);
}


TEST(wallet_tests, set_note_addrs_in_cwallettx) {
    auto sk = libzcash::SproutSpendingKey::random();
    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);
    EXPECT_EQ(0, wtx.mapSproutNoteData.size());

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteData);
    EXPECT_EQ(noteData, wtx.mapSproutNoteData);
}

TEST(wallet_tests, set_invalid_note_addrs_in_cwallettx) {
    CWalletTx wtx;
    EXPECT_EQ(0, wtx.mapSproutNoteData.size());

    mapSproutNoteData_t noteData;
    auto sk = libzcash::SproutSpendingKey::random();
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), uint256()};
    noteData[jsoutpt] = nd;

    EXPECT_THROW(wtx.SetSproutNoteData(noteData), std::logic_error);
}

TEST(wallet_tests, GetNoteNullifier) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    auto address = sk.address();
    auto dec = ZCNoteDecryption(sk.receiving_key());

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto hSig = wtx.vjoinsplit[0].h_sig(
        *params, wtx.joinSplitPubKey);

    auto ret = wallet.GetNoteNullifier(
        wtx.vjoinsplit[0],
        address,
        dec,
        hSig, 1);
    EXPECT_NE(nullifier, ret);

    wallet.AddSproutSpendingKey(sk);

    ret = wallet.GetNoteNullifier(
        wtx.vjoinsplit[0],
        address,
        dec,
        hSig, 1);
    EXPECT_EQ(nullifier, ret);
}

TEST(wallet_tests, FindMyNotes) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    auto sk2 = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk2);

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto noteMap = wallet.FindMyNotes(wtx);
    EXPECT_EQ(0, noteMap.size());

    wallet.AddSproutSpendingKey(sk);

    noteMap = wallet.FindMyNotes(wtx);
    EXPECT_EQ(2, noteMap.size());

    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    EXPECT_EQ(1, noteMap.count(jsoutpt));
    EXPECT_EQ(nd, noteMap[jsoutpt]);
}

TEST(wallet_tests, FindMyNotesInEncryptedWallet) {
    TestWallet wallet;
    uint256 r {GetRandHash()};
    CKeyingMaterial vMasterKey (r.begin(), r.end());

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    ASSERT_TRUE(wallet.EncryptKeys(vMasterKey));

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto noteMap = wallet.FindMyNotes(wtx);
    EXPECT_EQ(2, noteMap.size());

    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    EXPECT_EQ(1, noteMap.count(jsoutpt));
    EXPECT_NE(nd, noteMap[jsoutpt]);

    ASSERT_TRUE(wallet.Unlock(vMasterKey));

    noteMap = wallet.FindMyNotes(wtx);
    EXPECT_EQ(2, noteMap.size());
    EXPECT_EQ(1, noteMap.count(jsoutpt));
    EXPECT_EQ(nd, noteMap[jsoutpt]);
}

TEST(wallet_tests, get_conflicted_notes) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    auto wtx2 = GetValidSpend(sk, note, 5);
    auto wtx3 = GetValidSpend(sk, note, 10);
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

TEST(wallet_tests, nullifier_is_spent) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    EXPECT_FALSE(wallet.IsSpent(nullifier));

    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_FALSE(wallet.IsSpent(nullifier));

    auto wtx2 = GetValidSpend(sk, note, 5);
    wallet.AddToWallet(wtx2, true, NULL);
    EXPECT_FALSE(wallet.IsSpent(nullifier));

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
    EXPECT_TRUE(wallet.IsSpent(nullifier));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);
}

TEST(wallet_tests, navigate_from_nullifier_to_note) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteData[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteData);

    EXPECT_EQ(0, wallet.mapNullifiersToNotes.count(nullifier));

    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_EQ(1, wallet.mapNullifiersToNotes.count(nullifier));
    EXPECT_EQ(wtx.GetHash(), wallet.mapNullifiersToNotes[nullifier].hash);
    EXPECT_EQ(0, wallet.mapNullifiersToNotes[nullifier].js);
    EXPECT_EQ(1, wallet.mapNullifiersToNotes[nullifier].n);
}

TEST(wallet_tests, spent_note_is_from_me) {
    CWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);
    auto wtx2 = GetValidSpend(sk, note, 5);

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

TEST(wallet_tests, cached_witnesses_empty_chain) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true, 4);
    auto note = GetNote(sk, wtx, 0, 0);
    auto note2 = GetNote(sk, wtx, 0, 1);
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

TEST(wallet_tests, cached_witnesses_chain_tip) {
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
        auto wtx = GetValidReceive(sk, 50, true, 4);
        auto note = GetNote(sk, wtx, 0, 1);
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

TEST(wallet_tests, CachedWitnessesDecrementFirst) {
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
        auto wtx = GetValidReceive(sk, 20, true, 4);
        auto note = GetNote(sk, wtx, 0, 1);
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

TEST(wallet_tests, CachedWitnessesCleanIndex) {
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

TEST(wallet_tests, ClearNoteWitnessCache) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true, 4);
    auto hash = wtx.GetHash();
    auto note = GetNote(sk, wtx, 0, 0);
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
    auto anchros2 = GetWitnessesAndAnchors(wallet, sproutNotes, saplingNotes, sproutWitnesses, saplingWitnesses);
    EXPECT_FALSE((bool) sproutWitnesses[0]);
    EXPECT_FALSE((bool) sproutWitnesses[1]);
    EXPECT_FALSE((bool) saplingWitnesses[0]);
    EXPECT_FALSE((bool) saplingWitnesses[1]);
    EXPECT_EQ(-1, wallet.mapWallet[hash].mapSproutNoteData[jsoutpt].witnessHeight);
    EXPECT_EQ(-1, wallet.mapWallet[hash].mapSaplingNoteData[saplingNotes[0]].witnessHeight);
    EXPECT_EQ(0, wallet.nWitnessCacheSize);
}

TEST(wallet_tests, WriteWitnessCache) {
    TestWallet wallet;
    MockWalletDB walletdb;
    CBlockLocator loc;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
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

TEST(wallet_tests, UpdateNullifierNoteMap) {
    TestWallet wallet;
    uint256 r {GetRandHash()};
    CKeyingMaterial vMasterKey (r.begin(), r.end());

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    ASSERT_TRUE(wallet.EncryptKeys(vMasterKey));

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    // Pretend that we called FindMyNotes while the wallet was locked
    mapSproutNoteData_t noteData;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address()};
    noteData[jsoutpt] = nd;
    wtx.SetSproutNoteData(noteData);

    wallet.AddToWallet(wtx, true, NULL);
    EXPECT_EQ(0, wallet.mapNullifiersToNotes.count(nullifier));

    EXPECT_FALSE(wallet.UpdateNullifierNoteMap());

    ASSERT_TRUE(wallet.Unlock(vMasterKey));

    EXPECT_TRUE(wallet.UpdateNullifierNoteMap());
    EXPECT_EQ(1, wallet.mapNullifiersToNotes.count(nullifier));
    EXPECT_EQ(wtx.GetHash(), wallet.mapNullifiersToNotes[nullifier].hash);
    EXPECT_EQ(0, wallet.mapNullifiersToNotes[nullifier].js);
    EXPECT_EQ(1, wallet.mapNullifiersToNotes[nullifier].n);
}

TEST(wallet_tests, UpdatedNoteData) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
    auto note = GetNote(sk, wtx, 0, 0);
    auto note2 = GetNote(sk, wtx, 0, 1);
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

TEST(wallet_tests, MarkAffectedTransactionsDirty) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
    auto hash = wtx.GetHash();
    auto note = GetNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);
    auto wtx2 = GetValidSpend(sk, note, 5);

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

TEST(wallet_tests, NoteLocking) {
    TestWallet wallet;

    auto sk = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sk);

    auto wtx = GetValidReceive(sk, 10, true);
    auto wtx2 = GetValidReceive(sk, 10, true);

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
    wallet.UnlockAllNotes();
    EXPECT_FALSE(wallet.IsLockedNote(jsoutpt));
    EXPECT_FALSE(wallet.IsLockedNote(jsoutpt2));
}
