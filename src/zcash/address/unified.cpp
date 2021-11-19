// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "unified.h"
#include "bip44.h"

using namespace libzcash;

//
// Unified Keys
//

std::optional<std::pair<ZcashdUnifiedSpendingKey, HDKeyPath>> ZcashdUnifiedSpendingKey::ForAccount(const HDSeed& seed, uint32_t bip44CoinType, AccountId accountId) {
    ZcashdUnifiedSpendingKey usk;
    usk.accountId = accountId;

    auto transparentKey = DeriveBip44TransparentAccountKey(seed, bip44CoinType, accountId);
    if (!transparentKey.has_value()) return std::nullopt;
    usk.transparentKey = transparentKey.value().first;

    auto saplingKey = SaplingExtendedSpendingKey::ForAccount(seed, bip44CoinType, accountId);
    usk.saplingKey = saplingKey.first;

    return std::make_pair(usk, saplingKey.second);
}

ZcashdUnifiedFullViewingKey ZcashdUnifiedSpendingKey::ToFullViewingKey() const {
    ZcashdUnifiedFullViewingKey ufvk;

    if (transparentKey.has_value()) {
        ufvk.transparentKey = transparentKey.value().Neuter();
    }

    if (saplingKey.has_value()) {
        ufvk.saplingKey = saplingKey.value().ToXFVK();
    }

    return ufvk;
}

std::optional<UnifiedAddress> ZcashdUnifiedFullViewingKey::Address(diversifier_index_t j) const {
    UnifiedAddress ua;

    if (saplingKey.has_value()) {
        auto saplingAddress = saplingKey.value().Address(j);
        if (saplingAddress.has_value()) {
            ua.AddReceiver(saplingAddress.value());
        } else {
            return std::nullopt;
        }
    }

    if (transparentKey.has_value()) {
        auto childIndex = j.ToTransparentChildIndex();
        if (!childIndex.has_value()) return std::nullopt;

        CExtPubKey externalKey;
        if (!transparentKey.value().Derive(externalKey, 0)) {
            return std::nullopt;
        }

        CExtPubKey childKey;
        if (externalKey.Derive(childKey, childIndex.value())) {
            ua.AddReceiver(childKey.pubkey.GetID());
        } else {
            return std::nullopt;
        }
    }

    return ua;
}

