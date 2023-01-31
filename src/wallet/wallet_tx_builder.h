// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_WALLET_TX_BUILDER_H
#define ZCASH_WALLET_WALLET_TX_BUILDER_H

#include "wallet/memo.h"

using namespace libzcash;

/**
 * A payment that has been resolved to send to a specific
 * recipient address in a single pool.
 */
class ResolvedPayment : public RecipientMapping {
public:
    CAmount amount;
    std::optional<Memo> memo;

    ResolvedPayment(
            std::optional<libzcash::UnifiedAddress> ua,
            libzcash::RecipientAddress address,
            CAmount amount,
            std::optional<Memo> memo) :
        RecipientMapping(ua, address), amount(amount), memo(memo) {}
};
#endif
