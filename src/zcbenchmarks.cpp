#include <unistd.h>
#include <boost/filesystem.hpp>
#include "coins.h"
#include "util.h"
#include "init.h"
#include "primitives/transaction.h"
#include "crypto/equihash.h"
#include "chainparams.h"
#include "pow.h"
#include "sodium.h"
#include "streams.h"

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

