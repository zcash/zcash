#ifndef ZC_NOTE_H_
#define ZC_NOTE_H_

#include "uint256.h"
#include "Zcash.h"
#include "Address.hpp"
#include "NoteEncryption.hpp"
#include "consensus/params.h"
#include "consensus/consensus.h"
#include "zcash/memo.h"

#include <array>
#include <optional>

#include <rust/bridge.h>

namespace libzcash {

typedef std::array<uint8_t, 32> nullifier_t;

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

enum class Zip212Enabled {
    BeforeZip212,
    AfterZip212
};

class SaplingNote : public BaseNote {
private:
    uint256 rseed;
    friend class SaplingNotePlaintext;
    Zip212Enabled zip_212_enabled;
public:
    diversifier_t d;
    uint256 pk_d;

    SaplingNote(diversifier_t d, uint256 pk_d, uint64_t value, uint256 rseed, Zip212Enabled zip_212_enabled)
            : BaseNote(value), d(d), pk_d(pk_d), rseed(rseed), zip_212_enabled(zip_212_enabled) {}

    SaplingNote(const SaplingPaymentAddress &address, uint64_t value, Zip212Enabled zip_212_enabled);

    virtual ~SaplingNote() {};

    std::optional<uint256> cmu() const;
    std::optional<uint256> nullifier(const SaplingFullViewingKey &vk, const uint64_t position) const;
    uint256 rcm() const;

    Zip212Enabled get_zip_212_enabled() const {
        return zip_212_enabled;
    }
};

class BaseNotePlaintext {
protected:
    uint64_t value_ = 0;
    /// This needs to hold the `Memo::Bytes` directly because we encrypt the in-memory
    /// representation of this class.
    Memo::Bytes memo_;
public:
    BaseNotePlaintext() {}
    BaseNotePlaintext(const BaseNote& note, const std::optional<Memo>& memo)
        : value_(note.value()), memo_(Memo::ToBytes(memo)) {}
    virtual ~BaseNotePlaintext() {}

    inline uint64_t value() const { return value_; }
    inline std::optional<Memo> memo() const { return Memo::FromBytes(memo_); }
};

class SproutNotePlaintext : public BaseNotePlaintext {
public:
    uint256 rho;
    uint256 r;

    SproutNotePlaintext() {}

    SproutNotePlaintext(const SproutNote& note, const std::optional<Memo>& memo);

    SproutNote note(const SproutPaymentAddress& addr) const;

    virtual ~SproutNotePlaintext() {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        unsigned char leadbyte = 0x00;
        READWRITE(leadbyte);

        if (leadbyte != 0x00) {
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

class SaplingNotePlaintext : public BaseNotePlaintext {
private:
    uint256 rseed;
    unsigned char leadbyte;
public:
    diversifier_t d;

    SaplingNotePlaintext() {}

    SaplingNotePlaintext(const SaplingNote& note, const std::optional<Memo>& memo);

    static std::pair<SaplingNotePlaintext, SaplingPaymentAddress> from_rust(
        rust::Box<wallet::DecryptedSaplingOutput> decrypted);

    std::optional<SaplingNote> note(const SaplingIncomingViewingKey& ivk) const;

    virtual ~SaplingNotePlaintext() {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(leadbyte);

        if (leadbyte != 0x01 && leadbyte != 0x02) {
            throw std::ios_base::failure("lead byte of SaplingNotePlaintext is not recognized");
        }

        READWRITE(d);           // 11 bytes
        READWRITE(value_);      // 8 bytes
        READWRITE(rseed);       // 32 bytes
        READWRITE(memo_);       // 512 bytes
    }

    uint256 rcm() const;
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
};


}

#endif // ZC_NOTE_H_
