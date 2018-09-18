#ifndef ZC_NOTE_H_
#define ZC_NOTE_H_

#include "uint256.h"
#include "Zcash.h"
#include "Address.hpp"
#include "NoteEncryption.hpp"

#include <array>
#include <boost/optional.hpp>

namespace libzcash {

class BaseNote {
protected:
    uint64_t value_ = 0;
public:
    BaseNote() {}
    BaseNote(uint64_t value) : value_(value) {};
    virtual ~BaseNote() {};

    inline uint64_t value() const { return value_; };
};

class SproutNote : public BaseNote {
public:
    uint256 a_pk;
    uint256 rho;
    uint256 r;

    SproutNote(uint256 a_pk, uint64_t value, uint256 rho, uint256 r)
        : BaseNote(value), a_pk(a_pk), rho(rho), r(r) {}

    SproutNote();

    virtual ~SproutNote() {};

    uint256 cm() const;

    uint256 nullifier(const SproutSpendingKey& a_sk) const;
};


class SaplingNote : public BaseNote {
public:
    diversifier_t d;
    uint256 pk_d;
    uint256 r;

    SaplingNote(diversifier_t d, uint256 pk_d, uint64_t value, uint256 r)
            : BaseNote(value), d(d), pk_d(pk_d), r(r) {}

    SaplingNote() {};

    SaplingNote(const SaplingPaymentAddress &address, uint64_t value);

    virtual ~SaplingNote() {};

    boost::optional<uint256> cm() const;
    boost::optional<uint256> nullifier(const SaplingFullViewingKey &vk, const uint64_t position) const;
};

class BaseNotePlaintext {
protected:
    uint64_t value_ = 0;
    std::array<unsigned char, ZC_MEMO_SIZE> memo_;
public:
    BaseNotePlaintext() {}
    BaseNotePlaintext(const BaseNote& note, std::array<unsigned char, ZC_MEMO_SIZE> memo)
        : value_(note.value()), memo_(memo) {}
    virtual ~BaseNotePlaintext() {}

    inline uint64_t value() const { return value_; }
    inline const std::array<unsigned char, ZC_MEMO_SIZE> & memo() const { return memo_; }
};

class SproutNotePlaintext : public BaseNotePlaintext {
public:
    uint256 rho;
    uint256 r;

    SproutNotePlaintext() {}

    SproutNotePlaintext(const SproutNote& note, std::array<unsigned char, ZC_MEMO_SIZE> memo);

    SproutNote note(const SproutPaymentAddress& addr) const;

    virtual ~SproutNotePlaintext() {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        unsigned char leadingByte = 0x00;
        READWRITE(leadingByte);

        if (leadingByte != 0x00) {
            throw std::ios_base::failure("lead byte of SproutNotePlaintext is not recognized");
        }

        READWRITE(value_);
        READWRITE(rho);
        READWRITE(r);
        READWRITE(memo_);
    }

    static SproutNotePlaintext decrypt(const ZCNoteDecryption& decryptor,
                                 const ZCNoteDecryption::Ciphertext& ciphertext,
                                 const uint256& ephemeralKey,
                                 const uint256& h_sig,
                                 unsigned char nonce
                                );

    ZCNoteEncryption::Ciphertext encrypt(ZCNoteEncryption& encryptor,
                                         const uint256& pk_enc
                                        ) const;
};

typedef std::pair<SaplingEncCiphertext, SaplingNoteEncryption> SaplingNotePlaintextEncryptionResult;

class SaplingNotePlaintext : public BaseNotePlaintext {
public:
    diversifier_t d;
    uint256 rcm;

    SaplingNotePlaintext() {}

    SaplingNotePlaintext(const SaplingNote& note, std::array<unsigned char, ZC_MEMO_SIZE> memo);

    static boost::optional<SaplingNotePlaintext> decrypt(
        const SaplingEncCiphertext &ciphertext,
        const uint256 &ivk,
        const uint256 &epk,
        const uint256 &cmu
    );

    static boost::optional<SaplingNotePlaintext> decrypt(
        const SaplingEncCiphertext &ciphertext,
        const uint256 &epk,
        const uint256 &esk,
        const uint256 &pk_d,
        const uint256 &cmu
    );

    boost::optional<SaplingNote> note(const SaplingIncomingViewingKey& ivk) const;

    virtual ~SaplingNotePlaintext() {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        unsigned char leadingByte = 0x01;
        READWRITE(leadingByte);

        if (leadingByte != 0x01) {
            throw std::ios_base::failure("lead byte of SaplingNotePlaintext is not recognized");
        }

        READWRITE(d);           // 11 bytes
        READWRITE(value_);      // 8 bytes
        READWRITE(rcm);         // 32 bytes
        READWRITE(memo_);       // 512 bytes
    }

    boost::optional<SaplingNotePlaintextEncryptionResult> encrypt(const uint256& pk_d) const;
};

class SaplingOutgoingPlaintext
{
public:
    uint256 pk_d;
    uint256 esk;

    SaplingOutgoingPlaintext() {};

    SaplingOutgoingPlaintext(uint256 pk_d, uint256 esk) : pk_d(pk_d), esk(esk) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(pk_d);        // 8 bytes
        READWRITE(esk);         // 8 bytes
    }

    static boost::optional<SaplingOutgoingPlaintext> decrypt(
        const SaplingOutCiphertext &ciphertext,
        const uint256& ovk,
        const uint256& cv,
        const uint256& cm,
        const uint256& epk
    );

    SaplingOutCiphertext encrypt(
        const uint256& ovk,
        const uint256& cv,
        const uint256& cm,
        SaplingNoteEncryption& enc
    ) const;
};


}

#endif // ZC_NOTE_H_
