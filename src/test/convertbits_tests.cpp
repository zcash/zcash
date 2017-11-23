// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <utilstrencodings.h>
#include <test/test_bitcoin.h>
#include <zcash/NoteEncryption.hpp>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(convertbits_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(convertbits_deterministic)
{
    for (size_t i = 0; i < 256; i++) {
        std::vector<unsigned char> input(32, i);
        std::vector<unsigned char> data;
        std::vector<unsigned char> output;
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, input.begin(), input.end());
        ConvertBits<5, 8, false>([&](unsigned char c) { output.push_back(c); }, data.begin(), data.end());
        BOOST_CHECK_EQUAL(data.size(), 52);
        BOOST_CHECK_EQUAL(output.size(), 32);
        BOOST_CHECK(input == output);
    }

    for (size_t i = 0; i < 256; i++) {
        std::vector<unsigned char> input(43, i);
        std::vector<unsigned char> data;
        std::vector<unsigned char> output;
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, input.begin(), input.end());
        ConvertBits<5, 8, false>([&](unsigned char c) { output.push_back(c); }, data.begin(), data.end());
        BOOST_CHECK_EQUAL(data.size(), 69);
        BOOST_CHECK_EQUAL(output.size(), 43);
        BOOST_CHECK(input == output);
    }
}

BOOST_AUTO_TEST_CASE(convertbits_random)
{
    for (size_t i = 0; i < 1000; i++) {
        auto input = libzcash::random_uint256();
        std::vector<unsigned char> data;
        std::vector<unsigned char> output;
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, input.begin(), input.end());
        ConvertBits<5, 8, false>([&](unsigned char c) { output.push_back(c); }, data.begin(), data.end());
        BOOST_CHECK_EQUAL(data.size(), 52);
        BOOST_CHECK_EQUAL(output.size(), 32);
        BOOST_CHECK(input == uint256(output));
    }
}

BOOST_AUTO_TEST_SUITE_END()
