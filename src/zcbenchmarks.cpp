#include <cstdio>
#include <future>
#include <map>
#include <thread>
#include <unistd.h>

#include "coins.h"
#include "util/system.h"
#include "init.h"
#include "primitives/transaction.h"
#include "base58.h"
#include "crypto/equihash.h"
#include "chain.h"
#include "chainparams.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "main.h"
#include "miner.h"
#include "policy/policy.h"
#include "pow.h"
#include "proof_verifier.h"
#include "random.h"
#include "rpc/server.h"
#include "script/sign.h"
#include "streams.h"
#include "transaction_builder.h"
#include "txdb.h"
#include "util/test.h"
#include "wallet/wallet.h"

#include "zcbenchmarks.h"

#include "zcash/Zcash.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "zcash/Note.hpp"
#include "librustzcash.h"

#include <rust/ed25519/types.h>
#include <rust/sapling.h>

using namespace libzcash;
// This method is based on Shutdown from init.cpp
void pre_wallet_load()
{
    LogPrintf("%s: In progress...\n", __func__);
    if (ShutdownRequested())
        throw new std::runtime_error("The node is shutting down");

    if (pwalletMain)
        pwalletMain->Flush(false);
#ifdef ENABLE_MINING
    GenerateBitcoins(false, 0, Params());
#endif
    UnregisterNodeSignals(GetNodeSignals());
    if (pwalletMain)
        pwalletMain->Flush(true);

    UnregisterValidationInterface(pwalletMain);
    delete pwalletMain;
    pwalletMain = NULL;
    bitdb.Reset();
    RegisterNodeSignals(GetNodeSignals());
    LogPrintf("%s: done\n", __func__);
}

void post_wallet_load(){
    RegisterValidationInterface(pwalletMain);
#ifdef ENABLE_MINING
    // Generate coins in the background
    if (pwalletMain || !GetArg("-mineraddress", "").empty())
        GenerateBitcoins(GetBoolArg("-gen", false), GetArg("-genproclimit", 1), Params());
#endif
}


void timer_start(timeval &tv_start)
{
    gettimeofday(&tv_start, 0);
}

double timer_stop(timeval &tv_start)
{
    double elapsed;
    struct timeval tv_end;
    gettimeofday(&tv_end, 0);
    elapsed = double(tv_end.tv_sec-tv_start.tv_sec) +
        (tv_end.tv_usec-tv_start.tv_usec)/double(1000000);
    return elapsed;
}

double benchmark_sleep()
{
    struct timeval tv_start;
    timer_start(tv_start);
    sleep(1);
    return timer_stop(tv_start);
}

double benchmark_create_joinsplit()
{
    Ed25519VerificationKey joinSplitPubKey;

    /* Get the anchor of an empty commitment tree. */
    uint256 anchor = SproutMerkleTree().root();
    std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs({JSInput(), JSInput()});
    std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs({JSOutput(), JSOutput()});

    struct timeval tv_start;
    timer_start(tv_start);
    auto jsdesc = JSDescriptionInfo(joinSplitPubKey,
                         anchor,
                         inputs,
                         outputs,
                         0,
                         0).BuildDeterministic();
    double ret = timer_stop(tv_start);

    auto verifier = ProofVerifier::Strict();
    assert(verifier.VerifySprout(jsdesc, joinSplitPubKey));
    return ret;
}

std::vector<double> benchmark_create_joinsplit_threaded(int nThreads)
{
    std::vector<double> ret;
    std::vector<std::future<double>> tasks;
    std::vector<std::thread> threads;
    for (int i = 0; i < nThreads; i++) {
        std::packaged_task<double(void)> task(&benchmark_create_joinsplit);
        tasks.emplace_back(task.get_future());
        threads.emplace_back(std::move(task));
    }
    std::future_status status;
    for (auto it = tasks.begin(); it != tasks.end(); it++) {
        it->wait();
        ret.push_back(it->get());
    }
    for (auto it = threads.begin(); it != threads.end(); it++) {
        it->join();
    }
    return ret;
}

double benchmark_verify_joinsplit(const JSDescription &joinsplit)
{
    struct timeval tv_start;
    timer_start(tv_start);
    Ed25519VerificationKey joinSplitPubKey;
    auto verifier = ProofVerifier::Strict();
    verifier.VerifySprout(joinsplit, joinSplitPubKey);
    return timer_stop(tv_start);
}

#ifdef ENABLE_MINING
double benchmark_solve_equihash()
{
    CBlock pblock;
    CEquihashInput I{pblock};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    const Consensus::Params& params = Params(CBaseChainParams::MAIN).GetConsensus();
    unsigned int n = params.nEquihashN;
    unsigned int k = params.nEquihashK;
    eh_HashState eh_state = EhInitialiseState(n, k);
    eh_state.Update((unsigned char*)&ss[0], ss.size());

    uint256 nonce = GetRandHash();
    eh_state.Update(nonce.begin(), nonce.size());

    struct timeval tv_start;
    timer_start(tv_start);
    std::set<std::vector<unsigned int>> solns;
    EhOptimisedSolveUncancellable(n, k, eh_state,
                                  [](std::vector<unsigned char> soln) { return false; });
    return timer_stop(tv_start);
}

std::vector<double> benchmark_solve_equihash_threaded(int nThreads)
{
    std::vector<double> ret;
    std::vector<std::future<double>> tasks;
    std::vector<std::thread> threads;
    for (int i = 0; i < nThreads; i++) {
        std::packaged_task<double(void)> task(&benchmark_solve_equihash);
        tasks.emplace_back(task.get_future());
        threads.emplace_back(std::move(task));
    }
    std::future_status status;
    for (auto it = tasks.begin(); it != tasks.end(); it++) {
        it->wait();
        ret.push_back(it->get());
    }
    for (auto it = threads.begin(); it != threads.end(); it++) {
        it->join();
    }
    return ret;
}
#endif // ENABLE_MINING

