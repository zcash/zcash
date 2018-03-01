#include <gtest/gtest.h>

TEST(tautologies, seven_eq_seven) {
    ASSERT_EQ(7, 7);
}

TEST(tautologies, DISABLED_ObviousFailure)
{
    FAIL() << "This is expected";
}
