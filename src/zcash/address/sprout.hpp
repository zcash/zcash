// Copyright (c) 2016-2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZC_ADDRESS_SPROUT_H_
#define ZC_ADDRESS_SPROUT_H_

#include "serialize.h"
#include "uint252.h"
#include "uint256.h"

namespace libzcash {

const size_t SerializedSproutPaymentAddressSize = 64;
const size_t SerializedSproutViewingKeySize = 64;
const size_t SerializedSproutSpendingKeySize = 32;

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

} // namespace libzcash

#endif // ZC_ADDRESS_SPROUT_H_
