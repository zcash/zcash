/*
See the Zcash protocol specification for more information.
https://github.com/zcash/zips/blob/master/protocol/protocol.pdf

Zcash allows users to send and recieve value using notes. In order
to protect the privacy of the parties involved, this note is not
public, and for verification purposes, is cryptographically
commited. This commitment is public, and lives in the transaction.

However, recipients must be able to obtain values of the note,
including the value `v`, a secret `rho`, and the trapdoor `r`
otherwise they will be unable to spend it. In addition, we would
like to send a 64-byte memo to the recipient in the transaction.

In order to do this privately, we require the use of an IND-CCA2
secure encryption scheme. However, off-the-shelf schemes were
poorly specified or had inefficient encodings in all of the known,
credible implementations.

In this scheme, we use an ephemeral Curve25519 keypair to
construct a Diffie-Hellman secret with the recipient public key.
This secret, the public ephemeral key, the public key of the
recipient and a nonce (for indistinguishability of ciphertexts
using the same ephemeral key) are used for a KDF. The key is then
used to encrypt the message using AEAD_CHACHA20_POLY1305
authenticated, symmetric encryption.

The transaction contains the ciphertexts and the ephemeral key.

Recipients can recover the Diffie-Hellman secret using a scalar
multiplication and decrypt the plaintext.
*/

#ifndef ZC_NOTE_ENCRYPTION_H_
#define ZC_NOTE_ENCRYPTION_H_

#include <boost/array.hpp>
#include "uint256.h"

#define NOTEENCRYPTION_AUTH_BYTES 16

template<size_t MLEN>
class NoteEncryption {
private:
    enum { CLEN=MLEN+NOTEENCRYPTION_AUTH_BYTES };
    uint256 epk;
    uint256 esk;
    unsigned char nonce;
    uint256 seed;

public:
    typedef boost::array<unsigned char, CLEN> Ciphertext;
    typedef boost::array<unsigned char, MLEN> Plaintext;

    NoteEncryption(uint256 seed);
    ~NoteEncryption();

    // Gets the ephemeral public key
    uint256 get_epk() {
        return epk;
    }

    // Encrypts `message` with `pk_enc` and returns the ciphertext.
    // This can only be called twice for a given instantiation before
    // the nonce-space runs out.
    Ciphertext encrypt(const uint256 &pk_enc,
                       const Plaintext &message
                      );

    // Decrypts `ciphertext` with `sk_enc` and the ephemeral public key `epk`.
    static Plaintext decrypt(const uint256 &sk_enc,
                             const Ciphertext &ciphertext,
                             const uint256 &epk,
                             const uint256 &seed,
                             unsigned char nonce
                            );

    // Creates a NoteEncryption private key
    static uint256 generate_privkey(const uint256 &a_sk);

    // Creates a NoteEncryption public key from a private key
    static uint256 generate_pubkey(const uint256 &sk_enc);
};

typedef NoteEncryption<152> ZCNoteEncryption;

#endif /* ZC_NOTE_ENCRYPTION_H_ */