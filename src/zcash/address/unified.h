// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_UNIFIED_H
#define ZCASH_ZCASH_ADDRESS_UNIFIED_H

#include "zip32.h"
#include "bip44.h"

namespace libzcash {

enum class ReceiverType: uint32_t {
    P2PKH = 0x00,
    P2SH = 0x01,
    Sapling = 0x02,
    Orchard = 0x03
};

/**
 * Test whether the specified list of receiver types contains a
 * shielded receiver type
 */
bool HasShielded(const std::set<ReceiverType>& receiverTypes);

class ZcashdUnifiedSpendingKey;

// prototypes for the classes handling ZIP-316 encoding (in Address.hpp)
class UnifiedAddress;
class UnifiedFullViewingKey;

/**
 * An internal-only type for unified full viewing keys that represents only the
 * set of receiver types that are supported by zcashd. This type does not
 * support round-trip serialization to and from the UnifiedFullViewingKey type,
 * which should be used in most cases.
 */
class ZcashdUnifiedFullViewingKey {
private:
    std::optional<CChainablePubKey> transparentKey;
    std::optional<SaplingDiversifiableFullViewingKey> saplingKey;

    ZcashdUnifiedFullViewingKey() {}

    friend class ZcashdUnifiedSpendingKey;
public:
    /**
     * This constructor is lossy, and does not support round-trip transformations.
     */
    static ZcashdUnifiedFullViewingKey FromUnifiedFullViewingKey(const UnifiedFullViewingKey& ufvk);

    const std::optional<CChainablePubKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingDiversifiableFullViewingKey>& GetSaplingKey() const {
        return saplingKey;
    }

    /**
     * Creates a new unified address having the specified receiver types, at the specified
     * diversifier index, unless the diversifer index would generate an invalid receiver.
     * Returns `std::nullopt` if the diversifier index does not produce a valid receiver
     * for one or more of the specified receiver types; under this circumstance, the caller
     * should usually try successive diversifier indices until the operation returns a
     * non-null value.
     *
     * This method will throw if `receiverTypes` does not include a shielded receiver type.
     */
    std::optional<UnifiedAddress> Address(
            const diversifier_index_t& j,
            const std::set<ReceiverType>& receiverTypes) const;

    /**
     * Find the smallest diversifier index >= `j` such that it generates a valid
     * unified address according to the conditions specified in the documentation
     * for the `Address` method above, and returns the newly created address along
     * with the diversifier index used to produce it.
     *
     * This method will throw if `receiverTypes` does not include a shielded receiver type.
     */
    std::pair<UnifiedAddress, diversifier_index_t> FindAddress(
            const diversifier_index_t& j,
            const std::set<ReceiverType>& receiverTypes) const;

    std::pair<UnifiedAddress, diversifier_index_t> FindAddress(const diversifier_index_t& j) const;
};

/**
 * The type of unified spending keys supported by zcashd.
 */
class ZcashdUnifiedSpendingKey {
private:
    std::optional<CExtKey> transparentKey;
    std::optional<SaplingExtendedSpendingKey> saplingKey;

    ZcashdUnifiedSpendingKey() {}
public:
    static std::optional<ZcashdUnifiedSpendingKey> ForAccount(
            const HDSeed& seed,
            uint32_t bip44CoinType,
            libzcash::AccountId accountId);

    const std::optional<CExtKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingExtendedSpendingKey>& GetSaplingExtendedSpendingKey() const {
        return saplingKey;
    }

    ZcashdUnifiedFullViewingKey ToFullViewingKey() const;
};

} //namespace libzcash

#endif // ZCASH_ZCASH_ADDRESS_UNIFIED_H

