// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "asyncrpcqueue.h"
#include "amount.h"
#include "consensus/upgrades.h"
#include "core_io.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "script/interpreter.h"
#include "utiltime.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "sodium.h"
#include "miner.h"
#include "wallet/paymentdisclosuredb.h"

#include <array>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

#include "asyncrpcoperation_shieldcoinbase.h"

using namespace libzcash;

static int find_output(UniValue obj, int n) {
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

AsyncRPCOperation_shieldcoinbase::AsyncRPCOperation_shieldcoinbase(
        TransactionBuilder builder,
        CMutableTransaction contextualTx,
        std::vector<ShieldCoinbaseUTXO> inputs,
        std::string toAddress,
        CAmount fee,
        UniValue contextInfo) :
        builder_(builder), tx_(contextualTx), inputs_(inputs), fee_(fee), contextinfo_(contextInfo)
{
    assert(contextualTx.nVersion >= 2);  // transaction format version must support vjoinsplit

    if (fee < 0 || fee > MAX_MONEY) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Fee is out of range");
    }

    if (inputs.size() == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Empty inputs");
    }

    //  Check the destination address is valid for this network i.e. not testnet being used on mainnet
    auto address = DecodePaymentAddress(toAddress);
    if (IsValidPaymentAddress(address)) {
        tozaddr_ = address;
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to address");
    }

    // Log the context info
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_shieldcoinbase initialized (context=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_shieldcoinbase initialized\n", getId());
    }

    // Lock UTXOs
    lock_utxos();

    // Enable payment disclosure if requested
    paymentDisclosureMode = fExperimentalMode && GetBoolArg("-paymentdisclosure", false);
}

AsyncRPCOperation_shieldcoinbase::~AsyncRPCOperation_shieldcoinbase() {
}

