// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_UNIFIED_H
#define ZCASH_ZCASH_ADDRESS_UNIFIED_H

#include "zip32.h"
#include "bip44.h"

namespace libzcash {

class ZcashdUnifiedSpendingKey;
class ZcashdUnifiedFullViewingKey;

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

    std::optional<UnifiedAddress> Address(diversifier_index_t j) const;

    std::pair<UnifiedAddress, diversifier_index_t> FindAddress(diversifier_index_t j) const;
};

/**
 * The type of unified spending keys supported by zcashd.
 */
class ZcashdUnifiedSpendingKey {
private:
    libzcash::AccountId accountId;
    std::optional<CExtKey> transparentKey;
    std::optional<SaplingExtendedSpendingKey> saplingKey;

    ZcashdUnifiedSpendingKey() {}
public:
    static std::optional<std::pair<ZcashdUnifiedSpendingKey, HDKeyPath>> ForAccount(
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

