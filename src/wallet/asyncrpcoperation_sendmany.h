// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_ASYNCRPCOPERATION_SENDMANY_H
#define ZCASH_WALLET_ASYNCRPCOPERATION_SENDMANY_H

#include "asyncrpcoperation.h"
#include "amount.h"
#include "primitives/transaction.h"
#include "transaction_builder.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Address.hpp"
#include "wallet.h"
#include "wallet/paymentdisclosure.h"

#include <array>
#include <optional>
#include <unordered_map>
#include <tuple>

#include <univalue.h>

#include <rust/ed25519/types.h>

using namespace libzcash;

class FromAnyTaddr {
public:
    friend bool operator==(const FromAnyTaddr &a, const FromAnyTaddr &b) { return true; }
};

typedef std::variant<FromAnyTaddr, PaymentAddress> PaymentSource;

class SendManyRecipient {
public:
    PaymentAddress address;
    CAmount amount;
    std::optional<std::string> memo;

    SendManyRecipient(PaymentAddress address_, CAmount amount_, std::optional<std::string> memo_) :
        address(address_), amount(amount_), memo(memo_) {}
};

class SpendableInputs {
public:
    std::vector<COutput> utxos;
    std::vector<SproutNoteEntry> sproutNoteEntries;
    std::vector<SaplingNoteEntry> saplingNoteEntries;

    /**
     * Selectively discard notes that are not required to obtain the desired
     * amount. Returns `false` if the available inputs do not add up to the
     * desired amount.
     */
    bool LimitToAmount(CAmount amount);

    /**
     * Compute the total ZEC amount of spendable inputs.
     */
    CAmount Total() const {
        CAmount result = 0;
        for (const auto& t : utxos) {
            result += t.Value();
        }
        for (const auto& t : sproutNoteEntries) {
            result += t.note.value();
        }
        for (const auto& t : saplingNoteEntries) {
            result += t.note.value();
        }
        return result;
    }

    /**
     * Return whether or not the set of selected UTXOs contains
     * coinbase outputs.
     */
    bool HasTransparentCoinbase() const;

    /**
     * List spendable inputs in zrpcunsafe log entries.
     */
    void LogInputs(const AsyncRPCOperationId& id) const;
};

class AsyncRPCOperation_sendmany : public AsyncRPCOperation {
public:
    AsyncRPCOperation_sendmany(
        TransactionBuilder builder,
        PaymentSource paymentSource,
        std::vector<SendManyRecipient> recipients,
        int minDepth,
        CAmount fee = DEFAULT_FEE,
        UniValue contextInfo = NullUniValue);
    virtual ~AsyncRPCOperation_sendmany();

    // We don't want to be copied or moved around
    AsyncRPCOperation_sendmany(AsyncRPCOperation_sendmany const&) = delete;             // Copy construct
    AsyncRPCOperation_sendmany(AsyncRPCOperation_sendmany&&) = delete;                  // Move construct
    AsyncRPCOperation_sendmany& operator=(AsyncRPCOperation_sendmany const&) = delete;  // Copy assign
    AsyncRPCOperation_sendmany& operator=(AsyncRPCOperation_sendmany &&) = delete;      // Move assign

    virtual void main();

    virtual UniValue getStatus() const;

    bool testmode{false};  // Set to true to disable sending txs and generating proofs

private:
    friend class TEST_FRIEND_AsyncRPCOperation_sendmany;    // class for unit testing

    TransactionBuilder builder_;
    PaymentSource paymentSource_;
    std::vector<SendManyRecipient> recipients_;
    int mindepth_{1};
    CAmount fee_;
    UniValue contextinfo_;     // optional data to include in return value from getStatus()

    bool isfromtaddr_{false};
    bool isfromzaddr_{false};

    SpendableInputs FindSpendableInputs(bool fAcceptCoinbase);

    std::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s);

    uint256 main_impl();
};


// To test private methods, a friend class can act as a proxy
class TEST_FRIEND_AsyncRPCOperation_sendmany {
public:
    std::shared_ptr<AsyncRPCOperation_sendmany> delegate;

    TEST_FRIEND_AsyncRPCOperation_sendmany(std::shared_ptr<AsyncRPCOperation_sendmany> ptr) : delegate(ptr) {}

    std::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s) {
        return delegate->get_memo_from_hex_string(s);
    }

    uint256 main_impl() {
        return delegate->main_impl();
    }

    void set_state(OperationStatus state) {
        delegate->state_.store(state);
    }
};


#endif // ZCASH_WALLET_ASYNCRPCOPERATION_SENDMANY_H
