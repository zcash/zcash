// Copyright (c) 2017 The Zcash developers
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

#include "asyncrpcoperation_mergetoaddress.h"

#include "amount.h"
#include "asyncrpcqueue.h"
#include "core_io.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "miner.h"
#include "net.h"
#include "netbase.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "script/interpreter.h"
#include "sodium.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utiltime.h"
#include "wallet.h"
#include "walletdb.h"
#include "zcash/IncrementalMerkleTree.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "paymentdisclosuredb.h"
int32_t komodo_blockheight(uint256 hash);

using namespace libzcash;

extern UniValue sendrawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);

int mta_find_output(UniValue obj, int n)
{
    UniValue outputMapValue = find_value(obj, "outputmap");
    if (!outputMapValue.isArray()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing outputmap for JoinSplit operation");
    }

    UniValue outputMap = outputMapValue.get_array();
    assert(outputMap.size() == ZC_NUM_JS_OUTPUTS);
    for (size_t i = 0; i < outputMap.size(); i++) {
        if (outputMap[i].get_int() == n) {
            return i;
        }
    }

    throw std::logic_error("n is not present in outputmap");
}

AsyncRPCOperation_mergetoaddress::AsyncRPCOperation_mergetoaddress(
                                                                   boost::optional<TransactionBuilder> builder,
                                                                   CMutableTransaction contextualTx,
                                                                   std::vector<MergeToAddressInputUTXO> utxoInputs,
                                                                   std::vector<MergeToAddressInputSproutNote> sproutNoteInputs,
                                                                   std::vector<MergeToAddressInputSaplingNote> saplingNoteInputs,
                                                                   MergeToAddressRecipient recipient,
                                                                   CAmount fee,
                                                                   UniValue contextInfo) :
tx_(contextualTx), utxoInputs_(utxoInputs), sproutNoteInputs_(sproutNoteInputs),
saplingNoteInputs_(saplingNoteInputs), recipient_(recipient), fee_(fee), contextinfo_(contextInfo)
{
    if (fee < 0 || fee > MAX_MONEY) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Fee is out of range");
    }

    if (utxoInputs.empty() && sproutNoteInputs.empty() && saplingNoteInputs.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No inputs");
    }

    if (std::get<0>(recipient).size() == 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Recipient parameter missing");
    }

    if (sproutNoteInputs.size() > 0 && saplingNoteInputs.size() > 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress");
    }

    if (sproutNoteInputs.size() > 0 && builder) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Sprout notes are not supported by the TransactionBuilder");
    }

    isUsingBuilder_ = false;
    if (builder) {
        isUsingBuilder_ = true;
        builder_ = builder.get();
    }

    toTaddr_ = DecodeDestination(std::get<0>(recipient));
    isToTaddr_ = IsValidDestination(toTaddr_);
    isToZaddr_ = false;

    if (!isToTaddr_) {
        auto address = DecodePaymentAddress(std::get<0>(recipient));
        if (IsValidPaymentAddress(address)) {
            isToZaddr_ = true;
            toPaymentAddress_ = address;
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid recipient address");
        }
    }

    // Log the context info i.e. the call parameters to z_mergetoaddress
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_mergetoaddress initialized (params=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_mergetoaddress initialized\n", getId());
    }

    // Lock UTXOs
    lock_utxos();
    lock_notes();

    // Enable payment disclosure if requested
    paymentDisclosureMode = fExperimentalMode && GetBoolArg("-paymentdisclosure", true);
}

AsyncRPCOperation_mergetoaddress::~AsyncRPCOperation_mergetoaddress()
{
}

