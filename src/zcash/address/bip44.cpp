// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "bip44.h"

HDKeyPath libzcash::Bip44TransparentAccountKeyPath(uint32_t bip44CoinType, libzcash::AccountId accountId) {
    return "m/44'/" + std::to_string(bip44CoinType) + "'/" + std::to_string(accountId) + "'";
}

std::optional<std::pair<CExtKey, HDKeyPath>> libzcash::DeriveBip44TransparentAccountKey(const HDSeed& seed, uint32_t bip44CoinType, libzcash::AccountId accountId) {
    auto rawSeed = seed.RawSeed();
    auto m = CExtKey::Master(rawSeed.data(), rawSeed.size());

    // We use a fixed keypath scheme of m/44'/coin_type'/account'
    // Derive m/44'
    auto m_44h = m.Derive(44 | HARDENED_KEY_LIMIT);
    if (!m_44h.has_value()) return std::nullopt;

    // Derive m/44'/coin_type'
    auto m_44h_cth = m_44h.value().Derive(bip44CoinType | HARDENED_KEY_LIMIT);
    if (!m_44h_cth.has_value()) return std::nullopt;

    // Derive m/44'/coin_type'/account_id'
    auto result = m_44h_cth.value().Derive(accountId | HARDENED_KEY_LIMIT);
    if (!result.has_value()) return std::nullopt;

    auto hdKeypath = libzcash::Bip44TransparentAccountKeyPath(bip44CoinType, accountId);

    return std::make_pair(result.value(), hdKeypath);
}

std::optional<libzcash::Bip44AccountChains> libzcash::Bip44AccountChains::ForAccount(
        const HDSeed& seed,
        uint32_t bip44CoinType,
        libzcash::AccountId accountId) {
    auto accountKeyOpt = DeriveBip44TransparentAccountKey(seed, bip44CoinType, accountId);
    if (!accountKeyOpt.has_value()) return std::nullopt;

    auto accountKey = accountKeyOpt.value();
    auto external = accountKey.first.Derive(0);
    auto internal = accountKey.first.Derive(1);

    if (!(external.has_value() && internal.has_value())) return std::nullopt;

    return Bip44AccountChains(seed.Fingerprint(), bip44CoinType, accountId, external.value(), internal.value());
}

std::optional<std::pair<CKey, HDKeyPath>> libzcash::Bip44AccountChains::DeriveExternal(uint32_t addrIndex) {
    auto childKey = external.Derive(addrIndex);
    if (!childKey.has_value()) return std::nullopt;

    auto hdKeypath = Bip44TransparentAccountKeyPath(bip44CoinType, accountId) + "/0/" + std::to_string(addrIndex);

    return std::make_pair(childKey.value().key, hdKeypath);
}

std::optional<std::pair<CKey, HDKeyPath>> libzcash::Bip44AccountChains::DeriveInternal(uint32_t addrIndex) {
    auto childKey = internal.Derive(addrIndex);
    if (!childKey.has_value()) return std::nullopt;

    auto hdKeypath = Bip44TransparentAccountKeyPath(bip44CoinType, accountId) + "/1/" + std::to_string(addrIndex);

    return std::make_pair(childKey.value().key, hdKeypath);
}

