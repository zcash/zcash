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

bool libzcash::HasShielded(const std::set<ReceiverType>& receiverTypes) {
    auto has_shielded = [](ReceiverType r) {
        // TODO: update this as support for new shielded protocols is added.
        return r == ReceiverType::Sapling;
    };
    return std::find_if(receiverTypes.begin(), receiverTypes.end(), has_shielded) != receiverTypes.end();
}

bool libzcash::HasTransparent(const std::set<ReceiverType>& receiverTypes) {
    auto has_transparent = [](ReceiverType r) {
        // TODO: update this as support for new transparent protocols is added.
        return r == ReceiverType::P2PKH || r == ReceiverType::P2SH;
    };
    return std::find_if(receiverTypes.begin(), receiverTypes.end(), has_transparent) != receiverTypes.end();
}

std::optional<ZcashdUnifiedSpendingKey> ZcashdUnifiedSpendingKey::ForAccount(
        const HDSeed& seed,
        uint32_t bip44CoinType,
        AccountId accountId) {
    ZcashdUnifiedSpendingKey usk;

    auto transparentKey = DeriveBip44TransparentAccountKey(seed, bip44CoinType, accountId);
    if (!transparentKey.has_value()) return std::nullopt;
    usk.transparentKey = transparentKey.value().first;

    auto saplingKey = SaplingExtendedSpendingKey::ForAccount(seed, bip44CoinType, accountId);
    usk.saplingKey = saplingKey.first;

    return usk;
}

UnifiedFullViewingKey ZcashdUnifiedSpendingKey::ToFullViewingKey() const {
    UnifiedFullViewingKeyBuilder builder;

    auto extPubKey = transparentKey.Neuter();
    builder.AddTransparentKey(CChainablePubKey::FromParts(extPubKey.chaincode, extPubKey.pubkey).value());
    builder.AddSaplingKey(saplingKey.ToXFVK());

    // This call to .value() is safe as ZcashdUnifiedSpendingKey values are always
    // constructed to contain all required components.
    return builder.build().value();
}

ZcashdUnifiedFullViewingKey ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(
        const KeyConstants& keyConstants,
        const UnifiedFullViewingKey& ufvk) {
    ZcashdUnifiedFullViewingKey result;
    result.keyId = ufvk.GetKeyID(keyConstants);

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

std::optional<UnifiedAddress> ZcashdUnifiedFullViewingKey::Address(
        const diversifier_index_t& j,
        const std::set<ReceiverType>& receiverTypes) const
{
    if (!HasShielded(receiverTypes)) {
        throw std::runtime_error("Unified addresses must include a shielded receiver.");
    }

    UnifiedAddress ua;
    if (saplingKey.has_value() && receiverTypes.count(ReceiverType::Sapling) > 0) {
        auto saplingAddress = saplingKey.value().Address(j);
        if (saplingAddress.has_value()) {
            ua.AddReceiver(saplingAddress.value());
        } else {
            return std::nullopt;
        }
    }

    if (transparentKey.has_value() && receiverTypes.count(ReceiverType::P2PKH) > 0) {
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

std::optional<std::pair<UnifiedAddress, diversifier_index_t>> ZcashdUnifiedFullViewingKey::FindAddress(
        const diversifier_index_t& j,
        const std::set<ReceiverType>& receiverTypes) const {
    diversifier_index_t j0(j);
    bool hasTransparent = HasTransparent(receiverTypes);
    auto addr = Address(j0, receiverTypes);
    while (!addr.has_value()) {
        if (!j0.increment() || (hasTransparent && !j0.ToTransparentChildIndex().has_value()))
            return std::nullopt;
        addr = Address(j0, receiverTypes);
    }
    return std::make_pair(addr.value(), j0);
}

std::optional<std::pair<UnifiedAddress, diversifier_index_t>> ZcashdUnifiedFullViewingKey::FindAddress(
        const diversifier_index_t& j) const {
    return FindAddress(j, {ReceiverType::P2PKH, ReceiverType::Sapling});
}

RecipientAddress ZcashdUnifiedFullViewingKey::GetChangeAddress(const std::set<ChangeType>& changeOptions) const {
    throw std::runtime_error("TODO");
}
