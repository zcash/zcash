// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "rpc/server.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet.h"
#include "wallet/paymentdisclosure.h"
#include "wallet/paymentdisclosuredb.h"

#include <fstream>
#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <univalue.h>

#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"

using namespace std;
using namespace libzcash;

// Function declaration for function implemented in wallet/rpcwallet.cpp
bool EnsureWalletIsAvailable(bool avoidException);

/**
 * RPC call to generate a payment disclosure
 */
UniValue z_getpaymentdisclosure(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    string enableArg = "paymentdisclosure";
    auto fEnablePaymentDisclosure = fExperimentalMode && GetBoolArg("-" + enableArg, false);
    string strPaymentDisclosureDisabledMsg = "";
    if (!fEnablePaymentDisclosure) {
        strPaymentDisclosureDisabledMsg = experimentalDisabledHelpMsg("z_getpaymentdisclosure", enableArg);
    }

    if (fHelp || params.size() < 3 || params.size() > 4 )
        throw runtime_error(
            "z_getpaymentdisclosure \"txid\" \"js_index\" \"output_index\" (\"message\") \n"
            "\nGenerate a payment disclosure for a given joinsplit output.\n"
            "\nEXPERIMENTAL FEATURE\n"
            + strPaymentDisclosureDisabledMsg +
            "\nArguments:\n"
            "1. \"txid\"            (string, required) \n"
            "2. \"js_index\"        (string, required) \n"
            "3. \"output_index\"    (string, required) \n"
            "4. \"message\"         (string, optional) \n"
            "\nResult:\n"
            "\"paymentdisclosure\"  (string) Hex data string, with \"zpd:\" prefix.\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getpaymentdisclosure", "96f12882450429324d5f3b48630e3168220e49ab7b0f066e5c2935a6b88bb0f2 0 0 \"refund\"")
            + HelpExampleRpc("z_getpaymentdisclosure", "\"96f12882450429324d5f3b48630e3168220e49ab7b0f066e5c2935a6b88bb0f2\", 0, 0, \"refund\"")
        );

    if (!fEnablePaymentDisclosure) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: payment disclosure is disabled.");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    // Check wallet knows about txid
    string txid = params[0].get_str();
    uint256 hash;
    hash.SetHex(txid);

    CTransaction tx;
    uint256 hashBlock;

    // Check txid has been seen
    if (!GetTransaction(hash, tx, Params().GetConsensus(), hashBlock, true)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
    }

    // Check tx has been confirmed
    if (hashBlock.IsNull()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Transaction has not been confirmed yet");
    }

    // Check is mine
    if (!pwalletMain->mapWallet.count(hash)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Transaction does not belong to the wallet");
    }
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    // Check if shielded tx
    if (wtx.vJoinSplit.empty()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Transaction is not a shielded transaction");        
    }

    // Check js_index
    int js_index = params[1].get_int();
    if (js_index < 0 || js_index >= wtx.vJoinSplit.size()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid js_index");
    }

    // Check output_index
    int output_index = params[2].get_int();
    if (output_index < 0 || output_index >= ZC_NUM_JS_OUTPUTS) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid output_index");
    }

    // Get message if it exists
    string msg;
    if (params.size() == 4) {
        msg = params[3].get_str();
    }

    // Create PaymentDisclosureKey
    PaymentDisclosureKey key = {hash, (size_t)js_index, (uint8_t)output_index };

    // TODO: In future, perhaps init the DB in init.cpp
    shared_ptr<PaymentDisclosureDB> db = PaymentDisclosureDB::sharedInstance();
    PaymentDisclosureInfo info;
    if (!db->Get(key, info)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Could not find payment disclosure info for the given joinsplit output");
    }

    PaymentDisclosure pd( wtx.joinSplitPubKey, key, info, msg );
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pd;
    string strHex = HexStr(ss.begin(), ss.end());
    return PAYMENT_DISCLOSURE_BLOB_STRING_PREFIX + strHex;
}



/**
 * RPC call to validate a payment disclosure data blob.
 */
