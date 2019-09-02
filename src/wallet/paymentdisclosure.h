// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_PAYMENTDISCLOSURE_H
#define ZCASH_PAYMENTDISCLOSURE_H

#include "uint256.h"
#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "version.h"

// For JSOutPoint
#include "wallet/wallet.h"

#include <array>
#include <cstdint>
#include <string>


// Ensure that the two different protocol messages, payment disclosure blobs and transactions,
// which are signed with the same key, joinSplitPrivKey, have disjoint encodings such that an
// encoding from one context will be rejected in the other.  We know that the set of valid
// transaction versions is currently ({1..INT32_MAX}) so we will use a negative value for
// payment disclosure of -10328976 which in hex is 0xFF626470.  Serialization is in little endian
// format, so a payment disclosure hex string begins 706462FF, which in ISO-8859-1 is "pdbÿ".
#define PAYMENT_DISCLOSURE_PAYLOAD_MAGIC_BYTES    -10328976

#define PAYMENT_DISCLOSURE_VERSION_EXPERIMENTAL 0

#define PAYMENT_DISCLOSURE_BLOB_STRING_PREFIX    "zpd:"

typedef JSOutPoint PaymentDisclosureKey;

struct PaymentDisclosureInfo {
    uint8_t version;          // 0 = experimental, 1 = first production version, etc.
    uint256 esk;              // zcash/NoteEncryption.cpp
    uint256 joinSplitPrivKey; // primitives/transaction.h
    // ed25519 - not tied to implementation e.g. libsodium, see ed25519 rfc

    libzcash::SproutPaymentAddress zaddr;

    PaymentDisclosureInfo() : version(PAYMENT_DISCLOSURE_VERSION_EXPERIMENTAL) {
    }

    PaymentDisclosureInfo(uint8_t v, uint256 esk, uint256 key, libzcash::SproutPaymentAddress zaddr) : version(v), esk(esk), joinSplitPrivKey(key), zaddr(zaddr) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version);
        READWRITE(esk);
        READWRITE(joinSplitPrivKey);
        READWRITE(zaddr);
    }

    std::string ToString() const;

    friend bool operator==(const PaymentDisclosureInfo& a, const PaymentDisclosureInfo& b) {
        return (a.version == b.version && a.esk == b.esk && a.joinSplitPrivKey == b.joinSplitPrivKey && a.zaddr == b.zaddr);
    }

    friend bool operator!=(const PaymentDisclosureInfo& a, const PaymentDisclosureInfo& b) {
        return !(a == b);
    }

};


struct PaymentDisclosurePayload {
    int32_t marker = PAYMENT_DISCLOSURE_PAYLOAD_MAGIC_BYTES;  // to be disjoint from transaction encoding
    uint8_t version;        // 0 = experimental, 1 = first production version, etc.
    uint256 esk;            // zcash/NoteEncryption.cpp
    uint256 txid;           // primitives/transaction.h
    uint64_t js;            // Index into CTransaction.vJoinSplit
    uint8_t n;              // Index into JSDescription fields of length ZC_NUM_JS_OUTPUTS
    libzcash::SproutPaymentAddress zaddr; // zcash/Address.hpp
    std::string message;     // parameter to RPC call

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(marker);
        READWRITE(version);
        READWRITE(esk);
        READWRITE(txid);
        READWRITE(js);
        READWRITE(n);
        READWRITE(zaddr);
        READWRITE(message);
    }

    std::string ToString() const;

    friend bool operator==(const PaymentDisclosurePayload& a, const PaymentDisclosurePayload& b) {
        return (
            a.version == b.version &&
            a.esk == b.esk &&
            a.txid == b.txid &&
            a.js == b.js &&
            a.n == b.n &&
            a.zaddr == b.zaddr &&
            a.message == b.message
            );
    }

    friend bool operator!=(const PaymentDisclosurePayload& a, const PaymentDisclosurePayload& b) {
        return !(a == b);
    }
};

struct PaymentDisclosure {
    PaymentDisclosurePayload payload;
    std::array<unsigned char, 64> payloadSig;
    // We use boost array because serialize doesn't like char buffer, otherwise we could do: unsigned char payloadSig[64];

    PaymentDisclosure() {};
    PaymentDisclosure(const PaymentDisclosurePayload payload, const std::array<unsigned char, 64> sig) : payload(payload), payloadSig(sig) {};
    PaymentDisclosure(const uint256& joinSplitPubKey, const PaymentDisclosureKey& key, const PaymentDisclosureInfo& info, const std::string& message);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(payload);
        READWRITE(payloadSig);
    }

    std::string ToString() const;

    friend bool operator==(const PaymentDisclosure& a, const PaymentDisclosure& b) {
        return (a.payload == b.payload && a.payloadSig == b.payloadSig);
    }

    friend bool operator!=(const PaymentDisclosure& a, const PaymentDisclosure& b) {
        return !(a == b);
    }
};



typedef std::pair<PaymentDisclosureKey, PaymentDisclosureInfo> PaymentDisclosureKeyInfo;


#endif // ZCASH_PAYMENTDISCLOSURE_H