void AsyncRPCOperation_mergetoaddress::main()
{
    if (isCancelled()) {
        unlock_utxos(); // clean up
        unlock_notes();
        return;
    }

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

    bool success = false;

#ifdef ENABLE_MINING
#ifdef ENABLE_WALLET
    GenerateBitcoins(false, NULL, 0);
#else
    GenerateBitcoins(false, 0);
#endif
#endif

    try {
        success = main_impl();
    } catch (const UniValue& objError) {
        int code = find_value(objError, "code").get_int();
        std::string message = find_value(objError, "message").get_str();
        set_error_code(code);
        set_error_message(message);
    } catch (const runtime_error& e) {
        set_error_code(-1);
        set_error_message("runtime error: " + string(e.what()));
    } catch (const logic_error& e) {
        set_error_code(-1);
        set_error_message("logic error: " + string(e.what()));
    } catch (const exception& e) {
        set_error_code(-1);
        set_error_message("general exception: " + string(e.what()));
    } catch (...) {
        set_error_code(-2);
        set_error_message("unknown error");
    }

#ifdef ENABLE_MINING
#ifdef ENABLE_WALLET
    GenerateBitcoins(GetBoolArg("-gen", false), pwalletMain, GetArg("-genproclimit", 1));
#else
    GenerateBitcoins(GetBoolArg("-gen", false), GetArg("-genproclimit", 1));
#endif
#endif

    stop_execution_clock();

    if (success) {
        set_state(OperationStatus::SUCCESS);
    } else {
        set_state(OperationStatus::FAILED);
    }

    std::string s = strprintf("%s: z_mergetoaddress finished (status=%s", getId(), getStateAsString());
    if (success) {
        s += strprintf(", txid=%s)\n", tx_.GetHash().ToString());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s", s);

    unlock_utxos(); // clean up
    unlock_notes(); // clean up

    // !!! Payment disclosure START
    if (success && paymentDisclosureMode && paymentDisclosureData_.size() > 0) {
        uint256 txidhash = tx_.GetHash();
        std::shared_ptr<PaymentDisclosureDB> db = PaymentDisclosureDB::sharedInstance();
        for (PaymentDisclosureKeyInfo p : paymentDisclosureData_) {
            p.first.hash = txidhash;
            if (!db->Put(p.first, p.second)) {
                LogPrint("paymentdisclosure", "%s: Payment Disclosure: Error writing entry to database for key %s\n", getId(), p.first.ToString());
            } else {
                LogPrint("paymentdisclosure", "%s: Payment Disclosure: Successfully added entry to database for key %s\n", getId(), p.first.ToString());
            }
        }
    }
    // !!! Payment disclosure END
}

// Notes:
// 1. #1359 Currently there is no limit set on the number of joinsplits, so size of tx could be invalid.
// 2. #1277 Spendable notes are not locked, so an operation running in parallel could also try to use them.
bool AsyncRPCOperation_mergetoaddress::main_impl()
{
    assert(isToTaddr_ != isToZaddr_);

    bool isPureTaddrOnlyTx = (sproutNoteInputs_.empty() && saplingNoteInputs_.empty() && isToTaddr_);
    CAmount minersFee = fee_;

    size_t numInputs = utxoInputs_.size();

    // Check mempooltxinputlimit to avoid creating a transaction which the local mempool rejects
    size_t limit = (size_t)GetArg("-mempooltxinputlimit", 0);
    {
        LOCK(cs_main);
        if (NetworkUpgradeActive(chainActive.Height() + 1, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER)) {
            limit = 0;
        }
    }
    if (limit > 0 && numInputs > limit) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           strprintf("Number of transparent inputs %d is greater than mempooltxinputlimit of %d",
                                     numInputs, limit));
    }

    CAmount t_inputs_total = 0;
    for (MergeToAddressInputUTXO& t : utxoInputs_) {
        t_inputs_total += std::get<1>(t);
    }

    CAmount z_inputs_total = 0;
    for (const MergeToAddressInputSproutNote& t : sproutNoteInputs_) {
        z_inputs_total += std::get<2>(t);
    }

    for (const MergeToAddressInputSaplingNote& t : saplingNoteInputs_) {
        z_inputs_total += std::get<2>(t);
    }

    CAmount targetAmount = z_inputs_total + t_inputs_total;

    if (targetAmount <= minersFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                           strprintf("Insufficient funds, have %s and miners fee is %s",
                                     FormatMoney(targetAmount), FormatMoney(minersFee)));
    }

    CAmount sendAmount = targetAmount - minersFee;

    // update the transaction with the UTXO inputs and output (if any)
    if (!isUsingBuilder_) {
        CMutableTransaction rawTx(tx_);
        for (const MergeToAddressInputUTXO& t : utxoInputs_) {
            CTxIn in(std::get<0>(t));
            rawTx.vin.push_back(in);
        }
        if (isToTaddr_) {
            CScript scriptPubKey = GetScriptForDestination(toTaddr_);
            CTxOut out(sendAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        }
        tx_ = CTransaction(rawTx);
    }

    LogPrint(isPureTaddrOnlyTx ? "zrpc" : "zrpcunsafe", "%s: spending %s to send %s with fee %s\n",
             getId(), FormatMoney(targetAmount), FormatMoney(sendAmount), FormatMoney(minersFee));
    LogPrint("zrpc", "%s: transparent input: %s\n", getId(), FormatMoney(t_inputs_total));
    LogPrint("zrpcunsafe", "%s: private input: %s\n", getId(), FormatMoney(z_inputs_total));
    if (isToTaddr_) {
        LogPrint("zrpc", "%s: transparent output: %s\n", getId(), FormatMoney(sendAmount));
    } else {
        LogPrint("zrpcunsafe", "%s: private output: %s\n", getId(), FormatMoney(sendAmount));
    }
    LogPrint("zrpc", "%s: fee: %s\n", getId(), FormatMoney(minersFee));

    // Grab the current consensus branch ID
    {
        LOCK(cs_main);
        consensusBranchId_ = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    }

    /**
     * SCENARIO #0
     *
     * Sprout not involved, so we just use the TransactionBuilder and we're done.
     *
     * This is based on code from AsyncRPCOperation_sendmany::main_impl() and should be refactored.
     */
    if (isUsingBuilder_) {
        builder_.SetFee(minersFee);


        for (const MergeToAddressInputUTXO& t : utxoInputs_) {
            COutPoint outPoint = std::get<0>(t);
            CAmount amount = std::get<1>(t);
            CScript scriptPubKey = std::get<2>(t);
            builder_.AddTransparentInput(outPoint, scriptPubKey, amount);
        }

        boost::optional<uint256> ovk;
        // Select Sapling notes
        std::vector<SaplingOutPoint> saplingOPs;
        std::vector<SaplingNote> saplingNotes;
        std::vector<SaplingExpandedSpendingKey> expsks;
        for (const MergeToAddressInputSaplingNote& saplingNoteInput: saplingNoteInputs_) {
            saplingOPs.push_back(std::get<0>(saplingNoteInput));
            saplingNotes.push_back(std::get<1>(saplingNoteInput));
            auto expsk = std::get<3>(saplingNoteInput);
            expsks.push_back(expsk);
            if (!ovk) {
                ovk = expsk.full_viewing_key().ovk;
            }
        }

        // Fetch Sapling anchor and witnesses
        uint256 anchor;
        std::vector<boost::optional<SaplingWitness>> witnesses;
        {
            LOCK2(cs_main, pwalletMain->cs_wallet);
            pwalletMain->GetSaplingNoteWitnesses(saplingOPs, witnesses, anchor);
        }

        // Add Sapling spends
        for (size_t i = 0; i < saplingNotes.size(); i++) {
            if (!witnesses[i]) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Missing witness for Sapling note");
            }
            assert(builder_.AddSaplingSpend(expsks[i], saplingNotes[i], anchor, witnesses[i].get()));
        }

        if (isToTaddr_) {
            if (!builder_.AddTransparentOutput(toTaddr_, sendAmount)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid output address, not a valid taddr.");
            }
        } else {
            std::string zaddr = std::get<0>(recipient_);
            std::string memo = std::get<1>(recipient_);
            std::array<unsigned char, ZC_MEMO_SIZE> hexMemo = get_memo_from_hex_string(memo);
            auto saplingPaymentAddress = boost::get<libzcash::SaplingPaymentAddress>(&toPaymentAddress_);
            if (saplingPaymentAddress == nullptr) {
                // This should never happen as we have already determined that the payment is to sapling
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Could not get Sapling payment address.");
            }
            if (saplingNoteInputs_.size() == 0 && utxoInputs_.size() > 0) {
                // Sending from t-addresses, which we don't have ovks for. Instead,
                // generate a common one from the HD seed. This ensures the data is
                // recoverable, while keeping it logically separate from the ZIP 32
                // Sapling key hierarchy, which the user might not be using.
                HDSeed seed;
                if (!pwalletMain->GetHDSeed(seed)) {
                    throw JSONRPCError(
                                       RPC_WALLET_ERROR,
                                       "AsyncRPCOperation_sendmany: HD seed not found");
                }
                ovk = ovkForShieldingFromTaddr(seed);
            }
            if (!ovk) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Sending to a Sapling address requires an ovk.");
            }
            builder_.AddSaplingOutput(ovk.get(), *saplingPaymentAddress, sendAmount, hexMemo);
        }


        // Build the transaction
        auto maybe_tx = builder_.Build();
        if (!maybe_tx) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Failed to build transaction.");
        }
        tx_ = maybe_tx.get();

        // Send the transaction
        // TODO: Use CWallet::CommitTransaction instead of sendrawtransaction
        auto signedtxn = EncodeHexTx(tx_);
        if (!testmode) {
            UniValue params = UniValue(UniValue::VARR);
            params.push_back(signedtxn);
            UniValue sendResultValue = sendrawtransaction(params, false, CPubKey());
            if (sendResultValue.isNull()) {
                throw JSONRPCError(RPC_WALLET_ERROR, "sendrawtransaction did not return an error or a txid.");
            }

            auto txid = sendResultValue.get_str();

            UniValue o(UniValue::VOBJ);
            o.push_back(Pair("txid", txid));
            set_result(o);
        } else {
            // Test mode does not send the transaction to the network.
            UniValue o(UniValue::VOBJ);
            o.push_back(Pair("test", 1));
            o.push_back(Pair("txid", tx_.GetHash().ToString()));
            o.push_back(Pair("hex", signedtxn));
            set_result(o);
        }

        return true;
    }
    /**
     * END SCENARIO #0
     */


    /**
     * SCENARIO #1
     *
     * taddrs -> taddr
     *
     * There are no zaddrs or joinsplits involved.
     */
    if (isPureTaddrOnlyTx) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("rawtxn", EncodeHexTx(tx_)));
        sign_send_raw_transaction(obj);
        return true;
    }
    /**
     * END SCENARIO #1
     */


    // Prepare raw transaction to handle JoinSplits
    CMutableTransaction mtx(tx_);
    crypto_sign_keypair(joinSplitPubKey_.begin(), joinSplitPrivKey_);
    mtx.joinSplitPubKey = joinSplitPubKey_;
    tx_ = CTransaction(mtx);
    std::string hexMemo = std::get<1>(recipient_);


    /**
     * SCENARIO #2
     *
     * taddrs -> zaddr
     *
     * We only need a single JoinSplit.
     */
    if (sproutNoteInputs_.empty() && isToZaddr_) {
        // Create JoinSplit to target z-addr.
        MergeToAddressJSInfo info;
        info.vpub_old = sendAmount;
        info.vpub_new = 0;

        JSOutput jso = JSOutput(boost::get<libzcash::SproutPaymentAddress>(toPaymentAddress_), sendAmount);
        if (hexMemo.size() > 0) {
            jso.memo = get_memo_from_hex_string(hexMemo);
        }
        info.vjsout.push_back(jso);

        UniValue obj(UniValue::VOBJ);
        obj = perform_joinsplit(info);
        sign_send_raw_transaction(obj);
        return true;
    }
    /**
     * END SCENARIO #2
     */


    // Copy zinputs to more flexible containers
    std::deque<MergeToAddressInputSproutNote> zInputsDeque;
    for (const auto& o : sproutNoteInputs_) {
        zInputsDeque.push_back(o);
    }

    // When spending notes, take a snapshot of note witnesses and anchors as the treestate will
    // change upon arrival of new blocks which contain joinsplit transactions.  This is likely
    // to happen as creating a chained joinsplit transaction can take longer than the block interval.
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        for (auto t : sproutNoteInputs_) {
            JSOutPoint jso = std::get<0>(t);
            std::vector<JSOutPoint> vOutPoints = {jso};
            uint256 inputAnchor;
            std::vector<boost::optional<SproutWitness>> vInputWitnesses;
            pwalletMain->GetSproutNoteWitnesses(vOutPoints, vInputWitnesses, inputAnchor);
            jsopWitnessAnchorMap[jso.ToString()] = MergeToAddressWitnessAnchorData{vInputWitnesses[0], inputAnchor};
        }
    }

    /**
     * SCENARIO #3
     *
     * zaddrs -> zaddr
     * taddrs ->
     *
     * zaddrs ->
     * taddrs -> taddr
     *
     * Send to zaddr by chaining JoinSplits together and immediately consuming any change
     * Send to taddr by creating dummy z outputs and accumulating value in a change note
     * which is used to set vpub_new in the last chained joinsplit.
     */
    UniValue obj(UniValue::VOBJ);
    CAmount jsChange = 0;          // this is updated after each joinsplit
    int changeOutputIndex = -1;    // this is updated after each joinsplit if jsChange > 0
    bool vpubOldProcessed = false; // updated when vpub_old for taddr inputs is set in first joinsplit
    bool vpubNewProcessed = false; // updated when vpub_new for miner fee and taddr outputs is set in last joinsplit

    // At this point, we are guaranteed to have at least one input note.
    // Use address of first input note as the temporary change address.
    SproutSpendingKey changeKey = std::get<3>(zInputsDeque.front());
    SproutPaymentAddress changeAddress = changeKey.address();

    CAmount vpubOldTarget = 0;
    CAmount vpubNewTarget = 0;
    if (isToTaddr_) {
        vpubNewTarget = z_inputs_total;
    } else {
        if (utxoInputs_.empty()) {
            vpubNewTarget = minersFee;
        } else {
            vpubOldTarget = t_inputs_total - minersFee;
        }
    }

    // Keep track of treestate within this transaction
    boost::unordered_map<uint256, SproutMerkleTree, CCoinsKeyHasher> intermediates;
    std::vector<uint256> previousCommitments;

    while (!vpubNewProcessed) {
        MergeToAddressJSInfo info;
        info.vpub_old = 0;
        info.vpub_new = 0;

        // Set vpub_old in the first joinsplit
        if (!vpubOldProcessed) {
            if (t_inputs_total < vpubOldTarget) {
                throw JSONRPCError(RPC_WALLET_ERROR,
                                   strprintf("Insufficient transparent funds for vpub_old %s (miners fee %s, taddr inputs %s)",
                                             FormatMoney(vpubOldTarget), FormatMoney(minersFee), FormatMoney(t_inputs_total)));
            }
            info.vpub_old += vpubOldTarget; // funds flowing from public pool
            vpubOldProcessed = true;
        }

        CAmount jsInputValue = 0;
        uint256 jsAnchor;
        std::vector<boost::optional<SproutWitness>> witnesses;

        JSDescription prevJoinSplit;

        // Keep track of previous JoinSplit and its commitments
        if (tx_.vjoinsplit.size() > 0) {
            prevJoinSplit = tx_.vjoinsplit.back();
        }

        // If there is no change, the chain has terminated so we can reset the tracked treestate.
        if (jsChange == 0 && tx_.vjoinsplit.size() > 0) {
            intermediates.clear();
            previousCommitments.clear();
        }

        //
        // Consume change as the first input of the JoinSplit.
        //
        if (jsChange > 0) {
            LOCK2(cs_main, pwalletMain->cs_wallet);

            // Update tree state with previous joinsplit
            SproutMerkleTree tree;
            auto it = intermediates.find(prevJoinSplit.anchor);
            if (it != intermediates.end()) {
                tree = it->second;
            } else if (!pcoinsTip->GetSproutAnchorAt(prevJoinSplit.anchor, tree)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Could not find previous JoinSplit anchor");
            }

            assert(changeOutputIndex != -1);
            boost::optional<SproutWitness> changeWitness;
            int n = 0;
            for (const uint256& commitment : prevJoinSplit.commitments) {
                tree.append(commitment);
                previousCommitments.push_back(commitment);
                if (!changeWitness && changeOutputIndex == n++) {
                    changeWitness = tree.witness();
                } else if (changeWitness) {
                    changeWitness.get().append(commitment);
                }
            }
            if (changeWitness) {
                witnesses.push_back(changeWitness);
            }
            jsAnchor = tree.root();
            intermediates.insert(std::make_pair(tree.root(), tree)); // chained js are interstitial (found in between block boundaries)

            // Decrypt the change note's ciphertext to retrieve some data we need
            ZCNoteDecryption decryptor(changeKey.receiving_key());
            auto hSig = prevJoinSplit.h_sig(*pzcashParams, tx_.joinSplitPubKey);
            try {
                SproutNotePlaintext plaintext = SproutNotePlaintext::decrypt(
                                                                             decryptor,
                                                                             prevJoinSplit.ciphertexts[changeOutputIndex],
                                                                             prevJoinSplit.ephemeralKey,
                                                                             hSig,
                                                                             (unsigned char)changeOutputIndex);

                SproutNote note = plaintext.note(changeAddress);
                info.notes.push_back(note);
                info.zkeys.push_back(changeKey);

                jsInputValue += plaintext.value();

                LogPrint("zrpcunsafe", "%s: spending change (amount=%s)\n",
                         getId(),
                         FormatMoney(plaintext.value()));

            } catch (const std::exception& e) {
                throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Error decrypting output note of previous JoinSplit: %s", e.what()));
            }
        }


        //
        // Consume spendable non-change notes
        //
        std::vector<SproutNote> vInputNotes;
        std::vector<SproutSpendingKey> vInputZKeys;
        std::vector<JSOutPoint> vOutPoints;
        std::vector<boost::optional<SproutWitness>> vInputWitnesses;
        uint256 inputAnchor;
        int numInputsNeeded = (jsChange > 0) ? 1 : 0;
        while (numInputsNeeded++ < ZC_NUM_JS_INPUTS && zInputsDeque.size() > 0) {
            MergeToAddressInputSproutNote t = zInputsDeque.front();
            JSOutPoint jso = std::get<0>(t);
            SproutNote note = std::get<1>(t);
            CAmount noteFunds = std::get<2>(t);
            SproutSpendingKey zkey = std::get<3>(t);
            zInputsDeque.pop_front();

            MergeToAddressWitnessAnchorData wad = jsopWitnessAnchorMap[jso.ToString()];
            vInputWitnesses.push_back(wad.witness);
            if (inputAnchor.IsNull()) {
                inputAnchor = wad.anchor;
            } else if (inputAnchor != wad.anchor) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Selected input notes do not share the same anchor");
            }

            vOutPoints.push_back(jso);
            vInputNotes.push_back(note);
            vInputZKeys.push_back(zkey);

            jsInputValue += noteFunds;

            int wtxHeight = -1;
            int wtxDepth = -1;
            {
                LOCK2(cs_main, pwalletMain->cs_wallet);
                const CWalletTx& wtx = pwalletMain->mapWallet[jso.hash];
                // Zero confirmation notes belong to transactions which have not yet been mined
                if (mapBlockIndex.find(wtx.hashBlock) == mapBlockIndex.end()) {
                    throw JSONRPCError(RPC_WALLET_ERROR, strprintf("mapBlockIndex does not contain block hash %s", wtx.hashBlock.ToString()));
                }
                wtxHeight = komodo_blockheight(wtx.hashBlock);
                wtxDepth = wtx.GetDepthInMainChain();
            }
            LogPrint("zrpcunsafe", "%s: spending note (txid=%s, vjoinsplit=%d, ciphertext=%d, amount=%s, height=%d, confirmations=%d)\n",
                     getId(),
                     jso.hash.ToString().substr(0, 10),
                     jso.js,
                     int(jso.n), // uint8_t
                     FormatMoney(noteFunds),
                     wtxHeight,
                     wtxDepth);
        }

        // Add history of previous commitments to witness
        if (vInputNotes.size() > 0) {
            if (vInputWitnesses.size() == 0) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Could not find witness for note commitment");
            }

            for (auto& optionalWitness : vInputWitnesses) {
                if (!optionalWitness) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "Witness for note commitment is null");
                }
                SproutWitness w = *optionalWitness; // could use .get();
                if (jsChange > 0) {
                    for (const uint256& commitment : previousCommitments) {
                        w.append(commitment);
                    }
                    if (jsAnchor != w.root()) {
                        throw JSONRPCError(RPC_WALLET_ERROR, "Witness for spendable note does not have same anchor as change input");
                    }
                }
                witnesses.push_back(w);
            }

            // The jsAnchor is null if this JoinSplit is at the start of a new chain
            if (jsAnchor.IsNull()) {
                jsAnchor = inputAnchor;
            }

            // Add spendable notes as inputs
            std::copy(vInputNotes.begin(), vInputNotes.end(), std::back_inserter(info.notes));
            std::copy(vInputZKeys.begin(), vInputZKeys.end(), std::back_inserter(info.zkeys));
        }

        // Accumulate change
        jsChange = jsInputValue + info.vpub_old;

        // Set vpub_new in the last joinsplit (when there are no more notes to spend)
        if (zInputsDeque.empty()) {
            assert(!vpubNewProcessed);
            if (jsInputValue < vpubNewTarget) {
                throw JSONRPCError(RPC_WALLET_ERROR,
                                   strprintf("Insufficient funds for vpub_new %s (miners fee %s, taddr inputs %s)",
                                             FormatMoney(vpubNewTarget), FormatMoney(minersFee), FormatMoney(t_inputs_total)));
            }
            info.vpub_new += vpubNewTarget; // funds flowing back to public pool
            vpubNewProcessed = true;
            jsChange -= vpubNewTarget;
            // If we are merging to a t-addr, there should be no change
            if (isToTaddr_) assert(jsChange == 0);
        }

        // create dummy output
        info.vjsout.push_back(JSOutput()); // dummy output while we accumulate funds into a change note for vpub_new

        // create output for any change
        if (jsChange > 0) {
            std::string outputType = "change";
            auto jso = JSOutput(changeAddress, jsChange);
            // If this is the final output, set the target and memo
            if (isToZaddr_ && vpubNewProcessed) {
                outputType = "target";
                jso.addr = boost::get<libzcash::SproutPaymentAddress>(toPaymentAddress_);
                if (!hexMemo.empty()) {
                    jso.memo = get_memo_from_hex_string(hexMemo);
                }
            }
            info.vjsout.push_back(jso);

            LogPrint("zrpcunsafe", "%s: generating note for %s (amount=%s)\n",
                     getId(),
                     outputType,
                     FormatMoney(jsChange));
        }

        obj = perform_joinsplit(info, witnesses, jsAnchor);

        if (jsChange > 0) {
            changeOutputIndex = mta_find_output(obj, 1);
        }
    }

    // Sanity check in case changes to code block above exits loop by invoking 'break'
    assert(zInputsDeque.size() == 0);
    assert(vpubNewProcessed);

    sign_send_raw_transaction(obj);
    return true;
}


