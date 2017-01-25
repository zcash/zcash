// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ASYNCRPCOPERATION_SENDMANY_H
#define ASYNCRPCOPERATION_SENDMANY_H

#include "asyncrpcoperation.h"
#include "amount.h"
#include "base58.h"
#include "primitives/transaction.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Address.hpp"
#include "json/json_spirit_value.h"
#include "wallet.h"

#include <unordered_map>
#include <tuple>

// Default transaction fee if caller does not specify one.
#define ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE   10000

using namespace libzcash;
using namespace json_spirit;

// A recipient is a tuple of address, amount, memo (optional if zaddr)
typedef std::tuple<std::string, CAmount, std::string> SendManyRecipient;

// Input UTXO is a tuple (quadruple) of txid, vout, amount, coinbase)
typedef std::tuple<uint256, int, CAmount, bool> SendManyInputUTXO;

// Input JSOP is a tuple of JSOutpoint, note and amount
typedef std::tuple<JSOutPoint, Note, CAmount> SendManyInputJSOP;

// Package of info which is passed to perform_joinsplit methods.
struct AsyncJoinSplitInfo
{
    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    std::vector<Note> notes;
    CAmount vpub_old = 0;
    CAmount vpub_new = 0;
};

// A struct to help us track the witness and anchor for a given JSOutPoint
struct WitnessAnchorData {
	boost::optional<ZCIncrementalWitness> witness;
	uint256 anchor;
};

class AsyncRPCOperation_sendmany : public AsyncRPCOperation {
public:
    AsyncRPCOperation_sendmany(std::string fromAddress, std::vector<SendManyRecipient> tOutputs, std::vector<SendManyRecipient> zOutputs, int minDepth, CAmount fee = ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE, Value contextInfo = Value::null);
    virtual ~AsyncRPCOperation_sendmany();
    
    // We don't want to be copied or moved around
    AsyncRPCOperation_sendmany(AsyncRPCOperation_sendmany const&) = delete;             // Copy construct
    AsyncRPCOperation_sendmany(AsyncRPCOperation_sendmany&&) = delete;                  // Move construct
    AsyncRPCOperation_sendmany& operator=(AsyncRPCOperation_sendmany const&) = delete;  // Copy assign
    AsyncRPCOperation_sendmany& operator=(AsyncRPCOperation_sendmany &&) = delete;      // Move assign
    
    virtual void main();

    virtual Value getStatus() const;

    bool testmode = false;  // Set to true to disable sending txs and generating proofs

private:
    friend class TEST_FRIEND_AsyncRPCOperation_sendmany;    // class for unit testing

    Value contextinfo_;     // optional data to include in return value from getStatus()

    CAmount fee_;
    int mindepth_;
    std::string fromaddress_;
    bool isfromtaddr_;
    bool isfromzaddr_;
    CBitcoinAddress fromtaddr_;
    PaymentAddress frompaymentaddress_;
    SpendingKey spendingkey_;
    
    uint256 joinSplitPubKey_;
    unsigned char joinSplitPrivKey_[crypto_sign_SECRETKEYBYTES];

    // The key is the result string from calling JSOutPoint::ToString()
    std::unordered_map<std::string, WitnessAnchorData> jsopWitnessAnchorMap;

    std::vector<SendManyRecipient> t_outputs_;
    std::vector<SendManyRecipient> z_outputs_;
    std::vector<SendManyInputUTXO> t_inputs_;
    std::vector<SendManyInputJSOP> z_inputs_;
    
    CTransaction tx_;
   
    void add_taddr_change_output_to_tx(CAmount amount);
    void add_taddr_outputs_to_tx();
    bool find_unspent_notes();
    bool find_utxos(bool fAcceptCoinbase);
    boost::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s);
    bool main_impl();

    // JoinSplit without any input notes to spend
    Object perform_joinsplit(AsyncJoinSplitInfo &);

    // JoinSplit with input notes to spend (JSOutPoints))
    Object perform_joinsplit(AsyncJoinSplitInfo &, std::vector<JSOutPoint> & );

    // JoinSplit where you have the witnesses and anchor
    Object perform_joinsplit(
        AsyncJoinSplitInfo & info,
        std::vector<boost::optional < ZCIncrementalWitness>> witnesses,
        uint256 anchor);

    void sign_send_raw_transaction(Object obj);     // throws exception if there was an error

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
    
    void add_taddr_change_output_to_tx(CAmount amount) {
        delegate->add_taddr_change_output_to_tx(amount);
    }
    
    void add_taddr_outputs_to_tx() {
        delegate->add_taddr_outputs_to_tx();
    }
    
    bool find_unspent_notes() {
        return delegate->find_unspent_notes();
    }

    bool find_utxos(bool fAcceptCoinbase) {
        return delegate->find_utxos(fAcceptCoinbase);
    }
    
    boost::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s) {
        return delegate->get_memo_from_hex_string(s);
    }
    
    bool main_impl() {
        return delegate->main_impl();
    }

    Object perform_joinsplit(AsyncJoinSplitInfo &info) {
        return delegate->perform_joinsplit(info);
    }

    Object perform_joinsplit(AsyncJoinSplitInfo &info, std::vector<JSOutPoint> &v ) {
        return delegate->perform_joinsplit(info, v);
    }

    Object perform_joinsplit(
        AsyncJoinSplitInfo & info,
        std::vector<boost::optional < ZCIncrementalWitness>> witnesses,
        uint256 anchor)
    {
        return delegate->perform_joinsplit(info, witnesses, anchor);
    }

    void sign_send_raw_transaction(Object obj) {
        delegate->sign_send_raw_transaction(obj);
    }
    
    void set_state(OperationStatus state) {
        delegate->state_.store(state);
    }
};


#endif /* ASYNCRPCOPERATION_SENDMANY_H */

