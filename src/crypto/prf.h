/*
Zcash uses SHA256Compress as a PRF for various components
within the zkSNARK circuit.
*/

#include "uint256.h"
#include "crypto/sha256.h"
#include <boost/static_assert.hpp>

uint256 PRF(bool a, bool b, bool c, bool d,
            uint256 x,
            uint256 y)
{
    uint256 res;
    unsigned char blob[64];

    memcpy(&blob[0], x.begin(), 32);
    memcpy(&blob[32], y.begin(), 32);

    blob[0] &= 0x0F;
    blob[0] |= (a ? 1 << 7 : 0) | (b ? 1 << 6 : 0) | (c ? 1 << 5 : 0) | (d ? 1 << 4 : 0);

    CSHA256 hasher;
    hasher.Write(blob, 64);
    hasher.FinalizeNoPadding(res.begin());

    return res;
}

template<unsigned char T>
uint256 PRF_addr(uint256 a_sk)
{
    BOOST_STATIC_ASSERT(T <= 7);

    uint256 y;
    *(y.end() - 1) = T;

    return PRF(0, 0, 0, 0, a_sk, y);
}