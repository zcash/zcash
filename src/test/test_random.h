// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_TEST_TEST_RANDOM_H
#define BITCOIN_TEST_TEST_RANDOM_H

#include "random.h"

extern uint256 insecure_rand_seed;
thread_local extern FastRandomContext g_insecure_rand_ctx;

static inline void seed_insecure_rand(bool fDeterministic = false)
{
    if (fDeterministic) {
        insecure_rand_seed = uint256();
    } else {
        insecure_rand_seed = GetRandHash();
    }
    g_insecure_rand_ctx = FastRandomContext(insecure_rand_seed);
}

static inline uint32_t insecure_rand(void)
{
    return g_insecure_rand_ctx.rand32();
}

#endif
