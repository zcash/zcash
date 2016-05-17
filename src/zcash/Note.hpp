#ifndef _ZCNOTE_H_
#define _ZCNOTE_H_

#include "uint256.h"
#include "Zcash.h"
#include "Address.hpp"
#include "NoteEncryption.hpp"

namespace libzcash {

class Note {
public:
    uint256 a_pk;
    uint64_t value;
    uint256 rho;
    uint256 r;

    Note(uint256 a_pk, uint64_t value, uint256 rho, uint256 r)
        : a_pk(a_pk), value(value), rho(rho), r(r) {}

    Note();

    uint256 cm() const;
    uint256 nullifier(const SpendingKey& a_sk) const;
};

class NotePlaintext {
public:
    uint64_t value;
    uint256 rho;
    uint256 r;
    boost::array<unsigned char, ZC_MEMO_SIZE> memo;

    NotePlaintext() {}

    NotePlaintext(const Note& note, boost::array<unsigned char, ZC_MEMO_SIZE> memo);

    Note note(const PaymentAddress& addr) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        unsigned char leadingByte = 0x00;
        READWRITE(leadingByte);

        if (leadingByte != 0x00) {
            throw std::ios_base::failure("lead byte of NotePlaintext is not recognized");
        }

        READWRITE(value);
        READWRITE(rho);
        READWRITE(r);
        READWRITE(memo);
    }

    static NotePlaintext decrypt(const ZCNoteDecryption& decryptor,
                                 const ZCNoteDecryption::Ciphertext& ciphertext,
                                 const uint256& ephemeralKey,
                                 const uint256& h_sig,
                                 unsigned char nonce
                                );

    ZCNoteEncryption::Ciphertext encrypt(ZCNoteEncryption& encryptor,
                                         const uint256& pk_enc
                                        ) const;
};

}

#endif // _ZCNOTE_H_