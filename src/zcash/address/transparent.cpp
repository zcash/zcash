// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <rust/unified_keys.h>

#include "streams.h"
#include "transparent.h"

namespace libzcash {
namespace transparent {

std::optional<CPubKey> AccountPubKey::DeriveExternal(uint32_t addrIndex) const {
    auto externalKey = pubkey.Derive(0);
    if (!externalKey.has_value()) return std::nullopt;
    auto childKey = externalKey.value().Derive(addrIndex);
    if (!childKey.has_value()) return std::nullopt;
    return childKey.value().GetPubKey();
}

std::optional<CPubKey> AccountPubKey::DeriveInternal(uint32_t addrIndex) const {
    auto internalKey = pubkey.Derive(1);
    if (!internalKey.has_value()) return std::nullopt;
    auto childKey = internalKey.value().Derive(addrIndex);
    if (!childKey.has_value()) return std::nullopt;
    return childKey.value().GetPubKey();
}

std::optional<CKeyID> AccountPubKey::GetChangeAddress(const diversifier_index_t& j) const {
    auto childIndex = j.ToTransparentChildIndex();
    if (!childIndex.has_value()) return std::nullopt;

    auto changeKey = this->DeriveInternal(childIndex.value());
    if (!changeKey.has_value())  return std::nullopt;

    return changeKey.value().GetID();
}

std::pair<uint256, uint256> AccountPubKey::GetOVKsForShielding() const {
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pubkey;
    assert(ss.size() == 65);
    CSerializeData ss_bytes(ss.begin(), ss.end());

    uint256 internalOVK;
    uint256 externalOVK;

    assert(transparent_key_ovks(
        reinterpret_cast<unsigned char*>(ss_bytes.data()),
        internalOVK.begin(),
        externalOVK.begin()));

    return std::make_pair(internalOVK, externalOVK);
}

std::optional<std::pair<CKeyID, diversifier_index_t>> AccountPubKey::FindChangeAddress(diversifier_index_t j) const {
    while (true) {
        auto childIndex = j.ToTransparentChildIndex();
        if (!childIndex.has_value()) return std::nullopt;

        auto changeKey = this->DeriveInternal(childIndex.value());
        if (changeKey.has_value()) {
            return std::make_pair(changeKey.value().GetID(), j);
        } else {
            j.increment();
        }
    }
    return std::nullopt;
}

std::optional<AccountKey> AccountKey::ForAccount(
        const HDSeed& seed,
        uint32_t bip44CoinType,
        AccountId accountId) {
    auto rawSeed = seed.RawSeed();
    auto m = CExtKey::Master(rawSeed.data(), rawSeed.size());
    if (!m.has_value()) return std::nullopt;

    // We use a fixed keypath scheme of m/44'/coin_type'/account'
    // Derive m/44'
    auto m_44h = m.value().Derive(44 | HARDENED_KEY_LIMIT);
    if (!m_44h.has_value()) return std::nullopt;

    // Derive m/44'/coin_type'
    auto m_44h_cth = m_44h.value().Derive(bip44CoinType | HARDENED_KEY_LIMIT);
    if (!m_44h_cth.has_value()) return std::nullopt;

    // Derive m/44'/coin_type'/account_id'
    auto accountKeyOpt = m_44h_cth.value().Derive(accountId | HARDENED_KEY_LIMIT);
    if (!accountKeyOpt.has_value()) return std::nullopt;

    auto accountKey = accountKeyOpt.value();
    auto external = accountKey.Derive(0);
    auto internal = accountKey.Derive(1);

    if (!(external.has_value() && internal.has_value())) return std::nullopt;

    return AccountKey(accountKey, external.value(), internal.value());
}

std::optional<CKey> AccountKey::DeriveExternalSpendingKey(uint32_t addrIndex) const {
    auto childKey = external.Derive(addrIndex);
    if (!childKey.has_value()) return std::nullopt;

    return childKey.value().key;
}

std::optional<CKey> AccountKey::DeriveInternalSpendingKey(uint32_t addrIndex) const {
    auto childKey = internal.Derive(addrIndex);
    if (!childKey.has_value()) return std::nullopt;

    return childKey.value().key;
}

AccountPubKey AccountKey::ToAccountPubKey() const {
    // The .value() call is safe here because we never derive
    // non-compressed public keys.
    return accountKey.Neuter().ToChainablePubKey().value();
}

} //namespace transparent
} //namespace libzcash
