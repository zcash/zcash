// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ASYNCRPCOPERATION_SHIELDCOINBASE_H
#define ASYNCRPCOPERATION_SHIELDCOINBASE_H

#include "asyncrpcoperation.h"
#include "amount.h"
#include "base58.h"
#include "primitives/transaction.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Address.hpp"
#include "wallet.h"

#include <unordered_map>
#include <tuple>

#include <univalue.h>

#include "paymentdisclosure.h"

// Default transaction fee if caller does not specify one.
#define SHIELD_COINBASE_DEFAULT_MINERS_FEE   10000

using namespace libzcash;

struct ShieldCoinbaseUTXO {
    uint256 txid;
    int vout;
    CAmount amount;
};

// Package of info which is passed to perform_joinsplit methods.
struct ShieldCoinbaseJSInfo
{
    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    CAmount vpub_old = 0;
    CAmount vpub_new = 0;
};

class AsyncRPCOperation_shieldcoinbase : public AsyncRPCOperation {
public:
    AsyncRPCOperation_shieldcoinbase(CMutableTransaction contextualTx, std::vector<ShieldCoinbaseUTXO> inputs, std::string toAddress, CAmount fee = SHIELD_COINBASE_DEFAULT_MINERS_FEE, UniValue contextInfo = NullUniValue);
    virtual ~AsyncRPCOperation_shieldcoinbase();

    // We don't want to be copied or moved around
    AsyncRPCOperation_shieldcoinbase(AsyncRPCOperation_shieldcoinbase const&) = delete;             // Copy construct
    AsyncRPCOperation_shieldcoinbase(AsyncRPCOperation_shieldcoinbase&&) = delete;                  // Move construct
    AsyncRPCOperation_shieldcoinbase& operator=(AsyncRPCOperation_shieldcoinbase const&) = delete;  // Copy assign
    AsyncRPCOperation_shieldcoinbase& operator=(AsyncRPCOperation_shieldcoinbase &&) = delete;      // Move assign

    virtual void main();

    virtual UniValue getStatus() const;

    bool testmode = false;  // Set to true to disable sending txs and generating proofs

    bool paymentDisclosureMode = false; // Set to true to save esk for encrypted notes in payment disclosure database.

private:
    friend class TEST_FRIEND_AsyncRPCOperation_shieldcoinbase;    // class for unit testing

    UniValue contextinfo_;     // optional data to include in return value from getStatus()

    CAmount fee_;
    PaymentAddress tozaddr_;

    uint256 joinSplitPubKey_;
    unsigned char joinSplitPrivKey_[crypto_sign_SECRETKEYBYTES];

    std::vector<ShieldCoinbaseUTXO> inputs_;

    CTransaction tx_;

    bool main_impl();

    // JoinSplit without any input notes to spend
    UniValue perform_joinsplit(ShieldCoinbaseJSInfo &);

    void sign_send_raw_transaction(UniValue obj);     // throws exception if there was an error

    void lock_utxos();

    void unlock_utxos();

    // payment disclosure!
    std::vector<PaymentDisclosureKeyInfo> paymentDisclosureData_;
};


// To test private methods, a friend class can act as a proxy
class TEST_FRIEND_AsyncRPCOperation_shieldcoinbase {
public:
    std::shared_ptr<AsyncRPCOperation_shieldcoinbase> delegate;

    TEST_FRIEND_AsyncRPCOperation_shieldcoinbase(std::shared_ptr<AsyncRPCOperation_shieldcoinbase> ptr) : delegate(ptr) {}

    CTransaction getTx() {
        return delegate->tx_;
    }

    void setTx(CTransaction tx) {
        delegate->tx_ = tx;
    }

    // Delegated methods

    bool main_impl() {
        return delegate->main_impl();
    }

    UniValue perform_joinsplit(ShieldCoinbaseJSInfo &info) {
        return delegate->perform_joinsplit(info);
    }

    void sign_send_raw_transaction(UniValue obj) {
        delegate->sign_send_raw_transaction(obj);
    }

    void set_state(OperationStatus state) {
        delegate->state_.store(state);
    }
};


#endif /* ASYNCRPCOPERATION_SHIELDCOINBASE_H */

