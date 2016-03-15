#include "NewIncrementalMerkleTree.hpp"
#include "zerocash/utils/sha256.h"

namespace NewTree {

SHA256Compress SHA256Compress::combine(const SHA256Compress& a, const SHA256Compress& b)
{
    // This is a performance optimization.
    if (a == uint256() && b == uint256()) {
        return a;
    }

    SHA256Compress res = SHA256Compress();

    SHA256_CTX_mod ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, a.begin(), 32);
    sha256_update(&ctx, b.begin(), 32);

    sha256_final_no_padding(&ctx, res.begin());

    return res;
}

}