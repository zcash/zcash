#include <gtest/gtest.h>
#include "librustzcash.h"
#include "uint256.h"

TEST(PedersenHash, TestAPI) {
    const uint256 a = uint256S("0acaa62d40fcdd9192ed35ea9df31660ccf7f6c60566530faaa444fb5d0d410e");
    const uint256 b = uint256S("6041357de59ba64959d1b60f93de24dfe5ea1e26ed9e8a73d35b225a1845ba70");
    uint256 result;

    librustzcash_merkle_hash(25, a.begin(), b.begin(), result.begin());

    uint256 expected_result = uint256S("4253b36834b3f64cc6182f1816911e1c9460cb88afeafb155244dd0038ad4717");

    ASSERT_TRUE(result == expected_result);
}
