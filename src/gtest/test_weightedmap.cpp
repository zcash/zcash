// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <gtest/gtest.h>

#include "weighted_map.h"
#include "gtest/utils.h"
#include "util/test.h"


TEST(WeightedMapTests, WeightedMap)
{
    WeightedMap<int, int, int, GetRandInt> m;

    EXPECT_EQ(0, m.size());
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(0, m.getTotalWeight());
    m.checkInvariants();

    EXPECT_TRUE(m.add(3, 30, 3));
    EXPECT_EQ(1, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(3, m.getTotalWeight());
    m.checkInvariants();

    EXPECT_TRUE(m.add(1, 10, 2));
    EXPECT_EQ(2, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(5, m.getTotalWeight());
    m.checkInvariants();

    // adding a duplicate element should be ignored
    EXPECT_FALSE(m.add(1, 15, 64));
    EXPECT_EQ(2, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(5, m.getTotalWeight());
    m.checkInvariants();

    EXPECT_TRUE(m.add(2, 20, 1));
    EXPECT_EQ(3, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(6, m.getTotalWeight());
    m.checkInvariants();

    // regression test: adding three elements and deleting the first caused an invariant violation (not in committed code)
    EXPECT_EQ(30, m.remove(3).value());
    EXPECT_EQ(2, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(3, m.getTotalWeight());
    m.checkInvariants();

    // try to remove a non-existent element
    EXPECT_FALSE(m.remove(42).has_value());
    EXPECT_EQ(2, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(3, m.getTotalWeight());
    m.checkInvariants();

    EXPECT_EQ(20, m.remove(2).value());
    EXPECT_EQ(1, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(2, m.getTotalWeight());
    m.checkInvariants();

    EXPECT_TRUE(m.add(2, 20, 1));
    EXPECT_EQ(2, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(3, m.getTotalWeight());
    m.checkInvariants();

    // at this point the map should contain 1->10 (weight 2) and 2->20 (weight 1)
    auto [e1, c1, w1] = m.takeRandom().value();
    EXPECT_TRUE(e1 == 1 || e1 == 2);
    EXPECT_EQ(c1, e1*10);
    EXPECT_EQ(w1, 3-e1);
    EXPECT_EQ(1, m.size());
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(3-w1, m.getTotalWeight());
    m.checkInvariants();

    auto [e2, c2, w2] = m.takeRandom().value();
    EXPECT_EQ(3, e1 + e2);
    EXPECT_EQ(c2, e2*10);
    EXPECT_EQ(w2, 3-e2);
    EXPECT_EQ(0, m.size());
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(0, m.getTotalWeight());
    m.checkInvariants();

    EXPECT_FALSE(m.takeRandom().has_value());
    EXPECT_EQ(0, m.size());
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(0, m.getTotalWeight());
    m.checkInvariants();
}

TEST(WeightedMapTests, WeightedMapRandomOps)
{
    WeightedMap<int, int, int, GetRandInt> m;
    std::map<int, int> expected; // element -> weight
    int total_weight = 0;
    const int iterations = 1000;
    const int element_range = 20;
    const int max_weight = 10;
    static_assert(iterations <= std::numeric_limits<decltype(total_weight)>::max() / max_weight); // ensure total_weight cannot overflow

    EXPECT_EQ(0, m.size());
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(0, m.getTotalWeight());
    m.checkInvariants();
    for (int i = 0; i < iterations; i++) {
        switch (GetRandInt(4)) {
            // probability of add should be balanced with (remove or takeRandom)
            case 0: case 1: {
                int e = GetRandInt(element_range);
                int w = GetRandInt(max_weight) + 1;
                bool added = m.add(e, e*10, w);
                EXPECT_EQ(added, expected.count(e) == 0);
                if (added) {
                    total_weight += w;
                    expected[e] = w;
                }
                break;
            }
            case 2: {
                int e = GetRandInt(element_range);
                auto c = m.remove(e);
                if (expected.count(e) == 0) {
                    EXPECT_FALSE(c.has_value());
                } else {
                    ASSERT_TRUE(c.has_value());
                    EXPECT_EQ(c.value(), e*10);
                    total_weight -= expected[e];
                    expected.erase(e);
                }
                break;
            }
            case 3: {
                auto r = m.takeRandom();
                if (expected.empty()) {
                    EXPECT_FALSE(r.has_value());
                } else {
                    ASSERT_TRUE(r.has_value());
                    auto [e, c, w] = r.value();
                    EXPECT_EQ(1, expected.count(e));
                    EXPECT_EQ(c, e*10);
                    EXPECT_EQ(w, expected[e]);
                    total_weight -= expected[e];
                    expected.erase(e);
                }
                break;
            }
        }
        EXPECT_EQ(expected.size(), m.size());
        EXPECT_EQ(expected.empty(), m.empty());
        EXPECT_EQ(total_weight, m.getTotalWeight());
        m.checkInvariants();
    }
}