double benchmark_verify_equihash()
{
    CChainParams params = Params(CBaseChainParams::MAIN);
    CBlock genesis = params.GenesisBlock();
    CBlockHeader genesis_header = genesis.GetBlockHeader();
    struct timeval tv_start;
    timer_start(tv_start);
    CheckEquihashSolution(&genesis_header, params.GetConsensus());
    return timer_stop(tv_start);
}

double benchmark_large_tx(size_t nInputs)
{
    // Create priv/pub key
    CKey priv = CKey::TestOnlyRandomKey(true);
    auto pub = priv.GetPubKey();
    CBasicKeyStore tempKeystore;
    tempKeystore.AddKey(priv);

    // The "original" transaction that the spending transaction will spend
    // from.
    CMutableTransaction m_orig_tx;
    m_orig_tx.vout.resize(1);
    m_orig_tx.vout[0].nValue = 1000000;
    CScript prevPubKey = GetScriptForDestination(pub.GetID());
    m_orig_tx.vout[0].scriptPubKey = prevPubKey;

    auto orig_tx = CTransaction(m_orig_tx);

    CMutableTransaction spending_tx;
    spending_tx.fOverwintered = true;
    spending_tx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    spending_tx.nVersion = SAPLING_TX_VERSION;

    auto input_hash = orig_tx.GetHash();
    std::vector<CTxOut> allPrevOutputs;
    // Add nInputs inputs
    for (size_t i = 0; i < nInputs; i++) {
        spending_tx.vin.emplace_back(input_hash, 0);
        allPrevOutputs.push_back(orig_tx.vout[0]);
    }

    PrecomputedTransactionData txdata(spending_tx, allPrevOutputs);

    // Sign for all the inputs
    auto consensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId;
    for (size_t i = 0; i < nInputs; i++) {
        SignSignature(tempKeystore, prevPubKey, spending_tx, txdata, i, 1000000, SIGHASH_ALL, consensusBranchId);
    }

    // Spending tx has all its inputs signed and does not need to be mutated anymore
    CTransaction final_spending_tx(spending_tx);

    // Benchmark signature verification costs:
    struct timeval tv_start;
    timer_start(tv_start);
    for (size_t i = 0; i < nInputs; i++) {
        ScriptError serror = SCRIPT_ERR_OK;
        assert(VerifyScript(final_spending_tx.vin[i].scriptSig,
                            prevPubKey,
                            STANDARD_SCRIPT_VERIFY_FLAGS,
                            TransactionSignatureChecker(&final_spending_tx, txdata, i, 1000000),
                            consensusBranchId,
                            &serror));
    }
    return timer_stop(tv_start);
}

// The two benchmarks, try_decrypt_sprout_notes and try_decrypt_sapling_notes,
// are checking worst-case scenarios. In both we add n keys to a wallet,
// create a transaction using a key not in our original list of n, and then
// check that the transaction is not associated with any of the keys in our
// wallet. We call assert(...) to ensure that this is true.
double benchmark_try_decrypt_sprout_notes(size_t nKeys)
{
    CWallet wallet(Params());
    for (int i = 0; i < nKeys; i++) {
        auto sk = libzcash::SproutSpendingKey::random();
        wallet.AddSproutSpendingKey(sk);
    }

    auto sk = libzcash::SproutSpendingKey::random();
    auto tx = GetValidSproutReceive(sk, 10, true);

    struct timeval tv_start;
    timer_start(tv_start);
    auto noteDataMap = wallet.FindMySproutNotes(tx);

    assert(noteDataMap.empty());
    return timer_stop(tv_start);
}

double benchmark_try_decrypt_sapling_notes(size_t nKeys)
{
    // Set params
    const Consensus::Params& consensusParams = Params().GetConsensus();

    auto masterKey = GetTestMasterSaplingSpendingKey();

    CWallet wallet(Params());

    for (int i = 0; i < nKeys; i++) {
        auto sk = masterKey.Derive(i);
        wallet.AddSaplingSpendingKey(sk);
    }

    // Generate a key that has not been added to the wallet
    auto sk = masterKey.Derive(nKeys);
    auto tx = GetValidSaplingReceive(consensusParams, wallet, sk, 10);

    struct timeval tv_start;
    timer_start(tv_start);
    auto noteDataMapAndAddressesToAdd = wallet.FindMySaplingNotes(Params().GetConsensus(), tx, 1);
    assert(noteDataMapAndAddressesToAdd.first.empty());
    return timer_stop(tv_start);
}

CWalletTx CreateSproutTxWithNoteData(const libzcash::SproutSpendingKey& sk) {
    auto wtx = GetValidSproutReceive(sk, 10, true);
    auto note = GetSproutNote(sk, wtx, 0, 1);
    auto nullifier = note.nullifier(sk);

    mapSproutNoteData_t noteDataMap;
    JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
    SproutNoteData nd {sk.address(), nullifier};
    noteDataMap[jsoutpt] = nd;

    wtx.SetSproutNoteData(noteDataMap);

    return wtx;
}

double benchmark_increment_sprout_note_witnesses(size_t nTxs)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();

    CWallet wallet(Params());
    MerkleFrontiers frontiers;

    auto sproutSpendingKey = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sproutSpendingKey);

    // First block
    CBlock block1;
    for (int i = 0; i < nTxs; ++i) {
        auto wtx = CreateSproutTxWithNoteData(sproutSpendingKey);
        wallet.LoadWalletTx(wtx);
        block1.vtx.push_back(wtx);
    }

    CBlockIndex index1(block1);
    index1.nHeight = 1;

    // Increment to get transactions witnessed
    wallet.ChainTip(&index1, &block1, frontiers);

    // Second block
    CBlock block2;
    block2.hashPrevBlock = block1.GetHash();
    {
        auto sproutTx = CreateSproutTxWithNoteData(sproutSpendingKey);
        wallet.LoadWalletTx(sproutTx);
        block2.vtx.push_back(sproutTx);
    }

    CBlockIndex index2(block2);
    index2.nHeight = 2;

    struct timeval tv_start;
    timer_start(tv_start);
    wallet.ChainTip(&index2, &block2, frontiers);
    return timer_stop(tv_start);
}

