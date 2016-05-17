
#define BOOST_TEST_MODULE utilTest
#include <boost/test/included/unit_test.hpp>

#include "zerocash/utils/util.h"
#include "crypto/sha256.h"

#include "uint256.h"
#include "utilstrencodings.h"

BOOST_AUTO_TEST_CASE( testConvertVectorToInt ) {
    BOOST_CHECK(libzerocash::convertVectorToInt({0}) == 0);
    BOOST_CHECK(libzerocash::convertVectorToInt({1}) == 1);
    BOOST_CHECK(libzerocash::convertVectorToInt({0,1}) == 1);
    BOOST_CHECK(libzerocash::convertVectorToInt({1,0}) == 2);
    BOOST_CHECK(libzerocash::convertVectorToInt({1,1}) == 3);
    BOOST_CHECK(libzerocash::convertVectorToInt({1,0,0}) == 4);
    BOOST_CHECK(libzerocash::convertVectorToInt({1,0,1}) == 5);
    BOOST_CHECK(libzerocash::convertVectorToInt({1,1,0}) == 6);

    BOOST_CHECK_THROW(libzerocash::convertVectorToInt(std::vector<bool>(100)), std::length_error);

    {
        std::vector<bool> v(63, 1);
        BOOST_CHECK(libzerocash::convertVectorToInt(v) == 0x7fffffffffffffff);
    }

    {
        std::vector<bool> v(64, 1);
        BOOST_CHECK(libzerocash::convertVectorToInt(v) == 0xffffffffffffffff);
    }
}

BOOST_AUTO_TEST_CASE( testConvertBytesVectorToVector ) {
    std::vector<unsigned char> bytes = {0x00, 0x01, 0x03, 0x12, 0xFF};
    std::vector<bool> expected_bits = {
        // 0x00
        0, 0, 0, 0, 0, 0, 0, 0,
        // 0x01
        0, 0, 0, 0, 0, 0, 0, 1,
        // 0x03
        0, 0, 0, 0, 0, 0, 1, 1,
        // 0x12
        0, 0, 0, 1, 0, 0, 1, 0,
        // 0xFF
        1, 1, 1, 1, 1, 1, 1, 1
    };
    std::vector<bool> actual_bits;
    libzerocash::convertBytesVectorToVector(bytes, actual_bits);
    BOOST_CHECK(actual_bits == expected_bits);
}