UniValue z_validatepaymentdisclosure(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    string enableArg = "paymentdisclosure";
    auto fEnablePaymentDisclosure = fExperimentalMode && GetBoolArg("-" + enableArg, false);
    string strPaymentDisclosureDisabledMsg = "";
    if (!fEnablePaymentDisclosure) {
        strPaymentDisclosureDisabledMsg = experimentalDisabledHelpMsg("z_validatepaymentdisclosure", enableArg);
    }

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_validatepaymentdisclosure \"paymentdisclosure\"\n"
            "\nValidates a payment disclosure.\n"
            "\nEXPERIMENTAL FEATURE\n"
            + strPaymentDisclosureDisabledMsg +
            "\nArguments:\n"
            "1. \"paymentdisclosure\"     (string, required) Hex data string, with \"zpd:\" prefix.\n"
            "\nExamples:\n"
            + HelpExampleCli("z_validatepaymentdisclosure", "\"zpd:706462ff004c561a0447ba2ec51184e6c204...\"")
            + HelpExampleRpc("z_validatepaymentdisclosure", "\"zpd:706462ff004c561a0447ba2ec51184e6c204...\"")
        );

    if (!fEnablePaymentDisclosure) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: payment disclosure is disabled.");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    // Verify the payment disclosure input begins with "zpd:" prefix.
    string strInput = params[0].get_str();
    size_t pos = strInput.find(PAYMENT_DISCLOSURE_BLOB_STRING_PREFIX);
    if (pos != 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, payment disclosure prefix not found.");
    }
    string hexInput = strInput.substr(strlen(PAYMENT_DISCLOSURE_BLOB_STRING_PREFIX));
    if (!IsHex(hexInput))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected payment disclosure data in hexadecimal format.");
    }

    // Unserialize the payment disclosure data into an object
    PaymentDisclosure pd;
    CDataStream ss(ParseHex(hexInput), SER_NETWORK, PROTOCOL_VERSION);
    try {
        ss >> pd;
        // too much data is ignored, but if not enough data, exception of type ios_base::failure is thrown,
        // CBaseDataStream::read(): end of data: iostream error
    } catch (const std::exception &e) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, payment disclosure data is malformed.");        
    }

    if (pd.payload.marker != PAYMENT_DISCLOSURE_PAYLOAD_MAGIC_BYTES) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, payment disclosure marker not found.");
    }

    if (pd.payload.version != PAYMENT_DISCLOSURE_VERSION_EXPERIMENTAL) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Payment disclosure version is unsupported.");
    }

    uint256 hash = pd.payload.txid;
    CTransaction tx;
    uint256 hashBlock;
    // Check if we have seen the transaction
    if (!GetTransaction(hash, tx, Params().GetConsensus(), hashBlock, true)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
    }

    // Check if the transaction has been confirmed
    if (hashBlock.IsNull()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Transaction has not been confirmed yet");
    }

    // Check if shielded tx
    if (tx.vJoinSplit.empty()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Transaction is not a shielded transaction");        
    }

    UniValue errs(UniValue::VARR);
    UniValue o(UniValue::VOBJ);
    o.push_back(Pair("txid", pd.payload.txid.ToString()));

    // Check js_index
    if (pd.payload.js >= tx.vJoinSplit.size()) {
        errs.push_back("Payment disclosure refers to an invalid joinsplit index");
    }
    o.push_back(Pair("jsIndex", pd.payload.js));

    if (pd.payload.n < 0 || pd.payload.n >= ZC_NUM_JS_OUTPUTS) {
        errs.push_back("Payment disclosure refers to an invalid output index");
    }
    o.push_back(Pair("outputIndex", pd.payload.n));
    o.push_back(Pair("version", pd.payload.version));
    o.push_back(Pair("onetimePrivKey", pd.payload.esk.ToString()));
    o.push_back(Pair("message", pd.payload.message));
    o.push_back(Pair("joinSplitPubKey", tx.joinSplitPubKey.ToString()));

    // Verify the payment disclosure was signed using the same key as the transaction i.e. the joinSplitPrivKey.
    uint256 dataToBeSigned = SerializeHash(pd.payload, SER_GETHASH, 0);
    bool sigVerified = (crypto_sign_verify_detached(pd.payloadSig.data(),
        dataToBeSigned.begin(), 32,
        tx.joinSplitPubKey.begin()) == 0);
    o.push_back(Pair("signatureVerified", sigVerified));
    if (!sigVerified) {
        errs.push_back("Payment disclosure signature does not match transaction signature");        
    }
   
    // Check the payment address is valid
    SproutPaymentAddress zaddr = pd.payload.zaddr;
    {
        o.push_back(Pair("paymentAddress", EncodePaymentAddress(zaddr)));

        try {
            // Decrypt the note to get value and memo field
            JSDescription jsdesc = tx.vJoinSplit[pd.payload.js];
            uint256 h_sig = jsdesc.h_sig(*pzcashParams, tx.joinSplitPubKey);

            ZCPaymentDisclosureNoteDecryption decrypter;

            ZCNoteEncryption::Ciphertext ciphertext = jsdesc.ciphertexts[pd.payload.n];

            uint256 pk_enc = zaddr.pk_enc;
            auto plaintext = decrypter.decryptWithEsk(ciphertext, pk_enc, pd.payload.esk, h_sig, pd.payload.n);

            CDataStream ssPlain(SER_NETWORK, PROTOCOL_VERSION);
            ssPlain << plaintext;
            SproutNotePlaintext npt;
            ssPlain >> npt;

            string memoHexString = HexStr(npt.memo().data(), npt.memo().data() + npt.memo().size());
            o.push_back(Pair("memo", memoHexString));
            o.push_back(Pair("value", ValueFromAmount(npt.value())));
            
            // Check the blockchain commitment matches decrypted note commitment
            uint256 cm_blockchain =  jsdesc.commitments[pd.payload.n];
            SproutNote note = npt.note(zaddr);
            uint256 cm_decrypted = note.cm();
            bool cm_match = (cm_decrypted == cm_blockchain);
            o.push_back(Pair("commitmentMatch", cm_match));
            if (!cm_match) {
                errs.push_back("Commitment derived from payment disclosure does not match blockchain commitment");
            }
        } catch (const std::exception &e) {
            errs.push_back(string("Error while decrypting payment disclosure note: ") + string(e.what()) );
        }
    }

    bool isValid = errs.empty();
    o.push_back(Pair("valid", isValid));
    if (!isValid) {
        o.push_back(Pair("errors", errs));
    }

    return o;
}