CWalletTx CreateSaplingTxWithNoteData(const Consensus::Params& consensusParams,
                                      CBasicKeyStore& keyStore,
                                      const libzcash::SaplingExtendedSpendingKey &sk) {
    auto wtx = GetValidSaplingReceive(consensusParams, keyStore, sk, 10);
    auto testNote = GetTestSaplingNote(sk.ToXFVK().DefaultAddress(), 10);
    auto fvk = sk.expsk.full_viewing_key();
    auto nullifier = testNote.note.nullifier(fvk, testNote.tree.witness().position()).value();

    mapSaplingNoteData_t noteDataMap;
    SaplingOutPoint outPoint {wtx.GetHash(), 0};
    auto ivk = fvk.in_viewing_key();
    SaplingNoteData noteData {ivk, nullifier};
    noteDataMap[outPoint] = noteData;

    wtx.SetSaplingNoteData(noteDataMap);

    return wtx;
}

double benchmark_increment_sapling_note_witnesses(size_t nTxs)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();

    CWallet wallet(Params());
    MerkleFrontiers frontiers;

    auto saplingSpendingKey = GetTestMasterSaplingSpendingKey();
    wallet.AddSaplingSpendingKey(saplingSpendingKey);

    // First block
    CBlock block1;
    for (int i = 0; i < nTxs; ++i) {
        auto wtx = CreateSaplingTxWithNoteData(consensusParams, wallet, saplingSpendingKey);
        wallet.LoadWalletTx(wtx);
        block1.vtx.push_back(wtx);
    }

    CBlockIndex index1(block1);
    index1.nHeight = 1;

    // Increment to get transactions witnessed
    wallet.ChainTip(&index1, &block1, frontiers);

    // Second block
    CBlock block2;
    block2.hashPrevBlock = block1.GetHash();
    {
        auto saplingTx = CreateSaplingTxWithNoteData(consensusParams, wallet, saplingSpendingKey);
        wallet.LoadWalletTx(saplingTx);
        block1.vtx.push_back(saplingTx);
    }

    CBlockIndex index2(block2);
    index2.nHeight = 2;

    struct timeval tv_start;
    timer_start(tv_start);
    wallet.ChainTip(&index2, &block2, frontiers);
    return timer_stop(tv_start);
}

// Fake the input of a given block
// This class is based on the class CCoinsViewDB, but with limited functionality.
// The constructor and the functions `GetCoins` and `HaveCoins` come directly from
// CCoinsViewDB, but the rest are either mocks and/or don't really do anything.

// The following constant is a duplicate of the one found in txdb.cpp
static const char DB_COINS = 'c';

class FakeCoinsViewDB : public CCoinsView {

    CDBWrapper db;

    uint256 hash;
    SproutMerkleTree sproutTree;
    std::vector<SaplingMerkleTree> saplingTrees;
    std::vector<OrchardMerkleFrontier> orchardTrees;

public:
    FakeCoinsViewDB(std::string dbName, uint256& hash) : db(GetDataDir() / dbName, 100, false, false), hash(hash) {
        SaplingMerkleTree emptySaplingTree;
        saplingTrees.push_back(emptySaplingTree);

        OrchardMerkleFrontier emptyOrchardTree;
        orchardTrees.push_back(emptyOrchardTree);
    }

    void SetSaplingTrees(std::vector<std::string> trees) {
        saplingTrees.clear();
        for (const auto& treeHex : trees) {
            auto treeBytes = ParseHex(treeHex);
            SaplingMerkleTree tree;
            CDataStream so(treeBytes, SER_NETWORK, PROTOCOL_VERSION);
            so >> tree;
            saplingTrees.push_back(tree);
        }

        // Ensure the empty tree is present.
        SaplingMerkleTree emptyTree;
        saplingTrees.push_back(emptyTree);
    }

    void SetOrchardTrees(std::vector<std::string> trees) {
        orchardTrees.clear();
        for (const auto& treeHex : trees) {
            auto treeBytes = ParseHex(treeHex);
            OrchardMerkleFrontier tree;
            CDataStream so(treeBytes, SER_NETWORK, PROTOCOL_VERSION);
            so >> tree;
            orchardTrees.push_back(tree);
        }

        // Ensure the empty tree is present.
        OrchardMerkleFrontier emptyTree;
        orchardTrees.push_back(emptyTree);
    }

    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
        if (rt == sproutTree.root()) {
            tree = sproutTree;
            return true;
        }
        return false;
    }

    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
        for (const auto& saplingTree : saplingTrees) {
            if (rt == saplingTree.root()) {
                tree = saplingTree;
                return true;
            }
        }
        return false;
    }

    bool GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const {
        for (const auto& orchardTree : orchardTrees) {
            if (rt == orchardTree.root()) {
                tree = orchardTree;
                return true;
            }
        }
        return false;
    }

    bool GetNullifier(const uint256 &nf, ShieldedType type) const {
        return false;
    }

    bool GetCoins(const uint256 &txid, CCoins &coins) const {
        return db.Read(std::make_pair(DB_COINS, txid), coins);
    }

    bool HaveCoins(const uint256 &txid) const {
        return db.Exists(std::make_pair(DB_COINS, txid));
    }

    uint256 GetBestBlock() const {
        return hash;
    }

    uint256 GetBestAnchor(ShieldedType type) const {
        switch (type) {
            case SPROUT:
                return sproutTree.root();
            case SAPLING:
                return saplingTrees[0].root();
            case ORCHARD:
                return orchardTrees[0].root();
            default:
                throw new std::runtime_error("Unknown shielded type");
        }
    }

    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    const uint256 &hashOrchardAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CAnchorsOrchardMap &mapOrchardAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap &mapSaplingNullifiers,
                    CNullifiersMap &mapOrchardNullifiers,
                    CHistoryCacheMap &historyCacheMap) {
        return false;
    }

    bool GetStats(CCoinsStats &stats) const {
        return false;
    }
};

