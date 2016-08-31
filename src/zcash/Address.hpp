#ifndef _ZCADDRESS_H_
#define _ZCADDRESS_H_

#include "uint256.h"
#include "uint252.h"
#include "serialize.h"

namespace libzcash {

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

    friend inline bool operator==(const PaymentAddress& a, const PaymentAddress& b) {
        return a.a_pk == b.a_pk && a.pk_enc == b.pk_enc;
    }
    friend inline bool operator<(const PaymentAddress& a, const PaymentAddress& b) {
        return (a.a_pk < b.a_pk ||
                (a.a_pk == b.a_pk && a.pk_enc < b.pk_enc));
    }
};

class ViewingKey : public uint256 {
public:
    ViewingKey(uint256 sk_enc) : uint256(sk_enc) { }

    uint256 pk_enc();
};

class SpendingKey : public uint252 {
public:
    SpendingKey() : uint252() { }
    SpendingKey(uint252 a_sk) : uint252(a_sk) { }

    static SpendingKey random();

    ViewingKey viewing_key() const;
    PaymentAddress address() const;
};

}

#endif // _ZCADDRESS_H_
