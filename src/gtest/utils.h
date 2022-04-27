#ifndef BITCOIN_GTEST_UTILS_H
#define BITCOIN_GTEST_UTILS_H

#include "random.h"
#include "util.h"
#include "key_io.h"

int GenZero(int n);
int GenMax(int n);
void LoadProofParameters();
void LoadGlobalWallet();
void UnloadGlobalWallet();

extern uint256 insecure_rand_seed;
extern FastRandomContext insecure_rand_ctx;

static inline void seed_insecure_rand(bool fDeterministic = false)
{
    if (fDeterministic) {
        insecure_rand_seed = uint256();
    } else {
        insecure_rand_seed = GetRandHash();
    }
    insecure_rand_ctx = FastRandomContext(insecure_rand_seed);
}

static inline uint32_t insecure_rand(void)
{
    return insecure_rand_ctx.rand32();
}

#endif
