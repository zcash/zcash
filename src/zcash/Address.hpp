#ifndef ZC_ADDRESS_H_
#define ZC_ADDRESS_H_

#include "uint256.h"
#include "uint252.h"
#include "serialize.h"
#include "Zcash.h"

#include <boost/variant.hpp>

namespace libzcash {
class InvalidEncoding {
public:
    friend bool operator==(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
    friend bool operator<(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
};

const size_t SerializedSproutPaymentAddressSize = 64;
const size_t SerializedSproutViewingKeySize = 64;
const size_t SerializedSproutSpendingKeySize = 32;

const size_t SerializedSaplingPaymentAddressSize = 43;
const size_t SerializedSaplingFullViewingKeySize = 96;
const size_t SerializedSaplingExpandedSpendingKeySize = 96;
const size_t SerializedSaplingSpendingKeySize = 32;

typedef std::array<unsigned char, ZC_DIVERSIFIER_SIZE> diversifier_t;

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

//! Sapling functions. 
class SaplingPaymentAddress {
public:
    diversifier_t d;
    uint256 pk_d;

    SaplingPaymentAddress() : d(), pk_d() { }
    SaplingPaymentAddress(diversifier_t d, uint256 pk_d) : d(d), pk_d(pk_d) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(d);
        READWRITE(pk_d);
    }
    
    //! Get the 256-bit SHA256d hash of this payment address.
    uint256 GetHash() const;

    friend inline bool operator==(const SaplingPaymentAddress& a, const SaplingPaymentAddress& b) {
        return a.d == b.d && a.pk_d == b.pk_d;
    }
    friend inline bool operator<(const SaplingPaymentAddress& a, const SaplingPaymentAddress& b) {
        return (a.d < b.d ||
                (a.d == b.d && a.pk_d < b.pk_d));
    }
};

class SaplingIncomingViewingKey : public uint256 {
public:
    SaplingIncomingViewingKey() : uint256() { }
    SaplingIncomingViewingKey(uint256 ivk) : uint256(ivk) { }
    
    // Can pass in diversifier for Sapling addr
    boost::optional<SaplingPaymentAddress> address(diversifier_t d) const;
};

class SaplingFullViewingKey {
public: 
    uint256 ak;
    uint256 nk;
    uint256 ovk; 
    
    SaplingFullViewingKey() : ak(), nk(), ovk() { }
    SaplingFullViewingKey(uint256 ak, uint256 nk, uint256 ovk) : ak(ak), nk(nk), ovk(ovk) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ak);
        READWRITE(nk);
        READWRITE(ovk);
    }

    //! Get the fingerprint of this full viewing key (as defined in ZIP 32).
    uint256 GetFingerprint() const;

    SaplingIncomingViewingKey in_viewing_key() const;
    bool is_valid() const;

    friend inline bool operator==(const SaplingFullViewingKey& a, const SaplingFullViewingKey& b) {
        return a.ak == b.ak && a.nk == b.nk && a.ovk == b.ovk;
    }
    friend inline bool operator<(const SaplingFullViewingKey& a, const SaplingFullViewingKey& b) {
        return (a.ak < b.ak || 
            (a.ak == b.ak && a.nk < b.nk) || 
            (a.ak == b.ak && a.nk == b.nk && a.ovk < b.ovk));
    }
};


class SaplingExpandedSpendingKey {
public: 
    uint256 ask;
    uint256 nsk;
    uint256 ovk;
    
    SaplingExpandedSpendingKey() : ask(), nsk(), ovk() { }
    SaplingExpandedSpendingKey(uint256 ask, uint256 nsk, uint256 ovk) : ask(ask), nsk(nsk), ovk(ovk) { }
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ask);
        READWRITE(nsk);
        READWRITE(ovk);
    }
    
    SaplingFullViewingKey full_viewing_key() const;
    
    friend inline bool operator==(const SaplingExpandedSpendingKey& a, const SaplingExpandedSpendingKey& b) {
        return a.ask == b.ask && a.nsk == b.nsk && a.ovk == b.ovk;
    }
    friend inline bool operator<(const SaplingExpandedSpendingKey& a, const SaplingExpandedSpendingKey& b) {
        return (a.ask < b.ask || 
            (a.ask == b.ask && a.nsk < b.nsk) || 
            (a.ask == b.ask && a.nsk == b.nsk && a.ovk < b.ovk));
    }
};

class SaplingSpendingKey : public uint256 {
public:
    SaplingSpendingKey() : uint256() { }
    SaplingSpendingKey(uint256 sk) : uint256(sk) { }

    static SaplingSpendingKey random();

    SaplingExpandedSpendingKey expanded_spending_key() const;
    SaplingFullViewingKey full_viewing_key() const;
    
    // Can derive Sapling addr from default diversifier 
    SaplingPaymentAddress default_address() const;
};

typedef boost::variant<InvalidEncoding, SproutPaymentAddress, SaplingPaymentAddress> PaymentAddress;
typedef boost::variant<InvalidEncoding, SproutViewingKey, SaplingIncomingViewingKey> ViewingKey;

}

/** Check whether a PaymentAddress is not an InvalidEncoding. */
extern const uint32_t SAPLING_BRANCH_ID;
bool IsValidPaymentAddress(const libzcash::PaymentAddress& zaddr, uint32_t consensusBranchId = SAPLING_BRANCH_ID);

/** Check whether a ViewingKey is not an InvalidEncoding. */
bool IsValidViewingKey(const libzcash::ViewingKey& vk);

#endif // ZC_ADDRESS_H_
