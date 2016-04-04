#include "prf.h"
#include "crypto/sha256.h"

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
