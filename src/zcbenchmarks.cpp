#include <unistd.h>
#include <boost/filesystem.hpp>
#include "coins.h"
#include "util.h"
#include "init.h"
#include "primitives/transaction.h"
#include "base58.h"
#include "crypto/equihash.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "main.h"
#include "miner.h"
#include "pow.h"
#include "script/sign.h"
#include "sodium.h"
#include "streams.h"
#include "wallet/wallet.h"

#include "zcbenchmarks.h"

#include "zcash/Zcash.h"
#include "zcash/IncrementalMerkleTree.hpp"

using namespace libzcash;

struct timeval tv_start;

void timer_start()
{
    gettimeofday(&tv_start, 0);
}

double timer_stop()
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
    timer_start();
    sleep(1);
    return timer_stop();
}

double benchmark_parameter_loading()
{
    // FIXME: this is duplicated with the actual loading code
    boost::filesystem::path pk_path = ZC_GetParamsDir() / "z5-proving.key";
    boost::filesystem::path vk_path = ZC_GetParamsDir() / "z5-verifying.key";

    timer_start();

    auto newParams = ZCJoinSplit::Unopened();

    newParams->loadVerifyingKey(vk_path.string());
    newParams->setProvingKeyPath(pk_path.string());
    newParams->loadProvingKey();

    double ret = timer_stop();

    delete newParams;

    return ret;
}

double benchmark_create_joinsplit()
{
    uint256 pubKeyHash;

    /* Get the anchor of an empty commitment tree. */
    uint256 anchor = ZCIncrementalMerkleTree().root();

    timer_start();
    CPourTx pourtx(*pzcashParams,
                   pubKeyHash,
                   anchor,
                   {JSInput(), JSInput()},
                   {JSOutput(), JSOutput()},
                   0,
                   0);
    double ret = timer_stop();

    assert(pourtx.Verify(*pzcashParams, pubKeyHash));
    return ret;
}

double benchmark_verify_joinsplit(const CPourTx &joinsplit)
{
    timer_start();
    uint256 pubKeyHash;
    joinsplit.Verify(*pzcashParams, pubKeyHash);
    return timer_stop();
}

double benchmark_solve_equihash()
{
    CBlock pblock;
    CEquihashInput I{pblock};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    unsigned int n = Params(CBaseChainParams::MAIN).EquihashN();
    unsigned int k = Params(CBaseChainParams::MAIN).EquihashK();
    crypto_generichash_blake2b_state eh_state;
    EhInitialiseState(n, k, eh_state);
    crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());

    uint256 nonce;
    randombytes_buf(nonce.begin(), 32);
    crypto_generichash_blake2b_update(&eh_state,
                                    nonce.begin(),
                                    nonce.size());

    timer_start();
    std::set<std::vector<unsigned int>> solns;
    EhOptimisedSolve(n, k, eh_state, solns);
    return timer_stop();
}

double benchmark_verify_equihash()
{
    CChainParams params = Params(CBaseChainParams::MAIN);
    CBlock genesis = Params(CBaseChainParams::MAIN).GenesisBlock();
    CBlockHeader genesis_header = genesis.GetBlockHeader();
    timer_start();
    CheckEquihashSolution(&genesis_header, params);
    return timer_stop();
}

double benchmark_large_tx(bool testValidate)
{
    // Note that the transaction size is checked against the splitting
    // transaction, not the merging one. Thus we are assuming that transactions
    // with 1 input and N outputs are about the same size as transactions with
    // N inputs and 1 output.
    mapArgs["-blockmaxsize"] = itostr(MAX_BLOCK_SIZE);
    int nMaxBlockSize = MAX_BLOCK_SIZE-1000;

    std::vector<COutput> vCoins;
    pwalletMain->AvailableCoins(vCoins, true);

    // 1) Create a transaction splitting the first coinbase to many outputs
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash = vCoins[0].tx->GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout.resize(1);
    mtx.vout[0].scriptPubKey = vCoins[0].tx->vout[0].scriptPubKey;
    mtx.vout[0].nValue = 1000;
    SignSignature(*pwalletMain, *vCoins[0].tx, mtx, 0);

    // 1a) While the transaction is smaller than the maximum:
    CTransaction tx {mtx};
    unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
    while (nTxSize < nMaxBlockSize) {
        // 1b) Add another output
        size_t oldSize = mtx.vout.size();
        mtx.vout.resize(oldSize+1);
        mtx.vout[oldSize].scriptPubKey = vCoins[0].tx->vout[0].scriptPubKey;
        mtx.vout[oldSize].nValue = 1000;

        tx = mtx;
        nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
    }

    // 1c) Sign the splitting transaction
    SignSignature(*pwalletMain, *vCoins[0].tx, mtx, 0);
    uint256 hash = mtx.GetHash();
    mempool.clear();
    mempool.addUnchecked(hash, CTxMemPoolEntry(mtx, 11, GetTime(), 111.0, 11));

    // 2) Mine the splitting transaction into a block
    CScript scriptDummy = CScript() << OP_TRUE;
    CBlockTemplate* pblocktemplate = CreateNewBlock(scriptDummy);
    CBlock *pblock = &pblocktemplate->block;
    CBlockIndex* pindexPrev = chainActive.Tip();
    unsigned int nExtraNonce = 0;
    IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

    arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
    unsigned int n = Params().EquihashN();
    unsigned int k = Params().EquihashK();
    crypto_generichash_blake2b_state eh_state;
    EhInitialiseState(n, k, eh_state);
    CEquihashInput I{*pblock};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;
    crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());
    while (true) {
        crypto_generichash_blake2b_state curr_state;
        curr_state = eh_state;
        crypto_generichash_blake2b_update(&curr_state,
                                          pblock->nNonce.begin(),
                                          pblock->nNonce.size());
        std::set<std::vector<unsigned int>> solns;
        EhOptimisedSolve(n, k, curr_state, solns);
        for (auto soln : solns) {
            pblock->nSolution = soln;
            if (UintToArith256(pblock->GetHash()) > hashTarget) {
                continue;
            }
            goto processblock;
        }
        pblock->nNonce = ArithToUint256(UintToArith256(pblock->nNonce) + 1);
    }
processblock:
    CValidationState state;
    assert(ProcessNewBlock(state, NULL, pblock, true, NULL));

    timer_start();

    // 3) Create a transaction that merges all of the inputs
    CMutableTransaction mtx2;
    mtx2.vin.resize(mtx.vout.size());
    mtx2.vout.resize(1);
    mtx2.vout[0].scriptPubKey = vCoins[0].tx->vout[0].scriptPubKey;
    mtx2.vout[0].nValue = 0;

    for (int i = 0; i < mtx2.vin.size(); i++) {
        mtx2.vin[i].prevout.hash = hash;
        mtx2.vin[i].prevout.n = i;
    }

    for (int i = 0; i < mtx2.vin.size(); i++) {
        SignSignature(*pwalletMain, mtx, mtx2, i);
    }

    double ret = timer_stop();
    delete pblocktemplate;
    if (!testValidate)
        return ret;

    hash = mtx2.GetHash();
    mempool.addUnchecked(hash, CTxMemPoolEntry(mtx2, 11, GetTime(), 111.0, 11));

    // 4) Call CreateNewBlock (which itself calls TestBlockValidity)
    pblocktemplate = CreateNewBlock(scriptDummy);
    pblock = &pblocktemplate->block;

    // 5) Call TestBlockValidity again under timing
    timer_start();
    assert(TestBlockValidity(state, *pblock, pindexPrev, false, false));
    ret = timer_stop();
    delete pblocktemplate;
    mempool.clear();
    return ret;
}

