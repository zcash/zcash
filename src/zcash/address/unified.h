// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_UNIFIED_H
#define ZCASH_ZCASH_ADDRESS_UNIFIED_H

#include "zip32.h"
#include "bip44.h"
#include "zcash/Address.hpp"

namespace libzcash {

class ZcashdUnifiedSpendingKey;
class ZcashdUnifiedFullViewingKey;

class ZcashdUnifiedFullViewingKey {
private:
    std::optional<CExtPubKey> transparentKey;
    std::optional<SaplingDiversifiableFullViewingKey> saplingKey;

    ZcashdUnifiedFullViewingKey() {}

    friend class ZcashdUnifiedSpendingKey;
public:
    const std::optional<CExtPubKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingDiversifiableFullViewingKey>& GetSaplingKey() const {
        return saplingKey;
    }

    std::optional<UnifiedAddress> Address(diversifier_index_t j) const;

    std::pair<UnifiedAddress, diversifier_index_t> FindAddress(diversifier_index_t j) const {
        auto addr = Address(j);
        while (!addr.has_value()) {
            if (!j.increment())
                throw std::runtime_error(std::string(__func__) + ": diversifier index overflow.");;
            addr = Address(j);
        }
        return std::make_pair(addr.value(), j);
    }
};

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