double benchmark_connectblock_slow()
{
    // Test for issue 2017-05-01.a
    SelectParams(CBaseChainParams::MAIN);
    CBlock block;
    FILE* fp = fsbridge::fopen(GetDataDir() / "benchmark/block-107134.dat", "rb");
    if (!fp) throw new std::runtime_error("Failed to open block data file");
    CAutoFile blkFile(fp, SER_DISK, CLIENT_VERSION);
    blkFile >> block;
    blkFile.fclose();

    // Fake its inputs
    auto hashPrev = uint256S("00000000159a41f468e22135942a567781c3f3dc7ad62257993eb3c69c3f95ef");
    FakeCoinsViewDB fakeDB("benchmark/block-107134-inputs", hashPrev);
    CCoinsViewCache view(&fakeDB);

    // Fake the chain
    CBlockIndex index(block);
    index.nHeight = 107134;
    CBlockIndex indexPrev;
    indexPrev.phashBlock = &hashPrev;
    indexPrev.nHeight = index.nHeight - 1;
    index.pprev = &indexPrev;
    mapBlockIndex.insert(std::make_pair(hashPrev, &indexPrev));

    CValidationState state;
    struct timeval tv_start;
    timer_start(tv_start);
    assert(ConnectBlock(block, state, &index, view, Params(), true, CheckAs::SlowBenchmark));
    auto duration = timer_stop(tv_start);

    // Undo alterations to global state
    mapBlockIndex.erase(hashPrev);
    SelectParams(ChainNameFromCommandLine());

    return duration;
}

