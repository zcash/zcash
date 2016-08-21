// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "asyncrpcoperation_sendmany.h"
#include "asyncrpcqueue.h"
#include "amount.h"
#include "core_io.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "script/interpreter.h"
#include "utiltime.h"
#include "rpcprotocol.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "sodium.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>

using namespace libzcash;

AsyncRPCOperation_sendmany::AsyncRPCOperation_sendmany(
        std::string fromAddress,
        std::vector<SendManyRecipient> tOutputs,
        std::vector<SendManyRecipient> zOutputs,
        int minDepth) :
        fromaddress_(fromAddress), t_outputs_(tOutputs), z_outputs_(zOutputs), mindepth_(minDepth)
{
    if (minDepth < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minconf cannot be negative");

    if (fromAddress.size() == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "From address parameter missing");

    if (tOutputs.size() == 0 && zOutputs.size() == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No recipients");

    fromtaddr_ = CBitcoinAddress(fromAddress);
    isfromtaddr_ = fromtaddr_.IsValid();
    isfromzaddr_ = false;

    libzcash::PaymentAddress addr;
    if (!isfromtaddr_) {
        CZCPaymentAddress address(fromAddress);
        try {
            PaymentAddress addr = address.Get();

            // We don't need to lock on the wallet as spending key related methods are thread-safe
            if (!pwalletMain->HaveSpendingKey(addr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or zaddr.");

            SpendingKey key;
            if (!pwalletMain->GetSpendingKey(addr, key))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, no spending key found for zaddr");

            isfromzaddr_ = true;
            frompaymentaddress_ = addr;
            spendingkey_ = key;
        } catch (std::runtime_error) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or zaddr.");
        }
    }
}

AsyncRPCOperation_sendmany::~AsyncRPCOperation_sendmany() {
}

void AsyncRPCOperation_sendmany::main() {
    if (isCancelled())
        return;

    setState(OperationStatus::EXECUTING);
    startExecutionClock();

    bool success = false;

    try {
        success = main_impl();
    } catch (Object objError) {
        int code = find_value(objError, "code").get_int();
        std::string message = find_value(objError, "message").get_str();
        setErrorCode(code);
        setErrorMessage(message);
    } catch (runtime_error e) {
        setErrorCode(-1);
        setErrorMessage("runtime error: " + string(e.what()));
    } catch (logic_error e) {
        setErrorCode(-1);
        setErrorMessage("logic error: " + string(e.what()));
    } catch (...) {
        setErrorCode(-2);
        setErrorMessage("unknown error");
    }

    stopExecutionClock();

    if (success) {
        setState(OperationStatus::SUCCESS);
    } else {
        setState(OperationStatus::FAILED);
    }

}

// This alpha currently:
// - upto one zaddr as a recipient of funds
// - spends the first available note, and the second if required.
// - the first joinsplit will set vpub_old and vpub_new.
// - does not chain joinsplits together to use more notes and pass change on (TODO)
// Perhaps restrict chaining to pure zaddr->zaddr(s) tx only?)
bool AsyncRPCOperation_sendmany::main_impl() {

    bool isPureTaddrOnlyTx = (isfromtaddr_ && z_outputs_.size() == 0);
    CAmount minersFee = ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE;

    // Regardless of the from address, add all taddr outputs to the raw transaction.
    if (isfromtaddr_ && !find_utxos())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds, no UTXOs found for taddr from address.");

    if (isfromzaddr_ && !find_unspent_notes())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds, no unspent notes found for zaddr from address.");

    CAmount t_inputs_total = 0;
    for (SendManyInputUTXO & t : t_inputs_) {
        t_inputs_total += std::get<2>(t);
    }

    CAmount z_inputs_total = 0;
    for (SendManyInputNPT & p : z_inputs_) {
        z_inputs_total += p.second;
    }

    CAmount t_outputs_total = 0;
    for (SendManyRecipient & t : t_outputs_) {
        t_outputs_total += std::get<1>(t);
    }

    CAmount z_outputs_total = 0;
    for (SendManyRecipient & t : z_outputs_) {
        z_outputs_total += std::get<1>(t);
    }

    CAmount sendAmount = z_outputs_total + t_outputs_total;
    CAmount targetAmount = sendAmount + minersFee;

#if 0
    std::cout << "t_inputs_total: " << t_inputs_total << std::endl;
    std::cout << "z_inputs_total: " << z_inputs_total << std::endl;
    std::cout << "t_outputs_total: " << t_outputs_total << std::endl;
    std::cout << "z_outputs_total: " << z_outputs_total << std::endl;
    std::cout << "sendAmount: " << sendAmount << std::endl;
    std::cout << "feeAmount: " << minersFee << std::endl;
    std::cout << "targetAmount: " << targetAmount << std::endl;
#endif

    if (isfromtaddr_ && (t_inputs_total < targetAmount))
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strprintf("Insufficient transparent funds, have %ld, need %ld plus fee %ld", t_inputs_total, t_outputs_total, minersFee));

    if (isfromzaddr_ && (z_inputs_total < targetAmount))
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strprintf("Insufficient protected funds, have %ld, need %ld plus fee %ld", z_inputs_total, t_outputs_total, minersFee));


    // If from address is a taddr, select UTXOs to spend
    CAmount selectedUTXOAmount = 0;
    if (isfromtaddr_) {
        std::vector<SendManyInputUTXO> selectedTInputs;
        for (SendManyInputUTXO & t : t_inputs_) {
            selectedUTXOAmount += std::get<2>(t);
            selectedTInputs.push_back(t);
            if (selectedUTXOAmount >= targetAmount)
                break;
        }
        t_inputs_ = selectedTInputs;
        t_inputs_total = selectedUTXOAmount;

        // update the transaction with these inputs
        CMutableTransaction rawTx(tx_);
        for (SendManyInputUTXO & t : t_inputs_) {
            uint256 txid = std::get<0>(t);
            int vout = std::get<1>(t);
            CAmount amount = std::get<2>(t);
            CTxIn in(COutPoint(txid, vout));
            rawTx.vin.push_back(in);
        }
        tx_ = CTransaction(rawTx);

//        std::cout << "Added " << t_inputs_.size() << " utxos with total value " << t_inputs_total << " to the transaction" << std::endl;
    }


    //
    // Construct JoinSplit
    //
    AsyncJoinSplitInfo info;

    CAmount funds = 0;
    CAmount fundsSpent = 0;

    if (isfromtaddr_) {
        funds += selectedUTXOAmount;
    }

    if (isfromzaddr_) {
        SendManyInputNPT o = z_inputs_[0];
        NotePlaintext npt = o.first;
        CAmount noteFunds = o.second;

        libzcash::Note inputNote = npt.note(frompaymentaddress_);
        uint256 inputCommitment = inputNote.cm();
        info.notes.push_back(inputNote);
        info.commitments.push_back(inputCommitment);
        info.keys.push_back(spendingkey_);

        funds += noteFunds;

        // Do we need a second note (if available) ?
        if (funds<targetAmount && z_inputs_.size()>=2) {
            SendManyInputNPT o = z_inputs_[1];
            NotePlaintext npt = o.first;
            CAmount noteFunds = o.second;

            libzcash::Note inputNote = npt.note(frompaymentaddress_);
            uint256 inputCommitment = inputNote.cm();
            info.notes.push_back(inputNote);
            info.commitments.push_back(inputCommitment);
            info.keys.push_back(spendingkey_);

            funds += noteFunds;
        }
    }

    // Set up the movement of transparent funds across value pool
    info.vpub_old = 0;
    info.vpub_new = 0;

    // If source of funds is a taddr, we need to take from the value pool, zOutputsTotal, and consume it in the joinsplit
    if (isfromtaddr_) {
        info.vpub_old = z_outputs_total;
    }

    // If source of funds ia a zaddr, we need to add to the value pool, the miners fee and any taddr outputs.
    if (isfromzaddr_) {
        info.vpub_new += minersFee;
        info.vpub_new += t_outputs_total;
    }

    // Transparent outputs add to the value pool
    if (t_outputs_total > 0) {
        add_taddr_outputs_to_tx();
        fundsSpent += t_outputs_total;
    }

    // Alpha limitation: for now we are dealing with just one zaddr as output
    if (z_outputs_total > 0) {
        SendManyRecipient smr = z_outputs_[0];
        std::string address = std::get<0>(smr);
        CAmount value = std::get<1>(smr);
        std::string hexMemo = std::get<2>(smr);

        PaymentAddress pa = CZCPaymentAddress(address).Get();
        JSOutput jso = JSOutput(pa, value);

        if (hexMemo.size() > 0) {
            std::vector<unsigned char> rawMemo = ParseHex(hexMemo.c_str());
            boost::array<unsigned char, ZC_MEMO_SIZE> memo = {{0x00}};
            if (rawMemo.size() > ZC_MEMO_SIZE)
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Memo size of %d is too big, maximum allowed is %d", rawMemo.size(), ZC_MEMO_SIZE));

            int lenMemo = rawMemo.size();
            for (int i = 0; i < ZC_MEMO_SIZE && i < lenMemo; i++) {
                memo[i] = rawMemo[i];
            }

            jso.memo = memo;
        }

        info.vjsout.push_back(jso);

        fundsSpent += value;
    }

    // Miners fee will be consumed from the value pool
    fundsSpent += minersFee;

    // Change will flow back to sender address
    CAmount change = funds - fundsSpent;
    if (change < 0)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strprintf("Insufficient funds or internal error, spent too much leaving negative change %ld", change));
    if (change > 0) {
        if (isfromzaddr_) {
            info.vjsout.push_back(JSOutput(frompaymentaddress_, change));
        } else if (isfromtaddr_) {
            CMutableTransaction rawTx(tx_);
            CScript scriptPubKey = GetScriptForDestination(fromtaddr_.Get());
            CTxOut out(change, scriptPubKey);
            rawTx.vout.push_back(out);
            tx_ = CTransaction(rawTx);
        }
    }

    // Update the raw transaction
    Object obj;
    if (isPureTaddrOnlyTx) {
        obj.push_back(Pair("rawtxn", EncodeHexTx(tx_)));
    } else {
        obj = perform_joinsplit(info);
    }

    // Sign the raw transaction
    Value rawtxnValue = find_value(obj, "rawtxn");
    if (rawtxnValue.is_null()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for raw transaction");
    }
    std::string rawtxn = rawtxnValue.get_str();

    Value signResultValue = signrawtransaction({Value(rawtxn)}, false);
    Object signResultObject = signResultValue.get_obj();
    Value completeValue = find_value(signResultObject, "complete");
    bool complete = completeValue.get_bool();
    if (!complete) {
        // TODO: Maybe get "errors" and print array vErrors into a string
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Failed to sign transaction");
    }

    Value hexValue = find_value(signResultObject, "hex");
    if (hexValue.is_null()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for signed transaction");
    }
    std::string signedtxn = hexValue.get_str();

    // Send the signed transaction
    if (!testmode) {
        Value sendResultValue = sendrawtransaction({Value(signedtxn)}, false);
        if (sendResultValue.is_null()) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Send raw transaction did not return an error or a txid.");
        }

        std::string txid = sendResultValue.get_str();

        Object o;
        o.push_back(Pair("txid", txid));
        o.push_back(Pair("hex", signedtxn));
        setResult(Value(o));
    } else {
        // Test mode does not send the transaction to the network.

        CDataStream stream(ParseHex(signedtxn), SER_NETWORK, PROTOCOL_VERSION);
        CTransaction tx;
        stream >> tx;

        Object o;
        o.push_back(Pair("test", 1));
        o.push_back(Pair("txid", tx.GetTxid().ToString()));
        o.push_back(Pair("hex", signedtxn));
        setResult(Value(o));
    }

    return true;
}


