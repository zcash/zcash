#include <gtest/gtest.h>
#include "librustzcash.h"
#include "uint256.h"

TEST(PedersenHash, TestAPI) {
    const uint256 a = uint256S("7082b0badf222555f0ca66a0636fef330668cfb957acb74989bb3f02b4655350");
    const uint256 b = uint256S("0e5da2185a44dacbce5179b7647857a7fb247bc9f06d8b9a9265d9a7beac8206");
    uint256 result;

    librustzcash_merkle_hash(25, a.begin(), b.begin(), result.begin());

    uint256 expected_result = uint256S("e8e2b51c00bb224aa8df57f511d306293878896818f41863326fcd2c16cdca42");

    ASSERT_TRUE(result == expected_result);
}
