#include "test/test_bitcoin.h"
#include "sodium.h"

#include <stdexcept>

#include "zcash/NoteEncryption.hpp"
#include "zcash/prf.h"

#include <boost/test/unit_test.hpp>

class TestNoteDecryption : public ZCNoteDecryption {
public:
    TestNoteDecryption(uint256 sk_enc) : ZCNoteDecryption(sk_enc) {}

    void change_pk_enc(uint256 to) {
        pk_enc = to;
    }
};

BOOST_FIXTURE_TEST_SUITE(noteencryption_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(noteencryption)
{
    uint256 sk_enc = ZCNoteEncryption::generate_privkey(uint256S("21035d60bc1983e37950ce4803418a8fb33ea68d5b937ca382ecbae7564d6a77"));
    uint256 pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);

    ZCNoteEncryption b = ZCNoteEncryption(uint256());

    boost::array<unsigned char, 216> message;
    for (unsigned char i = 0; i < 216; i++) {
        // Fill the message with dummy data
        message[i] = (unsigned char) i;
    }

    for (int i = 0; i < 255; i++) {
        auto ciphertext = b.encrypt(pk_enc, message);

        {
            ZCNoteDecryption decrypter(sk_enc);

            // Test decryption
            auto plaintext = decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i);
            BOOST_CHECK(plaintext == message);

            // Test wrong nonce
            BOOST_CHECK_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), (i == 0) ? 1 : (i - 1)), std::runtime_error);
        
            // Test wrong ephemeral key
            BOOST_CHECK_THROW(decrypter.decrypt(ciphertext, ZCNoteEncryption::generate_privkey(uint256()), uint256(), i), std::runtime_error);
        
            // Test wrong seed
            BOOST_CHECK_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256S("11035d60bc1983e37950ce4803418a8fb33ea68d5b937ca382ecbae7564d6a77"), i), std::runtime_error);
        
            // Test corrupted ciphertext
            ciphertext[10] ^= 0xff;
            BOOST_CHECK_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i), std::runtime_error);
            ciphertext[10] ^= 0xff;
        }

        {
            // Test wrong private key
            uint256 sk_enc_2 = ZCNoteEncryption::generate_privkey(uint256());
            ZCNoteDecryption decrypter(sk_enc_2);

            BOOST_CHECK_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i), std::runtime_error);
        }

        {
            TestNoteDecryption decrypter(sk_enc);

            // Test decryption
            auto plaintext = decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i);
            BOOST_CHECK(plaintext == message);

            // Test wrong public key (test of KDF)
            decrypter.change_pk_enc(uint256());
            BOOST_CHECK_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i), std::runtime_error);
        }
    }

    // Nonce space should run out here
    BOOST_CHECK_THROW(b.encrypt(pk_enc, message), std::logic_error);
}

uint256 test_prf(
    unsigned char distinguisher,
    uint256 x,
    uint256 y
) {
    *x.begin() &= 0x0f;
    *x.begin() |= distinguisher;
    CSHA256 hasher;
    hasher.Write(x.begin(), 32);
    hasher.Write(y.begin(), 32);

    uint256 ret;
    hasher.FinalizeNoPadding(ret.begin());
    return ret;
}

BOOST_AUTO_TEST_CASE(prf_addr)
{
    for (size_t i = 0; i < 100; i++) {
        uint256 a_sk = libzcash::random_uint256();
        uint256 rest;
        BOOST_CHECK_MESSAGE(
            test_prf(0xc0, a_sk, rest) == PRF_addr_a_pk(a_sk),
            a_sk.GetHex()
        );
    }

    for (size_t i = 0; i < 100; i++) {
        uint256 a_sk = libzcash::random_uint256();
        uint256 rest;
        *rest.begin() = 0x01;
        BOOST_CHECK_MESSAGE(
            test_prf(0xc0, a_sk, rest) == PRF_addr_sk_enc(a_sk),
            a_sk.GetHex()
        );
    }
}

BOOST_AUTO_TEST_CASE(prf_nf)
{
    for (size_t i = 0; i < 100; i++) {
        uint256 a_sk = libzcash::random_uint256();
        uint256 rho = libzcash::random_uint256();
        BOOST_CHECK_MESSAGE(
            test_prf(0xe0, a_sk, rho) == PRF_nf(a_sk, rho),
            a_sk.GetHex() + " and " + rho.GetHex()
        );
    }
}

BOOST_AUTO_TEST_CASE(prf_pk)
{
    for (size_t i = 0; i < 100; i++) {
        uint256 a_sk = libzcash::random_uint256();
        uint256 h_sig = libzcash::random_uint256();
        BOOST_CHECK_MESSAGE(
            test_prf(0x00, a_sk, h_sig) == PRF_pk(a_sk, 0, h_sig),
            a_sk.GetHex() + " and " + h_sig.GetHex()
        );
    }

    for (size_t i = 0; i < 100; i++) {
        uint256 a_sk = libzcash::random_uint256();
        uint256 h_sig = libzcash::random_uint256();
        BOOST_CHECK_MESSAGE(
            test_prf(0x40, a_sk, h_sig) == PRF_pk(a_sk, 1, h_sig),
            a_sk.GetHex() + " and " + h_sig.GetHex()
        );
    }

    uint256 dummy;
    BOOST_CHECK_THROW(PRF_pk(dummy, 2, dummy), std::domain_error);
}

BOOST_AUTO_TEST_CASE(prf_rho)
{
    for (size_t i = 0; i < 100; i++) {
        uint256 phi = libzcash::random_uint256();
        uint256 h_sig = libzcash::random_uint256();
        BOOST_CHECK_MESSAGE(
            test_prf(0x20, phi, h_sig) == PRF_rho(phi, 0, h_sig),
            phi.GetHex() + " and " + h_sig.GetHex()
        );
    }

    for (size_t i = 0; i < 100; i++) {
        uint256 phi = libzcash::random_uint256();
        uint256 h_sig = libzcash::random_uint256();
        BOOST_CHECK_MESSAGE(
            test_prf(0x60, phi, h_sig) == PRF_rho(phi, 1, h_sig),
            phi.GetHex() + " and " + h_sig.GetHex()
        );
    }

    uint256 dummy;
    BOOST_CHECK_THROW(PRF_rho(dummy, 2, dummy), std::domain_error);
}

BOOST_AUTO_TEST_SUITE_END()
