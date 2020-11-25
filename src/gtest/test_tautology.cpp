#include <gtest/gtest.h>

TEST(tautologies, SevenEqSeven) {
    ASSERT_EQ(7, 7);
}

TEST(tautologies, DISABLED_ObviousFailure)
{
    FAIL() << "This is expected";
}