extern UniValue signrawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);

/**
 * Sign and send a raw transaction.
 * Raw transaction as hex string should be in object field "rawtxn"
 */
void AsyncRPCOperation_mergetoaddress::sign_send_raw_transaction(UniValue obj)
{
    // Sign the raw transaction
    UniValue rawtxnValue = find_value(obj, "rawtxn");
    if (rawtxnValue.isNull()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for raw transaction");
    }
    std::string rawtxn = rawtxnValue.get_str();

    UniValue params = UniValue(UniValue::VARR);
    params.push_back(rawtxn);
    UniValue signResultValue = signrawtransaction(params, false, CPubKey());
    UniValue signResultObject = signResultValue.get_obj();
    UniValue completeValue = find_value(signResultObject, "complete");
    bool complete = completeValue.get_bool();
    if (!complete) {
        // TODO: #1366 Maybe get "errors" and print array vErrors into a string
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Failed to sign transaction");
    }

    UniValue hexValue = find_value(signResultObject, "hex");
    if (hexValue.isNull()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for signed transaction");
    }
    std::string signedtxn = hexValue.get_str();

    // Send the signed transaction
    if (!testmode) {
        params.clear();
        params.setArray();
        params.push_back(signedtxn);
        UniValue sendResultValue = sendrawtransaction(params, false, CPubKey());
        if (sendResultValue.isNull()) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Send raw transaction did not return an error or a txid.");
        }

        std::string txid = sendResultValue.get_str();

        UniValue o(UniValue::VOBJ);
        o.push_back(Pair("txid", txid));
        set_result(o);
    } else {
        // Test mode does not send the transaction to the network.

        CDataStream stream(ParseHex(signedtxn), SER_NETWORK, PROTOCOL_VERSION);
        CTransaction tx;
        stream >> tx;

        UniValue o(UniValue::VOBJ);
        o.push_back(Pair("test", 1));
        o.push_back(Pair("txid", tx.GetHash().ToString()));
        o.push_back(Pair("hex", signedtxn));
        set_result(o);
    }

    // Keep the signed transaction so we can hash to the same txid
    CDataStream stream(ParseHex(signedtxn), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;
    tx_ = tx;
}


