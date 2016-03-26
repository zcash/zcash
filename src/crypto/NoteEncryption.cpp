#include "NoteEncryption.hpp"
#include "crypto/sha512.h"
#include <stdexcept>
#include "sodium.h"
#include <boost/static_assert.hpp>
#include "prf.h"

#define NOTEENCRYPTION_CIPHER_KEYSIZE 32

void clamp_curve25519(unsigned char *key)
{
    key[0] &= 248;
    key[31] &= 127;
    key[31] |= 64;
}

void KDF(unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE],
    const uint256 &dhsecret,
    const uint256 &epk,
    const uint256 &pk_enc,
    const uint256 &seed,
    unsigned char nonce
   )
{
    unsigned char K_tmp[64];
    CSHA512 hasher;

    // Trim the first byte from seed
    std::vector<unsigned char> first_248(seed.begin() + 1, seed.end());

    if (nonce >= 2) {
        throw std::runtime_error("KDF nonce space exceeded");
    }

    // First bit is the 'nonce' bit or `i`
    // Second bit is zero
    first_248[0] &= 0xFC;
    first_248[0] |= nonce;

    hasher.Write(&first_248[0], 31);
    hasher.Write(dhsecret.begin(), crypto_scalarmult_BYTES);
    hasher.Write(epk.begin(), crypto_scalarmult_BYTES);
    hasher.Write(pk_enc.begin(), crypto_scalarmult_BYTES);
    hasher.Finalize(K_tmp);

    memcpy(K, K_tmp + 32, 32);
}

template<size_t MLEN>
NoteEncryption<MLEN>::NoteEncryption(uint256 seed) : nonce(0), seed(seed) {
    // All of this code assumes crypto_scalarmult_BYTES is 32
    // There's no reason that will _ever_ change, but for
    // completeness purposes, let's check anyway.
    BOOST_STATIC_ASSERT(32 == crypto_scalarmult_BYTES);
    BOOST_STATIC_ASSERT(32 == crypto_scalarmult_SCALARBYTES);
    BOOST_STATIC_ASSERT(NOTEENCRYPTION_AUTH_BYTES == crypto_aead_chacha20poly1305_ABYTES);

    // Create the ephemeral keypair
    randombytes_buf(esk.begin(), crypto_scalarmult_SCALARBYTES);
    epk = generate_pubkey(esk);
}

template<size_t MLEN>
NoteEncryption<MLEN>::~NoteEncryption() {
    sodium_memzero(epk.begin(), crypto_scalarmult_BYTES);
    sodium_memzero(esk.begin(), crypto_scalarmult_SCALARBYTES);
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
    KDF(K, dhsecret, epk, pk_enc, seed, nonce);

    // Increment the number of encryptions we've performed
    nonce++;

    // The nonce is zero for our purposes
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_NPUBBYTES] = {};

    NoteEncryption<MLEN>::Ciphertext ciphertext;

    crypto_aead_chacha20poly1305_encrypt(ciphertext.begin(), NULL,
                                         message.begin(), MLEN,
                                         NULL, 0, // no "additional data"
                                         NULL, cipher_nonce, K);

    return ciphertext;
}

template<size_t MLEN>
typename NoteEncryption<MLEN>::Plaintext NoteEncryption<MLEN>::decrypt
                                         (const uint256 &sk_enc,
                                          const NoteEncryption<MLEN>::Ciphertext &ciphertext,
                                          const uint256 &epk,
                                          const uint256 &seed,
                                          unsigned char nonce
                                         )
{
    uint256 pk_enc = generate_pubkey(sk_enc);

    uint256 dhsecret;

    if (crypto_scalarmult(dhsecret.begin(), sk_enc.begin(), epk.begin()) != 0) {
        throw std::runtime_error("Could not create DH secret");
    }

    unsigned char K[NOTEENCRYPTION_CIPHER_KEYSIZE];
    KDF(K, dhsecret, epk, pk_enc, seed, nonce);

    // The nonce is zero for our purposes
    unsigned char cipher_nonce[crypto_aead_chacha20poly1305_NPUBBYTES] = {};

    NoteEncryption<MLEN>::Plaintext plaintext;

    if (crypto_aead_chacha20poly1305_decrypt(plaintext.begin(), NULL,
                                             NULL,
                                             ciphertext.begin(), NoteEncryption<MLEN>::CLEN,
                                             NULL,
                                             0,
                                             cipher_nonce, K) != 0) {
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

// 152-byte message for Zcash
// 8-byte value
// 32-byte rho
// 48-byte r
// 64-byte memo
template class NoteEncryption<152>;
