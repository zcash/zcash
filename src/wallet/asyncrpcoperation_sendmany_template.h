// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef ASYNCRPCOPERATION_SENDMANY_TEMPLATE_H
#define ASYNCRPCOPERATION_SENDMANY_TEMPLATE_H

#include "asyncrpcoperation.h"
#include "amount.h"
#include "primitives/transaction.h"
#include "transaction_builder.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Address.hpp"
#include "wallet.h"
#include "paymentdisclosure.h"

#include <array>
#include <unordered_map>
#include <tuple>

#include <univalue.h>

namespace z_template {

// Default transaction fee if caller does not specify one.
#define ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE   10000

using namespace libzcash;

// A recipient is a tuple of address, amount, memo (optional if zaddr)
typedef std::tuple<std::string, CAmount, std::string> SendManyRecipient;

// Input UTXO is a tuple (quadruple) of txid, vout, amount, coinbase)
typedef std::tuple<uint256, int, CAmount, bool, CTxDestination> SendManyInputUTXO;

// Input JSOP is a tuple of JSOutpoint, note and amount
typedef std::tuple<JSOutPoint, SproutNote, CAmount> SendManyInputJSOP;

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
	boost::optional<SproutWitness> witness;
	uint256 anchor;
};

class AsyncRPCOperation_sendmany_template : public AsyncRPCOperation {
public:
    AsyncRPCOperation_sendmany_template(
        boost::optional<TransactionBuilder> builder,
        CMutableTransaction contextualTx,
        std::string fromAddress,
        std::vector<SendManyRecipient> tOutputs,
        std::vector<SendManyRecipient> zOutputs,
        int minDepth,
        CAmount fee = ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE,
        UniValue contextInfo = NullUniValue,
        UniValue HltcInfo = NullUniValue);
    virtual ~AsyncRPCOperation_sendmany_template();
    
    // We don't want to be copied or moved around
    AsyncRPCOperation_sendmany_template(AsyncRPCOperation_sendmany_template const&) = delete;             // Copy construct
    AsyncRPCOperation_sendmany_template(AsyncRPCOperation_sendmany_template&&) = delete;                  // Move construct
    AsyncRPCOperation_sendmany_template& operator=(AsyncRPCOperation_sendmany_template const&) = delete;  // Copy assign
    AsyncRPCOperation_sendmany_template& operator=(AsyncRPCOperation_sendmany_template &&) = delete;      // Move assign
    
    virtual void main();

    virtual UniValue getStatus() const;

    bool testmode = false;  // Set to true to disable sending txs and generating proofs

    bool paymentDisclosureMode = true; // Set to true to save esk for encrypted notes in payment disclosure database.

private:

    UniValue contextinfo_;     // optional data to include in return value from getStatus()
    UniValue hltcinfo_;

    bool isUsingBuilder_; // Indicates that no Sprout addresses are involved
    uint32_t consensusBranchId_;
    CAmount fee_;
    int mindepth_;
    std::string fromaddress_;
    bool isfromtaddr_;
    bool isfromzaddr_;
    CTxDestination fromtaddr_;
    PaymentAddress frompaymentaddress_;
    SpendingKey spendingkey_;
    
    uint256 joinSplitPubKey_;
    unsigned char joinSplitPrivKey_[crypto_sign_SECRETKEYBYTES];

    // The key is the result string from calling JSOutPoint::ToString()
    std::unordered_map<std::string, WitnessAnchorData> jsopWitnessAnchorMap;

    std::vector<SendManyRecipient> t_outputs_;
    std::vector<SendManyRecipient> z_outputs_;
    std::vector<SendManyInputUTXO> t_inputs_;
    std::vector<SendManyInputJSOP> z_sprout_inputs_;
    std::vector<SaplingNoteEntry> z_sapling_inputs_;

    TransactionBuilder builder_;
    CTransaction tx_;
   
    void add_taddr_change_output_to_tx(CBitcoinAddress *fromaddress,CAmount amount);
    void add_taddr_outputs_to_tx();
    bool find_unspent_notes();
    bool find_utxos(bool fAcceptCoinbase);
    std::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s);
    bool main_impl();

    // JoinSplit without any input notes to spend
    UniValue perform_joinsplit(AsyncJoinSplitInfo &);

    // JoinSplit with input notes to spend (JSOutPoints))
    UniValue perform_joinsplit(AsyncJoinSplitInfo &, std::vector<JSOutPoint> & );

    // JoinSplit where you have the witnesses and anchor
    UniValue perform_joinsplit(
        AsyncJoinSplitInfo & info,
        std::vector<boost::optional < SproutWitness>> witnesses,
        uint256 anchor);

    void sign_send_raw_transaction(UniValue obj);     // throws exception if there was an error

    // payment disclosure!
    std::vector<PaymentDisclosureKeyInfo> paymentDisclosureData_;
};

} // z_template


#endif /* ASYNCRPCOPERATION_SENDMANY_TEMPLATE_H */


