/*
Zcash uses SHA256Compress as a PRF for various components
within the zkSNARK circuit.
*/

#include "uint256.h"
#include "crypto/sha256.h"
#include <boost/static_assert.hpp>

uint256 PRF(bool a, bool b, bool c, bool d,
            uint248 x,
            uint256 y)
{
    uint256 res;
    unsigned char blob[64];

    if (a) blob[0] |= 0x80;
    if (b) blob[0] |= 0x40;
    if (c) blob[0] |= 0x20;
    if (d) blob[0] |= 0x10;

    memcpy(&blob[1], x.begin(), 31);
    memcpy(&blob[32], y.begin(), 32);

    CSHA256 hasher;
    hasher.Write(blob, 64);
    hasher.FinalizeNoPadding(res.begin());

    return res;
}

template<unsigned char T>
uint256 PRF_addr(uint248 a_sk)
{
    BOOST_STATIC_ASSERT(T <= 7);

    uint256 y;
    *(y.end() - 1) = T;

    return PRF(false, false, false, false, a_sk, y);
}