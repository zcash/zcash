#include <cstdio>
#include <future>
#include <map>
#include <thread>
#include <unistd.h>
#include <boost/filesystem.hpp>

#include "coins.h"
#include "util.h"
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
#include "pow.h"
#include "rpc/server.h"
#include "script/sign.h"
#include "sodium.h"
#include "streams.h"
#include "txdb.h"
#include "utiltest.h"
#include "wallet/wallet.h"

#include "zcbenchmarks.h"

#include "zcash/Zcash.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "zcash/Note.hpp"
#include "librustzcash.h"

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

double benchmark_parameter_loading()
{
    // FIXME: this is duplicated with the actual loading code
    boost::filesystem::path pk_path = ZC_GetParamsDir() / "sprout-proving.key";
    boost::filesystem::path vk_path = ZC_GetParamsDir() / "sprout-verifying.key";

    struct timeval tv_start;
    timer_start(tv_start);

    auto newParams = ZCJoinSplit::Prepared(vk_path.string(), pk_path.string());

    double ret = timer_stop(tv_start);

    delete newParams;

    return ret;
}

double benchmark_create_joinsplit()
{
    uint256 joinSplitPubKey;

    /* Get the anchor of an empty commitment tree. */
    uint256 anchor = SproutMerkleTree().root();

    struct timeval tv_start;
    timer_start(tv_start);
    JSDescription jsdesc(true,
                         *pzcashParams,
                         joinSplitPubKey,
                         anchor,
                         {JSInput(), JSInput()},
                         {JSOutput(), JSOutput()},
                         0,
                         0);
    double ret = timer_stop(tv_start);

    auto verifier = libzcash::ProofVerifier::Strict();
    assert(jsdesc.Verify(*pzcashParams, verifier, joinSplitPubKey));
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
    uint256 joinSplitPubKey;
    auto verifier = libzcash::ProofVerifier::Strict();
    joinsplit.Verify(*pzcashParams, verifier, joinSplitPubKey);
    return timer_stop(tv_start);
}