double benchmark_connectblock_sapling()
{
    // Test for slowness encountered on 2022-07-01
    SelectParams(CBaseChainParams::MAIN);
    CBlock block;
    FILE* fp = fsbridge::fopen(GetDataDir() / "benchmark/block-1723244.dat", "rb");
    if (!fp) throw new std::runtime_error("Failed to open block data file");
    CAutoFile blkFile(fp, SER_DISK, CLIENT_VERSION);
    blkFile >> block;
    blkFile.fclose();

    // Fake its inputs
    auto hashPrev = uint256S("0000000001286c016284523a1b343e731539bdf1fac4159f96d70a711cf9c960");
    FakeCoinsViewDB fakeDB("benchmark/block-1723244-inputs", hashPrev);
    fakeDB.SetSaplingTrees({
        "012380d4632f51d9aff726d9f2d0013691c86da31279e110e852d49e4737ce7c14001401c4dedf5f6a0e5be05be051fec4608153084b0fdb74cd92f01d8deb77e72b6a5f01d5891ebfeae46b30846a301f8aaba34d64428e91fc406d0db13fcf723100753100018b4af5e391e5720a2b7a20c289a7691bb6a29ffcda574e06eaf4d6597048fd2200017cc935f49a763d29f9e876fe0a85c6dbbd32ab5b632bc2da058c44c652f713190191cac78cfcb2ca0eaaad52778c93cc2549c6ae0460c37f0b304843ccf8c15e3301c4a86822bec7ea056f984aa72a059a566addd797c08be929289123f698a4631d01e7cbe9d650e4c7dfbc78b328744515345f3e0988ea8d0dd5ce2af025d56d710001278234295da81c7fbfbb336719718f78c16025032ef9cc6007bc49351213d701014656c38189b9f768a492ee90758c63d4be59c92e21b3e9d00916acf1e528774601add4db5d8e6357c91d3b1fc605c0961353b5aa683520923f47daa51b2864563c00000001a02e924803cc430e6360a63732092f621a116b1f156ccdc3e0f815c30a01b2510001bcefb8862360baae2a9a164c1367df2c27372d0c6963068c9ea4b1f84f5c526d00015ec9e9b1295908beed437df4126032ca57ada8e3ebb67067cd22a73c79a84009",
        "0159a96b6b5a6dc61154f9b0cb66121ac04050978fdee3d1cf6deadac0b3118d4801318d4e70e26b35a2af00ee10cd4370553cfd6c058d762a9b37e7d43fba96a41f14013addafd08d0b8fcc60723f7ac79b589e6fd1e0209ec1865ef521ddc9537d010000018636e49531fdf3d5d7f817ed1488babf61ef0e750255e59f93cc111f521d800a0001a19338964f3cf13d3a74868b8a53c15dfd6b28f6aef345238372f0051dc3bc1d011f386f8123ec951aed8d51e25c549e3e007429c3d3882dc5daebf8d70e878e4d0001c4a86822bec7ea056f984aa72a059a566addd797c08be929289123f698a4631d01e7cbe9d650e4c7dfbc78b328744515345f3e0988ea8d0dd5ce2af025d56d710001278234295da81c7fbfbb336719718f78c16025032ef9cc6007bc49351213d701014656c38189b9f768a492ee90758c63d4be59c92e21b3e9d00916acf1e528774601add4db5d8e6357c91d3b1fc605c0961353b5aa683520923f47daa51b2864563c00000001a02e924803cc430e6360a63732092f621a116b1f156ccdc3e0f815c30a01b2510001bcefb8862360baae2a9a164c1367df2c27372d0c6963068c9ea4b1f84f5c526d00015ec9e9b1295908beed437df4126032ca57ada8e3ebb67067cd22a73c79a84009",
        "0175638110ef078c9c5e45f34399131e652d7e7e4e3ea28fa13702854c3e889d38001401b445b70b61de6c561ae9203c5ddcd594c3da14550493154a4fb6e770bff2d65901981edfb711be11868d221643fdbc0403f607b8ec06f6c2b7968c8ddb2e0968080001b97e4e33d8c0c08cabf8ee7a897d09d1b2e6a395bc43214b61d7ae7cea560e650001060784c088dee8e54ec94a47e7de9308cab6069e77d7303e1ac9f8fbc920a45a000001e7cbe9d650e4c7dfbc78b328744515345f3e0988ea8d0dd5ce2af025d56d710001278234295da81c7fbfbb336719718f78c16025032ef9cc6007bc49351213d701014656c38189b9f768a492ee90758c63d4be59c92e21b3e9d00916acf1e528774601add4db5d8e6357c91d3b1fc605c0961353b5aa683520923f47daa51b2864563c00000001a02e924803cc430e6360a63732092f621a116b1f156ccdc3e0f815c30a01b2510001bcefb8862360baae2a9a164c1367df2c27372d0c6963068c9ea4b1f84f5c526d00015ec9e9b1295908beed437df4126032ca57ada8e3ebb67067cd22a73c79a84009",
        "01ca5608a4c5e15340674e0a91ad1c128a49c04230141a955a9d87f2c84734ee2401ed8561fa9f71a0283dcc4865dc44dc886ceb8a91c36abbbb139795bc86cbba321401e2488b560f9f641e4371cadb8569776df9170f82888869c7902f80909aae5f1e01ad01014509c3242d3fbf6754e485f8c87eca413ceace72a742e033947e3f014c0001e6de90c780bf39b4a40dedee586a89686133013b138529eea245add88685e15c00013c16c0b1a41f32caebe41ec1322521b8eaf0303c3969044fa6daaa8a20948d2100000000000001591e0969b34ce995643b3f043be0b07f45b641dd90fce7f4713b90b4a545fa16000001a02e924803cc430e6360a63732092f621a116b1f156ccdc3e0f815c30a01b2510001bcefb8862360baae2a9a164c1367df2c27372d0c6963068c9ea4b1f84f5c526d00015ec9e9b1295908beed437df4126032ca57ada8e3ebb67067cd22a73c79a84009",
        "01cb51537fbdc52f8c92ab7d14156dc3d81d9d433fa438d479f4b822924f4fb64500140139c4c235ce18df178bff8169c4d2ecd5c1eb1d112a0dbde3bc22349a2a7a1e590000014c998c0f24b3cc9640acccca4af19cba8c1dd728570fc7080ed7487e7a232c4100011f386f8123ec951aed8d51e25c549e3e007429c3d3882dc5daebf8d70e878e4d0001c4a86822bec7ea056f984aa72a059a566addd797c08be929289123f698a4631d01e7cbe9d650e4c7dfbc78b328744515345f3e0988ea8d0dd5ce2af025d56d710001278234295da81c7fbfbb336719718f78c16025032ef9cc6007bc49351213d701014656c38189b9f768a492ee90758c63d4be59c92e21b3e9d00916acf1e528774601add4db5d8e6357c91d3b1fc605c0961353b5aa683520923f47daa51b2864563c00000001a02e924803cc430e6360a63732092f621a116b1f156ccdc3e0f815c30a01b2510001bcefb8862360baae2a9a164c1367df2c27372d0c6963068c9ea4b1f84f5c526d00015ec9e9b1295908beed437df4126032ca57ada8e3ebb67067cd22a73c79a84009",
        "01dffd4c5286c411a7efc955f740f311f979d6ceb39f37f12e2530e45610452b4200140001180211de427aaf56cc398c972a158fa0ac1a4fc79c76746972907fb2e7b668360001be1c46b85e67695418b39e799aa8bcc19a8634463e3fa293c3e75c471fb8475301072f3b28b20e0306b48c1b1fa25b2d845b6125069299df3028d58cfd360b4953010ed297014c2acf3d0840b3dad9038071e150935054c3dcc0f8981dfeb822821501b5cf5b5ac994c75a11e2e47a0d8077d39d01e79a9ac24a1b83b108128a86f35d0001e7cbe9d650e4c7dfbc78b328744515345f3e0988ea8d0dd5ce2af025d56d710001278234295da81c7fbfbb336719718f78c16025032ef9cc6007bc49351213d701014656c38189b9f768a492ee90758c63d4be59c92e21b3e9d00916acf1e528774601add4db5d8e6357c91d3b1fc605c0961353b5aa683520923f47daa51b2864563c00000001a02e924803cc430e6360a63732092f621a116b1f156ccdc3e0f815c30a01b2510001bcefb8862360baae2a9a164c1367df2c27372d0c6963068c9ea4b1f84f5c526d00015ec9e9b1295908beed437df4126032ca57ada8e3ebb67067cd22a73c79a84009"
    });
    fakeDB.SetOrchardTrees({
        "01a8fb1700000000002b768d317551b491c8b993cd7e74c9196f3028b2a1a7a3f22971f7f4007c833e000e7005f57c540bf3e32695854f52d27a0456ca1be179bd073d9ca87acc92248413aeacb64a31a230338cfa6a2907f12bc5321a709ef298fb93ced3082c444f8a3814764dc108e86dc2c6d7c20a6e1759027d87029b5dc7e1e6b5be970ecbed913a186d95ac66b184f8844e57fece62a4d64cfeb73e7ed6e99146e79aacae7f5e002f9cd86767aea1f11147f65c588f94dce188315c40b22c0fa8751365ba453c28130cfb41380fdd7836985e2c4c488fdc3d1d1bd4390f350f0e1b8a448f47ac1c2bcbdd308beca04006b18928c4418aad2b3650677289b1b45ea5a21095c53103100ed4d0a8a440b03f1254ce1efb2d82a94cf001cffa0e7fd6ada813a2688b2430a69de998b87aebcd4c556503a45e559a422ecfbdf2f0e6318a8427e41a7b097676cfe97afff13797f82f8d631bd29edde424854139c64ab8241e2a23315514da371f0d3294843fd8f645019a04c07607342c70cf4cc793068355eaccdd6716bc79f0119f97113775379bf58f3f5d9d122766909b797947127b28248ff07205c1eb3aa1717c2a696ce0aba6c5b5bda44c4eda5cf69ae376cc8b334a2c23fb2b374feb2041bfd423c6cc3e064ee2b4705748a082836d39dd723515357fb06e30",
    });
    CCoinsViewCache view(&fakeDB);

    // Fake the chain
    CBlockIndex index(block);
    index.nHeight = 1723244;
    CBlockIndex indexPrev;
    indexPrev.phashBlock = &hashPrev;
    indexPrev.nHeight = index.nHeight - 1;
    indexPrev.hashFinalSaplingRoot = uint256S("3922921f92d9abe9e440bb5a21fa901a0647057aa85c201e38d150a5e00043b8");
    indexPrev.hashFinalOrchardRoot = uint256S("0e9a0a55e201062a5677b8ab15c96e9f4983726af31cd96b14b71ac6a931aea2");
    index.pprev = &indexPrev;
    mapBlockIndex.insert(std::make_pair(hashPrev, &indexPrev));

    CValidationState state;
    struct timeval tv_start;
    timer_start(tv_start);
    assert(ConnectBlock(block, state, &index, view, Params(), true, CheckAs::SlowBenchmark));
    auto duration = timer_stop(tv_start);

    // Undo alterations to global state
    mapBlockIndex.erase(hashPrev);
    SelectParams(ChainNameFromCommandLine());

    return duration;
}

