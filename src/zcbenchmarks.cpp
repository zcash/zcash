#include "zerocash/IncrementalMerkleTree.h"

#include <unistd.h>
#include <boost/filesystem.hpp>
#include "zcash/Zcash.h"
#include "zerocash/ZerocashParams.h"
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
    boost::filesystem::path pk_path = ZC_GetParamsDir() / "zc-testnet-public-alpha-proving.key";
    boost::filesystem::path vk_path = ZC_GetParamsDir() / "zc-testnet-public-alpha-verification.key";

    timer_start();
    auto vk_loaded = libzerocash::ZerocashParams::LoadVerificationKeyFromFile(
        vk_path.string(),
        INCREMENTAL_MERKLE_TREE_DEPTH
    );
    auto pk_loaded = libzerocash::ZerocashParams::LoadProvingKeyFromFile(
        pk_path.string(),
        INCREMENTAL_MERKLE_TREE_DEPTH
    );
    libzerocash::ZerocashParams zerocashParams = libzerocash::ZerocashParams(
        INCREMENTAL_MERKLE_TREE_DEPTH,
        &pk_loaded,
        &vk_loaded
    );
    return timer_stop();
}

double benchmark_create_joinsplit()
{
    CScript scriptPubKey;

    std::vector<PourInput> vpourin;
    std::vector<PourOutput> vpourout;

    while (vpourin.size() < ZC_NUM_JS_INPUTS) {
        vpourin.push_back(PourInput(INCREMENTAL_MERKLE_TREE_DEPTH));
    }

    while (vpourout.size() < ZC_NUM_JS_OUTPUTS) {
        vpourout.push_back(PourOutput(0));
    }

    /* Get the anchor of an empty commitment tree. */
    uint256 anchor = ZCIncrementalMerkleTree().root();

    timer_start();
    CPourTx pourtx(*pzerocashParams,
                   scriptPubKey,
                   anchor,
                   {vpourin[0], vpourin[1]},
                   {vpourout[0], vpourout[1]},
                   0,
                   0);
    double ret = timer_stop();
    assert(pourtx.Verify(*pzerocashParams));
    return ret;
}

double benchmark_verify_joinsplit(const CPourTx &joinsplit)
{
    timer_start();
    joinsplit.Verify(*pzerocashParams);
    return timer_stop();
}

double benchmark_solve_equihash()
{
    CBlock pblock;
    CEquihashInput I{pblock};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    Equihash eh {Params(CBaseChainParams::MAIN).EquihashN(), Params(CBaseChainParams::MAIN).EquihashK()};
    crypto_generichash_blake2b_state eh_state;
    eh.InitialiseState(eh_state);
    crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());

    uint256 nonce;
    randombytes_buf(nonce.begin(), 32);
    crypto_generichash_blake2b_update(&eh_state,
                                    nonce.begin(),
                                    nonce.size());

    timer_start();
    eh.BasicSolve(eh_state);
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

