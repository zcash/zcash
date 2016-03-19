/*
See the Zcash protocol specification for more information.
https://github.com/zcash/zips/blob/master/protocol/protocol.pdf

Zcash allows users to send and recieve value using notes. In order
to protect the privacy of the parties involved, this note is not
public, and for verification purposes, is cryptographically
commited. This commitment is public, and lives in the transaction.

However, recipients must be able to obtain values of the note,
including the value `v`, the trapdoor `r`, and a secret `rho`,
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
used to encrypt the message using AEAD_CHACHA20_POLY1309
authenticated, symmetric encryption.

The transaction contains the ciphertexts and the ephemeral key.

Recipients can recover the Diffie-Hellman secret using a scalar
multiplication and decrypt the plaintext.
*/

#include <boost/array.hpp>
#include "uint256.h"

#define BORT_AUTH_BYTES 16

template<size_t MLEN>
class Bort {
private:
    enum { CLEN=MLEN+BORT_AUTH_BYTES };
    uint256 epk;
    uint256 esk;

    typedef boost::array<unsigned char, CLEN> Ciphertext;
    typedef boost::array<unsigned char, MLEN> Plaintext;

public:
    Bort();
    ~Bort();

    // Gets the ephemeral public key
    uint256 get_epk() {
        return epk;
    }

    // Encrypts `message` with `pk_enc` and returns the ciphertext.
    // Provide a nonce if you encrypt multiple messages.
    Ciphertext encrypt(const uint256 &pk_enc,
                       const Plaintext &message,
                       unsigned char in_nonce
                      );

    // Decrypts `ciphertext` with `sk_enc` and the ephemeral public key `epk`.
    static Plaintext decrypt(const uint256 &sk_enc,
                             const Ciphertext &ciphertext,
                             const uint256 &epk,
                             unsigned char in_nonce
                            );

    // Creates a bort private key
    static uint256 generate_privkey(const uint248 &a_sk);

    // Creates a bort public key from a private key
    static uint256 generate_pubkey(const uint256 &sk_enc);
};