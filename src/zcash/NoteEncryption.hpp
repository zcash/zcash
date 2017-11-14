/*
See the Zcash protocol specification for more information.
https://github.com/zcash/zips/blob/master/protocol/protocol.pdf
*/

#ifndef ZC_NOTE_ENCRYPTION_H_
#define ZC_NOTE_ENCRYPTION_H_

#include <boost/array.hpp>
#include "uint256.h"
#include "uint252.h"

#include "zcash/Zcash.h"

namespace libzcash {

#define NOTEENCRYPTION_AUTH_BYTES 16

template<size_t MLEN>
class NoteEncryption {
protected:
    enum { CLEN=MLEN+NOTEENCRYPTION_AUTH_BYTES };
    uint256 epk;
    uint256 esk;
    unsigned char nonce;
    uint256 hSig;

public:
    typedef boost::array<unsigned char, CLEN> Ciphertext;
    typedef boost::array<unsigned char, MLEN> Plaintext;

    NoteEncryption(uint256 hSig);

    // Gets the ephemeral secret key
    uint256 get_esk() {
        return esk;
    }

    // Gets the ephemeral public key
    uint256 get_epk() {
        return epk;
    }

    // Encrypts `message` with `pk_enc` and returns the ciphertext.
    // This is only called ZC_NUM_JS_OUTPUTS times for a given instantiation; 
    // but can be called 255 times before the nonce-space runs out.
    Ciphertext encrypt(const uint256 &pk_enc,
                       const Plaintext &message
                      );

    // Creates a NoteEncryption private key
    static uint256 generate_privkey(const uint252 &a_sk);

    // Creates a NoteEncryption public key from a private key
    static uint256 generate_pubkey(const uint256 &sk_enc);
};

template<size_t MLEN>
class NoteDecryption {
protected:
    enum { CLEN=MLEN+NOTEENCRYPTION_AUTH_BYTES };
    uint256 sk_enc;
    uint256 pk_enc;

public:
    typedef boost::array<unsigned char, CLEN> Ciphertext;
    typedef boost::array<unsigned char, MLEN> Plaintext;

    NoteDecryption() { }
    NoteDecryption(uint256 sk_enc);

    Plaintext decrypt(const Ciphertext &ciphertext,
                      const uint256 &epk,
                      const uint256 &hSig,
                      unsigned char nonce
                     ) const;

    friend inline bool operator==(const NoteDecryption& a, const NoteDecryption& b) {
        return a.sk_enc == b.sk_enc && a.pk_enc == b.pk_enc;
    }
    friend inline bool operator<(const NoteDecryption& a, const NoteDecryption& b) {
        return (a.sk_enc < b.sk_enc ||
                (a.sk_enc == b.sk_enc && a.pk_enc < b.pk_enc));
    }
};

uint256 random_uint256();
uint252 random_uint252();

class note_decryption_failed : public std::runtime_error {
public:
    note_decryption_failed() : std::runtime_error("Could not decrypt message") { }
};



// Subclass PaymentDisclosureNoteDecryption provides a method to decrypt a note with esk.
template<size_t MLEN>
class PaymentDisclosureNoteDecryption : public NoteDecryption<MLEN> {
protected:
public:
    enum { CLEN=MLEN+NOTEENCRYPTION_AUTH_BYTES };
    typedef boost::array<unsigned char, CLEN> Ciphertext;
    typedef boost::array<unsigned char, MLEN> Plaintext;

    PaymentDisclosureNoteDecryption() : NoteDecryption<MLEN>() {}
    PaymentDisclosureNoteDecryption(uint256 sk_enc) : NoteDecryption<MLEN>(sk_enc) {}

    Plaintext decryptWithEsk(
        const Ciphertext &ciphertext,
        const uint256 &pk_enc,
        const uint256 &esk,
        const uint256 &hSig,
        unsigned char nonce
        ) const;
};

}

typedef libzcash::NoteEncryption<ZC_NOTEPLAINTEXT_SIZE> ZCNoteEncryption;
typedef libzcash::NoteDecryption<ZC_NOTEPLAINTEXT_SIZE> ZCNoteDecryption;

typedef libzcash::PaymentDisclosureNoteDecryption<ZC_NOTEPLAINTEXT_SIZE> ZCPaymentDisclosureNoteDecryption;

#endif /* ZC_NOTE_ENCRYPTION_H_ */