UniValue AsyncRPCOperation_mergetoaddress::perform_joinsplit(MergeToAddressJSInfo& info)
{
    std::vector<boost::optional<SproutWitness>> witnesses;
    uint256 anchor;
    {
        LOCK(cs_main);
        anchor = pcoinsTip->GetBestAnchor(SPROUT); // As there are no inputs, ask the wallet for the best anchor
    }
    return perform_joinsplit(info, witnesses, anchor);
}


UniValue AsyncRPCOperation_mergetoaddress::perform_joinsplit(MergeToAddressJSInfo& info, std::vector<JSOutPoint>& outPoints)
{
    std::vector<boost::optional<SproutWitness>> witnesses;
    uint256 anchor;
    {
        LOCK(cs_main);
        pwalletMain->GetSproutNoteWitnesses(outPoints, witnesses, anchor);
    }
    return perform_joinsplit(info, witnesses, anchor);
}

UniValue AsyncRPCOperation_mergetoaddress::perform_joinsplit(
                                                             MergeToAddressJSInfo& info,
                                                             std::vector<boost::optional<SproutWitness>> witnesses,
                                                             uint256 anchor)
{
    if (anchor.IsNull()) {
        throw std::runtime_error("anchor is null");
    }

    if (witnesses.size() != info.notes.size()) {
        throw runtime_error("number of notes and witnesses do not match");
    }

    if (info.notes.size() != info.zkeys.size()) {
        throw runtime_error("number of notes and spending keys do not match");
    }

    for (size_t i = 0; i < witnesses.size(); i++) {
        if (!witnesses[i]) {
            throw runtime_error("joinsplit input could not be found in tree");
        }
        info.vjsin.push_back(JSInput(*witnesses[i], info.notes[i], info.zkeys[i]));
    }

    // Make sure there are two inputs and two outputs
    while (info.vjsin.size() < ZC_NUM_JS_INPUTS) {
        info.vjsin.push_back(JSInput());
    }

    while (info.vjsout.size() < ZC_NUM_JS_OUTPUTS) {
        info.vjsout.push_back(JSOutput());
    }

    if (info.vjsout.size() != ZC_NUM_JS_INPUTS || info.vjsin.size() != ZC_NUM_JS_OUTPUTS) {
        throw runtime_error("unsupported joinsplit input/output counts");
    }

    CMutableTransaction mtx(tx_);

    LogPrint("zrpcunsafe", "%s: creating joinsplit at index %d (vpub_old=%s, vpub_new=%s, in[0]=%s, in[1]=%s, out[0]=%s, out[1]=%s)\n",
             getId(),
             tx_.vjoinsplit.size(),
             FormatMoney(info.vpub_old), FormatMoney(info.vpub_new),
             FormatMoney(info.vjsin[0].note.value()), FormatMoney(info.vjsin[1].note.value()),
             FormatMoney(info.vjsout[0].value), FormatMoney(info.vjsout[1].value));

    // Generate the proof, this can take over a minute.
    std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs{info.vjsin[0], info.vjsin[1]};
    std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs{info.vjsout[0], info.vjsout[1]};
    std::array<size_t, ZC_NUM_JS_INPUTS> inputMap;
    std::array<size_t, ZC_NUM_JS_OUTPUTS> outputMap;

    uint256 esk; // payment disclosure - secret

    JSDescription jsdesc = JSDescription::Randomized(
                                                     mtx.fOverwintered && (mtx.nVersion >= SAPLING_TX_VERSION),
                                                     *pzcashParams,
                                                     joinSplitPubKey_,
                                                     anchor,
                                                     inputs,
                                                     outputs,
                                                     inputMap,
                                                     outputMap,
                                                     info.vpub_old,
                                                     info.vpub_new,
                                                     !this->testmode,
                                                     &esk); // parameter expects pointer to esk, so pass in address
    {
        auto verifier = libzcash::ProofVerifier::Strict();
        if (!(jsdesc.Verify(*pzcashParams, verifier, joinSplitPubKey_))) {
            throw std::runtime_error("error verifying joinsplit");
        }
    }

    mtx.vjoinsplit.push_back(jsdesc);

    // Empty output script.
    CScript scriptCode;
    CTransaction signTx(mtx);
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId_);

    // Add the signature
    if (!(crypto_sign_detached(&mtx.joinSplitSig[0], NULL,
                               dataToBeSigned.begin(), 32,
                               joinSplitPrivKey_) == 0)) {
        throw std::runtime_error("crypto_sign_detached failed");
    }

    // Sanity check
    if (!(crypto_sign_verify_detached(&mtx.joinSplitSig[0],
                                      dataToBeSigned.begin(), 32,
                                      mtx.joinSplitPubKey.begin()) == 0)) {
        throw std::runtime_error("crypto_sign_verify_detached failed");
    }

    CTransaction rawTx(mtx);
    tx_ = rawTx;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;

    std::string encryptedNote1;
    std::string encryptedNote2;
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char)0x00);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[0];
        ss2 << jsdesc.h_sig(*pzcashParams, joinSplitPubKey_);

        encryptedNote1 = HexStr(ss2.begin(), ss2.end());
    }
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char)0x01);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[1];
        ss2 << jsdesc.h_sig(*pzcashParams, joinSplitPubKey_);

        encryptedNote2 = HexStr(ss2.begin(), ss2.end());
    }

    UniValue arrInputMap(UniValue::VARR);
    UniValue arrOutputMap(UniValue::VARR);
    for (size_t i = 0; i < ZC_NUM_JS_INPUTS; i++) {
        arrInputMap.push_back(static_cast<uint64_t>(inputMap[i]));
    }
    for (size_t i = 0; i < ZC_NUM_JS_OUTPUTS; i++) {
        arrOutputMap.push_back(static_cast<uint64_t>(outputMap[i]));
    }


    // !!! Payment disclosure START
    unsigned char buffer[32] = {0};
    memcpy(&buffer[0], &joinSplitPrivKey_[0], 32); // private key in first half of 64 byte buffer
    std::vector<unsigned char> vch(&buffer[0], &buffer[0] + 32);
    uint256 joinSplitPrivKey = uint256(vch);
    size_t js_index = tx_.vjoinsplit.size() - 1;
    uint256 placeholder;
    for (int i = 0; i < ZC_NUM_JS_OUTPUTS; i++) {
        uint8_t mapped_index = outputMap[i];
        // placeholder for txid will be filled in later when tx has been finalized and signed.
        PaymentDisclosureKey pdKey = {placeholder, js_index, mapped_index};
        JSOutput output = outputs[mapped_index];
        libzcash::SproutPaymentAddress zaddr = output.addr; // randomized output
        PaymentDisclosureInfo pdInfo = {PAYMENT_DISCLOSURE_VERSION_EXPERIMENTAL, esk, joinSplitPrivKey, zaddr};
        paymentDisclosureData_.push_back(PaymentDisclosureKeyInfo(pdKey, pdInfo));

        LogPrint("paymentdisclosure", "%s: Payment Disclosure: js=%d, n=%d, zaddr=%s\n", getId(), js_index, int(mapped_index), EncodePaymentAddress(zaddr));
    }
    // !!! Payment disclosure END

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("encryptednote1", encryptedNote1));
    obj.push_back(Pair("encryptednote2", encryptedNote2));
    obj.push_back(Pair("rawtxn", HexStr(ss.begin(), ss.end())));
    obj.push_back(Pair("inputmap", arrInputMap));
    obj.push_back(Pair("outputmap", arrOutputMap));
    return obj;
}

