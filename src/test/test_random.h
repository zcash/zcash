// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_TEST_TEST_RANDOM_H
#define BITCOIN_TEST_TEST_RANDOM_H

#include "random.h"

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

static inline uint32_t insecure_rand() { return insecure_rand_ctx.rand32(); }
static inline uint256 insecure_rand256() { return insecure_rand_ctx.rand256(); }
static inline uint64_t insecure_randbits(int bits) { return insecure_rand_ctx.randbits(bits); }
static inline uint64_t insecure_randrange(uint64_t range) { return insecure_rand_ctx.randrange(range); }
static inline bool insecure_randbool() { return insecure_rand_ctx.randbool(); }
static inline std::vector<unsigned char> insecure_randbytes(size_t len) { return insecure_rand_ctx.randbytes(len); }

#endif
