// Copyright (c) 2017-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_ASYNCRPCOPERATION_SHIELDCOINBASE_H
#define ZCASH_WALLET_ASYNCRPCOPERATION_SHIELDCOINBASE_H

#include "asyncrpcoperation.h"
#include "amount.h"
#include "primitives/transaction.h"
#include "transaction_builder.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Address.hpp"
#include "wallet.h"
#include "wallet/wallet_tx_builder.h"

#include <unordered_map>
#include <tuple>

#include <univalue.h>

#include <rust/ed25519.h>

using namespace libzcash;

/**
When estimating the number of coinbase utxos we can shield in a single transaction:
1. An Orchard receiver is 9165 bytes.
2. Transaction overhead ~ 100 bytes
3. Spending a typical P2PKH is >=148 bytes, as defined in CTXIN_SPEND_P2PKH_SIZE.
4. Spending a multi-sig P2SH address can vary greatly:
   https://github.com/bitcoin/bitcoin/blob/c3ad56f4e0b587d8d763af03d743fdfc2d180c9b/src/main.cpp#L517
   In real-world coinbase utxos, we consider a 3-of-3 multisig, where the size is roughly:
    (3*(33+1))+3 = 105 byte redeem script
    105 + 1 + 3*(73+1) = 328 bytes of scriptSig, rounded up to 400 based on testnet experiments.
*/
#define CTXIN_SPEND_P2SH_SIZE 400

#define SHIELD_COINBASE_DEFAULT_LIMIT 50

// transaction.h comment: spending taddr output requires CTxIn >= 148 bytes and
// typical taddr txout is 34 bytes
#define CTXIN_SPEND_P2PKH_SIZE   148
#define CTXOUT_REGULAR_SIZE     34

class Remaining {
public:
    Remaining(size_t utxoCounter, size_t numUtxos, CAmount remainingValue, CAmount shieldingValue):
        utxoCounter(utxoCounter), numUtxos(numUtxos), remainingValue(remainingValue), shieldingValue(shieldingValue) { }

    size_t utxoCounter;
    size_t numUtxos;
    CAmount remainingValue;
    CAmount shieldingValue;
};

class AsyncRPCOperation_shieldcoinbase : public AsyncRPCOperation {
public:
    AsyncRPCOperation_shieldcoinbase(
        WalletTxBuilder builder,
        ZTXOSelector ztxoSelector,
        PaymentAddress toAddress,
        std::optional<Memo> memo,
        TransactionStrategy strategy,
        int nUTXOLimit,
        std::optional<CAmount> fee,
        UniValue contextInfo = NullUniValue);
    virtual ~AsyncRPCOperation_shieldcoinbase();

    // We don't want to be copied or moved around
    AsyncRPCOperation_shieldcoinbase(AsyncRPCOperation_shieldcoinbase const&) = delete;             // Copy construct
    AsyncRPCOperation_shieldcoinbase(AsyncRPCOperation_shieldcoinbase&&) = delete;                  // Move construct
    AsyncRPCOperation_shieldcoinbase& operator=(AsyncRPCOperation_shieldcoinbase const&) = delete;  // Copy assign
    AsyncRPCOperation_shieldcoinbase& operator=(AsyncRPCOperation_shieldcoinbase &&) = delete;      // Move assign

    Remaining prepare(CWallet& wallet);

    virtual void main();

    virtual UniValue getStatus() const;

    bool testmode{false};  // Set to true to disable sending txs and generating proofs

private:
    friend class TEST_FRIEND_AsyncRPCOperation_shieldcoinbase;    // class for unit testing

    WalletTxBuilder builder_;
    ZTXOSelector ztxoSelector_;
    PaymentAddress toAddress_;
    std::optional<Memo> memo_;
    TransactionStrategy strategy_;
    int nUTXOLimit_;
    std::optional<CAmount> fee_;
    std::optional<TransactionEffects> effects_;

    UniValue contextinfo_;     // optional data to include in return value from getStatus()

    uint256 main_impl(CWallet& wallet);
};

// To test private methods, a friend class can act as a proxy
class TEST_FRIEND_AsyncRPCOperation_shieldcoinbase {
public:
    std::shared_ptr<AsyncRPCOperation_shieldcoinbase> delegate;

    TEST_FRIEND_AsyncRPCOperation_shieldcoinbase(std::shared_ptr<AsyncRPCOperation_shieldcoinbase> ptr) : delegate(ptr) {}

    // Delegated methods

    uint256 main_impl(CWallet& wallet) {
        return delegate->main_impl(wallet);
    }

    void set_state(OperationStatus state) {
        delegate->state_.store(state);
    }
};


#endif // ZCASH_WALLET_ASYNCRPCOPERATION_SHIELDCOINBASE_H