#ifdef ENABLE_MINING
double benchmark_solve_equihash()
{
    CBlock pblock;
    CEquihashInput I{pblock};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    auto params = Params(CBaseChainParams::MAIN).GetConsensus();
    unsigned int n = params.nEquihashN;
    unsigned int k = params.nEquihashK;
    crypto_generichash_blake2b_state eh_state;
    EhInitialiseState(n, k, eh_state);
    crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());

    uint256 nonce;
    randombytes_buf(nonce.begin(), 32);
    crypto_generichash_blake2b_update(&eh_state,
                                    nonce.begin(),
                                    nonce.size());

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
    CKey priv;
    priv.MakeNewKey(false);
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
    // Add nInputs inputs
    for (size_t i = 0; i < nInputs; i++) {
        spending_tx.vin.emplace_back(input_hash, 0);
    }

    // Sign for all the inputs
    auto consensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId;
    for (size_t i = 0; i < nInputs; i++) {
        SignSignature(tempKeystore, prevPubKey, spending_tx, i, 1000000, SIGHASH_ALL, consensusBranchId);
    }

    // Spending tx has all its inputs signed and does not need to be mutated anymore
    CTransaction final_spending_tx(spending_tx);

    // Benchmark signature verification costs:
    struct timeval tv_start;
    timer_start(tv_start);
    PrecomputedTransactionData txdata(final_spending_tx);
    for (size_t i = 0; i < nInputs; i++) {
        ScriptError serror = SCRIPT_ERR_OK;
        assert(VerifyScript(final_spending_tx.vin[i].scriptSig,
                            prevPubKey,
                            STANDARD_SCRIPT_VERIFY_FLAGS,
                            TransactionSignatureChecker(&final_spending_tx, i, 1000000, txdata),
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
    CWallet wallet;
    for (int i = 0; i < nKeys; i++) {
        auto sk = libzcash::SproutSpendingKey::random();
        wallet.AddSproutSpendingKey(sk);
    }

    auto sk = libzcash::SproutSpendingKey::random();
    auto tx = GetValidSproutReceive(*pzcashParams, sk, 10, true);

    struct timeval tv_start;
    timer_start(tv_start);
    auto noteDataMap = wallet.FindMySproutNotes(tx);

    assert(noteDataMap.empty());
    return timer_stop(tv_start);
}

double benchmark_try_decrypt_sapling_notes(size_t nKeys)
{
    // Set params
    auto consensusParams = Params().GetConsensus();

    auto masterKey = GetTestMasterSaplingSpendingKey();

    CWallet wallet;

    for (int i = 0; i < nKeys; i++) {
        auto sk = masterKey.Derive(i);
        wallet.AddSaplingSpendingKey(sk, sk.DefaultAddress());
    }

    // Generate a key that has not been added to the wallet
    auto sk = masterKey.Derive(nKeys);
    auto tx = GetValidSaplingReceive(consensusParams, wallet, sk, 10);

    struct timeval tv_start;
    timer_start(tv_start);
    auto noteDataMapAndAddressesToAdd = wallet.FindMySaplingNotes(tx);
    assert(noteDataMapAndAddressesToAdd.first.empty());
    return timer_stop(tv_start);
}

CWalletTx CreateSproutTxWithNoteData(const libzcash::SproutSpendingKey& sk) {
    auto wtx = GetValidSproutReceive(*pzcashParams, sk, 10, true);
    auto note = GetSproutNote(*pzcashParams, sk, wtx, 0, 1);
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
    auto consensusParams = Params().GetConsensus();

    CWallet wallet;
    SproutMerkleTree sproutTree;
    SaplingMerkleTree saplingTree;

    auto sproutSpendingKey = libzcash::SproutSpendingKey::random();
    wallet.AddSproutSpendingKey(sproutSpendingKey);

    // First block
    CBlock block1;
    for (int i = 0; i < nTxs; ++i) {
        auto wtx = CreateSproutTxWithNoteData(sproutSpendingKey);
        wallet.AddToWallet(wtx, true, NULL);
        block1.vtx.push_back(wtx);
    }

    CBlockIndex index1(block1);
    index1.nHeight = 1;

    // Increment to get transactions witnessed
    wallet.ChainTip(&index1, &block1, sproutTree, saplingTree, true);

    // Second block
    CBlock block2;
    block2.hashPrevBlock = block1.GetHash();
    {
        auto sproutTx = CreateSproutTxWithNoteData(sproutSpendingKey);
        wallet.AddToWallet(sproutTx, true, NULL);
        block2.vtx.push_back(sproutTx);
    }

    CBlockIndex index2(block2);
    index2.nHeight = 2;

    struct timeval tv_start;
    timer_start(tv_start);
    wallet.ChainTip(&index2, &block2, sproutTree, saplingTree, true);
    return timer_stop(tv_start);
}

CWalletTx CreateSaplingTxWithNoteData(const Consensus::Params& consensusParams,
                                      CBasicKeyStore& keyStore,
                                      const libzcash::SaplingExtendedSpendingKey &sk) {
    auto wtx = GetValidSaplingReceive(consensusParams, keyStore, sk, 10);
    auto testNote = GetTestSaplingNote(sk.DefaultAddress(), 10);
    auto fvk = sk.expsk.full_viewing_key();
    auto nullifier = testNote.note.nullifier(fvk, testNote.tree.witness().position()).get();

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
    auto consensusParams = Params().GetConsensus();

    CWallet wallet;
    SproutMerkleTree sproutTree;
    SaplingMerkleTree saplingTree;

    auto saplingSpendingKey = GetTestMasterSaplingSpendingKey();
    wallet.AddSaplingSpendingKey(saplingSpendingKey, saplingSpendingKey.DefaultAddress());

    // First block
    CBlock block1;
    for (int i = 0; i < nTxs; ++i) {
        auto wtx = CreateSaplingTxWithNoteData(consensusParams, wallet, saplingSpendingKey);
        wallet.AddToWallet(wtx, true, NULL);
        block1.vtx.push_back(wtx);
    }

    CBlockIndex index1(block1);
    index1.nHeight = 1;

    // Increment to get transactions witnessed
    wallet.ChainTip(&index1, &block1, sproutTree, saplingTree, true);

    // Second block
    CBlock block2;
    block2.hashPrevBlock = block1.GetHash();
    {
        auto saplingTx = CreateSaplingTxWithNoteData(consensusParams, wallet, saplingSpendingKey);
        wallet.AddToWallet(saplingTx, true, NULL);
        block1.vtx.push_back(saplingTx);
    }

    CBlockIndex index2(block2);
    index2.nHeight = 2;

    struct timeval tv_start;
    timer_start(tv_start);
    wallet.ChainTip(&index2, &block2, sproutTree, saplingTree, true);
    return timer_stop(tv_start);
}

// Fake the input of a given block
// This class is based on the class CCoinsViewDB, but with limited functionality.
// The construtor and the functions `GetCoins` and `HaveCoins` come directly from
// CCoinsViewDB, but the rest are either mocks and/or don't really do anything.

// The following constant is a duplicate of the one found in txdb.cpp
static const char DB_COINS = 'c';

class FakeCoinsViewDB : public CCoinsView {

    CDBWrapper db;

    uint256 hash;
    SproutMerkleTree sproutTree;
    SaplingMerkleTree saplingTree;

public:
    FakeCoinsViewDB(std::string dbName, uint256& hash) : db(GetDataDir() / dbName, 100, false, false), hash(hash) {}

    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
        if (rt == sproutTree.root()) {
            tree = sproutTree;
            return true;
        }
        return false;
    }

    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
        if (rt == saplingTree.root()) {
            tree = saplingTree;
            return true;
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
                return saplingTree.root();
            default:
                throw new std::runtime_error("Unknown shielded type");
        }
    }

    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap &mapSaplingNullifiers) {
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
    FILE* fp = fopen((GetDataDir() / "benchmark/block-107134.dat").string().c_str(), "rb");
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
    assert(ConnectBlock(block, state, &index, view, Params(), true));
    auto duration = timer_stop(tv_start);

    // Undo alterations to global state
    mapBlockIndex.erase(hashPrev);
    SelectParamsFromCommandLine();

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
    pwalletMain = new CWallet("wallet.dat");
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
    SaplingNote note(address, GetRand(MAX_MONEY));
    SaplingMerkleTree tree;
    auto maybe_cm = note.cm();
    tree.append(maybe_cm.get());
    auto anchor = tree.root();
    auto witness = tree.witness();
    auto maybe_nf = note.nullifier(expsk.full_viewing_key(), witness.position());
    if (!(maybe_cm && maybe_nf)) {
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
    bool result = librustzcash_sapling_spend_proof(
        ctx,
        expsk.full_viewing_key().ak.begin(),
        expsk.nsk.begin(),
        note.d.data(),
        note.r.begin(),
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
    SaplingNote note(address, GetRand(MAX_MONEY));

    libzcash::SaplingNotePlaintext notePlaintext(note, memo);
    auto res = notePlaintext.encrypt(note.pk_d);
    if (!res) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "SaplingNotePlaintext::encrypt() failed");
    }

    auto enc = res.get();
    auto encryptor = enc.second;

    auto ctx = librustzcash_sapling_proving_ctx_init();

    struct timeval tv_start;
    timer_start(tv_start);

    OutputDescription odesc;
    bool result = librustzcash_sapling_output_proof(
        ctx,
        encryptor.get_esk().begin(),
        note.d.data(),
        note.pk_d.begin(),
        note.r.begin(),
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

    auto ctx = librustzcash_sapling_verification_ctx_init();

    struct timeval tv_start;
    timer_start(tv_start);

    bool result = librustzcash_sapling_check_spend(
                ctx,
                spend.cv.begin(),
                spend.anchor.begin(),
                spend.nullifier.begin(),
                spend.rk.begin(),
                spend.zkproof.begin(),
                spend.spendAuthSig.begin(),
                dataToBeSigned.begin()
            );

    double t = timer_stop(tv_start);
    librustzcash_sapling_verification_ctx_free(ctx);
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

    auto ctx = librustzcash_sapling_verification_ctx_init();

    struct timeval tv_start;
    timer_start(tv_start);

    bool result = librustzcash_sapling_check_output(
                ctx,
                output.cv.begin(),
                output.cm.begin(),
                output.ephemeralKey.begin(),
                output.zkproof.begin()
            );

    double t = timer_stop(tv_start);
    librustzcash_sapling_verification_ctx_free(ctx);
    if (!result) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "librustzcash_sapling_check_output() should return true");
    }
    return timer_stop(tv_start);
}
