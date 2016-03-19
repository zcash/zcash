#include "test/test_bitcoin.h"
#include "sodium.h"
#include "zerocash/utils/sha256.h"
#include "crypto/sha256.h"

#include "crypto/bort.hpp"

#include <iostream>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(keyprivate_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bort)
{
    // 64 + 32 + 24 + 8
    typedef Bort<128> MyBort;

    uint256 sk_enc = MyBort::generate_privkey(uint256S("21035d60bc1983e37950ce4803418a8fb33ea68d5b937ca382ecbae7564d6a77"));
    uint256 pk_enc = MyBort::generate_pubkey(sk_enc);

    MyBort b;

    boost::array<unsigned char, 128> message;
    for (unsigned char i = 0; i < 128; i++) {
        // Fill the message with dummy data
        message[i] = (unsigned char) i;
    }

    // Test nonce space
    for (unsigned char i = 0x00; i < 0xff; i++) {
        auto ciphertext = b.encrypt(pk_enc, message);

        auto plaintext = MyBort::decrypt(sk_enc, ciphertext, b.get_epk(), i);

        BOOST_CHECK(plaintext == message);

        // Test wrong nonce
        BOOST_CHECK_THROW(MyBort::decrypt(sk_enc, ciphertext, b.get_epk(), i + 1), std::runtime_error);

        // Test wrong private key
        uint256 sk_enc_2 = MyBort::generate_privkey(uint256());
        BOOST_CHECK_THROW(MyBort::decrypt(sk_enc_2, ciphertext, b.get_epk(), i), std::runtime_error);

        // Test wrong ephemeral key
        BOOST_CHECK_THROW(MyBort::decrypt(sk_enc, ciphertext, MyBort::generate_privkey(uint256()), i), std::runtime_error);

        // Test corrupted ciphertext
        ciphertext[10] ^= 0xff;
        BOOST_CHECK_THROW(MyBort::decrypt(sk_enc, ciphertext, b.get_epk(), i), std::runtime_error);
    }

    // Nonce space should run out here
    BOOST_CHECK_THROW(b.encrypt(pk_enc, message), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(migration_of_sha256compress)
{
    unsigned char digest_1[32];
    unsigned char digest_2[32];

    unsigned char preimage[64];

    {
        CSHA256 hasher;
        hasher.Write(preimage, 64);
        hasher.FinalizeNoPadding(digest_1);
    }

    {
        SHA256_CTX_mod state;
        sha256_init(&state);
        sha256_update(&state, preimage, 64);
        sha256_final_no_padding(&state, digest_2);
    }

    BOOST_CHECK(memcmp(digest_1, digest_2, 32) == 0);
}

BOOST_AUTO_TEST_SUITE_END()
