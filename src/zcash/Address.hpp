#ifndef ZC_ADDRESS_H_
#define ZC_ADDRESS_H_

#include "uint256.h"
#include "uint252.h"
#include "serialize.h"

namespace libzcash {

const size_t SerializedPaymentAddressSize = 64;
const size_t SerializedViewingKeySize = 64;
const size_t SerializedSpendingKeySize = 32;

class PaymentAddress {
public:
    uint256 a_pk;
    uint256 pk_enc;

    PaymentAddress() : a_pk(), pk_enc() { }
    PaymentAddress(uint256 a_pk, uint256 pk_enc) : a_pk(a_pk), pk_enc(pk_enc) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(a_pk);
        READWRITE(pk_enc);
    }

    //! Get the 256-bit SHA256d hash of this payment address.
    uint256 GetHash() const;

    friend inline bool operator==(const PaymentAddress& a, const PaymentAddress& b) {
        return a.a_pk == b.a_pk && a.pk_enc == b.pk_enc;
    }
    friend inline bool operator<(const PaymentAddress& a, const PaymentAddress& b) {
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

class ViewingKey {
public:
    uint256 a_pk;
    ReceivingKey sk_enc;

    ViewingKey() : a_pk(), sk_enc() { }
    ViewingKey(uint256 a_pk, ReceivingKey sk_enc) : a_pk(a_pk), sk_enc(sk_enc) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(a_pk);
        READWRITE(sk_enc);
    }

    PaymentAddress address() const;

    friend inline bool operator==(const ViewingKey& a, const ViewingKey& b) {
        return a.a_pk == b.a_pk && a.sk_enc == b.sk_enc;
    }
    friend inline bool operator<(const ViewingKey& a, const ViewingKey& b) {
        return (a.a_pk < b.a_pk ||
                (a.a_pk == b.a_pk && a.sk_enc < b.sk_enc));
    }
};

class SpendingKey : public uint252 {
public:
    SpendingKey() : uint252() { }
    SpendingKey(uint252 a_sk) : uint252(a_sk) { }

    static SpendingKey random();

    ReceivingKey receiving_key() const;
    ViewingKey viewing_key() const;
    PaymentAddress address() const;
};

}

#endif // ZC_ADDRESS_H_
