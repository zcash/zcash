// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "random.h"

#include "support/cleanse.h"
#ifdef WIN32
#include "compat.h" // for Windows API
#endif
#include "logging.h"  // for LogPrint()
#include "util/time.h" // for GetTime()

#include <limits>

#ifndef WIN32
#include <fcntl.h>
#include <sys/time.h>
#endif

#include <rust/random.h>

static inline int64_t GetPerformanceCounter()
{
    int64_t nCounter = 0;
#ifdef WIN32
    QueryPerformanceCounter((LARGE_INTEGER*)&nCounter);
#else
    timeval t;
    gettimeofday(&t, NULL);
    nCounter = (int64_t)(t.tv_sec * 1000000 + t.tv_usec);
#endif
    return nCounter;
}

void GetRandBytes(unsigned char* buf, size_t num)
{
    rust::getrandom({buf, num});
}

uint128_t GetRandUInt128(uint128_t nMax)
{
    return GetRandGeneric(nMax);
}

int128_t GetRandInt128(int128_t nMax)
{
    assert(nMax >= 0);
    return GetRandUInt128(nMax);
}

uint64_t GetRand(uint64_t nMax)
{
    return GetRandGeneric(nMax);
}

int64_t GetRandInt64(int64_t nMax)
{
    assert(nMax >= 0);
    return GetRand(nMax);
}

int GetRandInt(int nMax)
{
    assert(nMax >= 0);
    return GetRand(nMax);
}

uint256 GetRandHash()
{
    uint256 hash;
    GetRandBytes((unsigned char*)&hash, sizeof(hash));
    return hash;
}

void FastRandomContext::RandomSeed()
{
    uint256 seed = GetRandHash();
    rng.SetKey(seed.begin(), 32);
    requires_seed = false;
}

uint256 FastRandomContext::rand256()
{
    if (bytebuf_size < 32) {
        FillByteBuffer();
    }
    uint256 ret;
    memcpy(ret.begin(), bytebuf + 64 - bytebuf_size, 32);
    bytebuf_size -= 32;
    return ret;
}

std::vector<unsigned char> FastRandomContext::randbytes(size_t len)
{
    std::vector<unsigned char> ret(len);
    if (len > 0) {
        rng.Output(&ret[0], len);
    }
    return ret;
}

FastRandomContext::FastRandomContext(const uint256& seed) : requires_seed(false), bytebuf_size(0), bitbuf_size(0)
{
    rng.SetKey(seed.begin(), 32);
}

int GenIdentity(int n)
{
    return n-1;
}

FastRandomContext::FastRandomContext(bool fDeterministic) : requires_seed(!fDeterministic), bytebuf_size(0), bitbuf_size(0)
{
    if (!fDeterministic) {
        return;
    }
    uint256 seed;
    rng.SetKey(seed.begin(), 32);
}
