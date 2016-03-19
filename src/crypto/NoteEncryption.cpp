#include "NoteEncryption.hpp"
#include "crypto/sha256.h"
#include <stdexcept>
#include "sodium.h"
#include <boost/static_assert.hpp>
#include "prf.h"

void clamp_curve25519(unsigned char *key)
{
    key[0] &= 248;
    key[31] &= 127;
    key[31] |= 64;
}

void KDF(unsigned char *K,
    const uint256 &dhsecret,
    const uint256 &in_epk,
    const uint256 &pk_enc,
    unsigned char in_nonce
   )
{
    unsigned char distinguisher = 0xC2;

    CSHA256 hasher;
    hasher.Write(&distinguisher, 1);
    hasher.Write(dhsecret.begin(), crypto_scalarmult_BYTES);
    hasher.Write(in_epk.begin(), crypto_scalarmult_BYTES);
    hasher.Write(pk_enc.begin(), crypto_scalarmult_BYTES);
    hasher.Write(&in_nonce, 1);
    hasher.Finalize(K);
}

template<size_t MLEN>
NoteEncryption<MLEN>::NoteEncryption() : ciphertext_nonce(0) {
    // All of this code assumes crypto_scalarmult_BYTES is 32
    // There's no reason that will _ever_ change, but for
    // completeness purposes, let's check anyway.
    BOOST_STATIC_ASSERT(32 == crypto_scalarmult_BYTES);
    BOOST_STATIC_ASSERT(NOTEENCRYPTION_AUTH_BYTES == crypto_aead_chacha20poly1305_ABYTES);

    // Create the ephemeral keypair
    randombytes_buf(esk.begin(), crypto_scalarmult_BYTES);
    epk = generate_pubkey(esk);
}

template<size_t MLEN>
NoteEncryption<MLEN>::~NoteEncryption() {
    sodium_memzero(epk.begin(), crypto_scalarmult_BYTES);
    sodium_memzero(esk.begin(), crypto_scalarmult_BYTES);
}

template<size_t MLEN>
typename NoteEncryption<MLEN>::Ciphertext NoteEncryption<MLEN>::encrypt(const uint256 &pk_enc,
                                                    const NoteEncryption<MLEN>::Plaintext &message
                                                   )
{
    uint256 dhsecret;

    if (crypto_scalarmult(dhsecret.begin(), esk.begin(), pk_enc.begin()) != 0) {
        throw std::logic_error("Could not create DH secret");
    }

    // Nonce space runs out after this, let's just stop now.
    if (ciphertext_nonce == 0xff) {
        throw std::runtime_error("NoteEncryption encryption called more times than supported");
    }

    // Construct the symmetric key
    unsigned char K[32];
    KDF(K, dhsecret, epk, pk_enc, ciphertext_nonce);

    // Increment nonce
    ciphertext_nonce++;

    // The nonce is null for our purposes
    unsigned char nonce[crypto_aead_chacha20poly1305_NPUBBYTES];
    sodium_memzero(nonce, sizeof nonce);

    NoteEncryption<MLEN>::Ciphertext ciphertext;

    crypto_aead_chacha20poly1305_encrypt(ciphertext.begin(), NULL,
                                         message.begin(), MLEN,
                                         NULL, 0, // no "additional data"
                                         NULL, nonce, K);

    return ciphertext;
}

template<size_t MLEN>
typename NoteEncryption<MLEN>::Plaintext NoteEncryption<MLEN>::decrypt(const uint256 &sk_enc,
                                                   const NoteEncryption<MLEN>::Ciphertext &ciphertext,
                                                   const uint256 &epk,
                                                   unsigned char in_nonce
                                                  )
{
    uint256 pk_enc = generate_pubkey(sk_enc);

    uint256 dhsecret;

    if (crypto_scalarmult(dhsecret.begin(), sk_enc.begin(), epk.begin()) != 0) {
        throw std::runtime_error("Could not create DH secret");
    }

    unsigned char K[32];
    KDF(K, dhsecret, epk, pk_enc, in_nonce);

    // The nonce is null for our purposes
    unsigned char nonce[crypto_aead_chacha20poly1305_NPUBBYTES];
    sodium_memzero(nonce, sizeof nonce);

    NoteEncryption<MLEN>::Plaintext plaintext;

    if (crypto_aead_chacha20poly1305_decrypt(plaintext.begin(), NULL,
                                     NULL,
                                     ciphertext.begin(), NoteEncryption<MLEN>::CLEN,
                                     NULL,
                                     0,
                                     nonce, K) != 0) {
        throw std::runtime_error("Could not decrypt message");
    }

    return plaintext;
}

template<size_t MLEN>
uint256 NoteEncryption<MLEN>::generate_privkey(const uint256 &a_sk)
{
    uint256 sk = PRF_addr<0x01>(a_sk);

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

// 128-byte message for Zcash
// 8-byte value
// 32-byte rho
// 24-byte r
// 64-byte memo
template class NoteEncryption<128>;
