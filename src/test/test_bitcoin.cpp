// Copyright (c) 2011-2013 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#ifdef ENABLE_MINING
#include "crypto/equihash.h"
#endif
#include "crypto/sha256.h"
#include "fs.h"
#include "key.h"
#include "main.h"
#include "miner.h"
#include "pubkey.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "rpc/server.h"
#include "rpc/register.h"
#include "script/sigcache.h"

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <sodium.h>
#include <tracing.h>

#include <rust/bridge.h>
#include <rust/init.h>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h

TracingHandle* pTracingHandle = nullptr;
uint256 insecure_rand_seed = GetRandHash();
FastRandomContext insecure_rand_ctx(insecure_rand_seed);

extern bool fPrintToConsole;
extern void noui_connect();

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
{
    assert(sodium_init() != -1);
    SHA256AutoDetect();
    ECC_Start();
    SetupEnvironment();
    SetupNetworking();
    InitSignatureCache(DEFAULT_MAX_SIG_CACHE_SIZE * ((size_t) 1 << 20));
    bundlecache::init(DEFAULT_MAX_SIG_CACHE_SIZE * ((size_t) 1 << 20));

    // Uncomment this to log all errors to stdout so we see them in test output.
    // We don't enable this by default because several tests intentionally cause
    // verbose error output (while exercising failure cases).
    // if (pTracingHandle == nullptr) {
    //     std::string initialFilter = "error";
    //     pTracingHandle = tracing_init(
    //         nullptr, 0,
    //         initialFilter.c_str(),
    //         false);
    // }

    fCheckBlockIndex = true;
    SelectParams(chainName);
    noui_connect();
    TestSetIBD(false);
}

BasicTestingSetup::~BasicTestingSetup()
{
    ECC_Stop();
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    fs::path sprout_groth16 = ZC_GetParamsDir() / "sprout-groth16.params";

    static_assert(
        sizeof(fs::path::value_type) == sizeof(codeunit),
        "librustzcash not configured correctly");
    auto sprout_groth16_str = sprout_groth16.native();

    init::zksnark_params(
        rust::String(
            reinterpret_cast<const codeunit*>(sprout_groth16_str.data()),
            sprout_groth16_str.size()),
        // Only load the verifying keys, which some tests need.
        false
    );

    const CChainParams& chainparams = Params();
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.
        RegisterAllCoreRPCCommands(tableRPC);

        // Save current path, in case a test changes it
        orig_current_path = fs::current_path();

        ClearDatadirCache();
        pathTemp = fs::temp_directory_path() / strprintf("test_bitcoin_%lu_%i", (unsigned long)GetTime(), (int)(InsecureRandRange(100000)));
        fs::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        InitBlockIndex(chainparams);
        {
            CValidationState state;
            bool ok = ActivateBestChain(state, chainparams);
            BOOST_CHECK(ok);
        }
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
        UnregisterNodeSignals(GetNodeSignals());
        threadGroup.interrupt_all();
        threadGroup.join_all();
        UnloadBlockIndex();
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;

        // Restore the previous current path so temporary directory can be deleted
        fs::current_path(orig_current_path);

        fs::remove_all(pathTemp);
}

#ifdef ENABLE_MINING
TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // Generate a 100-block chain:
    coinbaseKey = CKey::TestOnlyRandomKey(true);
    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < COINBASE_MATURITY; i++)
    {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        coinbaseTxns.push_back(b.vtx[0]);
    }
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    const CChainParams& chainparams = Params();
    unsigned int n = chainparams.GetConsensus().nEquihashN;
    unsigned int k = chainparams.GetConsensus().nEquihashK;

    boost::shared_ptr<CReserveScript> mAddr(new CReserveScript());
    mAddr->reserveScript = scriptPubKey;

    CBlockTemplate *pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(mAddr);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    for (const CMutableTransaction& tx : txns)
        block.vtx.emplace_back(tx);
    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    unsigned int extraNonce = 0;
    IncrementExtraNonce(pblocktemplate, chainActive.Tip(), extraNonce, chainparams.GetConsensus());

    // Hash state
    eh_HashState eh_state = EhInitialiseState(n, k);

    // I = the block header minus nonce and solution.
    CEquihashInput I{block};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    // H(I||...
    eh_state.Update((unsigned char*)&ss[0], ss.size());

    bool found = false;
    do {
        block.nNonce = ArithToUint256(UintToArith256(block.nNonce) + 1);

        // H(I||V||...
        eh_HashState curr_state(eh_state);
        curr_state.Update(block.nNonce.begin(), block.nNonce.size());

        // (x_1, x_2, ...) = A(I, V, n, k)
        std::function<bool(std::vector<unsigned char>)> validBlock =
                [&block, &chainparams](std::vector<unsigned char> soln) {
            block.nSolution = soln;
            return CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus());
        };
        found = EhBasicSolveUncancellable(n, k, curr_state, validBlock);
    } while (!found);

    CValidationState state;
    ProcessNewBlock(state, chainparams, NULL, &block, true, NULL);

    CBlock result = block;
    delete pblocktemplate;
    return result;
}

TestChain100Setup::~TestChain100Setup()
{
}
#endif // ENABLE_MINING


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransaction &tx, CTxMemPool *pool) {
    bool hasNoDependencies = pool ? pool->HasNoInputsOf(tx) : hadNoDependencies;

    return CTxMemPoolEntry(tx, nFee, nTime, nHeight,
                           hasNoDependencies, spendsCoinbase, sigOpCount, nBranchId);
}

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

bool ShutdownRequested()
{
  return false;
}
