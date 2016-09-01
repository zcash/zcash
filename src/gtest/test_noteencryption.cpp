#include <gtest/gtest.h>
#include "sodium.h"

#include <stdexcept>

#include "zcash/NoteEncryption.hpp"
#include "zcash/prf.h"
#include "crypto/sha256.h"

class TestNoteDecryption : public ZCNoteDecryption {
public:
    TestNoteDecryption(uint256 sk_enc) : ZCNoteDecryption(sk_enc) {}

    void change_pk_enc(uint256 to) {
        pk_enc = to;
    }
};

TEST(noteencryption, api)
{
    uint256 sk_enc = ZCNoteEncryption::generate_privkey(uint252(uint256S("21035d60bc1983e37950ce4803418a8fb33ea68d5b937ca382ecbae7564d6a07")));
    uint256 pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);

    ZCNoteEncryption b = ZCNoteEncryption(uint256());
    for (size_t i = 0; i < 100; i++)
    {
        ZCNoteEncryption c = ZCNoteEncryption(uint256());

        ASSERT_TRUE(b.get_epk() != c.get_epk());
    }

    boost::array<unsigned char, ZC_NOTEPLAINTEXT_SIZE> message;
    for (size_t i = 0; i < ZC_NOTEPLAINTEXT_SIZE; i++) {
        // Fill the message with dummy data
        message[i] = (unsigned char) i;
    }

    for (int i = 0; i < 255; i++) {
        auto ciphertext = b.encrypt(pk_enc, message);

        {
            ZCNoteDecryption decrypter(sk_enc);

            // Test decryption
            auto plaintext = decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i);
            ASSERT_TRUE(plaintext == message);

            // Test wrong nonce
            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), (i == 0) ? 1 : (i - 1)), std::runtime_error);
        
            // Test wrong ephemeral key
            {
                ZCNoteEncryption c = ZCNoteEncryption(uint256());

                ASSERT_THROW(decrypter.decrypt(ciphertext, c.get_epk(), uint256(), i), std::runtime_error);
            }
        
            // Test wrong seed
            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256S("11035d60bc1983e37950ce4803418a8fb33ea68d5b937ca382ecbae7564d6a77"), i), std::runtime_error);
        
            // Test corrupted ciphertext
            ciphertext[10] ^= 0xff;
            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i), std::runtime_error);
            ciphertext[10] ^= 0xff;
        }

        {
            // Test wrong private key
            uint256 sk_enc_2 = ZCNoteEncryption::generate_privkey(uint252());
            ZCNoteDecryption decrypter(sk_enc_2);

            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i), std::runtime_error);
        }

        {
            TestNoteDecryption decrypter(sk_enc);

            // Test decryption
            auto plaintext = decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i);
            ASSERT_TRUE(plaintext == message);

            // Test wrong public key (test of KDF)
            decrypter.change_pk_enc(uint256());
            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i), std::runtime_error);
        }
    }

    // Nonce space should run out here
    try {
        b.encrypt(pk_enc, message);
        FAIL() << "Expected std::logic_error";
    }
    catch(std::logic_error const & err) {
        EXPECT_EQ(err.what(), std::string("no additional nonce space for KDF"));
    }
    catch(...) {
        FAIL() << "Expected std::logic_error";
    }
}

uint256 test_prf(
    unsigned char distinguisher,
    uint252 seed_x,
    uint256 y
) {
    uint256 x = seed_x.inner();
    *x.begin() &= 0x0f;
    *x.begin() |= distinguisher;
    CSHA256 hasher;
    hasher.Write(x.begin(), 32);
    hasher.Write(y.begin(), 32);

    uint256 ret;
    hasher.FinalizeNoPadding(ret.begin());
    return ret;
}

TEST(noteencryption, prf_addr)
{
    for (size_t i = 0; i < 100; i++) {
        uint252 a_sk = libzcash::random_uint252();
        uint256 rest;
        ASSERT_TRUE(
            test_prf(0xc0, a_sk, rest) == PRF_addr_a_pk(a_sk)
        );
    }

    for (size_t i = 0; i < 100; i++) {
        uint252 a_sk = libzcash::random_uint252();
        uint256 rest;
        *rest.begin() = 0x01;
        ASSERT_TRUE(
            test_prf(0xc0, a_sk, rest) == PRF_addr_sk_enc(a_sk)
        );
    }
}

TEST(noteencryption, prf_nf)
{
    for (size_t i = 0; i < 100; i++) {
        uint252 a_sk = libzcash::random_uint252();
        uint256 rho = libzcash::random_uint256();
        ASSERT_TRUE(
            test_prf(0xe0, a_sk, rho) == PRF_nf(a_sk, rho)
        );
    }
}

TEST(noteencryption, prf_pk)
{
    for (size_t i = 0; i < 100; i++) {
        uint252 a_sk = libzcash::random_uint252();
        uint256 h_sig = libzcash::random_uint256();
        ASSERT_TRUE(
            test_prf(0x00, a_sk, h_sig) == PRF_pk(a_sk, 0, h_sig)
        );
    }

    for (size_t i = 0; i < 100; i++) {
        uint252 a_sk = libzcash::random_uint252();
        uint256 h_sig = libzcash::random_uint256();
        ASSERT_TRUE(
            test_prf(0x40, a_sk, h_sig) == PRF_pk(a_sk, 1, h_sig)
        );
    }

    uint252 dummy_a;
    uint256 dummy_b;
    ASSERT_THROW(PRF_pk(dummy_a, 2, dummy_b), std::domain_error);
}

TEST(noteencryption, prf_rho)
{
    for (size_t i = 0; i < 100; i++) {
        uint252 phi = libzcash::random_uint252();
        uint256 h_sig = libzcash::random_uint256();
        ASSERT_TRUE(
            test_prf(0x20, phi, h_sig) == PRF_rho(phi, 0, h_sig)
        );
    }

    for (size_t i = 0; i < 100; i++) {
        uint252 phi = libzcash::random_uint252();
        uint256 h_sig = libzcash::random_uint256();
        ASSERT_TRUE(
            test_prf(0x60, phi, h_sig) == PRF_rho(phi, 1, h_sig)
        );
    }

    uint252 dummy_a;
    uint256 dummy_b;
    ASSERT_THROW(PRF_rho(dummy_a, 2, dummy_b), std::domain_error);
}

TEST(noteencryption, uint252)
{
    ASSERT_THROW(uint252(uint256S("f6da8716682d600f74fc16bd0187faad6a26b4aa4c24d5c055b216d94516847e")), std::domain_error);
}
