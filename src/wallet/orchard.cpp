// Copyright (c) 2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "wallet/orchard.h"

std::vector<orchard::SpendInfo> OrchardWallet::GetSpendInfo(
    const std::vector<OrchardNoteMetadata>& noteMetadata) const
{
    std::vector<orchard::SpendInfo> result;
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

            result.push_back(std::move(spendInfo));
        }
    }
    return result;
}