double benchmark_connectblock_orchard()
{
    // Test for slowness encountered on 2022-06-20
    SelectParams(CBaseChainParams::MAIN);
    CBlock block;
    FILE* fp = fsbridge::fopen(GetDataDir() / "benchmark/block-1708048.dat", "rb");
    if (!fp) throw new std::runtime_error("Failed to open block data file");
    CAutoFile blkFile(fp, SER_DISK, CLIENT_VERSION);
    blkFile >> block;
    blkFile.fclose();

    // Fake its inputs
    auto hashPrev = uint256S("0000000001337f1dd6df5ab7dc19f67c47ffdc6f2b6aae8626b023dc7d1dd601");
    FakeCoinsViewDB fakeDB("benchmark/block-1708048-inputs", hashPrev);
    fakeDB.SetSaplingTrees({
        "01b02bd52471dff0de47602867308090d3b902382437252dff22bbff4c53053a5001f8bacfe2858595371f7220fbb20d3671d777ad6fe2b5dac1b4001eff6c52bb5a140000011e4b4d444e6dd1dbfba6d2b46fcc465a02fdbd813e17c05c04e105b64a94f3640000010de1ce54980e3bf469fff5d430c792ab02d162dfbff9abab55015c5cab46c86b00000001c33f17aa4e36e2e93faeb3fb811e8b499c5d6e11c709dd93fea6ae175d853673012accb7e30be11669222dff6ed9b5575815be8dbbb9f793ec796c30f30cbde80500000000000134e093f073973de750e85daff57c07a102dbc7bbc77436e99b0074f36f1cbf130000015ec9e9b1295908beed437df4126032ca57ada8e3ebb67067cd22a73c79a84009",
    });
    fakeDB.SetOrchardTrees({
        "0191d40100000000008af1cad8a7bc344581480f11b2896e6ef03799b98edc19aaa0123117ddef512e01d700a097a7dfce6fe2e77ba57843ac865ea69a7f664050e42d096cc144744422074f5c8c014e17d86ee296de28ae0c50a55506f14f06258a8120023e75a06a0230a0b80c66f79f318890c3e1eca5c5b98bb23ae56d9c79ed094d31ce96f850120320616e517dcf87f0c5457996c307537eaf63664217d87ad9bb5e7539b90ad73a9e7a903df0df8217724cf10f95f41317776054e4b2e5f50a3d5a9369f298470787a4fc067681dbcf704b64da10a3de6c24f1d04b286a5345d56a735262fb871723f6d3c7b9a8f205cb0c5cc3b1bda25a6ab45ac17f894df8e925610aa14dfa3ad4e323b3ae0cabfb6be4087fec8c66d9a9bbfc354bf1d9588b6620448182063b",
        "01a9d30100000000001a471915f689c1ebb722a8cb1f326914a2a0288a6ad6dd60bc123cdb5c938a3801cc096845e1e49f812073b8369b5ab0f0da6ae4413bf86f6d28e54039acde11080980947f9699f6b4081e94ec552ef6f9341ca101805cf809653183a98f4e2d8a35ee63096d5c673128918e6790176f252287693aa90774fd46bd27b49c18b3dc0a9180c0b126beac04eebdb32227b223fc95ba5a40e06408192dd9ff8e0c45ac36dd64e135a13d15a59e83b973cadafab39acb47b455a7abb075f68f748ede1c3fb55d42d0cb363605f01d31dca64e00b6c7036f39c296bd36e5ea3cab99cb59319e7a903df0df8217724cf10f95f41317776054e4b2e5f50a3d5a9369f298470787a4fc067681dbcf704b64da10a3de6c24f1d04b286a5345d56a735262fb871723f6d3c7b9a8f205cb0c5cc3b1bda25a6ab45ac17f894df8e925610aa14dfa3ad4e323b3ae0cabfb6be4087fec8c66d9a9bbfc354bf1d9588b6620448182063b",
    });
    CCoinsViewCache view(&fakeDB);

    // Fake the chain
    CBlockIndex index(block);
    index.nHeight = 1708048;
    CBlockIndex indexPrev;
    indexPrev.phashBlock = &hashPrev;
    indexPrev.nHeight = index.nHeight - 1;
    indexPrev.hashFinalOrchardRoot = uint256S("03fa83e2eb5fd7dcf22a413a0226394cb7525c066e7b07f976e7bda75d2eb0a5");
    index.pprev = &indexPrev;
    mapBlockIndex.insert(std::make_pair(hashPrev, &indexPrev));

    CValidationState state;
    struct timeval tv_start;
    timer_start(tv_start);
    assert(ConnectBlock(block, state, &index, view, Params(), true, CheckAs::SlowBenchmark));
    auto duration = timer_stop(tv_start);

    // Undo alterations to global state
    mapBlockIndex.erase(hashPrev);
    SelectParams(ChainNameFromCommandLine());

    return duration;
}

