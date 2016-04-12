#include "test/test_bitcoin.h"
#include "sodium.h"

#include "zcash/NoteEncryption.hpp"

#include <boost/test/unit_test.hpp>

class TestNoteDecryption : public ZCNoteDecryption {
public:
    TestNoteDecryption(uint256 sk_enc) : ZCNoteDecryption(sk_enc) {}

    void change_pk_enc(uint256 to) {
        pk_enc = to;
    }
};

BOOST_FIXTURE_TEST_SUITE(keyprivate_tests, BasicTestingSetup)

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

BOOST_AUTO_TEST_SUITE_END()
