// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "util/string.h"

// Replace with std::identity once we’re on C++20
auto identity = [](const auto& x) { return x; };

TEST(FormatList, Basic)
{
    // empty collection
    EXPECT_EQ(FormatList(std::vector<std::string>(), "and", identity), "");

    // one element
    EXPECT_EQ(FormatList(std::vector<std::string>{"foo"}, "and", identity), "foo");

    // two elements, conjunction, custom element formatter
    EXPECT_EQ(FormatList(std::vector<std::string>{"foo", "bar"},
                         "and",
                         [](std::string s) { return s.replace(1, 1, "baz"); }),
              "fbazo and bbazr");

    // >2 elements, conjunction
    EXPECT_EQ(FormatList(std::vector<std::string>{"foo", "bar", "baz"}, "or", identity),
              "foo, bar, or baz");

    // >2 elements, no conjunction
    EXPECT_EQ(FormatList(std::vector<std::string>{"foo", "bar", "baz", "quux"}, std::nullopt, identity),
              "foo, bar, baz, quux");
}