extern UniValue getnewaddress(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue sendtoaddress(const UniValue& params, bool fHelp);

double benchmark_sendtoaddress(CAmount amount)
{
    UniValue params(UniValue::VARR);
    auto addr = getnewaddress(params, false);

    params.push_back(addr);
    params.push_back(ValueFromAmount(amount));

    struct timeval tv_start;
    timer_start(tv_start);
    auto txid = sendtoaddress(params, false);
    return timer_stop(tv_start);
}

double benchmark_loadwallet()
{
    pre_wallet_load();
    struct timeval tv_start;
    bool fFirstRunRet=true;
    timer_start(tv_start);
    pwalletMain = new CWallet(Params(), "wallet.dat");
    DBErrors nLoadWalletRet = pwalletMain->LoadWallet(fFirstRunRet);
    auto res = timer_stop(tv_start);
    post_wallet_load();
    return res;
}

extern UniValue listunspent(const UniValue& params, bool fHelp);

double benchmark_listunspent()
{
    UniValue params(UniValue::VARR);
    struct timeval tv_start;
    timer_start(tv_start);
    auto unspent = listunspent(params, false);
    return timer_stop(tv_start);
}

double benchmark_create_sapling_spend()
{
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto address = sk.default_address();
    SaplingNote note(address, GetRand(MAX_MONEY), libzcash::Zip212Enabled::BeforeZip212);
    SaplingMerkleTree tree;
    auto maybe_cmu = note.cmu();
    tree.append(maybe_cmu.value());
    auto anchor = tree.root();
    auto witness = tree.witness();
    auto maybe_nf = note.nullifier(expsk.full_viewing_key(), witness.position());
    if (!(maybe_cmu && maybe_nf)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Could not create note commitment and nullifier");
    }

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << witness.path();
    std::vector<unsigned char> witnessChars(ss.begin(), ss.end());

    uint256 alpha;
    librustzcash_sapling_generate_r(alpha.begin());

    auto ctx = librustzcash_sapling_proving_ctx_init();

    struct timeval tv_start;
    timer_start(tv_start);

    SpendDescription sdesc;
    uint256 rcm = note.rcm();
    bool result = librustzcash_sapling_spend_proof(
        ctx,
        expsk.full_viewing_key().ak.begin(),
        expsk.nsk.begin(),
        note.d.data(),
        rcm.begin(),
        alpha.begin(),
        note.value(),
        anchor.begin(),
        witnessChars.data(),
        sdesc.cv.begin(),
        sdesc.rk.begin(),
        sdesc.zkproof.data());

    double t = timer_stop(tv_start);
    librustzcash_sapling_proving_ctx_free(ctx);
    if (!result) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "librustzcash_sapling_spend_proof() should return true");
    }
    return t;
}

double benchmark_create_sapling_output()
{
    auto sk = libzcash::SaplingSpendingKey::random();
    auto address = sk.default_address();

    std::array<unsigned char, ZC_MEMO_SIZE> memo;
    SaplingNote note(address, GetRand(MAX_MONEY),  libzcash::Zip212Enabled::BeforeZip212);

    libzcash::SaplingNotePlaintext notePlaintext(note, memo);
    auto res = notePlaintext.encrypt(note.pk_d);
    if (!res) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "SaplingNotePlaintext::encrypt() failed");
    }

    auto enc = res.value();
    auto encryptor = enc.second;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << address;
    std::vector<unsigned char> addressBytes(ss.begin(), ss.end());

    auto ctx = librustzcash_sapling_proving_ctx_init();

    struct timeval tv_start;
    timer_start(tv_start);

    OutputDescription odesc;
    uint256 rcm = note.rcm();
    bool result = librustzcash_sapling_output_proof(
        ctx,
        encryptor.get_esk().begin(),
        addressBytes.data(),
        rcm.begin(),
        note.value(),
        odesc.cv.begin(),
        odesc.zkproof.begin());

    double t = timer_stop(tv_start);
    librustzcash_sapling_proving_ctx_free(ctx);
    if (!result) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "librustzcash_sapling_output_proof() should return true");
    }
    return t;
}

