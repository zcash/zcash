#ifndef ZC_ADDRESS_H_
#define ZC_ADDRESS_H_

#include "uint256.h"
#include "uint252.h"
#include "serialize.h"

#include <boost/variant.hpp>

namespace libzcash {
class InvalidEncoding {
public:
    friend bool operator==(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
    friend bool operator<(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
};

const size_t SerializedPaymentAddressSize = 64;
const size_t SerializedViewingKeySize = 64;
const size_t SerializedSpendingKeySize = 32;

class SproutPaymentAddress {
public:
    uint256 a_pk;
    uint256 pk_enc;

    SproutPaymentAddress() : a_pk(), pk_enc() { }
    SproutPaymentAddress(uint256 a_pk, uint256 pk_enc) : a_pk(a_pk), pk_enc(pk_enc) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(a_pk);
        READWRITE(pk_enc);
    }

    //! Get the 256-bit SHA256d hash of this payment address.
    uint256 GetHash() const;

    friend inline bool operator==(const SproutPaymentAddress& a, const SproutPaymentAddress& b) {
        return a.a_pk == b.a_pk && a.pk_enc == b.pk_enc;
    }
    friend inline bool operator<(const SproutPaymentAddress& a, const SproutPaymentAddress& b) {
        return (a.a_pk < b.a_pk ||
                (a.a_pk == b.a_pk && a.pk_enc < b.pk_enc));
    }
};

class ReceivingKey : public uint256 {
public:
    ReceivingKey() { }
    ReceivingKey(uint256 sk_enc) : uint256(sk_enc) { }

    uint256 pk_enc() const;
};

class SproutViewingKey {
public:
    uint256 a_pk;
    ReceivingKey sk_enc;

    SproutViewingKey() : a_pk(), sk_enc() { }
    SproutViewingKey(uint256 a_pk, ReceivingKey sk_enc) : a_pk(a_pk), sk_enc(sk_enc) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(a_pk);
        READWRITE(sk_enc);
    }

    SproutPaymentAddress address() const;

    friend inline bool operator==(const SproutViewingKey& a, const SproutViewingKey& b) {
        return a.a_pk == b.a_pk && a.sk_enc == b.sk_enc;
    }
    friend inline bool operator<(const SproutViewingKey& a, const SproutViewingKey& b) {
        return (a.a_pk < b.a_pk ||
                (a.a_pk == b.a_pk && a.sk_enc < b.sk_enc));
    }
};

class SproutSpendingKey : public uint252 {
public:
    SproutSpendingKey() : uint252() { }
    SproutSpendingKey(uint252 a_sk) : uint252(a_sk) { }

    static SproutSpendingKey random();

    ReceivingKey receiving_key() const;
    SproutViewingKey viewing_key() const;
    SproutPaymentAddress address() const;
};

typedef boost::variant<InvalidEncoding, SproutPaymentAddress> PaymentAddress;
typedef boost::variant<InvalidEncoding, SproutViewingKey> ViewingKey;
typedef boost::variant<InvalidEncoding, SproutSpendingKey> SpendingKey;

}

/** Check whether a PaymentAddress is not an InvalidEncoding. */
bool IsValidPaymentAddress(const libzcash::PaymentAddress& zaddr);

/** Check whether a ViewingKey is not an InvalidEncoding. */
bool IsValidViewingKey(const libzcash::ViewingKey& vk);

/** Check whether a SpendingKey is not an InvalidEncoding. */
bool IsValidSpendingKey(const libzcash::SpendingKey& zkey);

#endif // ZC_ADDRESS_H_
