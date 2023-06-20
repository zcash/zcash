#include <gtest/gtest.h>
#include "sodium.h"

#include <array>
#include <optional>
#include <stdexcept>

#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"
#include "zcash/prf.h"
#include "zcash/Address.hpp"
#include "crypto/sha256.h"
#include "librustzcash.h"
#include "consensus/params.h"
#include "util/test.h"

class TestNoteDecryption : public ZCNoteDecryption {
public:
    TestNoteDecryption(uint256 sk_enc) : ZCNoteDecryption(sk_enc) {}

    void change_pk_enc(uint256 to) {
        pk_enc = to;
    }
};

TEST(NoteEncryption, NotePlaintext)
{
    SelectParams(CBaseChainParams::REGTEST);

    std::vector<libzcash::Zip212Enabled> zip_212_enabled = {libzcash::Zip212Enabled::BeforeZip212, libzcash::Zip212Enabled::AfterZip212};
    const Consensus::Params& (*activations [])() = {RegtestActivateSapling, RegtestActivateCanopy};
    void (*deactivations [])() = {RegtestDeactivateSapling, RegtestDeactivateCanopy};

    using namespace libzcash;
    auto xsk = SaplingSpendingKey(uint256()).expanded_spending_key();
    auto fvk = xsk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    SaplingPaymentAddress addr = *ivk.address({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

    Memo::Bytes memo;
    for (size_t i = 0; i < Memo::SIZE; i++) {
        // Fill the message with dummy data
        memo[i] = (unsigned char) i;
    }

    for (int ver = 0; ver < zip_212_enabled.size(); ver++){
        auto params = (*activations[ver])();

        auto ovk = random_uint256();
        auto value = 39393;

        auto builder = sapling::new_builder(*Params().RustNetwork(), 10);
        builder->add_recipient(
            ovk.GetRawBytes(),
            addr.GetRawBytes(),
            value,
            memo);
        auto bundle = sapling::apply_bundle_signatures(sapling::build_bundle(std::move(builder), 10), {});
        auto outputs = bundle->outputs();

        std::optional<SaplingOutgoingPlaintext> decrypted_out_ct;
        uint256 cmu;
        uint256 epk;
        libzcash::SaplingEncCiphertext ct;
        for (auto& output : outputs) {
            decrypted_out_ct = SaplingOutgoingPlaintext::decrypt(
                output.out_ciphertext(),
                ovk,
                uint256::FromRawBytes(output.cv()),
                uint256::FromRawBytes(output.cmu()),
                uint256::FromRawBytes(output.ephemeral_key()));
            if (decrypted_out_ct) {
                cmu = uint256::FromRawBytes(output.cmu());
                epk = uint256::FromRawBytes(output.ephemeral_key());
                ct = output.enc_ciphertext();
                break;
            }
        }

        ASSERT_TRUE(decrypted_out_ct.has_value());

        auto decrypted_out_ct_unwrapped = decrypted_out_ct.value();

        ASSERT_TRUE(decrypted_out_ct_unwrapped.pk_d == addr.pk_d);

        // Test sender won't accept invalid commitments
        ASSERT_FALSE(
            SaplingNotePlaintext::decrypt(
                params,
                1,
                ct,
                epk,
                decrypted_out_ct_unwrapped.esk,
                decrypted_out_ct_unwrapped.pk_d,
                uint256()
            )
        );

        // Test sender can decrypt the note ciphertext.
        auto foo = SaplingNotePlaintext::decrypt(
            params,
            1,
            ct,
            epk,
            decrypted_out_ct_unwrapped.esk,
            decrypted_out_ct_unwrapped.pk_d,
            cmu
        );

        if (!foo) {
            FAIL();
        }

        auto bar = foo.value();

        ASSERT_TRUE(bar.value() == value);
        ASSERT_TRUE(bar.memo() == Memo::FromBytes(memo));
        ASSERT_TRUE(bar.d == addr.d);

        (*deactivations[ver])();
    }
}

TEST(NoteEncryption, SaplingApi)
{
    using namespace libzcash;

    // Create recipient addresses
    auto sk = SaplingSpendingKey(uint256()).expanded_spending_key();
    auto vk = sk.full_viewing_key();
    auto ivk = vk.in_viewing_key();
    SaplingPaymentAddress pk_1 = *ivk.address({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    SaplingPaymentAddress pk_2 = *ivk.address({4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

    // Blob of stuff we're encrypting
    std::array<unsigned char, SAPLING_ENCPLAINTEXT_SIZE> message;
    for (size_t i = 0; i < SAPLING_ENCPLAINTEXT_SIZE; i++) {
        // Fill the message with dummy data
        message[i] = (unsigned char) i;
    }

    std::array<unsigned char, SAPLING_OUTPLAINTEXT_SIZE> small_message;
    for (size_t i = 0; i < SAPLING_OUTPLAINTEXT_SIZE; i++) {
        // Fill the message with dummy data
        small_message[i] = (unsigned char) i;
    }

    uint256 esk;
    librustzcash_sapling_generate_r(esk.begin());

    // Invalid diversifier
    ASSERT_EQ(std::nullopt, SaplingNoteEncryption::FromDiversifier({1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, esk));

    // Encrypt to pk_1
    auto enc = *SaplingNoteEncryption::FromDiversifier(pk_1.d, esk);
    auto ciphertext_1 = *enc.encrypt_to_recipient(
        pk_1.pk_d,
        message
    );
    auto epk_1 = enc.get_epk();
    {
        uint256 test_epk;
        uint256 test_esk = enc.get_esk();
        ASSERT_TRUE(librustzcash_sapling_ka_derivepublic(pk_1.d.begin(), test_esk.begin(), test_epk.begin()));
        ASSERT_TRUE(test_epk == epk_1);
    }
    auto cv_1 = random_uint256();
    auto cm_1 = random_uint256();
    auto out_ciphertext_1 = enc.encrypt_to_ourselves(
        sk.ovk,
        cv_1,
        cm_1,
        small_message
    );

    // Encrypt to pk_2
    enc = *SaplingNoteEncryption::FromDiversifier(pk_2.d, esk);
    auto ciphertext_2 = *enc.encrypt_to_recipient(
        pk_2.pk_d,
        message
    );
    auto epk_2 = enc.get_epk();

    auto cv_2 = random_uint256();
    auto cm_2 = random_uint256();
    auto out_ciphertext_2 = enc.encrypt_to_ourselves(
        sk.ovk,
        cv_2,
        cm_2,
        small_message
    );

    // Test nonce-reuse resistance of API
    {
        auto tmp_enc = *SaplingNoteEncryption::FromDiversifier(pk_1.d, esk);

        tmp_enc.encrypt_to_recipient(
            pk_1.pk_d,
            message
        );

        ASSERT_THROW(tmp_enc.encrypt_to_recipient(
            pk_1.pk_d,
            message
        ), std::logic_error);

        tmp_enc.encrypt_to_ourselves(
            sk.ovk,
            cv_2,
            cm_2,
            small_message
        );

        ASSERT_THROW(tmp_enc.encrypt_to_ourselves(
            sk.ovk,
            cv_2,
            cm_2,
            small_message
        ), std::logic_error);
    }

    // Try to decrypt
    auto plaintext_1 = *AttemptSaplingEncDecryption(
        ciphertext_1,
        ivk,
        epk_1
    );
    ASSERT_TRUE(message == plaintext_1);

    auto small_plaintext_1 = *AttemptSaplingOutDecryption(
        out_ciphertext_1,
        sk.ovk,
        cv_1,
        cm_1,
        epk_1
    );
    ASSERT_TRUE(small_message == small_plaintext_1);

    auto plaintext_2 = *AttemptSaplingEncDecryption(
        ciphertext_2,
        ivk,
        epk_2
    );
    ASSERT_TRUE(message == plaintext_2);

    auto small_plaintext_2 = *AttemptSaplingOutDecryption(
        out_ciphertext_2,
        sk.ovk,
        cv_2,
        cm_2,
        epk_2
    );
    ASSERT_TRUE(small_message == small_plaintext_2);

    // Try to decrypt out ciphertext with wrong key material
    ASSERT_FALSE(AttemptSaplingOutDecryption(
        out_ciphertext_1,
        random_uint256(),
        cv_1,
        cm_1,
        epk_1
    ));
    ASSERT_FALSE(AttemptSaplingOutDecryption(
        out_ciphertext_1,
        sk.ovk,
        random_uint256(),
        cm_1,
        epk_1
    ));
    ASSERT_FALSE(AttemptSaplingOutDecryption(
        out_ciphertext_1,
        sk.ovk,
        cv_1,
        random_uint256(),
        epk_1
    ));
    ASSERT_FALSE(AttemptSaplingOutDecryption(
        out_ciphertext_1,
        sk.ovk,
        cv_1,
        cm_1,
        random_uint256()
    ));

    // Try to decrypt with wrong ephemeral key
    ASSERT_FALSE(AttemptSaplingEncDecryption(
        ciphertext_1,
        ivk,
        epk_2
    ));
    ASSERT_FALSE(AttemptSaplingEncDecryption(
        ciphertext_2,
        ivk,
        epk_1
    ));

    // Try to decrypt with wrong ivk
    ASSERT_FALSE(AttemptSaplingEncDecryption(
        ciphertext_1,
        uint256(),
        epk_1
    ));
    ASSERT_FALSE(AttemptSaplingEncDecryption(
        ciphertext_2,
        uint256(),
        epk_2
    ));
}

TEST(NoteEncryption, api)
{
    uint256 sk_enc = ZCNoteEncryption::generate_privkey(uint252(uint256S("21035d60bc1983e37950ce4803418a8fb33ea68d5b937ca382ecbae7564d6a07")));
    uint256 pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);

    ZCNoteEncryption b = ZCNoteEncryption(uint256());
    for (size_t i = 0; i < 100; i++)
    {
        ZCNoteEncryption c = ZCNoteEncryption(uint256());

        ASSERT_TRUE(b.get_epk() != c.get_epk());
    }

    std::array<unsigned char, libzcash::NOTEPLAINTEXT_SIZE> message;
    for (size_t i = 0; i < libzcash::NOTEPLAINTEXT_SIZE; i++) {
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
            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), (i == 0) ? 1 : (i - 1)),
                         libzcash::note_decryption_failed);

            // Test wrong ephemeral key
            {
                ZCNoteEncryption c = ZCNoteEncryption(uint256());

                ASSERT_THROW(decrypter.decrypt(ciphertext, c.get_epk(), uint256(), i),
                             libzcash::note_decryption_failed);
            }

            // Test wrong seed
            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256S("11035d60bc1983e37950ce4803418a8fb33ea68d5b937ca382ecbae7564d6a77"), i),
                         libzcash::note_decryption_failed);

            // Test corrupted ciphertext
            ciphertext[10] ^= 0xff;
            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i),
                         libzcash::note_decryption_failed);
            ciphertext[10] ^= 0xff;
        }

        {
            // Test wrong private key
            uint256 sk_enc_2 = ZCNoteEncryption::generate_privkey(uint252());
            ZCNoteDecryption decrypter(sk_enc_2);

            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i),
                         libzcash::note_decryption_failed);
        }

        {
            TestNoteDecryption decrypter(sk_enc);

            // Test decryption
            auto plaintext = decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i);
            ASSERT_TRUE(plaintext == message);

            // Test wrong public key (test of KDF)
            decrypter.change_pk_enc(uint256());
            ASSERT_THROW(decrypter.decrypt(ciphertext, b.get_epk(), uint256(), i),
                         libzcash::note_decryption_failed);
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

TEST(NoteEncryption, PrfAddr)
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

TEST(NoteEncryption, PrfNf)
{
    for (size_t i = 0; i < 100; i++) {
        uint252 a_sk = libzcash::random_uint252();
        uint256 rho = libzcash::random_uint256();
        ASSERT_TRUE(
            test_prf(0xe0, a_sk, rho) == PRF_nf(a_sk, rho)
        );
    }
}

TEST(NoteEncryption, PrfPk)
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

TEST(NoteEncryption, PrfRho)
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

TEST(NoteEncryption, uint252)
{
    ASSERT_THROW(uint252(uint256S("f6da8716682d600f74fc16bd0187faad6a26b4aa4c24d5c055b216d94516847e")), std::domain_error);
}
