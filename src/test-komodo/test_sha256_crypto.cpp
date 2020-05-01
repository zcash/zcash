#include <gtest/gtest.h>
#include "crypto/sha256.h"
#include "uint256.h"
#include <stdexcept>
#include "random.h"
#include "utilstrencodings.h"

namespace TestSHA256Crypto {

    class TestSHA256Crypto : public ::testing::Test {};
    static const std::string log_tab = "             ";

    template<typename Hasher, typename In, typename Out>
    void TestVector(const Hasher &h, const In &in, const Out &out) {
        Out hash;
        ASSERT_TRUE(out.size() == h.OUTPUT_SIZE);
        hash.resize(out.size());
        {
            // Test that writing the whole input string at once works.
            Hasher(h).Write((unsigned char*)&in[0], in.size()).Finalize(&hash[0]);
            ASSERT_TRUE(hash == out);
        }
        for (int i=0; i<32; i++) {
            // Test that writing the string broken up in random pieces works.
            Hasher hasher(h);
            size_t pos = 0;
            while (pos < in.size()) {
                size_t len = insecure_rand() % ((in.size() - pos + 1) / 2 + 1);
                hasher.Write((unsigned char*)&in[pos], len);
                pos += len;
                if (pos > 0 && pos + 2 * out.size() > in.size() && pos < in.size()) {
                    // Test that writing the rest at once to a copy of a hasher works.
                    Hasher(hasher).Write((unsigned char*)&in[pos], in.size() - pos).Finalize(&hash[0]);
                    ASSERT_TRUE(hash == out);
                }
            }
            hasher.Finalize(&hash[0]);
            ASSERT_TRUE(hash == out);
        }
    }

    void TestSHA256(const std::string &in, const std::string &hexout) { TestVector(CSHA256(), in, ParseHex(hexout));}

    std::string LongTestString(void) {
        std::string ret;
        for (int i=0; i<200000; i++) {
            ret += (unsigned char)(i);
            ret += (unsigned char)(i >> 4);
            ret += (unsigned char)(i >> 8);
            ret += (unsigned char)(i >> 12);
            ret += (unsigned char)(i >> 16);
        }
        return ret;
    }

    const std::string test1 = LongTestString();

    TEST(TestSHA256Crypto, compression) // sha256compress_tests.cpp
    {
        {
            std::cerr << log_tab << "Using \"" << SHA256AutoDetect() << "\" SHA256 implementation ..." << std::endl;

            unsigned char preimage[64] = {};
            CSHA256 hasher;
            hasher.Write(&preimage[0], 64);

            uint256 digest;

            hasher.FinalizeNoPadding(digest.begin());

            ASSERT_TRUE(digest == uint256S("d8a93718eaf9feba4362d2c091d4e58ccabe9f779957336269b4b917be9856da")) << digest.GetHex();
        }

        {
            unsigned char preimage[63] = {};
            CSHA256 hasher;
            hasher.Write(&preimage[0], 63);
            uint256 digest;
            ASSERT_THROW(hasher.FinalizeNoPadding(digest.begin()), std::length_error);
        }

        {
            unsigned char preimage[65] = {};
            CSHA256 hasher;
            hasher.Write(&preimage[0], 65);
            uint256 digest;
            ASSERT_THROW(hasher.FinalizeNoPadding(digest.begin()), std::length_error);
        }

        {
            unsigned char n = 0x00;
            CSHA256 hasher;
            for (size_t i = 0; i < 64; i++) {
                hasher.Write(&n, 1);
            }
            uint256 digest;

            hasher.FinalizeNoPadding(digest.begin());

            ASSERT_TRUE(digest == uint256S("d8a93718eaf9feba4362d2c091d4e58ccabe9f779957336269b4b917be9856da")) << digest.GetHex();
        }

        {
            unsigned char preimage[64] = {  'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd',
                                            'a', 'b', 'c', 'd'
                                         };
            CSHA256 hasher;
            hasher.Write(&preimage[0], 64);

            uint256 digest;

            hasher.FinalizeNoPadding(digest.begin());

            ASSERT_TRUE(digest == uint256S("da70ec41879e36b000281733d4deb27ddf41e8e343a38f2fabbd2d8611987d86")) << digest.GetHex();

        }
    }

    TEST(TestSHA256Crypto, sha256_testvectors) // crypto_tests.cpp
    {
        TestSHA256("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        TestSHA256("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        TestSHA256("message digest",
            "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650");
        TestSHA256("secure hash algorithm",
            "f30ceb2bb2829e79e4ca9753d35a8ecc00262d164cc077080295381cbd643f0d");
        TestSHA256("SHA256 is considered to be safe",
            "6819d915c73f4d1e77e4e1b52d1fa0f9cf9beaead3939f15874bd988e2a23630");
        TestSHA256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
        TestSHA256("For this sample, this 63-byte string will be used as input data",
            "f08a78cbbaee082b052ae0708f32fa1e50c5c421aa772ba5dbb406a2ea6be342");
        TestSHA256("This is exactly 64 bytes long, not counting the terminating byte",
            "ab64eff7e88e2e46165e29f2bce41826bd4c7b3552f6b382a9e7d3af47c245f8");
        TestSHA256("As Bitcoin relies on 80 byte header hashes, we want to have an example for that.",
            "7406e8de7d6e4fffc573daef05aefb8806e7790f55eab5576f31349743cca743");
        TestSHA256(std::string(1000000, 'a'),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
        TestSHA256(test1, "a316d55510b49662420f49d145d42fb83f31ef8dc016aa4e32df049991a91e26");
        TestSHA256("The quick brown fox jumps over the lazy dog",
            "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    }

}