bool AsyncRPCOperation_sendmany::find_utxos() {
    set<CBitcoinAddress> setAddress = {fromtaddr_};
    vector<COutput> vecOutputs;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);

    BOOST_FOREACH(const COutput& out, vecOutputs) {
        if (out.nDepth < mindepth_)
            continue;

        if (setAddress.size()) {
            CTxDestination address;
            if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
                continue;

            if (!setAddress.count(address))
                continue;
        }

        // TODO: Also examine out.fSpendable ?
        CAmount nValue = out.tx->vout[out.i].nValue;
        SendManyInputUTXO utxo(out.tx->GetTxid(), out.i, nValue);
        t_inputs_.push_back(utxo);
    }
    return t_inputs_.size() > 0;
}


bool AsyncRPCOperation_sendmany::find_unspent_notes() {

    LOCK2(cs_main, pwalletMain->cs_wallet);

    for (auto & p : pwalletMain->mapWallet) {
        CWalletTx wtx = p.second;

        // Filter the transactions before checking for notes
        if (!CheckFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < mindepth_)
            continue;

        mapNoteData_t mapNoteData = pwalletMain->FindMyNotes(wtx);

        if (mapNoteData.size() == 0)
            continue;

        for (auto & pair : mapNoteData) {
            JSOutPoint jsop = pair.first;
            CNoteData nd = pair.second;

            PaymentAddress pa = nd.address;

            // skip notes which belong to a different payment address in the wallet
            if (!(pa == frompaymentaddress_))
                continue;

            int i = jsop.js; // Index into CTransaction.vjoinsplit
            int j = jsop.n; // Index into JSDescription.ciphertexts

            // determine amount of funds in the note and if it has been spent
            ZCNoteDecryption decryptor(spendingkey_.viewing_key());
            auto hSig = wtx.vjoinsplit[i].h_sig(*pzcashParams, wtx.joinSplitPubKey);
            try {
                NotePlaintext plaintext = NotePlaintext::decrypt(
                        decryptor,
                        wtx.vjoinsplit[i].ciphertexts[j],
                        wtx.vjoinsplit[i].ephemeralKey,
                        hSig,
                        (unsigned char) j);

                uint256 nullifier = plaintext.note(frompaymentaddress_).nullifier(spendingkey_);
                bool isSpent = pwalletMain->IsSpent(nullifier);

                if (isSpent)
                    continue;

                z_inputs_.push_back(SendManyInputNPT(plaintext, CAmount(plaintext.value)));

#if 0
                std::cout << "Found note at txid     : " << wtx.GetTxid().ToString() << std::endl;
                std::cout << "...    vjoinsplit index: " << i << std::endl;
                std::cout << "... jsdescription index: " << j << std::endl;
                std::cout << "...     payment address: " << CZCPaymentAddress(pa).ToString() << std::endl;
                std::cout << "...               spent: " << isSpent << std::endl;
                std::string data(plaintext.memo.begin(), plaintext.memo.end());
                std::cout << "...                memo: " << HexStr(data) << std::endl;
                std::cout << "...              amount: " << FormatMoney(plaintext.value, false) << std::endl;
#endif

            } catch (const std::exception &) {
                // Couldn't decrypt with this spending key
            }
        }
    }

    if (z_inputs_.size() == 0)
        return false;

    // sort in descending order, so big notes appear first
    std::sort(z_inputs_.begin(), z_inputs_.end(), [](SendManyInputNPT i, SendManyInputNPT j) -> bool {
        return (i.second > j.second);
    });

    return true;
}

