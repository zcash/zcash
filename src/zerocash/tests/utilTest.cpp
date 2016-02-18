
#define BOOST_TEST_MODULE utilTest
#include <boost/test/included/unit_test.hpp>

#include "zerocash/utils/util.h"
#include "zerocash/utils/sha256.h"

#define SHA256_PREIMAGE_BYTES 3
const unsigned char sha256_preimage[SHA256_PREIMAGE_BYTES] = { 'a', 'b', 'c' };
/* This is the SHA256 hash of "abc" according to the modified implementation of
 * SHA256 included in libzerocash. */
const unsigned char sha256_hash[32] = { 0x6a, 0x09, 0xe6, 0x67, 0xbb, 0x67, 0xae,
                                        0x85, 0x3c, 0x6e, 0xf3, 0x72, 0xa5, 0x4f,
                                        0xf5, 0x3a, 0x51, 0x0e, 0x52, 0x7f, 0x9b,
                                        0x05, 0x68, 0x8c, 0x1f, 0x83, 0xd9, 0xab,
                                        0x5b, 0xe0, 0xcd, 0x19 };

BOOST_AUTO_TEST_CASE( testGetRandBytes ) {
    unsigned char bytes1[32];
    unsigned char bytes2[32];

    memset(bytes1, 0, 32);
    memset(bytes2, 0, 32);

    libzerocash::getRandBytes(bytes1, 32);
    libzerocash::getRandBytes(bytes2, 32);

    BOOST_CHECK( memcmp(bytes1, bytes2, 32) != 0 );
    BOOST_CHECK( memcmp(bytes1, bytes1+16, 16) != 0 );
}

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

