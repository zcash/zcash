// Copyright (c) 2017-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_ASYNCRPCOPERATION_MERGETOADDRESS_H
#define ZCASH_WALLET_ASYNCRPCOPERATION_MERGETOADDRESS_H

#include "amount.h"
#include "asyncrpcoperation.h"
#include "primitives/transaction.h"
#include "transaction_builder.h"
#include "uint256.h"
#include "wallet.h"
#include "wallet/paymentdisclosure.h"
#include "wallet/wallet_tx_builder.h"
#include "zcash/Address.hpp"
#include "zcash/JoinSplit.hpp"
#include "zcash/memo.h"

#include <array>
#include <optional>
#include <tuple>
#include <unordered_map>

#include <univalue.h>

#include <rust/ed25519.h>

using namespace libzcash;

// Package of info which is passed to perform_joinsplit methods.
struct MergeToAddressJSInfo {
    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    std::vector<SproutNote> notes;
    std::vector<SproutSpendingKey> zkeys;
    CAmount vpub_old = 0;
    CAmount vpub_new = 0;
};

// A struct to help us track the witness and anchor for a given JSOutPoint
struct MergeToAddressWitnessAnchorData {
    std::optional<SproutWitness> witness;
    uint256 anchor;
};

class AsyncRPCOperation_mergetoaddress : public AsyncRPCOperation
{
public:
    AsyncRPCOperation_mergetoaddress(
            CWallet& wallet,
            TransactionStrategy strategy,
            TransactionEffects effects,
            UniValue contextInfo = NullUniValue);
    virtual ~AsyncRPCOperation_mergetoaddress();

    // We don't want to be copied or moved around
    AsyncRPCOperation_mergetoaddress(AsyncRPCOperation_mergetoaddress const&) = delete;            // Copy construct
    AsyncRPCOperation_mergetoaddress(AsyncRPCOperation_mergetoaddress&&) = delete;                 // Move construct
    AsyncRPCOperation_mergetoaddress& operator=(AsyncRPCOperation_mergetoaddress const&) = delete; // Copy assign
    AsyncRPCOperation_mergetoaddress& operator=(AsyncRPCOperation_mergetoaddress&&) = delete;      // Move assign

    virtual void main();

    virtual UniValue getStatus() const;

    /// Set to true to disable sending txs and generating proofs
    bool testmode = false;

private:
    const TransactionStrategy strategy_;

    const TransactionEffects effects_;

    /// optional data to include in return value from getStatus()
    UniValue contextinfo_;
};

#endif // ZCASH_WALLET_ASYNCRPCOPERATION_MERGETOADDRESS_H
