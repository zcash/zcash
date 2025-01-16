#ifndef ZC_ADDRESS_H_
#define ZC_ADDRESS_H_

#include "consensus/params.h"
#include "key_constants.h"
#include "pubkey.h"
#include "key_constants.h"
#include "script/script.h"
#include "uint256.h"
#include "pubkey.h"
#include "script/script.h"
#include "util/match.h"
#include "zcash/address/orchard.hpp"
#include "zcash/address/sapling.hpp"
#include "zcash/address/sprout.hpp"
#include "zcash/address/unified.h"
#include "zcash/address/zip32.h"

#include <variant>
#include <rust/constants.h>
#include <rust/unified_keys.h>

const unsigned char ZCASH_UFVK_ID_PERSONAL[blake2b::PERSONALBYTES] =
    {'Z', 'c', 'a', 's', 'h', '_', 'U', 'F', 'V', 'K', '_', 'I', 'd', '_', 'F', 'P'};

namespace libzcash {

bool HasKnownReceiverType(const Receiver& receiver);

struct ReceiverIterator {
    using iterator_category = std::random_access_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = const Receiver;
    using pointer           = const Receiver*;
    using reference         = const Receiver&;

    ReceiverIterator(std::vector<const Receiver*> sorted, size_t cur) :
        sortedReceivers(sorted), cur(cur) {}

    reference operator*() const { return *sortedReceivers[cur]; }
    pointer operator->() { return sortedReceivers[cur]; }

    ReceiverIterator& operator++() { cur++; return *this; }
    ReceiverIterator operator++(int) {
        ReceiverIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const ReceiverIterator& a, const ReceiverIterator& b) {
        return a.sortedReceivers == b.sortedReceivers && a.cur == b.cur;
    }
    friend bool operator!=(const ReceiverIterator& a, const ReceiverIterator& b) {
        return !(a == b);
    }

private:
    std::vector<const Receiver*> sortedReceivers;
    size_t cur;
};


class UnifiedAddress {
    std::vector<Receiver> receivers;

    std::vector<const Receiver*> GetSorted() const;

public:
    UnifiedAddress() {}

    static UnifiedAddress ForSingleReceiver(Receiver receiver) {
        UnifiedAddress ua;
        ua.AddReceiver(receiver);
        return ua;
    }

    static std::optional<UnifiedAddress> Parse(const KeyConstants& keyConstants, const std::string& str);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(receivers);
    }

    /**
     * Adds the given receiver to this unified address.
     *
     * Receivers are stored in the unified address (and its encoding) in the order they
     * are added. When generating a new UA, call this method in preference order.
     *
     * Returns false if adding this receiver would result in an invalid unified address:
     * - The UA already contains this receiver type.
     * - The UA would contain both P2PKH and P2SH receivers.
     */
    bool AddReceiver(Receiver receiver);

    bool ContainsReceiver(const Receiver& receiver) const;

    const std::vector<Receiver>& GetReceiversAsParsed() const { return receivers; }

    std::set<ReceiverType> GetKnownReceiverTypes() const {
        std::set<ReceiverType> result;
        for (const auto& receiver : receivers) {
            examine(receiver, match {
                [&](const libzcash::OrchardRawAddress &zaddr) {
                    result.insert(ReceiverType::Orchard);
                },
                [&](const libzcash::SaplingPaymentAddress &zaddr) {
                    result.insert(ReceiverType::Sapling);
                },
                [&](const CScriptID &zaddr) {
                    result.insert(ReceiverType::P2SH);
                },
                [&](const CKeyID &zaddr) {
                    result.insert(ReceiverType::P2PKH);
                },
                [&](const libzcash::UnknownReceiver &uaddr) {
                }
            });
        }
        return result;
    }

    ReceiverIterator begin() const {
        return ReceiverIterator(GetSorted(), 0);
    }
    ReceiverIterator end() const {
        return ReceiverIterator(GetSorted(), receivers.size());
    }
    size_t size() const {
        return receivers.size();
    }

    std::optional<CKeyID> GetP2PKHReceiver() const;

    std::optional<CScriptID> GetP2SHReceiver() const;

    std::optional<SaplingPaymentAddress> GetSaplingReceiver() const;

    std::optional<OrchardRawAddress> GetOrchardReceiver() const;

    /**
     * Return the most-preferred receiver from among the receiver types
     * that we recognize and can use at the given block height.
     */
    std::optional<RecipientAddress> GetPreferredRecipientAddress(
        const Consensus::Params& consensus, int height) const;

    friend inline bool operator==(const UnifiedAddress& a, const UnifiedAddress& b) {
        return a.receivers == b.receivers;
    }
    friend inline bool operator!=(const UnifiedAddress& a, const UnifiedAddress& b) {
        return a.receivers != b.receivers;
    }
    friend inline bool operator<(const UnifiedAddress& a, const UnifiedAddress& b) {
        return a.receivers < b.receivers;
    }
};

class UnifiedFullViewingKeyBuilder;

/**
 * Wrapper for a zcash_address::unified::Ufvk.
 */
class UnifiedFullViewingKey {
private:
    std::unique_ptr<UnifiedFullViewingKeyPtr, decltype(&unified_full_viewing_key_free)> inner;

    UnifiedFullViewingKey() :
        inner(nullptr, unified_full_viewing_key_free) {}

    UnifiedFullViewingKey(UnifiedFullViewingKeyPtr* ptr) :
        inner(ptr, unified_full_viewing_key_free) {}

    friend class UnifiedFullViewingKeyBuilder;
public:
    UnifiedFullViewingKey(UnifiedFullViewingKey&& key) : inner(std::move(key.inner)) {}

