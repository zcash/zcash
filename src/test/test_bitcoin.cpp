// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "crypto/common.h"

#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#ifdef ENABLE_MINING
#include "crypto/equihash.h"
#endif
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

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

#include "librustzcash.h"

CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h

TracingHandle* pTracingHandle = nullptr;
FastRandomContext insecure_rand_ctx(true);

extern bool fPrintToConsole;
extern void noui_connect();

JoinSplitTestingSetup::JoinSplitTestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    boost::filesystem::path sapling_spend = ZC_GetParamsDir() / "sapling-spend.params";
    boost::filesystem::path sapling_output = ZC_GetParamsDir() / "sapling-output.params";
    boost::filesystem::path sprout_groth16 = ZC_GetParamsDir() / "sprout-groth16.params";

    static_assert(
        sizeof(boost::filesystem::path::value_type) == sizeof(codeunit),
        "librustzcash not configured correctly");
    auto sapling_spend_str = sapling_spend.native();
    auto sapling_output_str = sapling_output.native();
    auto sprout_groth16_str = sprout_groth16.native();

    librustzcash_init_zksnark_params(
        reinterpret_cast<const codeunit*>(sapling_spend_str.c_str()),
        sapling_spend_str.length(),
        reinterpret_cast<const codeunit*>(sapling_output_str.c_str()),
        sapling_output_str.length(),
        reinterpret_cast<const codeunit*>(sprout_groth16_str.c_str()),
        sprout_groth16_str.length()
    );
}

JoinSplitTestingSetup::~JoinSplitTestingSetup()
{
}

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
{
    assert(init_and_check_sodium() != -1);
    ECC_Start();
    SetupEnvironment();
    SetupNetworking();
    fPrintToDebugLog = false; // don't want to write to debug.log file
    fCheckBlockIndex = true;
    SelectParams(chainName);
    noui_connect();
}

BasicTestingSetup::~BasicTestingSetup()
{
    ECC_Stop();
}

TestingSetup::TestingSetup(const std::string& chainName) : JoinSplitTestingSetup(chainName)
{
    const CChainParams& chainparams = Params();
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.
        RegisterAllCoreRPCCommands(tableRPC);

        // Save current path, in case a test changes it
        orig_current_path = boost::filesystem::current_path();

        ClearDatadirCache();
        pathTemp = GetTempPath() / strprintf("test_bitcoin_%lu_%i", (unsigned long)GetTime(), (int)(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        InitBlockIndex(chainparams);
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
        boost::filesystem::current_path(orig_current_path);

        boost::filesystem::remove_all(pathTemp);
}

#ifdef ENABLE_MINING
TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // Generate a 100-block chain:
    coinbaseKey.MakeNewKey(true);
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

    CBlockTemplate *pblocktemplate = CreateNewBlock(chainparams, mAddr);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    BOOST_FOREACH(const CMutableTransaction& tx, txns)
        block.vtx.push_back(tx);
    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

    // Hash state
    crypto_generichash_blake2b_state eh_state;
    EhInitialiseState(n, k, eh_state);

    // I = the block header minus nonce and solution.
    CEquihashInput I{block};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    // H(I||...
    crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());

    bool found = false;
    do {
        block.nNonce = ArithToUint256(UintToArith256(block.nNonce) + 1);

        // H(I||V||...
        crypto_generichash_blake2b_state curr_state;
        curr_state = eh_state;
        crypto_generichash_blake2b_update(&curr_state,
                                          block.nNonce.begin(),
                                          block.nNonce.size());

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


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(CMutableTransaction &tx, CTxMemPool *pool) {
    return CTxMemPoolEntry(tx, nFee, nTime, dPriority, nHeight,
                           pool ? pool->HasNoInputsOf(tx) : hadNoDependencies,
                           spendsCoinbase, nBranchId);
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
