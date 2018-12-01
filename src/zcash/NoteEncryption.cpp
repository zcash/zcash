#include "NoteEncryption.hpp"
#include <stdexcept>
#include "sodium.h"
#include <boost/static_assert.hpp>
#include "prf.h"
#include "librustzcash.h"

#define NOTEENCRYPTION_CIPHER_KEYSIZE 32

void clamp_curve25519(unsigned char key[crypto_scalarmult_SCALARBYTES])
{
    key[0] &= 248;
    key[31] &= 127;
    key[31] |= 64;
}

void PRF_ock(
    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE],
    const uint256 &ovk,
    const uint256 &cv,
    const uint256 &cm,
    const uint256 &epk
)
{
    unsigned char block[128] = {};
    memcpy(block+0, ovk.begin(), 32);
    memcpy(block+32, cv.begin(), 32);
    memcpy(block+64, cm.begin(), 32);
    memcpy(block+96, epk.begin(), 32);

    unsigned char personalization[crypto_generichash_blake2b_PERSONALBYTES] = {};
    memcpy(personalization, "Zcash_Derive_ock", 16);

    if (crypto_generichash_blake2b_salt_personal(K, NOTEENCRYPTION_CIPHER_KEYSIZE,
                                                 block, 128,
                                                 NULL, 0, // No key.
                                                 NULL,    // No salt.
                                                 personalization
                                                ) != 0)
    {
        throw std::logic_error("hash function failure");
    }
}

void KDF_Sapling(
    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE],
    const uint256 &dhsecret,
    const uint256 &epk
)
{
    unsigned char block[64] = {};
    memcpy(block+0, dhsecret.begin(), 32);
    memcpy(block+32, epk.begin(), 32);

    unsigned char personalization[crypto_generichash_blake2b_PERSONALBYTES] = {};
    memcpy(personalization, "Zcash_SaplingKDF", 16);

    if (crypto_generichash_blake2b_salt_personal(K, NOTEENCRYPTION_CIPHER_KEYSIZE,
                                                 block, 64,
                                                 NULL, 0, // No key.
                                                 NULL,    // No salt.
                                                 personalization
                                                ) != 0)
    {
        throw std::logic_error("hash function failure");
    }
}

void KDF(unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE],
    const uint256 &dhsecret,
    const uint256 &epk,
    const uint256 &pk_enc,
    const uint256 &hSig,
    unsigned char nonce
   )
{
    if (nonce == 0xff) {
        throw std::logic_error("no additional nonce space for KDF");
    }

    unsigned char block[128] = {};
    memcpy(block+0, hSig.begin(), 32);
    memcpy(block+32, dhsecret.begin(), 32);
    memcpy(block+64, epk.begin(), 32);
    memcpy(block+96, pk_enc.begin(), 32);

    unsigned char personalization[crypto_generichash_blake2b_PERSONALBYTES] = {};
    memcpy(personalization, "ZcashKDF", 8);
    memcpy(personalization+8, &nonce, 1);

    if (crypto_generichash_blake2b_salt_personal(K, NOTEENCRYPTION_CIPHER_KEYSIZE,
                                                 block, 128,
                                                 NULL, 0, // No key.
                                                 NULL,    // No salt.
                                                 personalization
                                                ) != 0)
    {
        throw std::logic_error("hash function failure");
    }
}

namespace libzcash {

boost::optional<SaplingNoteEncryption> SaplingNoteEncryption::FromDiversifier(diversifier_t d) {
    uint256 epk;
    uint256 esk;

    // Pick random esk
    librustzcash_sapling_generate_r(esk.begin());

    // Compute epk given the diversifier
    if (!librustzcash_sapling_ka_derivepublic(d.begin(), esk.begin(), epk.begin())) {
        return boost::none;
    }

    return SaplingNoteEncryption(epk, esk);
}

boost::optional<SaplingEncCiphertext> SaplingNoteEncryption::encrypt_to_recipient(
    const uint256 &pk_d,
    const SaplingEncPlaintext &message
)
{
    if (already_encrypted_enc) {
        throw std::logic_error("already encrypted to the recipient using this key");
    }

    uint256 dhsecret;

    if (!librustzcash_sapling_ka_agree(pk_d.begin(), esk.begin(), dhsecret.begin())) {
        return boost::none;
    }

    // Construct the symmetric key
    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    KDF_Sapling(K, dhsecret, epk);

    // The nonce is zero because we never reuse keys
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES] = {};

    SaplingEncCiphertext ciphertext;

    crypto_aead_chacha20poly1305_ietf_encrypt(
        ciphertext.begin(), NULL,
        message.begin(), ZC_SAPLING_ENCPLAINTEXT_SIZE,
        NULL, 0, // no "additional data"
        NULL, cipher_nonce, K
    );

