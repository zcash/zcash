// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "zcash/Address.hpp"
#include "unified.h"
#include "bip44.h"

using namespace libzcash;

//
// Unified Keys
//

std::optional<std::pair<ZcashdUnifiedSpendingKey, HDKeyPath>> ZcashdUnifiedSpendingKey::ForAccount(
        const HDSeed& seed,
        const uint32_t bip44CoinType,
        AccountId accountId) {
    ZcashdUnifiedSpendingKey usk;

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
        auto extPubKey = transparentKey.value().Neuter();

        // TODO: how to ensure that the pubkey is in its compressed representation?
        // Is that already guaranteed?
        ufvk.transparentKey = CChainablePubKey::FromParts(extPubKey.chaincode, extPubKey.pubkey).value();
    }

    if (saplingKey.has_value()) {
        ufvk.saplingKey = saplingKey.value().ToXFVK();
    }

    return ufvk;
}

ZcashdUnifiedFullViewingKey ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(
        const UnifiedFullViewingKey& ufvk) {
    ZcashdUnifiedFullViewingKey result;

    auto transparentKey = ufvk.GetTransparentKey();
    if (transparentKey.has_value()) {
        result.transparentKey = transparentKey.value();
    }

    auto saplingKey = ufvk.GetSaplingKey();
    if (saplingKey.has_value()) {
        result.saplingKey = saplingKey.value();
    }

    return result;
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
        const auto& tkey = transparentKey.value();
        auto childIndex = j.ToTransparentChildIndex();
        if (!childIndex.has_value()) return std::nullopt;

        CPubKey externalKey;
        ChainCode externalChainCode;
        if (!tkey.GetPubKey().Derive(externalKey, externalChainCode, 0, tkey.GetChainCode())) {
            return std::nullopt;
        }

        CPubKey childKey;
        ChainCode childChainCode;
        if (externalKey.Derive(childKey, childChainCode, childIndex.value(), externalChainCode)) {
            ua.AddReceiver(childKey.GetID());
        } else {
            return std::nullopt;
        }
    }

    return ua;
}

std::pair<UnifiedAddress, diversifier_index_t> ZcashdUnifiedFullViewingKey::FindAddress(diversifier_index_t j) const {
    auto addr = Address(j);
    while (!addr.has_value()) {
        if (!j.increment())
            throw std::runtime_error(std::string(__func__) + ": diversifier index overflow.");;
        addr = Address(j);
    }
    return std::make_pair(addr.value(), j);
}