BOOST_AUTO_TEST_CASE( testConvertBytesToVector ) {
    unsigned char bytes[5] = {0x00, 0x01, 0x03, 0x12, 0xFF};
    std::vector<bool> v1(5*8);
    libzerocash::convertBytesToVector(bytes, v1);

    std::vector<bool> v2 = {
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

    BOOST_CHECK(v1 == v2);

    std::vector<bool> unevensize(4);
    unsigned char abyte[1] = { 0x55 };
    libzerocash::convertBytesToVector(abyte, unevensize);

    /* This may not be what we would expect, but this test will alert us if the
     * behavior changes. */
    v2 = { 0, 0, 0, 0 };
    BOOST_CHECK(unevensize == v2);
}

BOOST_AUTO_TEST_CASE( testConvertVectorToBytes) {
    unsigned char bytes[5] = {0x00, 0x01, 0x03, 0x12, 0xFF};
    std::vector<bool> v = {
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
    unsigned char output[5];
    libzerocash::convertVectorToBytes(v, output);
    BOOST_CHECK( memcmp(bytes, output, sizeof(bytes)) == 0 );

    /* This is not necessarily the behavior one would expect, but this test will
     * notify us if it changes. */
    unsigned char onebyte[1];
    std::vector<bool> unevensize { 1, 1, 1, 1, 1, 1, 1 };
    libzerocash::convertVectorToBytes(unevensize, onebyte);
    BOOST_CHECK(onebyte[0] == 0);
}

BOOST_AUTO_TEST_CASE( testConvertBytesToBytesVector ) {
    unsigned char bytes[16];
    for (int i = 0; i < 16; i++) {
        bytes[i] = i;
    }
    std::vector<unsigned char> v(16);
    libzerocash::convertBytesToBytesVector(bytes, v);
    for (int i = 0; i < 16; i++) {
        BOOST_CHECK(v.at(i) == bytes[i]);
    }
}

BOOST_AUTO_TEST_CASE( testConvertBytesVectorToBytes ) {
    std::vector<unsigned char>v(16);
    for (int i = 0; i < 16; i++) {
        v[i] = i;
    }
    unsigned char bytes[16];
    memset(bytes, 0, 16);
    libzerocash::convertBytesVectorToBytes(v, bytes);
    for (int i = 0; i < 16; i++) {
        BOOST_CHECK(bytes[i] == v.at(i));
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

BOOST_AUTO_TEST_CASE( testConvertVectorToBytesVector ) {
    std::vector<unsigned char> expected_bytes = {0x00, 0x01, 0x03, 0x12, 0xFF};
    std::vector<bool> bits = {
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
    // TODO: evaluate whether initializing with 5 should be necessary.
    std::vector<unsigned char> actual_bytes(5);
    libzerocash::convertVectorToBytesVector(bits, actual_bytes);
    BOOST_CHECK(actual_bytes == expected_bytes);
}

BOOST_AUTO_TEST_CASE( testConvertIntToBytesVector ) {
    uint64_t val;
    std::vector<unsigned char> expected;
    std::vector<unsigned char> bytes(8);

    val = 0ULL;
    expected = { 0, 0, 0, 0, 0, 0, 0, 0 };
    libzerocash::convertIntToBytesVector(val, bytes);
    BOOST_CHECK( expected == bytes );

    val = 1ULL;
    expected = { 0, 0, 0, 0, 0, 0, 0, 1 };
    libzerocash::convertIntToBytesVector(val, bytes);
    BOOST_CHECK( expected == bytes );

    val = 0xffffffffffffffffULL;
    expected = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    libzerocash::convertIntToBytesVector(val, bytes);
    BOOST_CHECK( expected == bytes );

    val = 0x8000000080000001ULL; // sign extension
    expected = { 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01 };
    libzerocash::convertIntToBytesVector(val, bytes);
    BOOST_CHECK( expected == bytes );

    // The following two tests aren't necessarily desired results. They merely
    // document the behavior so that we'll be alerted if it changes in the
    // future.

    val = 0xffffffffdeadbeefULL; // truncation
    expected = { 0xde, 0xad, 0xbe, 0xef };
    std::vector<unsigned char> small_bytes(4);
    libzerocash::convertIntToBytesVector(val, small_bytes);
    BOOST_CHECK( expected == small_bytes );

    val = 0xf1f2f3f401020304ULL; // bytes buffer is too big
    // The first byte is 4 because `>> 64` is undefined, and that's the result
    // it has on my system (note that it's the same as the original LSB).
    expected = { 0x04, 0xf1, 0xf2, 0xf3, 0xf4, 0x01, 0x02, 0x03, 0x04 };
    std::vector<unsigned char> big_bytes(9);
    libzerocash::convertIntToBytesVector(val, big_bytes);
    BOOST_CHECK( expected == big_bytes);
}

BOOST_AUTO_TEST_CASE( testConvertBytesVectorToInt ) {
    uint64_t val;
    uint64_t expected;
    std::vector<unsigned char> bytes;

    bytes = { 0, 0, 0, 0, 0, 0, 0, 0 };
    expected = 0ULL;
    val = libzerocash::convertBytesVectorToInt(bytes);
    BOOST_CHECK( expected == val );

    bytes = { 0, 0, 0, 0, 0, 0, 0, 1 };
    expected = 1ULL;
    val = libzerocash::convertBytesVectorToInt(bytes);
    BOOST_CHECK( expected == val );

    bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    expected = 0xffffffffffffffffULL;
    val = libzerocash::convertBytesVectorToInt(bytes);
    BOOST_CHECK( expected == val );

    bytes = { 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01 };
    expected = 0x8000000080000001ULL;
    val = libzerocash::convertBytesVectorToInt(bytes);
    BOOST_CHECK( expected == val );

    bytes = { 0xde, 0xad, 0xbe, 0xef }; // opposite of truncation
    expected = 0xdeadbeefULL;
    val = libzerocash::convertBytesVectorToInt(bytes);
    BOOST_CHECK( expected == val );
}

BOOST_AUTO_TEST_CASE( testConvertIntToVector ) {
    uint64_t val;
    std::vector<bool> expected;
    std::vector<bool> vector;

    val = 0ULL;
    expected = { 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0 };
    libzerocash::convertIntToVector(val, vector);
    BOOST_CHECK( expected == vector );

    val = 1ULL;
    expected = { 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 1 };
    libzerocash::convertIntToVector(val, vector);
    BOOST_CHECK( expected == vector );

    val = 0xffffffffffffffffULL;
    expected = { 1, 1, 1, 1, 1, 1, 1, 1,
                 1, 1, 1, 1, 1, 1, 1, 1,
                 1, 1, 1, 1, 1, 1, 1, 1,
                 1, 1, 1, 1, 1, 1, 1, 1,
                 1, 1, 1, 1, 1, 1, 1, 1,
                 1, 1, 1, 1, 1, 1, 1, 1,
                 1, 1, 1, 1, 1, 1, 1, 1,
                 1, 1, 1, 1, 1, 1, 1, 1 };
    libzerocash::convertIntToVector(val, vector);
    BOOST_CHECK( expected == vector );

    val = 0x8000000080000001ULL; // sign extension
    expected = { 1, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 1, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 1 };
    libzerocash::convertIntToVector(val, vector);
    BOOST_CHECK( expected == vector );

    std::vector<bool> too_big(100);
    libzerocash::convertIntToVector(0, too_big);
    BOOST_CHECK(too_big.size() == 64);

    std::vector<bool> too_small(10);
    libzerocash::convertIntToVector(0, too_small);
    BOOST_CHECK(too_big.size() == 64);
}

BOOST_AUTO_TEST_CASE( testConcatenateTwoBoolVectors ) {
    std::vector<bool> front = { 0, 1, 0 };
    std::vector<bool> back = { 1, 0, 1 };
    std::vector<bool> expected = { 0, 1, 0, 1, 0, 1 };
    std::vector<bool> actual;
    libzerocash::concatenateVectors(front, back, actual);
    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testConcatenateTwoByteVectors ) {
    std::vector<unsigned char> front = { 0, 1, 2 };
    std::vector<unsigned char> back = { 3, 4, 5 };
    std::vector<unsigned char> expected = { 0, 1, 2, 3, 4, 5 };
    std::vector<unsigned char> actual;
    libzerocash::concatenateVectors(front, back, actual);
    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testConcatenateThreeBoolVectors ) {
    std::vector<bool> front = { 0, 1, 0 };
    std::vector<bool> middle { 1, 1, 1 };
    std::vector<bool> back = { 1, 0, 1 };
    std::vector<bool> expected = { 0, 1, 0, 1, 1, 1, 1, 0, 1 };
    std::vector<bool> actual;
    libzerocash::concatenateVectors(front, middle, back, actual);
    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testConcatenateThreeByteVectors ) {
    std::vector<unsigned char> front = { 0, 1, 0 };
    std::vector<unsigned char> middle { 1, 1, 1 };
    std::vector<unsigned char> back = { 1, 0, 1 };
    std::vector<unsigned char> expected = { 0, 1, 0, 1, 1, 1, 1, 0, 1 };
    std::vector<unsigned char> actual;
    libzerocash::concatenateVectors(front, middle, back, actual);
    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testSHA256ModifiedTestVectors ) {
    unsigned char actual_hash[32];
    libzerocash::sha256(sha256_preimage, actual_hash, 3);
    BOOST_CHECK( memcmp(sha256_hash, actual_hash, 32) == 0 );
}

BOOST_AUTO_TEST_CASE( testSHA256ModifiedTestVectorsCTX ) {
    unsigned char actual_hash[32];
    SHA256_CTX_mod ctx256;
    libzerocash::sha256(&ctx256, sha256_preimage, actual_hash, 3);
    BOOST_CHECK( memcmp(sha256_hash, actual_hash, 32) == 0 );
}

BOOST_AUTO_TEST_CASE( testSHA256TestVectors ) {
    /* Tests an actual SHA256 test vector (with length padding) to make sure
     * libzerocash's implementation is actually the same as SHA256 with the
     * length padding removed. */
    unsigned char preimage[3] = { 'a', 'b', 'c' };
    unsigned char expected_hash[32] = { 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01,
                                        0xcf, 0xea, 0x41, 0x41, 0x40, 0xde,
                                        0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03,
                                        0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                                        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00,
                                        0x15, 0xad };
    unsigned char actual_hash[32];
    SHA256_CTX_mod ctx256;
    sha256_init(&ctx256);
    sha256_update(&ctx256, preimage, 3);
    sha256_length_padding(&ctx256);
    sha256_final_no_padding(&ctx256, actual_hash);

    BOOST_CHECK( memcmp(expected_hash, actual_hash, 32) == 0 );
}

BOOST_AUTO_TEST_CASE( testHashBoolVectorToBoolVectorCTX ) {
    SHA256_CTX_mod ctx256;

    std::vector<bool> preimage(SHA256_PREIMAGE_BYTES * 8);
    libzerocash::convertBytesToVector(sha256_preimage, preimage);

    std::vector<bool> expected(32*8);
    libzerocash::convertBytesToVector(sha256_hash, expected);

    // TODO: evaluate whether this should be a necessary precondition.
    std::vector<bool> actual(32*8);
    libzerocash::hashVector(&ctx256, preimage, actual);

    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testHashByteVectorToByteVectorCTX ) {
    SHA256_CTX_mod ctx256;

    std::vector<unsigned char> preimage(SHA256_PREIMAGE_BYTES);
    libzerocash::convertBytesToBytesVector(sha256_preimage, preimage);

    std::vector<unsigned char> expected(32);
    libzerocash::convertBytesToBytesVector(sha256_hash, expected);

    // TODO: evaluate whether this should be a necessary precondition.
    std::vector<unsigned char> actual(32);
    libzerocash::hashVector(&ctx256, preimage, actual);

    BOOST_CHECK( expected == actual );
}


BOOST_AUTO_TEST_CASE( testHashBoolVectorToBoolVector ) {
    std::vector<bool> preimage(SHA256_PREIMAGE_BYTES * 8);
    libzerocash::convertBytesToVector(sha256_preimage, preimage);

    std::vector<bool> expected(32*8);
    libzerocash::convertBytesToVector(sha256_hash, expected);

    // TODO: evaluate whether this should be a necessary precondition.
    std::vector<bool> actual(32*8);
    libzerocash::hashVector(preimage, actual);

    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testHashByteVectorToByteVector ) {
    std::vector<unsigned char> preimage(SHA256_PREIMAGE_BYTES);
    libzerocash::convertBytesToBytesVector(sha256_preimage, preimage);

    std::vector<unsigned char> expected(32);
    libzerocash::convertBytesToBytesVector(sha256_hash, expected);

    // TODO: evaluate whether this should be a necessary precondition.
    std::vector<unsigned char> actual(32);
    libzerocash::hashVector(preimage, actual);

    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testHashBoolVectorsCTX ) {
    SHA256_CTX_mod ctx256;

    std::vector<bool> preimage1(8);
    libzerocash::convertBytesToVector(sha256_preimage, preimage1);

    std::vector<bool> preimage2((SHA256_PREIMAGE_BYTES - 1) * 8);
    libzerocash::convertBytesToVector(sha256_preimage + 1, preimage2);

    std::vector<bool> expected(32*8);
    libzerocash::convertBytesToVector(sha256_hash, expected);

    // TODO: evaluate whether this should be a necessary precondition.
    std::vector<bool> actual(32 * 8);
    libzerocash::hashVectors(&ctx256, preimage1, preimage2, actual);

    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testHashByteVectorsCTX ) {
    SHA256_CTX_mod ctx256;

    std::vector<unsigned char> preimage1(1);
    libzerocash::convertBytesToBytesVector(sha256_preimage, preimage1);

    std::vector<unsigned char> preimage2(SHA256_PREIMAGE_BYTES - 1);
    libzerocash::convertBytesToBytesVector(sha256_preimage + 1, preimage2);

    std::vector<unsigned char> expected(32);
    libzerocash::convertBytesToBytesVector(sha256_hash, expected);

    // TODO: evaluate whether this should be a necessary precondition.
    std::vector<unsigned char> actual(32);
    libzerocash::hashVectors(&ctx256, preimage1, preimage2, actual);

    BOOST_CHECK( expected == actual );
}


BOOST_AUTO_TEST_CASE( testHashBoolVectors ) {
    std::vector<bool> preimage1(8);
    libzerocash::convertBytesToVector(sha256_preimage, preimage1);

    std::vector<bool> preimage2((SHA256_PREIMAGE_BYTES - 1) * 8);
    libzerocash::convertBytesToVector(sha256_preimage + 1, preimage2);

    std::vector<bool> expected(32*8);
    libzerocash::convertBytesToVector(sha256_hash, expected);

    // TODO: evaluate whether this should be a necessary precondition.
    std::vector<bool> actual(32 * 8);
    libzerocash::hashVectors(preimage1, preimage2, actual);

    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testHashByteVectors ) {
    std::vector<unsigned char> preimage1(1);
    libzerocash::convertBytesToBytesVector(sha256_preimage, preimage1);

    std::vector<unsigned char> preimage2(SHA256_PREIMAGE_BYTES - 1);
    libzerocash::convertBytesToBytesVector(sha256_preimage + 1, preimage2);

    std::vector<unsigned char> expected(32);
    libzerocash::convertBytesToBytesVector(sha256_hash, expected);

    // TODO: evaluate whether this should be a necessary precondition.
    std::vector<unsigned char> actual(32);
    libzerocash::hashVectors(preimage1, preimage2, actual);

    BOOST_CHECK( expected == actual );
}

BOOST_AUTO_TEST_CASE( testVectorIsZero ) {
    std::vector<bool> bits;
    BOOST_CHECK( libzerocash::VectorIsZero(bits) );

    bits = { 0 };
    BOOST_CHECK( libzerocash::VectorIsZero(bits) );

    bits = { 0, 0 };
    BOOST_CHECK( libzerocash::VectorIsZero(bits) );

    bits = { 1 };
    BOOST_CHECK( !libzerocash::VectorIsZero(bits) );

    bits = { 0, 1 };
    BOOST_CHECK( !libzerocash::VectorIsZero(bits) );
}

