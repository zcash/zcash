// Copyright (c) 2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "wallet/orchard.h"

std::optional<libzcash::OrchardSpendingKey> OrchardWallet::GetSpendingKeyForAddress(
    const libzcash::OrchardRawAddress& addr) const
{
    auto skPtr = orchard_wallet_get_spending_key_for_address(inner.get(), addr.inner.get());
    if (skPtr == nullptr) return std::nullopt;
    return libzcash::OrchardSpendingKey(skPtr);
}

std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> OrchardWallet::GetSpendInfo(
    const std::vector<OrchardNoteMetadata>& noteMetadata) const
{
    std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> result;
    for (const auto& note : noteMetadata) {
        auto pSpendInfo = orchard_wallet_get_spend_info(
            inner.get(),
            note.GetOutPoint().hash.begin(),
            note.GetOutPoint().n);
        if (pSpendInfo == nullptr) {
            throw std::logic_error("Called OrchardWallet::GetSpendInfo with unknown outpoint");
        } else {
            auto spendInfo = orchard::SpendInfo(
                pSpendInfo,
                note.GetAddress(),
                note.GetNoteValue());

            auto sk = GetSpendingKeyForAddress(note.GetAddress());
            if (sk.has_value()) {
                result.push_back(std::pair(std::move(sk.value()), std::move(spendInfo)));
            } else {
                throw std::logic_error("Unknown spending key for given outpoint");
            }
        }
    }
    return result;
}
