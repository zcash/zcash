// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_UNIFIED_H
#define ZCASH_ZCASH_ADDRESS_UNIFIED_H

#include "transparent.h"
#include "key_constants.h"
#include "script/script.h"
#include "zip32.h"

namespace libzcash {

// prototypes for the classes handling ZIP-316 encoding (in Address.hpp)
// TODO: ZIP-316 encoding should probably be moved here
class UnifiedAddress;
class UnifiedFullViewingKey;

enum class ReceiverType: uint32_t {
    P2PKH = 0x00,
    P2SH = 0x01,
    Sapling = 0x02,
    //Orchard = 0x03
};

enum class UnifiedAddressGenerationError {
    ShieldedReceiverNotFound,
    ReceiverTypeNotAvailable,
    NoAddressForDiversifier,
    DiversifierSpaceExhausted,
    InvalidTransparentChildIndex
};

typedef std::variant<
    std::pair<UnifiedAddress, diversifier_index_t>,
    UnifiedAddressGenerationError> UnifiedAddressGenerationResult;

/** A recipient address to which a unified address can be resolved */
typedef std::variant<
    CKeyID,
    CScriptID,
    libzcash::SaplingPaymentAddress> RecipientAddress;

/**
 * An enumeration of the types of change that a transaction may produce.  It is
 * sorted in descending preference order, so that when iterating over a set of
 * change types the most-preferred type is selected first.
 */
enum class ChangeType {
    Sapling,
    Transparent,
};

class TransparentChangeRequest {
private:
    const diversifier_index_t& index;
public:
    TransparentChangeRequest(const diversifier_index_t& indexIn): index(indexIn) {}

    const diversifier_index_t& GetIndex() const {
        return index;
    }
};

class SaplingChangeRequest {};

typedef std::variant<
    TransparentChangeRequest,
    SaplingChangeRequest> ChangeRequest;

/**
 * Test whether the specified list of receiver types contains a
 * shielded receiver type
 */
bool HasShielded(const std::set<ReceiverType>& receiverTypes);

/**
 * Test whether the specified list of receiver types contains a
 * shielded receiver type
 */
bool HasTransparent(const std::set<ReceiverType>& receiverTypes);

class ZcashdUnifiedSpendingKey;

class UnknownReceiver {
public:
    uint32_t typecode;
    std::vector<uint8_t> data;

    UnknownReceiver(uint32_t typecode, std::vector<uint8_t> data) :
        typecode(typecode), data(data) {}

    friend inline bool operator==(const UnknownReceiver& a, const UnknownReceiver& b) {
        return a.typecode == b.typecode && a.data == b.data;
    }
    friend inline bool operator<(const UnknownReceiver& a, const UnknownReceiver& b) {
        // We don't know for certain the preference order of unknown receivers, but it is
        // _likely_ that the higher typecode has higher preference. The exact sort order
        // doesn't really matter, as unknown receivers have lower preference than known
        // receivers.
        return (a.typecode > b.typecode ||
                (a.typecode == b.typecode && a.data < b.data));
    }
};

/**
 * Receivers that can appear in a Unified Address.
 *
 * These types are given in order of preference (as defined in ZIP 316), so that sorting
 * variants by `operator<` is equivalent to sorting by preference.
 */
typedef std::variant<
    SaplingPaymentAddress,
    CScriptID,
    CKeyID,
    UnknownReceiver> Receiver;

/**
 * An internal identifier for a unified full viewing key, derived as a
 * blake2b hash of the serialized form of the UFVK.
 */
class UFVKId: public uint256 {
public:
    UFVKId() : uint256() {}
    UFVKId(const uint256& in) : uint256(in) {}
};

/**
 * An internal-only type for unified full viewing keys that represents only the
 * set of receiver types that are supported by zcashd. This type does not
 * support round-trip serialization to and from the UnifiedFullViewingKey type,
 * which should be used in most cases.
 */
class ZcashdUnifiedFullViewingKey {
private:
    UFVKId keyId;
    std::optional<transparent::AccountPubKey> transparentKey;
    std::optional<SaplingDiversifiableFullViewingKey> saplingKey;