    already_encrypted_enc = true;

    return ciphertext;
}

boost::optional<SaplingEncPlaintext> AttemptSaplingEncDecryption(
    const SaplingEncCiphertext &ciphertext,
    const uint256 &ivk,
    const uint256 &epk
)
{
    uint256 dhsecret;

    if (!librustzcash_sapling_ka_agree(epk.begin(), ivk.begin(), dhsecret.begin())) {
        return boost::none;
    }

    // Construct the symmetric key
    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    KDF_Sapling(K, dhsecret, epk);

    // The nonce is zero because we never reuse keys
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES] = {};

    SaplingEncPlaintext plaintext;

    if (crypto_aead_chacha20poly1305_ietf_decrypt(
        plaintext.begin(), NULL,
        NULL,
        ciphertext.begin(), ZC_SAPLING_ENCCIPHERTEXT_SIZE,
        NULL,
        0,
        cipher_nonce, K) != 0)
    {
        return boost::none;
    }

    return plaintext;
}

boost::optional<SaplingEncPlaintext> AttemptSaplingEncDecryption (
    const SaplingEncCiphertext &ciphertext,
    const uint256 &epk,
    const uint256 &esk,
    const uint256 &pk_d
)
{
    uint256 dhsecret;

    if (!librustzcash_sapling_ka_agree(pk_d.begin(), esk.begin(), dhsecret.begin())) {
        return boost::none;
    }

    // Construct the symmetric key
    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    KDF_Sapling(K, dhsecret, epk);

    // The nonce is zero because we never reuse keys
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES] = {};

    SaplingEncPlaintext plaintext;

    if (crypto_aead_chacha20poly1305_ietf_decrypt(
        plaintext.begin(), NULL,
        NULL,
        ciphertext.begin(), ZC_SAPLING_ENCCIPHERTEXT_SIZE,
        NULL,
        0,
        cipher_nonce, K) != 0)
    {
        return boost::none;
    }

    return plaintext;
}


SaplingOutCiphertext SaplingNoteEncryption::encrypt_to_ourselves(
    const uint256 &ovk,
    const uint256 &cv,
    const uint256 &cm,
    const SaplingOutPlaintext &message
)
{
    if (already_encrypted_out) {
        throw std::logic_error("already encrypted to the recipient using this key");
    }

    // Construct the symmetric key
    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    PRF_ock(K, ovk, cv, cm, epk);

    // The nonce is zero because we never reuse keys
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES] = {};

    SaplingOutCiphertext ciphertext;

    crypto_aead_chacha20poly1305_ietf_encrypt(
        ciphertext.begin(), NULL,
        message.begin(), ZC_SAPLING_OUTPLAINTEXT_SIZE,
        NULL, 0, // no "additional data"
        NULL, cipher_nonce, K
    );

    already_encrypted_out = true;

    return ciphertext;
}

boost::optional<SaplingOutPlaintext> AttemptSaplingOutDecryption(
    const SaplingOutCiphertext &ciphertext,
    const uint256 &ovk,
    const uint256 &cv,
    const uint256 &cm,
    const uint256 &epk
)
{
    // Construct the symmetric key
    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    PRF_ock(K, ovk, cv, cm, epk);

    // The nonce is zero because we never reuse keys
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES] = {};

    SaplingOutPlaintext plaintext;

    if (crypto_aead_chacha20poly1305_ietf_decrypt(
        plaintext.begin(), NULL,
        NULL,
        ciphertext.begin(), ZC_SAPLING_OUTCIPHERTEXT_SIZE,
        NULL,
        0,
        cipher_nonce, K) != 0)
    {
        return boost::none;
    }

    return plaintext;
}

template<size_t MLEN>
NoteEncryption<MLEN>::NoteEncryption(uint256 hSig) : nonce(0), hSig(hSig) {
    // All of this code assumes crypto_scalarmult_BYTES is 32
    // There's no reason that will _ever_ change, but for
    // completeness purposes, let's check anyway.
    BOOST_STATIC_ASSERT(32 == crypto_scalarmult_BYTES);
    BOOST_STATIC_ASSERT(32 == crypto_scalarmult_SCALARBYTES);
    BOOST_STATIC_ASSERT(NOTEENCRYPTION_AUTH_BYTES == crypto_aead_chacha20poly1305_ABYTES);

    // Create the ephemeral keypair
    esk = random_uint256();
    epk = generate_pubkey(esk);
}

template<size_t MLEN>
NoteDecryption<MLEN>::NoteDecryption(uint256 sk_enc) : sk_enc(sk_enc) {
    this->pk_enc = NoteEncryption<MLEN>::generate_pubkey(sk_enc);
}

