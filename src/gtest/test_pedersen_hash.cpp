#include <gtest/gtest.h>
#include "librustzcash.h"
#include "uint256.h"

TEST(PedersenHash, TestAPI) {
    const uint256 a = uint256S("87a086ae7d2252d58729b30263fb7b66308bf94ef59a76c9c86e7ea016536505");
    const uint256 b = uint256S("a75b84a125b2353da7e8d96ee2a15efe4de23df9601b9d9564ba59de57130406");
    uint256 result;

    librustzcash_merkle_hash(25, a.begin(), b.begin(), result.begin());

    uint256 expected_result = uint256S("5bf43b5736c19b714d1f462c9d22ba3492c36e3d9bbd7ca24d94b440550aa561");

    ASSERT_TRUE(result == expected_result);
}