Object AsyncRPCOperation_sendmany::perform_joinsplit(AsyncJoinSplitInfo & info) {
    std::vector<boost::optional < ZCIncrementalWitness>> witnesses;
    uint256 anchor;

    // Lock critical section (accesses blockchain)
    {
        LOCK(cs_main);
        pwalletMain->WitnessNoteCommitment(info.commitments, witnesses, anchor);
    }
    // Unlock critical section


    if (!(witnesses.size() == info.notes.size()) || !(info.notes.size() == info.keys.size()))
        throw runtime_error("number of notes and witnesses and keys do not match");

    for (size_t i = 0; i < witnesses.size(); i++) {
        if (!witnesses[i]) {
            throw runtime_error("joinsplit input could not be found in tree");
        }
        info.vjsin.push_back(JSInput(*witnesses[i], info.notes[i], info.keys[i]));
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


    // Start to make joinsplit
    uint256 joinSplitPubKey;
    unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(joinSplitPubKey.begin(), joinSplitPrivKey);

    CMutableTransaction mtx(tx_);
    mtx.nVersion = 2;
    mtx.joinSplitPubKey = joinSplitPubKey;

#if 0
    std::cout << "number of existing joinsplits in tx = " << mtx.vjoinsplit.size() << std::endl;
    std::cout << "vpub_old: " << info.vpub_old << std::endl;
    std::cout << "vpub_new: " << info.vpub_new << std::endl;
    for (JSInput & o : info.vjsin)
        std::cout << " in: " << o.note.value << std::endl;
    for (JSOutput & o : info.vjsout)
        std::cout << "out: " << o.value << std::endl;
#endif

    // Generate the proof, this can take over a minute.
    JSDescription jsdesc(*pzcashParams,
            joinSplitPubKey,
            anchor,
            {info.vjsin[0], info.vjsin[1]},
            {info.vjsout[0], info.vjsout[1]},
            info.vpub_old,
            info.vpub_new);

    if (!(jsdesc.Verify(*pzcashParams, joinSplitPubKey)))
        throw std::runtime_error("error verifying joinsplt");

    mtx.vjoinsplit.push_back(jsdesc);

    // Empty output script.
    CScript scriptCode;
    CTransaction signTx(mtx);
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL);

    // Add the signature
    if (!(crypto_sign_detached(&mtx.joinSplitSig[0], NULL,
            dataToBeSigned.begin(), 32,
            joinSplitPrivKey
            ) == 0))
        throw std::runtime_error("crypto_sign_detached failed");

    // Sanity check
    if (!(crypto_sign_verify_detached(&mtx.joinSplitSig[0],
            dataToBeSigned.begin(), 32,
            mtx.joinSplitPubKey.begin()
            ) == 0))
        throw std::runtime_error("crypto_sign_verify_detached failed");

    CTransaction rawTx(mtx);
    tx_ = rawTx;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;

    std::string encryptedNote1;
    std::string encryptedNote2;
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char) 0x00);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[0];
        ss2 << jsdesc.h_sig(*pzcashParams, joinSplitPubKey);

        encryptedNote1 = HexStr(ss2.begin(), ss2.end());
    }
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char) 0x01);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[1];
        ss2 << jsdesc.h_sig(*pzcashParams, joinSplitPubKey);

        encryptedNote2 = HexStr(ss2.begin(), ss2.end());
    }

    Object obj;
    obj.push_back(Pair("encryptednote1", encryptedNote1));
    obj.push_back(Pair("encryptednote2", encryptedNote2));
    obj.push_back(Pair("rawtxn", HexStr(ss.begin(), ss.end())));
    return obj;
}

void AsyncRPCOperation_sendmany::add_taddr_outputs_to_tx() {

    CMutableTransaction rawTx(tx_);

    for (SendManyRecipient & r : t_outputs_) {
        std::string outputAddress = std::get<0>(r);
        CAmount nAmount = std::get<1>(r);

        CBitcoinAddress address(outputAddress);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid output address, not a valid taddr.");

        CScript scriptPubKey = GetScriptForDestination(address.Get());

        CTxOut out(nAmount, scriptPubKey);
        rawTx.vout.push_back(out);
    }

    tx_ = CTransaction(rawTx);
}