template<size_t MLEN>
typename NoteEncryption<MLEN>::Ciphertext NoteEncryption<MLEN>::encrypt
                                          (const uint256 &pk_enc,
                                           const NoteEncryption<MLEN>::Plaintext &message
                                          )
{
    uint256 dhsecret;

    if (crypto_scalarmult(dhsecret.begin(), esk.begin(), pk_enc.begin()) != 0) {
        throw std::logic_error("Could not create DH secret");
    }

    // Construct the symmetric key
    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    KDF(K, dhsecret, epk, pk_enc, hSig, nonce);

    // Increment the number of encryptions we've performed
    nonce++;

    // The nonce is zero because we never reuse keys
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES] = {};

    NoteEncryption<MLEN>::Ciphertext ciphertext;

    crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext.begin(), NULL,
                                         message.begin(), MLEN,
                                         NULL, 0, // no "additional data"
                                         NULL, cipher_nonce, K);

    return ciphertext;
}

template<size_t MLEN>
typename NoteDecryption<MLEN>::Plaintext NoteDecryption<MLEN>::decrypt
                                         (const NoteDecryption<MLEN>::Ciphertext &ciphertext,
                                          const uint256 &epk,
                                          const uint256 &hSig,
                                          unsigned char nonce
                                         ) const
{
    uint256 dhsecret;

    if (crypto_scalarmult(dhsecret.begin(), sk_enc.begin(), epk.begin()) != 0) {
        throw std::logic_error("Could not create DH secret");
    }

    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    KDF(K, dhsecret, epk, pk_enc, hSig, nonce);

    // The nonce is zero because we never reuse keys
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES] = {};

    NoteDecryption<MLEN>::Plaintext plaintext;

    // Message length is always NOTEENCRYPTION_AUTH_BYTES less than
    // the ciphertext length.
    if (crypto_aead_chacha20poly1305_ietf_decrypt(plaintext.begin(), NULL,
                                             NULL,
                                             ciphertext.begin(), NoteDecryption<MLEN>::CLEN,
                                             NULL,
                                             0,
                                             cipher_nonce, K) != 0) {
        throw note_decryption_failed();
    }

    return plaintext;
}

//
// Payment disclosure - decrypt with esk
//
template<size_t MLEN>
typename PaymentDisclosureNoteDecryption<MLEN>::Plaintext PaymentDisclosureNoteDecryption<MLEN>::decryptWithEsk
                                         (const PaymentDisclosureNoteDecryption<MLEN>::Ciphertext &ciphertext,
                                          const uint256 &pk_enc,
                                          const uint256 &esk,
                                          const uint256 &hSig,
                                          unsigned char nonce
                                         ) const
{
    uint256 dhsecret;

    if (crypto_scalarmult(dhsecret.begin(), esk.begin(), pk_enc.begin()) != 0) {
        throw std::logic_error("Could not create DH secret");
    }

    // Regenerate keypair
    uint256 epk = NoteEncryption<MLEN>::generate_pubkey(esk);

    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    KDF(K, dhsecret, epk, pk_enc, hSig, nonce);

    // The nonce is zero because we never reuse keys
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES] = {};

    PaymentDisclosureNoteDecryption<MLEN>::Plaintext plaintext;

    // Message length is always NOTEENCRYPTION_AUTH_BYTES less than
    // the ciphertext length.
    if (crypto_aead_chacha20poly1305_ietf_decrypt(plaintext.begin(), NULL,
                                             NULL,
                                             ciphertext.begin(), PaymentDisclosureNoteDecryption<MLEN>::CLEN,
                                             NULL,
                                             0,
                                             cipher_nonce, K) != 0) {
        throw note_decryption_failed();
    }

    return plaintext;
}




template<size_t MLEN>
uint256 NoteEncryption<MLEN>::generate_privkey(const uint252 &a_sk)
{
    uint256 sk = PRF_addr_sk_enc(a_sk);

    clamp_curve25519(sk.begin());

    return sk;
}

template<size_t MLEN>
uint256 NoteEncryption<MLEN>::generate_pubkey(const uint256 &sk_enc)
{
    uint256 pk;

    if (crypto_scalarmult_base(pk.begin(), sk_enc.begin()) != 0) {
        throw std::logic_error("Could not create public key");
    }

    return pk;
}

uint256 random_uint256()
{
    uint256 ret;
    randombytes_buf(ret.begin(), 32);

    return ret;
}

uint252 random_uint252()
{
    uint256 rand = random_uint256();
    (*rand.begin()) &= 0x0F;

    return uint252(rand);
}

template class NoteEncryption<ZC_NOTEPLAINTEXT_SIZE>;
template class NoteDecryption<ZC_NOTEPLAINTEXT_SIZE>;

template class PaymentDisclosureNoteDecryption<ZC_NOTEPLAINTEXT_SIZE>;

}