std::array<unsigned char, ZC_MEMO_SIZE> AsyncRPCOperation_mergetoaddress::get_memo_from_hex_string(std::string s)
{
    std::array<unsigned char, ZC_MEMO_SIZE> memo = {{0x00}};

    std::vector<unsigned char> rawMemo = ParseHex(s.c_str());

    // If ParseHex comes across a non-hex char, it will stop but still return results so far.
    size_t slen = s.length();
    if (slen % 2 != 0 || (slen > 0 && rawMemo.size() != slen / 2)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Memo must be in hexadecimal format");
    }

    if (rawMemo.size() > ZC_MEMO_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Memo size of %d is too big, maximum allowed is %d", rawMemo.size(), ZC_MEMO_SIZE));
    }

    // copy vector into boost array
    int lenMemo = rawMemo.size();
    for (int i = 0; i < ZC_MEMO_SIZE && i < lenMemo; i++) {
        memo[i] = rawMemo[i];
    }
    return memo;
}

/**
 * Override getStatus() to append the operation's input parameters to the default status object.
 */
UniValue AsyncRPCOperation_mergetoaddress::getStatus() const
{
    UniValue v = AsyncRPCOperation::getStatus();
    if (contextinfo_.isNull()) {
        return v;
    }

    UniValue obj = v.get_obj();
    obj.push_back(Pair("method", "z_mergetoaddress"));
    obj.push_back(Pair("params", contextinfo_));
    return obj;
}

/**
 * Lock input utxos
 */
void AsyncRPCOperation_mergetoaddress::lock_utxos() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : utxoInputs_) {
        pwalletMain->LockCoin(std::get<0>(utxo));
    }
}

/**
 * Unlock input utxos
 */
void AsyncRPCOperation_mergetoaddress::unlock_utxos() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : utxoInputs_) {
        pwalletMain->UnlockCoin(std::get<0>(utxo));
    }
}


/**
 * Lock input notes
 */
void AsyncRPCOperation_mergetoaddress::lock_notes() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto note : sproutNoteInputs_) {
        pwalletMain->LockNote(std::get<0>(note));
    }
    for (auto note : saplingNoteInputs_) {
        pwalletMain->LockNote(std::get<0>(note));
    }
}

/**
 * Unlock input notes
 */
void AsyncRPCOperation_mergetoaddress::unlock_notes() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto note : sproutNoteInputs_) {
        pwalletMain->UnlockNote(std::get<0>(note));
    }
    for (auto note : saplingNoteInputs_) {
        pwalletMain->UnlockNote(std::get<0>(note));
    }
}
