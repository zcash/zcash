// Copyright (c) 2016-2020 The Zcash developers
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
class TxValues;

class SendManyRecipient {
public:
    std::string address;
    CAmount amount;
    std::string memo;

    SendManyRecipient(std::string address_, CAmount amount_, std::string memo_) :
        address(address_), amount(amount_), memo(memo_) {}
};

class SendManyInputJSOP {
public:
    JSOutPoint point;
    SproutNote note;
    CAmount amount;

    SendManyInputJSOP(JSOutPoint point_, SproutNote note_, CAmount amount_) :
        point(point_), note(note_), amount(amount_) {}
};

// Package of info which is passed to perform_joinsplit methods.
struct AsyncJoinSplitInfo
{
    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    std::vector<SproutNote> notes;
    CAmount vpub_old = 0;
    CAmount vpub_new = 0;
};

// A struct to help us track the witness and anchor for a given JSOutPoint
struct WitnessAnchorData {
	std::optional<SproutWitness> witness;
	uint256 anchor;
};

class AsyncRPCOperation_sendmany : public AsyncRPCOperation {
public:
    AsyncRPCOperation_sendmany(
        std::optional<TransactionBuilder> builder,
        CMutableTransaction contextualTx,
        std::string fromAddress,
        std::vector<SendManyRecipient> tOutputs,
        std::vector<SendManyRecipient> zOutputs,
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

    bool testmode = false;  // Set to true to disable sending txs and generating proofs

    bool paymentDisclosureMode = false; // Set to true to save esk for encrypted notes in payment disclosure database.

private:
    friend class TEST_FRIEND_AsyncRPCOperation_sendmany;    // class for unit testing

    UniValue contextinfo_;     // optional data to include in return value from getStatus()

    bool isUsingBuilder_; // Indicates that no Sprout addresses are involved
    uint32_t consensusBranchId_;
    CAmount fee_;
    int mindepth_;
    std::string fromaddress_;
    bool useanyutxo_;
    bool isfromtaddr_;
    bool isfromzaddr_;
    CTxDestination fromtaddr_;
    PaymentAddress frompaymentaddress_;
    SpendingKey spendingkey_;

    Ed25519VerificationKey joinSplitPubKey_;
    Ed25519SigningKey joinSplitPrivKey_;

    // The key is the result string from calling JSOutPoint::ToString()
    std::unordered_map<std::string, WitnessAnchorData> jsopWitnessAnchorMap;

    std::vector<SendManyRecipient> t_outputs_;
    std::vector<SendManyRecipient> z_outputs_;
    std::vector<COutput> t_inputs_;
    std::vector<SendManyInputJSOP> z_sprout_inputs_;
    std::vector<SaplingNoteEntry> z_sapling_inputs_;

    TransactionBuilder builder_;
    CTransaction tx_;

    void add_taddr_change_output_to_tx(CReserveKey& keyChange, CAmount amount);
    void add_taddr_outputs_to_tx();
    bool find_unspent_notes();
    bool find_utxos(bool fAcceptCoinbase, TxValues& txValues);
    // Load transparent inputs into the transaction or the transactionBuilder (in case of have it)
    bool load_inputs(TxValues& txValues);
    std::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s);
    bool main_impl();

    // JoinSplit without any input notes to spend
    UniValue perform_joinsplit(AsyncJoinSplitInfo &);

    // JoinSplit with input notes to spend (JSOutPoints))
    UniValue perform_joinsplit(AsyncJoinSplitInfo &, std::vector<JSOutPoint> & );

    // JoinSplit where you have the witnesses and anchor
    UniValue perform_joinsplit(
        AsyncJoinSplitInfo & info,
        std::vector<std::optional < SproutWitness>> witnesses,
        uint256 anchor);

    // payment disclosure!
    std::vector<PaymentDisclosureKeyInfo> paymentDisclosureData_;
};


// To test private methods, a friend class can act as a proxy
class TEST_FRIEND_AsyncRPCOperation_sendmany {
public:
    std::shared_ptr<AsyncRPCOperation_sendmany> delegate;

    TEST_FRIEND_AsyncRPCOperation_sendmany(std::shared_ptr<AsyncRPCOperation_sendmany> ptr) : delegate(ptr) {}

    CTransaction getTx() {
        return delegate->tx_;
    }

    void setTx(CTransaction tx) {
        delegate->tx_ = tx;
    }

    // Delegated methods

    void add_taddr_change_output_to_tx(CReserveKey& keyChange, CAmount amount) {
        delegate->add_taddr_change_output_to_tx(keyChange, amount);
    }

    void add_taddr_outputs_to_tx() {
        delegate->add_taddr_outputs_to_tx();
    }

    bool find_unspent_notes() {
        return delegate->find_unspent_notes();
    }

    std::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s) {
        return delegate->get_memo_from_hex_string(s);
    }

    bool main_impl() {
        return delegate->main_impl();
    }

    UniValue perform_joinsplit(AsyncJoinSplitInfo &info) {
        return delegate->perform_joinsplit(info);
    }

    UniValue perform_joinsplit(AsyncJoinSplitInfo &info, std::vector<JSOutPoint> &v ) {
        return delegate->perform_joinsplit(info, v);
    }

    UniValue perform_joinsplit(
        AsyncJoinSplitInfo & info,
        std::vector<std::optional < SproutWitness>> witnesses,
        uint256 anchor)
    {
        return delegate->perform_joinsplit(info, witnesses, anchor);
    }

    void set_state(OperationStatus state) {
        delegate->state_.store(state);
    }
};


#endif // ZCASH_WALLET_ASYNCRPCOPERATION_SENDMANY_H