void AsyncRPCOperation_shieldcoinbase::main() {
    if (isCancelled()) {
        unlock_utxos(); // clean up
        return;
    }

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

    bool success = false;

#ifdef ENABLE_MINING
    GenerateBitcoins(false, 0, Params());
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
    GenerateBitcoins(GetBoolArg("-gen",false), GetArg("-genproclimit", 1), Params());
#endif

    stop_execution_clock();

    if (success) {
        set_state(OperationStatus::SUCCESS);
    } else {
        set_state(OperationStatus::FAILED);
    }

    std::string s = strprintf("%s: z_shieldcoinbase finished (status=%s", getId(), getStateAsString());
    if (success) {
        s += strprintf(", txid=%s)\n", tx_.GetHash().ToString());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s",s);

    unlock_utxos(); // clean up

    // !!! Payment disclosure START
    if (success && paymentDisclosureMode && paymentDisclosureData_.size()>0) {
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

bool AsyncRPCOperation_shieldcoinbase::main_impl() {

    CAmount minersFee = fee_;

    size_t numInputs = inputs_.size();

    // Check mempooltxinputlimit to avoid creating a transaction which the local mempool rejects
    size_t limit = (size_t)GetArg("-mempooltxinputlimit", 0);
    {
        LOCK(cs_main);
        if (NetworkUpgradeActive(chainActive.Height() + 1, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER)) {
            limit = 0;
        }
    }
    if (limit>0 && numInputs > limit) {
        throw JSONRPCError(RPC_WALLET_ERROR,
            strprintf("Number of inputs %d is greater than mempooltxinputlimit of %d",
            numInputs, limit));
    }

    CAmount targetAmount = 0;
    for (ShieldCoinbaseUTXO & utxo : inputs_) {
        targetAmount += utxo.amount;
    }

    if (targetAmount <= minersFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
            strprintf("Insufficient coinbase funds, have %s and miners fee is %s",
            FormatMoney(targetAmount), FormatMoney(minersFee)));
    }

    CAmount sendAmount = targetAmount - minersFee;
    LogPrint("zrpc", "%s: spending %s to shield %s with fee %s\n",
            getId(), FormatMoney(targetAmount), FormatMoney(sendAmount), FormatMoney(minersFee));

    return boost::apply_visitor(ShieldToAddress(this, sendAmount), tozaddr_);
}

bool ShieldToAddress::operator()(const libzcash::SproutPaymentAddress &zaddr) const {
    // update the transaction with these inputs
    CMutableTransaction rawTx(m_op->tx_);
    for (ShieldCoinbaseUTXO & t : m_op->inputs_) {
        CTxIn in(COutPoint(t.txid, t.vout));
        rawTx.vin.push_back(in);
    }
    m_op->tx_ = CTransaction(rawTx);

    // Prepare raw transaction to handle JoinSplits
    CMutableTransaction mtx(m_op->tx_);
    crypto_sign_keypair(m_op->joinSplitPubKey_.begin(), m_op->joinSplitPrivKey_);
    mtx.joinSplitPubKey = m_op->joinSplitPubKey_;
    m_op->tx_ = CTransaction(mtx);

    // Create joinsplit
    UniValue obj(UniValue::VOBJ);
    ShieldCoinbaseJSInfo info;
    info.vpub_old = sendAmount;
    info.vpub_new = 0;
    JSOutput jso = JSOutput(zaddr, sendAmount);
    info.vjsout.push_back(jso);
    obj = m_op->perform_joinsplit(info);

    m_op->sign_send_raw_transaction(obj);
    return true;
}


extern UniValue signrawtransaction(const UniValue& params, bool fHelp);
extern UniValue sendrawtransaction(const UniValue& params, bool fHelp);

bool ShieldToAddress::operator()(const libzcash::SaplingPaymentAddress &zaddr) const {
    m_op->builder_.SetFee(m_op->fee_);

    // Sending from a t-address, which we don't have an ovk for. Instead,
    // generate a common one from the HD seed. This ensures the data is
    // recoverable, while keeping it logically separate from the ZIP 32
    // Sapling key hierarchy, which the user might not be using.
    HDSeed seed = pwalletMain->GetHDSeedForRPC();
    uint256 ovk = ovkForShieldingFromTaddr(seed);

    // Add transparent inputs
    for (auto t : m_op->inputs_) {
        m_op->builder_.AddTransparentInput(COutPoint(t.txid, t.vout), t.scriptPubKey, t.amount);
    }

    // Send all value to the target z-addr
    m_op->builder_.SendChangeTo(zaddr, ovk);

    // Build the transaction
    m_op->tx_ = m_op->builder_.Build().GetTxOrThrow();

    // Send the transaction
    // TODO: Use CWallet::CommitTransaction instead of sendrawtransaction
    auto signedtxn = EncodeHexTx(m_op->tx_);
    if (!m_op->testmode) {
        UniValue params = UniValue(UniValue::VARR);
        params.push_back(signedtxn);
        UniValue sendResultValue = sendrawtransaction(params, false);
        if (sendResultValue.isNull()) {
            throw JSONRPCError(RPC_WALLET_ERROR, "sendrawtransaction did not return an error or a txid.");
        }

        auto txid = sendResultValue.get_str();

        UniValue o(UniValue::VOBJ);
        o.push_back(Pair("txid", txid));
        m_op->set_result(o);
    } else {
        // Test mode does not send the transaction to the network.
        UniValue o(UniValue::VOBJ);
        o.push_back(Pair("test", 1));
        o.push_back(Pair("txid", m_op->tx_.GetHash().ToString()));
        o.push_back(Pair("hex", signedtxn));
        m_op->set_result(o);
    }

    return true;
}

bool ShieldToAddress::operator()(const libzcash::InvalidEncoding& no) const {
    return false;
}


/**
 * Sign and send a raw transaction.
 * Raw transaction as hex string should be in object field "rawtxn"
 */
void AsyncRPCOperation_shieldcoinbase::sign_send_raw_transaction(UniValue obj)
{
    // Sign the raw transaction
    UniValue rawtxnValue = find_value(obj, "rawtxn");
    if (rawtxnValue.isNull()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for raw transaction");
    }
    std::string rawtxn = rawtxnValue.get_str();

    UniValue params = UniValue(UniValue::VARR);
    params.push_back(rawtxn);
    UniValue signResultValue = signrawtransaction(params, false);
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
        UniValue sendResultValue = sendrawtransaction(params, false);
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


UniValue AsyncRPCOperation_shieldcoinbase::perform_joinsplit(ShieldCoinbaseJSInfo & info) {
    uint32_t consensusBranchId;
    uint256 anchor;
    {
        LOCK(cs_main);
        consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
        anchor = pcoinsTip->GetBestAnchor(SPROUT);
    }


    if (anchor.IsNull()) {
        throw std::runtime_error("anchor is null");
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
            FormatMoney(info.vjsout[0].value), FormatMoney(info.vjsout[1].value)
            );

    // Generate the proof, this can take over a minute.
    std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs
            {info.vjsin[0], info.vjsin[1]};
    std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs
            {info.vjsout[0], info.vjsout[1]};
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
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);

    // Add the signature
    if (!(crypto_sign_detached(&mtx.joinSplitSig[0], NULL,
            dataToBeSigned.begin(), 32,
            joinSplitPrivKey_
            ) == 0))
    {
        throw std::runtime_error("crypto_sign_detached failed");
    }

    // Sanity check
    if (!(crypto_sign_verify_detached(&mtx.joinSplitSig[0],
            dataToBeSigned.begin(), 32,
            mtx.joinSplitPubKey.begin()
            ) == 0))
    {
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
        ss2 << ((unsigned char) 0x00);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[0];
        ss2 << jsdesc.h_sig(*pzcashParams, joinSplitPubKey_);

        encryptedNote1 = HexStr(ss2.begin(), ss2.end());
    }
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char) 0x01);
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
        libzcash::SproutPaymentAddress zaddr = output.addr;  // randomized output
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

/**
 * Override getStatus() to append the operation's context object to the default status object.
 */
UniValue AsyncRPCOperation_shieldcoinbase::getStatus() const {
    UniValue v = AsyncRPCOperation::getStatus();
    if (contextinfo_.isNull()) {
        return v;
    }

    UniValue obj = v.get_obj();
    obj.push_back(Pair("method", "z_shieldcoinbase"));
    obj.push_back(Pair("params", contextinfo_ ));
    return obj;
}

/**
 * Lock input utxos
 */
 void AsyncRPCOperation_shieldcoinbase::lock_utxos() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : inputs_) {
        COutPoint outpt(utxo.txid, utxo.vout);
        pwalletMain->LockCoin(outpt);
    }
}

/**
 * Unlock input utxos
 */
void AsyncRPCOperation_shieldcoinbase::unlock_utxos() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : inputs_) {
        COutPoint outpt(utxo.txid, utxo.vout);
        pwalletMain->UnlockCoin(outpt);
    }
}
