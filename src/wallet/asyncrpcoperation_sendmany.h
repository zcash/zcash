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

#include <tuple>

// TODO: Compute fee based on a heuristic, e.g. (num tx output * dust threshold) + joinsplit bytes * ?
#define ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE   10000

using namespace libzcash;
using namespace json_spirit;

// A recipient is a tuple of address, amount, memo (optional if zaddr)
typedef std::tuple<std::string, CAmount, std::string> SendManyRecipient;

// Input UTXO is a tuple of txid, vout, amount
typedef std::tuple<uint256, int, CAmount> SendManyInputUTXO;

// Input NPT is a pair of the plaintext note and amount
typedef std::pair<NotePlaintext, CAmount> SendManyInputNPT;

// Package of info needed to perform a joinsplit
struct AsyncJoinSplitInfo
{
    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    std::vector<Note> notes;
    std::vector<SpendingKey> keys;
    std::vector<uint256> commitments;
    CAmount vpub_old = 0;
    CAmount vpub_new = 0;
};

class AsyncRPCOperation_sendmany : public AsyncRPCOperation {
public:
    AsyncRPCOperation_sendmany(std::string fromAddress, std::vector<SendManyRecipient> tOutputs, std::vector<SendManyRecipient> zOutputs, int minDepth);
    virtual ~AsyncRPCOperation_sendmany();
    
    // We don't want to be copied or moved around
    AsyncRPCOperation_sendmany(AsyncRPCOperation_sendmany const&) = delete;             // Copy construct
    AsyncRPCOperation_sendmany(AsyncRPCOperation_sendmany&&) = delete;                  // Move construct
    AsyncRPCOperation_sendmany& operator=(AsyncRPCOperation_sendmany const&) = delete;  // Copy assign
    AsyncRPCOperation_sendmany& operator=(AsyncRPCOperation_sendmany &&) = delete;      // Move assign
    
    virtual void main();

    bool testmode = false;  // Set this to true to disable sending transactions to the network

private:
    int mindepth_;
    std::string fromaddress_;
    bool isfromtaddr_;
    bool isfromzaddr_;
    CBitcoinAddress fromtaddr_;
    PaymentAddress frompaymentaddress_;
    SpendingKey spendingkey_;
    
    uint256 joinSplitPubKey_;
    unsigned char joinSplitPrivKey_[crypto_sign_SECRETKEYBYTES];

        
    std::vector<SendManyRecipient> t_outputs_;
    std::vector<SendManyRecipient> z_outputs_;
    std::vector<SendManyInputUTXO> t_inputs_;
    std::vector<SendManyInputNPT> z_inputs_;
    
    CTransaction tx_;
   
    void add_taddr_change_output_to_tx(CAmount amount);
    void add_taddr_outputs_to_tx();
    bool find_unspent_notes();
    bool find_utxos();
    boost::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s);
    bool main_impl();
    Object perform_joinsplit( AsyncJoinSplitInfo &);
    Object perform_joinsplit(
        AsyncJoinSplitInfo & info,
        std::vector<boost::optional < ZCIncrementalWitness>> witnesses,
        uint256 anchor);
    void sign_send_raw_transaction(Object obj);     // throws exception if there was an error

};

#endif /* ASYNCRPCOPERATION_SENDMANY_H */

