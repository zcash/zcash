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

#include <array>
#include <optional>
#include <tuple>
#include <unordered_map>

#include <univalue.h>

#include <rust/ed25519.h>

using namespace libzcash;

// A recipient is a tuple of address, memo (optional if zaddr)
typedef std::pair<libzcash::PaymentAddress, std::optional<Memo>> MergeToAddressRecipient;

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
        WalletTxBuilder builder,
        ZTXOSelector ztxoSelector,
        SpendableInputs allInputs,
        MergeToAddressRecipient recipient,
        TransactionStrategy strategy,
        CAmount fee = DEFAULT_FEE,
        UniValue contextInfo = NullUniValue);
    virtual ~AsyncRPCOperation_mergetoaddress();

    // We don't want to be copied or moved around
    AsyncRPCOperation_mergetoaddress(AsyncRPCOperation_mergetoaddress const&) = delete;            // Copy construct
    AsyncRPCOperation_mergetoaddress(AsyncRPCOperation_mergetoaddress&&) = delete;                 // Move construct
    AsyncRPCOperation_mergetoaddress& operator=(AsyncRPCOperation_mergetoaddress const&) = delete; // Copy assign
    AsyncRPCOperation_mergetoaddress& operator=(AsyncRPCOperation_mergetoaddress&&) = delete;      // Move assign

    tl::expected<void, InputSelectionError> prepare(const CChainParams& chainparams, CWallet& wallet);

    virtual void main();

    virtual UniValue getStatus() const;

    bool testmode = false; // Set to true to disable sending txs and generating proofs

    bool paymentDisclosureMode = false; // Set to true to save esk for encrypted notes in payment disclosure database.

private:
    friend class TEST_FRIEND_AsyncRPCOperation_mergetoaddress; // class for unit testing

    UniValue contextinfo_; // optional data to include in return value from getStatus()

    uint32_t consensusBranchId_;
    CAmount fee_;
    int mindepth_;
    bool isToTaddr_;
    bool isToZaddr_;
    PaymentAddress toPaymentAddress_;
    std::optional<Memo> memo_;

    ed25519::VerificationKey joinSplitPubKey_;
    ed25519::SigningKey joinSplitPrivKey_;

    // The key is the result string from calling JSOutPoint::ToString()
    std::unordered_map<std::string, MergeToAddressWitnessAnchorData> jsopWitnessAnchorMap;

    WalletTxBuilder builder_;
    ZTXOSelector ztxoSelector_;
    SpendableInputs allInputs_;
    TransactionStrategy strategy_;

    std::optional<TransactionEffects> effects_;

    uint256 main_impl(CWallet& wallet, const CChainParams& chainparams);

    // payment disclosure!
    std::vector<PaymentDisclosureKeyInfo> paymentDisclosureData_;
};


// To test private methods, a friend class can act as a proxy
class TEST_FRIEND_AsyncRPCOperation_mergetoaddress
{
public:
    std::shared_ptr<AsyncRPCOperation_mergetoaddress> delegate;

    TEST_FRIEND_AsyncRPCOperation_mergetoaddress(std::shared_ptr<AsyncRPCOperation_mergetoaddress> ptr) : delegate(ptr) {}

    // Delegated methods

    uint256 main_impl(CWallet& wallet, const CChainParams& chainparams)
    {
        return delegate->main_impl(wallet, chainparams);
    }

    void set_state(OperationStatus state)
    {
        delegate->state_.store(state);
    }
};


#endif // ZCASH_WALLET_ASYNCRPCOPERATION_MERGETOADDRESS_H