// Verify Sapling spend from testnet
// txid: abbd823cbd3d4e3b52023599d81a96b74817e95ce5bb58354f979156bd22ecc8
// position: 0
double benchmark_verify_sapling_spend()
{
    SpendDescription spend;
    CDataStream ss(ParseHex("8c6cf86bbb83bf0d075e5bd9bb4b5cd56141577be69f032880b11e26aa32aa5ef09fd00899e4b469fb11f38e9d09dc0379f0b11c23b5fe541765f76695120a03f0261d32af5d2a2b1e5c9a04200cd87d574dc42349de9790012ce560406a8a876a1e54cfcdc0eb74998abec2a9778330eeb2a0ac0e41d0c9ed5824fbd0dbf7da930ab299966ce333fd7bc1321dada0817aac5444e02c754069e218746bf879d5f2a20a8b028324fb2c73171e63336686aa5ec2e6e9a08eb18b87c14758c572f4531ccf6b55d09f44beb8b47563be4eff7a52598d80959dd9c9fee5ac4783d8370cb7d55d460053d3e067b5f9fe75ff2722623fb1825fcba5e9593d4205b38d1f502ff03035463043bd393a5ee039ce75a5d54f21b395255df6627ef96751566326f7d4a77d828aa21b1827282829fcbc42aad59cdb521e1a3aaa08b99ea8fe7fff0a04da31a52260fc6daeccd79bb877bdd8506614282258e15b3fe74bf71a93f4be3b770119edf99a317b205eea7d5ab800362b97384273888106c77d633600"), SER_NETWORK, PROTOCOL_VERSION);
    ss >> spend;
    uint256 dataToBeSigned = uint256S("0x2dbf83fe7b88a7cbd80fac0c719483906bb9a0c4fc69071e4780d5f2c76e592c");

    auto ctx = sapling::init_verifier();

    struct timeval tv_start;
    timer_start(tv_start);

    bool result = ctx->check_spend(
        spend.cv.GetRawBytes(),
        spend.anchor.GetRawBytes(),
        spend.nullifier.GetRawBytes(),
        spend.rk.GetRawBytes(),
        spend.zkproof,
        spend.spendAuthSig,
        dataToBeSigned.GetRawBytes()
    );

    double t = timer_stop(tv_start);
    if (!result) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "librustzcash_sapling_check_spend() should return true");
    }
    return t;
}

// Verify Sapling output from testnet
// txid: abbd823cbd3d4e3b52023599d81a96b74817e95ce5bb58354f979156bd22ecc8
// position: 0
double benchmark_verify_sapling_output()
{
    OutputDescription output;
    CDataStream ss(ParseHex("edd742af18857e5ec2d71d346a7fe2ac97c137339bd5268eea86d32e0ff4f38f76213fa8cfed3347ac4e8572dd88aff395c0c10a59f8b3f49d2bc539ed6c726667e29d4763f914ddd0abf1cdfa84e44de87c233434c7e69b8b5b8f4623c8aa444163425bae5cef842972fed66046c1c6ce65c866ad894d02e6e6dcaae7a962d9f2ef95757a09c486928e61f0f7aed90ad0a542b0d3dc5fe140dfa7626b9315c77e03b055f19cbacd21a866e46f06c00e0c7792b2a590a611439b510a9aaffcf1073bad23e712a9268b36888e3727033eee2ab4d869f54a843f93b36ef489fb177bf74b41a9644e5d2a0a417c6ac1c8869bc9b83273d453f878ed6fd96b82a5939903f7b64ecaf68ea16e255a7fb7cc0b6d8b5608a1c6b0ed3024cc62c2f0f9c5cfc7b431ae6e9d40815557aa1d010523f9e1960de77b2274cb6710d229d475c87ae900183206ba90cb5bbc8ec0df98341b82726c705e0308ca5dc08db4db609993a1046dfb43dfd8c760be506c0bed799bb2205fc29dc2e654dce731034a23b0aaf6da0199248702ee0523c159f41f4cbfff6c35ace4dd9ae834e44e09c76a0cbdda1d3f6a2c75ad71212daf9575ab5f09ca148718e667f29ddf18c8a330a86ace18a86e89454653902aa393c84c6b694f27d0d42e24e7ac9fe34733de5ec15f5066081ce912c62c1a804a2bb4dedcef7cc80274f6bb9e89e2fce91dc50d6a73c8aefb9872f1cf3524a92626a0b8f39bbf7bf7d96ca2f770fc04d7f457021c536a506a187a93b2245471ddbfb254a71bc4a0d72c8d639a31c7b1920087ffca05c24214157e2e7b28184e91989ef0b14f9b34c3dc3cc0ac64226b9e337095870cb0885737992e120346e630a416a9b217679ce5a778fb15779c136bcecca5efe79012013d77d90b4e99dd22c8f35bc77121716e160d05bd30d288ee8886390ee436f85bdc9029df888a3a3326d9d4ddba5cb5318b3274928829d662e96fea1d601f7a306251ed8c6cc4e5a3a7a98c35a3650482a0eee08f3b4c2da9b22947c96138f1505c2f081f8972d429f3871f32bef4aaa51aa6945df8e9c9760531ac6f627d17c1518202818a91ca304fb4037875c666060597976144fcbbc48a776a2c61beb9515fa8f3ae6d3a041d320a38a8ac75cb47bb9c866ee497fc3cd13299970c4b369c1c2ceb4220af082fbecdd8114492a8e4d713b5a73396fd224b36c1185bd5e20d683e6c8db35346c47ae7401988255da7cfffdced5801067d4d296688ee8fe424b4a8a69309ce257eefb9345ebfda3f6de46bb11ec94133e1f72cd7ac54934d6cf17b3440800e70b80ebc7c7bfc6fb0fc2c"), SER_NETWORK, PROTOCOL_VERSION);
    ss >> output;

    auto ctx = sapling::init_verifier();

    struct timeval tv_start;
    timer_start(tv_start);

    bool result = ctx->check_output(
        output.cv.GetRawBytes(),
        output.cmu.GetRawBytes(),
        output.ephemeralKey.GetRawBytes(),
        output.zkproof
    );

    double t = timer_stop(tv_start);
    if (!result) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "librustzcash_sapling_check_output() should return true");
    }
    return timer_stop(tv_start);
}