    ZcashdUnifiedFullViewingKey() {}

    friend class ZcashdUnifiedSpendingKey;
public:
    /**
     * This constructor is lossy; it ignores unknown receiver types
     * and therefore does not support round-trip transformations.
     */
    static ZcashdUnifiedFullViewingKey FromUnifiedFullViewingKey(
            const KeyConstants& keyConstants,
            const UnifiedFullViewingKey& ufvk);

    const UFVKId& GetKeyID() const {
        return keyId;
    }

    /**
     * Return the transparent key at the account level;
     */
    const std::optional<transparent::AccountPubKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingDiversifiableFullViewingKey>& GetSaplingKey() const {
        return saplingKey;
    }

    /**
     * Creates a new unified address having the specified receiver types, at the specified
     * diversifier index, unless the diversifer index would generate an invalid receiver.
     * Returns UnifiedAddressGenerationError::NoAddressForDiversifier if the diversifier
     * index does not produce a valid receiver for one or more of the specified receiver
     * types; under this circumstance, the caller should usually try successive diversifier
     * indices until the operation returns a valid address. Returns
     * `UnifiedAddressGenerationError::InvalidTransparentChildIndex` if a transparent
     * receiver was requested but the specified diversifier was out of range.
     *
     * If successful in deriving an address, this method returns a pair consisting of the
     * newly derived address and the provided value `j`.
     */
    UnifiedAddressGenerationResult Address(
            const diversifier_index_t& j,
            const std::set<ReceiverType>& receiverTypes) const;

    /**
     * Find the smallest diversifier index >= `j` such that it generates a valid
     * unified address according to the conditions specified in the documentation
     * for the `Address` method above, and returns the newly created address along
     * with the diversifier index used to produce it.
     *
     * Returns UnifiedAddressGenerationError::NoAddressForDiversifier if the
     * diversifier space is exhausted, or if the set of receiver types contains a
     * transparent receiver and the diversifier exceeds the maximum transparent
     * child index.
     */
    UnifiedAddressGenerationResult FindAddress(
            const diversifier_index_t& j,
            const std::set<ReceiverType>& receiverTypes) const;

    /**
     * Find the next available address that contains all supported receiver types.
     */
    UnifiedAddressGenerationResult FindAddress(const diversifier_index_t& j) const;

    /**
     * Return the change address for this UFVK, given the provided
     * set of receiver types for pools involved in this transaction.
     * If the provided set is empty, return the change address
     * corresponding to the most-preferred pool.
     */
    std::optional<RecipientAddress> GetChangeAddress(const ChangeRequest& req) const;

    friend bool operator==(const ZcashdUnifiedFullViewingKey& a, const ZcashdUnifiedFullViewingKey& b)
    {
        return a.transparentKey == b.transparentKey && a.saplingKey == b.saplingKey;
    }
};

/**
 * The type of unified spending keys supported by zcashd.
 */
class ZcashdUnifiedSpendingKey {
private:
    transparent::AccountKey transparentKey;
    SaplingExtendedSpendingKey saplingKey;

    ZcashdUnifiedSpendingKey(
            transparent::AccountKey tkey,
            SaplingExtendedSpendingKey skey): transparentKey(tkey), saplingKey(skey) {}
public:
    static std::optional<ZcashdUnifiedSpendingKey> ForAccount(
            const HDSeed& seed,
            uint32_t bip44CoinType,
            libzcash::AccountId accountId);

    const transparent::AccountKey& GetTransparentKey() const {
        return transparentKey;
    }

    const SaplingExtendedSpendingKey& GetSaplingExtendedSpendingKey() const {
        return saplingKey;
    }

    UnifiedFullViewingKey ToFullViewingKey() const;
};

} //namespace libzcash

#endif // ZCASH_ZCASH_ADDRESS_UNIFIED_H

