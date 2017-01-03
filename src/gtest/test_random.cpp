#include <gtest/gtest.h>

#include "random.h"

extern int GenZero(int n);
extern int GenMax(int n);

TEST(Random, MappedShuffle) {
    std::vector<int> a {8, 4, 6, 3, 5};
    std::vector<int> m {0, 1, 2, 3, 4};

    auto a1 = a;
    auto m1 = m;
    MappedShuffle(a1.begin(), m1.begin(), a1.size(), GenZero);
    std::vector<int> ea1 {4, 6, 3, 5, 8};
    std::vector<int> em1 {1, 2, 3, 4, 0};
    EXPECT_EQ(ea1, a1);
    EXPECT_EQ(em1, m1);

    auto a2 = a;
    auto m2 = m;
    MappedShuffle(a2.begin(), m2.begin(), a2.size(), GenMax);
    std::vector<int> ea2 {8, 4, 6, 3, 5};
    std::vector<int> em2 {0, 1, 2, 3, 4};
    EXPECT_EQ(ea2, a2);
    EXPECT_EQ(em2, m2);

    auto a3 = a;
    auto m3 = m;
    MappedShuffle(a3.begin(), m3.begin(), a3.size(), GenIdentity);
    std::vector<int> ea3 {8, 4, 6, 3, 5};
    std::vector<int> em3 {0, 1, 2, 3, 4};
    EXPECT_EQ(ea3, a3);
    EXPECT_EQ(em3, m3);
}