    UnifiedFullViewingKey(const UnifiedFullViewingKey& key) :
        inner(unified_full_viewing_key_clone(key.inner.get()), unified_full_viewing_key_free) {}

    /**
     * This method should only be used for serialization of unified full
     * viewing keys that have been generated internally from unified spending
     * keys by zcashd.  It is not suitable for use in any case where the
     * ZcashdUnifiedFullViewingKey value may have been produced by a
     * potentially-lossy conversion from a UnifiedFullViewingKey value that
     * originated outside of zcashd.
     */
    static UnifiedFullViewingKey FromZcashdUFVK(const ZcashdUnifiedFullViewingKey&);

    static std::optional<UnifiedFullViewingKey> Decode(
            const std::string& str,
            const KeyConstants& keyConstants);

    std::string Encode(const KeyConstants& keyConstants) const;

    std::optional<OrchardFullViewingKey> GetOrchardKey() const;

    std::optional<SaplingDiversifiableFullViewingKey> GetSaplingKey() const;

    std::optional<transparent::AccountPubKey> GetTransparentKey() const;

    UFVKId GetKeyID(const KeyConstants& keyConstants) const;

    std::set<ReceiverType> GetKnownReceiverTypes() const {
        std::set<ReceiverType> result;
        if (GetTransparentKey().has_value()) {
            result.insert(ReceiverType::P2PKH);
        }
        if (GetSaplingKey().has_value()) {
            result.insert(ReceiverType::Sapling);
        }
        if (GetOrchardKey().has_value()) {
            result.insert(ReceiverType::Orchard);
        }
        return result;
    }

    UnifiedFullViewingKey& operator=(UnifiedFullViewingKey&& key)
    {
        if (this != &key) {
            inner = std::move(key.inner);
        }
        return *this;
    }

    UnifiedFullViewingKey& operator=(const UnifiedFullViewingKey& key)
    {
        if (this != &key) {
            inner.reset(unified_full_viewing_key_clone(key.inner.get()));
        }
        return *this;
    }
};

class UnifiedFullViewingKeyBuilder {
private:
    std::optional<std::vector<uint8_t>> t_bytes;
    std::optional<std::vector<uint8_t>> sapling_bytes;
    std::optional<std::vector<uint8_t>> orchard_bytes;
public:
    UnifiedFullViewingKeyBuilder():
        t_bytes(std::nullopt),
        sapling_bytes(std::nullopt),
        orchard_bytes(std::nullopt) {}

    bool AddTransparentKey(const transparent::AccountPubKey&);
    bool AddSaplingKey(const SaplingDiversifiableFullViewingKey&);
    bool AddOrchardKey(const OrchardFullViewingKey&);

    std::optional<UnifiedFullViewingKey> build() const;
};

/** Addresses that can appear in a string encoding. */
typedef std::variant<
    CKeyID,
    CScriptID,
    SproutPaymentAddress,
    SaplingPaymentAddress,
    UnifiedAddress> PaymentAddress;
/** Viewing keys that can have a string encoding. */
typedef std::variant<
    SproutViewingKey,
    SaplingExtendedFullViewingKey,
    UnifiedFullViewingKey> ViewingKey;
/** Spending keys that can have a string encoding. */
typedef std::variant<
    SproutSpendingKey,
    SaplingExtendedSpendingKey> SpendingKey;

class HasShieldedRecipient {
public:
    bool operator()(const CKeyID& p2pkh) { return false; }
    bool operator()(const CScriptID& p2sh) { return false; }
    bool operator()(const SproutPaymentAddress& addr) { return true; }
    bool operator()(const SaplingPaymentAddress& addr) { return true; }
    bool operator()(const UnifiedAddress& addr) { return true; }
};

class SelectRecipientAddress {
    const Consensus::Params& consensus;
    int height;

public:
    SelectRecipientAddress(const Consensus::Params& consensus, int height) :
        consensus(consensus), height(height) {}

    std::optional<RecipientAddress> operator()(const CKeyID& p2pkh) { return p2pkh; }
    std::optional<RecipientAddress> operator()(const CScriptID& p2sh) { return p2sh; }
    std::optional<RecipientAddress> operator()(const SproutPaymentAddress& addr) { return std::nullopt; }
    std::optional<RecipientAddress> operator()(const SaplingPaymentAddress& addr) { return addr; }
    std::optional<RecipientAddress> operator()(const UnifiedAddress& addr) {
        return addr.GetPreferredRecipientAddress(consensus, height);
    }
};

class AddressInfoFromSpendingKey {
public:
    std::pair<std::string, PaymentAddress> operator()(const SproutSpendingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const struct SaplingExtendedSpendingKey&) const;
};

class AddressInfoFromViewingKey {
private:
    const KeyConstants& keyConstants;

public:
    AddressInfoFromViewingKey(const KeyConstants& keyConstants): keyConstants(keyConstants) {}
    std::pair<std::string, PaymentAddress> operator()(const SproutViewingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const struct SaplingExtendedFullViewingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const UnifiedFullViewingKey&) const;
};

} //namespace libzcash

/**
 * Gets the typecode for the given UA receiver.
 */
class TypecodeForReceiver {
public:
    TypecodeForReceiver() {}

    uint32_t operator()(const libzcash::OrchardRawAddress &zaddr) const;
    uint32_t operator()(const libzcash::SaplingPaymentAddress &zaddr) const;
    uint32_t operator()(const CScriptID &p2sh) const;
    uint32_t operator()(const CKeyID &p2pkh) const;
    uint32_t operator()(const libzcash::UnknownReceiver &p2pkh) const;
};

#endif // ZC_ADDRESS_H_
