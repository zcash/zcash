// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "clientversion.h"
#include "init.h"
#include "key_io.h"
#include "experimental_features.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpc/server.h"
#include "txmempool.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include <univalue.h>

#include "zcash/Address.hpp"

using namespace std;

/**
 * @note Do not add or change anything in the information returned by this
 * method. `getinfo` exists for backwards-compatibility only. It combines
 * information from wildly different sources in the program, which is a mess,
 * and is thus planned to be deprecated eventually.
 *
 * Based on the source of the information, new information should be added to:
 * - `getblockchaininfo`,
 * - `getnetworkinfo` or
 * - `getwalletinfo`
 *
 * Or alternatively, create a specific query method for the information.
 **/
UniValue getinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getinfo\n"
            "Returns an object containing various state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,           (numeric) the server version\n"
            "  \"protocolversion\": xxxxx,   (numeric) the protocol version\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total Zcash balance of the wallet\n"
            "  \"blocks\": xxxxxx,           (numeric) the current number of blocks processed in the server\n"
            "  \"timeoffset\": xxxxx,        (numeric) the time offset (deprecated; always 0)\n"
            "  \"connections\": xxxxx,       (numeric) the number of connections\n"
            "  \"proxy\": \"host:port\",     (string, optional) the proxy used by the server\n"
            "  \"difficulty\": xxxxxx,       (numeric) the current difficulty\n"
            "  \"testnet\": true|false,      (boolean) if the server is using testnet or not\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee set in " + CURRENCY_UNIT + "/kB\n"
            "  \"relayfee\": x.xxxx,         (numeric) minimum relay fee for non-free transactions in " + CURRENCY_UNIT + "/kB\n"
            "  \"errors\": \"...\"           (string) any error messages\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getinfo", "")
            + HelpExampleRpc("getinfo", "")
        );

#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwalletMain ? &pwalletMain->cs_wallet : NULL);
#else
    LOCK(cs_main);
#endif

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("version", CLIENT_VERSION);
    obj.pushKV("protocolversion", PROTOCOL_VERSION);
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.pushKV("walletversion", pwalletMain->GetVersion());
        obj.pushKV("balance",       ValueFromAmount(pwalletMain->GetBalance()));
    }
#endif
    obj.pushKV("blocks",        (int)chainActive.Height());
    obj.pushKV("timeoffset",    0);
    obj.pushKV("connections",   (int)vNodes.size());
    obj.pushKV("proxy",         (proxy.IsValid() ? proxy.proxy.ToStringIPPort() : string()));
    obj.pushKV("difficulty",    (double)GetDifficulty());
    obj.pushKV("testnet",       Params().TestnetToBeDeprecatedFieldRPC());
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.pushKV("keypoololdest", pwalletMain->GetOldestKeyPoolTime());
        obj.pushKV("keypoolsize",   (int)pwalletMain->GetKeyPoolSize());
    }
    if (pwalletMain && pwalletMain->IsCrypted())
        obj.pushKV("unlocked_until", nWalletUnlockTime);
    obj.pushKV("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK()));
#endif
    obj.pushKV("relayfee",      ValueFromAmount(::minRelayTxFee.GetFeePerK()));
    auto warnings = GetWarnings("statusbar");
    obj.pushKV("errors",           warnings.first);
    obj.pushKV("errorstimestamp",  warnings.second);
    return obj;
}

#ifdef ENABLE_WALLET
class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    UniValue operator()(const CNoDestination &dest) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const CKeyID &keyID) const {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        obj.pushKV("isscript", false);
        if (pwalletMain && pwalletMain->GetPubKey(keyID, vchPubKey)) {
            obj.pushKV("pubkey", HexStr(vchPubKey));
            obj.pushKV("iscompressed", vchPubKey.IsCompressed());
        }
        return obj;
    }

    UniValue operator()(const CScriptID &scriptID) const {
        KeyIO keyIO(Params());
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.pushKV("isscript", true);
        if (pwalletMain && pwalletMain->GetCScript(scriptID, subscript)) {
            std::vector<CTxDestination> addresses;
            txnouttype whichType;
            int nRequired;
            ExtractDestinations(subscript, whichType, addresses, nRequired);
            obj.pushKV("script", GetTxnOutputType(whichType));
            obj.pushKV("hex", HexStr(subscript.begin(), subscript.end()));
            UniValue a(UniValue::VARR);
            for (const CTxDestination& addr : addresses) {
                a.push_back(keyIO.EncodeDestination(addr));
            }
            obj.pushKV("addresses", a);
            if (whichType == TX_MULTISIG)
                obj.pushKV("sigsrequired", nRequired);
        }
        return obj;
    }
};
#endif

UniValue validateaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "validateaddress \"zcashaddress\"\n"
            "\nReturn information about the given Zcash address.\n"
            "\nArguments:\n"
            "1. \"zcashaddress\"     (string, required) The Zcash address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,         (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"zcashaddress\",   (string) The Zcash address validated\n"
            "  \"scriptPubKey\" : \"hex\",       (string) The hex encoded scriptPubKey generated by the address\n"
            "  \"ismine\" : true|false,          (boolean) If the address is yours or not\n"
            "  \"isscript\" : true|false,        (boolean) If the key is a script\n"
            "  \"pubkey\" : \"publickeyhex\",    (string) The hex value of the raw public key\n"
            "  \"iscompressed\" : true|false,    (boolean) If the address is compressed\n"
            "  \"account\" : \"account\"         (string) DEPRECATED. The account associated with the address, \"\" is the default account\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
            + HelpExampleRpc("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
        );

#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwalletMain ? &pwalletMain->cs_wallet : NULL);
#else
    LOCK(cs_main);
#endif

    KeyIO keyIO(Params());
    CTxDestination dest = keyIO.DecodeDestination(params[0].get_str());
    bool isValid = IsValidDestination(dest);

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("isvalid", isValid);
    if (isValid)
    {
        std::string currentAddress = keyIO.EncodeDestination(dest);
        ret.pushKV("address", currentAddress);

        CScript scriptPubKey = GetScriptForDestination(dest);
        ret.pushKV("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end()));

#ifdef ENABLE_WALLET
        isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;
        ret.pushKV("ismine", (mine & ISMINE_SPENDABLE) ? true : false);
        ret.pushKV("iswatchonly", (mine & ISMINE_WATCH_ONLY) ? true: false);
        UniValue detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
        ret.pushKVs(detail);
        if (pwalletMain && pwalletMain->mapAddressBook.count(dest))
            ret.pushKV("account", pwalletMain->mapAddressBook[dest].name);
#endif
    }
    return ret;
}


class DescribePaymentAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    UniValue operator()(const libzcash::InvalidEncoding &zaddr) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const libzcash::SproutPaymentAddress &zaddr) const {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("type", "sprout");
        obj.pushKV("payingkey", zaddr.a_pk.GetHex());
        obj.pushKV("transmissionkey", zaddr.pk_enc.GetHex());
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            obj.pushKV("ismine", HaveSpendingKeyForPaymentAddress(pwalletMain)(zaddr));
        }
#endif
        return obj;
    }

    UniValue operator()(const libzcash::SaplingPaymentAddress &zaddr) const {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("type", "sapling");
        obj.pushKV("diversifier", HexStr(zaddr.d));
        obj.pushKV("diversifiedtransmissionkey", zaddr.pk_d.GetHex());
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            obj.pushKV("ismine", HaveSpendingKeyForPaymentAddress(pwalletMain)(zaddr));
        }
#endif
        return obj;
    }
};

UniValue z_validateaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_validateaddress \"zaddr\"\n"
            "\nReturn information about the given z address.\n"
            "\nArguments:\n"
            "1. \"zaddr\"     (string, required) The z address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,      (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"zaddr\",         (string) The z address validated\n"
            "  \"type\" : \"xxxx\",             (string) \"sprout\" or \"sapling\"\n"
            "  \"ismine\" : true|false,       (boolean) If the address is yours or not\n"
            "  \"payingkey\" : \"hex\",         (string) [sprout] The hex value of the paying key, a_pk\n"
            "  \"transmissionkey\" : \"hex\",   (string) [sprout] The hex value of the transmission key, pk_enc\n"
            "  \"diversifier\" : \"hex\",       (string) [sapling] The hex value of the diversifier, d\n"
            "  \"diversifiedtransmissionkey\" : \"hex\", (string) [sapling] The hex value of pk_d\n"

            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_validateaddress", "\"zcWsmqT4X2V4jgxbgiCzyrAfRT1vi1F4sn7M5Pkh66izzw8Uk7LBGAH3DtcSMJeUb2pi3W4SQF8LMKkU2cUuVP68yAGcomL\"")
            + HelpExampleRpc("z_validateaddress", "\"zcWsmqT4X2V4jgxbgiCzyrAfRT1vi1F4sn7M5Pkh66izzw8Uk7LBGAH3DtcSMJeUb2pi3W4SQF8LMKkU2cUuVP68yAGcomL\"")
        );


#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwalletMain->cs_wallet);
#else
    LOCK(cs_main);
#endif

    KeyIO keyIO(Params());
    string strAddress = params[0].get_str();
    auto address = keyIO.DecodePaymentAddress(strAddress);
    bool isValid = IsValidPaymentAddress(address);

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("isvalid", isValid);
    if (isValid)
    {
        ret.pushKV("address", strAddress);
        UniValue detail = boost::apply_visitor(DescribePaymentAddressVisitor(), address);
        ret.pushKVs(detail);
    }
    return ret;
}


/**
 * Used by addmultisigaddress / createmultisig:
 */
CScript _createmultisig_redeemScript(const UniValue& params)
{
    int nRequired = params[0].get_int();
    const UniValue& keys = params[1].get_array();

    // Gather public keys
    if (nRequired < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if ((int)keys.size() < nRequired)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %u keys, but need at least %d to redeem)", keys.size(), nRequired));
    if (keys.size() > 16)
        throw runtime_error("Number of addresses involved in the multisignature address creation > 16\nReduce the number");

    KeyIO keyIO(Params());

    std::vector<CPubKey> pubkeys;
    pubkeys.resize(keys.size());
    for (unsigned int i = 0; i < keys.size(); i++)
    {
        const std::string& ks = keys[i].get_str();
#ifdef ENABLE_WALLET
        // Case 1: Bitcoin address and we have full public key:
        CTxDestination dest = keyIO.DecodeDestination(ks);
        if (pwalletMain && IsValidDestination(dest)) {
            const CKeyID *keyID = boost::get<CKeyID>(&dest);
            if (!keyID) {
                throw std::runtime_error(strprintf("%s does not refer to a key", ks));
            }
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(*keyID, vchPubKey)) {
                throw std::runtime_error(strprintf("no full public key for address %s", ks));
            }
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }

        // Case 2: hex public key
        else
#endif
        if (IsHex(ks))
        {
            CPubKey vchPubKey(ParseHex(ks));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }
        else
        {
            throw runtime_error(" Invalid public key: "+ks);
        }
    }
    CScript result = GetScriptForMultisig(nRequired, pubkeys);

    if (result.size() > MAX_SCRIPT_ELEMENT_SIZE)
        throw runtime_error(
                strprintf("redeemScript exceeds size limit: %d > %d", result.size(), MAX_SCRIPT_ELEMENT_SIZE));

    return result;
}

UniValue createmultisig(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 2)
    {
        string msg = "createmultisig nrequired [\"key\",...]\n"
            "\nCreates a multi-signature address with n signature of m keys required.\n"
            "It returns a json object with the address and redeemScript.\n"

            "\nArguments:\n"
            "1. nrequired      (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keys\"       (string, required) A json array of keys which are Zcash addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"key\"    (string) Zcash address or hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",  (string) The value of the new multisig address.\n"
            "  \"redeemScript\":\"script\"       (string) The string value of the hex-encoded redemption script.\n"
            "}\n"

            "\nExamples:\n"
            "\nCreate a multisig address from 2 addresses\n"
            + HelpExampleCli("createmultisig", "2 \"[\\\"t16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"t171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("createmultisig", "2, \"[\\\"t16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"t171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);

    KeyIO keyIO(Params());
    UniValue result(UniValue::VOBJ);
    result.pushKV("address", keyIO.EncodeDestination(innerID));
    result.pushKV("redeemScript", HexStr(inner.begin(), inner.end()));

    return result;
}

UniValue verifymessage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "verifymessage \"zcashaddress\" \"signature\" \"message\"\n"
            "\nVerify a signed message\n"
            "\nArguments:\n"
            "1. \"zcashaddress\"    (string, required) The Zcash address to use for the signature.\n"
            "2. \"signature\"       (string, required) The signature provided by the signer in base 64 encoding (see signmessage).\n"
            "3. \"message\"         (string, required) The message that was signed.\n"
            "\nResult:\n"
            "true|false   (boolean) If the signature is verified or not.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("verifymessage", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\", \"signature\", \"my message\"")
        );

    LOCK(cs_main);

    string strAddress  = params[0].get_str();
    string strSign     = params[1].get_str();
    string strMessage  = params[2].get_str();

    KeyIO keyIO(Params());
    CTxDestination destination = keyIO.DecodeDestination(strAddress);
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = boost::get<CKeyID>(&destination);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == *keyID);
}

UniValue setmocktime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "setmocktime timestamp\n"
            "\nSet the local time to given timestamp (-regtest only)\n"
            "\nArguments:\n"
            "1. timestamp  (integer, required) Unix seconds-since-epoch timestamp\n"
            "   Pass 0 to go back to using the system time."
        );

    if (!Params().MineBlocksOnDemand())
        throw runtime_error("setmocktime for regression testing (-regtest mode) only");

    // cs_vNodes is locked and node send/receive times are updated
    // atomically with the time change to prevent peers from being
    // disconnected because we think we haven't communicated with them
    // in a long time.
    LOCK2(cs_main, cs_vNodes);

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));
    SetMockTime(params[0].get_int64());

    uint64_t t = GetTime();
    BOOST_FOREACH(CNode* pnode, vNodes) {
        pnode->nLastSend = pnode->nLastRecv = t;
    }

    return NullUniValue;
}

UniValue getexperimentalfeatures(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getexperimentalfeatures\n"
            "\nReturns enabled experimental features.\n"
            "\nResult:\n"
            "  [\n"
            "     \"experimentalfeature\"     (string) The enabled experimental feature\n"
            "     ,...\n"
            "  ],\n"            "\nExamples:\n"
            + HelpExampleCli("getexperimentalfeatures", "")
            + HelpExampleRpc("getexperimentalfeatures", "")
        );

    LOCK(cs_main);
    UniValue experimentalfeatures(UniValue::VARR);
    for (auto& enabledfeature : GetExperimentalFeatures()) {
        experimentalfeatures.push_back(enabledfeature);
    }
    return experimentalfeatures;
}

// insightexplorer
static bool getAddressFromIndex(
    int type, const uint160 &hash, std::string &address)
{
    KeyIO keyIO(Params());
    if (type == CScript::P2SH) {
        address = keyIO.EncodeDestination(CScriptID(hash));
    } else if (type == CScript::P2PKH) {
        address = keyIO.EncodeDestination(CKeyID(hash));
    } else {
        return false;
    }
    return true;
}

// This function accepts an address and returns in the output parameters
// the version and raw bytes for the RIPEMD-160 hash.
static bool getIndexKey(
    const CTxDestination& dest, uint160& hashBytes, int& type)
{
    if (!IsValidDestination(dest)) {
        return false;
    }
    if (IsKeyDestination(dest)) {
        auto x = boost::get<CKeyID>(&dest);
        memcpy(&hashBytes, x->begin(), 20);
        type = CScript::P2PKH;
        return true;
    }
    if (IsScriptDestination(dest)) {
        auto x = boost::get<CScriptID>(&dest);
        memcpy(&hashBytes, x->begin(), 20);
        type = CScript::P2SH;
        return true;
    }
    return false;
}

// insightexplorer
static bool getAddressesFromParams(
    const UniValue& params,
    std::vector<std::pair<uint160, int>> &addresses)
{
    std::vector<std::string> param_addresses;
    if (params[0].isStr()) {
        param_addresses.push_back(params[0].get_str());
    } else if (params[0].isObject()) {
        UniValue addressValues = find_value(params[0].get_obj(), "addresses");
        if (!addressValues.isArray()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                "Addresses is expected to be an array");
        }
        for (const auto& it : addressValues.getValues()) {
            param_addresses.push_back(it.get_str());
        }

    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    KeyIO keyIO(Params());
    for (const auto& it : param_addresses) {
        CTxDestination address = keyIO.DecodeDestination(it);
        uint160 hashBytes;
        int type = 0;
        if (!getIndexKey(address, hashBytes, type)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
        }
        addresses.push_back(std::make_pair(hashBytes, type));
    }
    return true;
}

// insightexplorer
UniValue getaddressmempool(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        disabledMsg = experimentalDisabledHelpMsg("getaddressmempool", {"insightexplorer", "lightwalletd"});
    }
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressmempool {\"addresses\": [\"taddr\", ...]}\n"
            "\nReturns all mempool deltas for an address.\n"
            + disabledMsg +
            "\nArguments:\n"
            "{\n"
            "  \"addresses\":\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "}\n"
            "(or)\n"
            "\"address\"  (string) The base58check encoded address\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\"  (string) The base58check encoded address\n"
            "    \"txid\"  (string) The related txid\n"
            "    \"index\"  (number) The related input or output index\n"
            "    \"satoshis\"  (number) The difference of zatoshis\n"
            "    \"timestamp\"  (number) The time the transaction entered the mempool (seconds)\n"
            "    \"prevtxid\"  (string) The previous txid (if spending)\n"
            "    \"prevout\"  (string) The previous transaction output index (if spending)\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressmempool", "'{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"]}'")
            + HelpExampleRpc("getaddressmempool", "{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"]}")
        );

    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: getaddressmempool is disabled. "
            "Run './zcash-cli help getaddressmempool' for instructions on how to enable this feature.");
    }

    std::vector<std::pair<uint160, int>> addresses;

    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta>> indexes;
    mempool.getAddressIndex(addresses, indexes);
    std::sort(indexes.begin(), indexes.end(),
        [](const std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta>& a,
           const std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta>& b) -> bool {
               return a.second.time < b.second.time;
           });

    UniValue result(UniValue::VARR);

    for (const auto& it : indexes) {
        std::string address;
        if (!getAddressFromIndex(it.first.type, it.first.addressBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }
        UniValue delta(UniValue::VOBJ);
        delta.pushKV("address", address);
        delta.pushKV("txid", it.first.txhash.GetHex());
        delta.pushKV("index", (int)it.first.index);
        delta.pushKV("satoshis", it.second.amount);
        delta.pushKV("timestamp", it.second.time);
        if (it.second.amount < 0) {
            delta.pushKV("prevtxid", it.second.prevhash.GetHex());
            delta.pushKV("prevout", (int)it.second.prevout);
        }
        result.push_back(delta);
    }
    return result;
}

// insightexplorer
UniValue getaddressutxos(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        disabledMsg = experimentalDisabledHelpMsg("getaddressutxos", {"insightexplorer", "lightwalletd"});
    }
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressutxos {\"addresses\": [\"taddr\", ...], (\"chainInfo\": true|false)}\n"
            "\nReturns all unspent outputs for an address.\n"
            + disabledMsg +
            "\nArguments:\n"
            "{\n"
            "  \"addresses\":\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ],\n"
            "  \"chainInfo\"  (boolean, optional, default=false) Include chain info with results\n"
            "}\n"
            "(or)\n"
            "\"address\"  (string) The base58check encoded address\n"
            "\nResult\n"
            "[\n"
            "  {\n"
            "    \"address\"  (string) The address base58check encoded\n"
            "    \"txid\"  (string) The output txid\n"
            "    \"height\"  (number) The block height\n"
            "    \"outputIndex\"  (number) The output index\n"
            "    \"script\"  (string) The script hex encoded\n"
            "    \"satoshis\"  (number) The number of zatoshis of the output\n"
            "  }, ...\n"
            "]\n\n"
            "(or, if chainInfo is true):\n\n"
            "{\n"
            "  \"utxos\":\n"
            "    [\n"
            "      {\n"
            "        \"address\"     (string)  The address base58check encoded\n"
            "        \"txid\"        (string)  The output txid\n"
            "        \"height\"      (number)  The block height\n"
            "        \"outputIndex\" (number)  The output index\n"
            "        \"script\"      (string)  The script hex encoded\n"
            "        \"satoshis\"    (number)  The number of zatoshis of the output\n"
            "      }, ...\n"
            "    ],\n"
            "  \"hash\"              (string)  The block hash\n"
            "  \"height\"            (numeric) The block height\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressutxos", "'{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"], \"chainInfo\": true}'")
            + HelpExampleRpc("getaddressutxos", "{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"], \"chainInfo\": true}")
            );

    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: getaddressutxos is disabled. "
            "Run './zcash-cli help getaddressutxos' for instructions on how to enable this feature.");
    }

    bool includeChainInfo = false;
    if (params[0].isObject()) {
        UniValue chainInfo = find_value(params[0].get_obj(), "chainInfo");
        if (!chainInfo.isNull()) {
            includeChainInfo = chainInfo.get_bool();
        }
    }
    std::vector<std::pair<uint160, int>> addresses;
    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    std::vector<CAddressUnspentDbEntry> unspentOutputs;
    for (const auto& it : addresses) {
        if (!GetAddressUnspent(it.first, it.second, unspentOutputs)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }
    std::sort(unspentOutputs.begin(), unspentOutputs.end(),
        [](const CAddressUnspentDbEntry& a, const CAddressUnspentDbEntry& b) -> bool {
            return a.second.blockHeight < b.second.blockHeight;
        });

    UniValue utxos(UniValue::VARR);
    for (const auto& it : unspentOutputs) {
        UniValue output(UniValue::VOBJ);
        std::string address;
        if (!getAddressFromIndex(it.first.type, it.first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        output.pushKV("address", address);
        output.pushKV("txid", it.first.txhash.GetHex());
        output.pushKV("outputIndex", (int)it.first.index);
        output.pushKV("script", HexStr(it.second.script.begin(), it.second.script.end()));
        output.pushKV("satoshis", it.second.satoshis);
        output.pushKV("height", it.second.blockHeight);
        utxos.push_back(output);
    }

    if (!includeChainInfo)
        return utxos;

    UniValue result(UniValue::VOBJ);
    result.pushKV("utxos", utxos);

    LOCK(cs_main);  // for chainActive
    result.pushKV("hash", chainActive.Tip()->GetBlockHash().GetHex());
    result.pushKV("height", (int)chainActive.Height());
    return result;
}

static void getHeightRange(const UniValue& params, int& start, int& end)
{
    start = 0;
    end = 0;
    if (params[0].isObject()) {
        UniValue startValue = find_value(params[0].get_obj(), "start");
        UniValue endValue = find_value(params[0].get_obj(), "end");
        // If either is not specified, the other is ignored.
        if (!startValue.isNull() && !endValue.isNull()) {
            start = startValue.get_int();
            end = endValue.get_int();
            if (start <= 0 || end <= 0) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                    "Start and end are expected to be greater than zero");
            }
            if (end < start) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                    "End value is expected to be greater than start");
            }
        }
    }

    LOCK(cs_main);  // for chainActive
    if (start > chainActive.Height() || end > chainActive.Height()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Start or end is outside chain range");
    }
}

// Parse an address list then fetch the corresponding addressindex information.
static void getAddressesInHeightRange(
    const UniValue& params,
    int start, int end,
    std::vector<std::pair<uint160, int>>& addresses,
    std::vector<std::pair<CAddressIndexKey, CAmount>> &addressIndex)
{
    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    for (const auto& it : addresses) {
        if (!GetAddressIndex(it.first, it.second, addressIndex, start, end)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                "No information available for address");
        }
    }
}

// insightexplorer
UniValue getaddressdeltas(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        disabledMsg = experimentalDisabledHelpMsg("getaddressdeltas", {"insightexplorer", "lightwalletd"});
    }
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressdeltas {\"addresses\": [\"taddr\", ...], (\"start\": n), (\"end\": n), (\"chainInfo\": true|false)}\n"
            "\nReturns all changes for an address.\n"
            "\nReturns information about all changes to the given transparent addresses within the given (inclusive)\n"
            "\nblock height range, default is the full blockchain.\n"
            + disabledMsg +
            "\nArguments:\n"
            "{\n"
            "  \"addresses\":\n"
            "    [\n"
            "      \"address\" (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "  \"start\"       (number, optional) The start block height\n"
            "  \"end\"         (number, optional) The end block height\n"
            "  \"chainInfo\"   (boolean, optional, default=false) Include chain info in results, only applies if start and end specified\n"
            "}\n"
            "(or)\n"
            "\"address\"       (string) The base58check encoded address\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"satoshis\"  (number) The difference of zatoshis\n"
            "    \"txid\"      (string) The related txid\n"
            "    \"index\"     (number) The related input or output index\n"
            "    \"height\"    (number) The block height\n"
            "    \"address\"   (string) The base58check encoded address\n"
            "  }, ...\n"
            "]\n\n"
            "(or, if chainInfo is true):\n\n"
            "{\n"
            "  \"deltas\":\n"
            "    [\n"
            "      {\n"
            "        \"satoshis\"    (number) The difference of zatoshis\n"
            "        \"txid\"        (string) The related txid\n"
            "        \"index\"       (number) The related input or output index\n"
            "        \"height\"      (number) The block height\n"
            "        \"address\"     (string)  The address base58check encoded\n"
            "      }, ...\n"
            "    ],\n"
            "  \"start\":\n"
            "    {\n"
            "      \"hash\"          (string)  The start block hash\n"
            "      \"height\"        (numeric) The height of the start block\n"
            "    }\n"
            "  \"end\":\n"
            "    {\n"
            "      \"hash\"          (string)  The end block hash\n"
            "      \"height\"        (numeric) The height of the end block\n"
            "    }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressdeltas", "'{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"], \"start\": 1000, \"end\": 2000, \"chainInfo\": true}'")
            + HelpExampleRpc("getaddressdeltas", "{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"], \"start\": 1000, \"end\": 2000, \"chainInfo\": true}")
        );

    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: getaddressdeltas is disabled. "
            "Run './zcash-cli help getaddressdeltas' for instructions on how to enable this feature.");
    }

    int start = 0;
    int end = 0;
    getHeightRange(params, start, end);

    std::vector<std::pair<uint160, int>> addresses;
    std::vector<std::pair<CAddressIndexKey, CAmount>> addressIndex;
    getAddressesInHeightRange(params, start, end, addresses, addressIndex);

    bool includeChainInfo = false;
    if (params[0].isObject()) {
        UniValue chainInfo = find_value(params[0].get_obj(), "chainInfo");
        if (!chainInfo.isNull()) {
            includeChainInfo = chainInfo.get_bool();
        }
    }

    UniValue deltas(UniValue::VARR);
    for (const auto& it : addressIndex) {
        std::string address;
        if (!getAddressFromIndex(it.first.type, it.first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        UniValue delta(UniValue::VOBJ);
        delta.pushKV("address", address);
        delta.pushKV("blockindex", (int)it.first.txindex);
        delta.pushKV("height", it.first.blockHeight);
        delta.pushKV("index", (int)it.first.index);
        delta.pushKV("satoshis", it.second);
        delta.pushKV("txid", it.first.txhash.GetHex());
        deltas.push_back(delta);
    }

    UniValue result(UniValue::VOBJ);

    if (!(includeChainInfo && start > 0 && end > 0)) {
        return deltas;
    }

    UniValue startInfo(UniValue::VOBJ);
    UniValue endInfo(UniValue::VOBJ);
    {
        LOCK(cs_main);  // for chainActive
        if (start > chainActive.Height() || end > chainActive.Height()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Start or end is outside chain range");
        }
        startInfo.pushKV("hash", chainActive[start]->GetBlockHash().GetHex());
        endInfo.pushKV("hash", chainActive[end]->GetBlockHash().GetHex());
    }
    startInfo.pushKV("height", start);
    endInfo.pushKV("height", end);

    result.pushKV("deltas", deltas);
    result.pushKV("start", startInfo);
    result.pushKV("end", endInfo);

    return result;
}

// insightexplorer
UniValue getaddressbalance(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        disabledMsg = experimentalDisabledHelpMsg("getaddressbalance", {"insightexplorer", "lightwalletd"});
    }
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressbalance {\"addresses\": [\"taddr\", ...]}\n"
            "\nReturns the balance for addresses.\n"
            + disabledMsg +
            "\nArguments:\n"
            "{\n"
            "  \"addresses:\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "}\n"
            "(or)\n"
            "\"address\"  (string) The base58check encoded address\n"
            "\nResult:\n"
            "{\n"
            "  \"balance\"  (string) The current balance in zatoshis\n"
            "  \"received\"  (string) The total number of zatoshis received (including change)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressbalance", "'{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"]}'")
            + HelpExampleRpc("getaddressbalance", "{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"]}")
        );

    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: getaddressbalance is disabled. "
            "Run './zcash-cli help getaddressbalance' for instructions on how to enable this feature.");
    }

    std::vector<std::pair<uint160, int>> addresses;
    std::vector<std::pair<CAddressIndexKey, CAmount>> addressIndex;
    // this method doesn't take start and end block height params, so set
    // to zero (full range, entire blockchain)
    getAddressesInHeightRange(params, 0, 0, addresses, addressIndex);

    CAmount balance = 0;
    CAmount received = 0;
    for (const auto& it : addressIndex) {
        if (it.second > 0) {
            received += it.second;
        }
        balance += it.second;
    }
    UniValue result(UniValue::VOBJ);
    result.pushKV("balance", balance);
    result.pushKV("received", received);
    return result;
}

// insightexplorer
UniValue getaddresstxids(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        disabledMsg = experimentalDisabledHelpMsg("getaddresstxids", {"insightexplorer", "lightwalletd"});
    }
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddresstxids {\"addresses\": [\"taddr\", ...], (\"start\": n), (\"end\": n)}\n"
            "\nReturns the txids for given transparent addresses within the given (inclusive)\n"
            "\nblock height range, default is the full blockchain.\n"
            + disabledMsg +
            "\nArguments:\n"
            "{\n"
            "  \"addresses\":\n"
            "    [\n"
            "      \"taddr\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "  \"start\" (number, optional) The start block height\n"
            "  \"end\" (number, optional) The end block height\n"
            "}\n"
            "(or)\n"
            "\"address\"  (string) The base58check encoded address\n"
            "\nResult:\n"
            "[\n"
            "  \"transactionid\"  (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddresstxids", "'{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"], \"start\": 1000, \"end\": 2000}'")
            + HelpExampleRpc("getaddresstxids", "{\"addresses\": [\"tmYXBYJj1K7vhejSec5osXK2QsGa5MTisUQ\"], \"start\": 1000, \"end\": 2000}")
        );

    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: getaddresstxids is disabled. "
            "Run './zcash-cli help getaddresstxids' for instructions on how to enable this feature.");
    }

    int start = 0;
    int end = 0;
    getHeightRange(params, start, end);

    std::vector<std::pair<uint160, int>> addresses;
    std::vector<std::pair<CAddressIndexKey, CAmount>> addressIndex;
    getAddressesInHeightRange(params, start, end, addresses, addressIndex);

    // This is an ordered set, sorted by height, so result also sorted by height.
    std::set<std::pair<int, std::string>> txids;

    for (const auto& it : addressIndex) {
        const int height = it.first.blockHeight;
        const std::string txid = it.first.txhash.GetHex();
        // Duplicate entries (two addresses in same tx) are suppressed
        txids.insert(std::make_pair(height, txid));
    }
    UniValue result(UniValue::VARR);
    for (const auto& it : txids) {
        // only push the txid, not the height
        result.push_back(it.second);
    }
    return result;
}

// insightexplorer
UniValue getspentinfo(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!fExperimentalInsightExplorer) {
        disabledMsg = experimentalDisabledHelpMsg("getspentinfo", {"insightexplorer"});
    }
    if (fHelp || params.size() != 1 || !params[0].isObject())
        throw runtime_error(
            "getspentinfo {\"txid\": \"txidhex\", \"index\": n}\n"
            "\nReturns the txid and index where an output is spent.\n"
            + disabledMsg +
            "\nArguments:\n"
            "{\n"
            "  \"txid\"   (string) The hex string of the txid\n"
            "  \"index\"  (number) The vout (output) index\n"
            "}\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\"   (string) The transaction id\n"
            "  \"index\"  (number) The spending (vin, input) index\n"
            "  ,...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getspentinfo", "'{\"txid\": \"33990288fb116981260be1de10b8c764f997674545ab14f9240f00346333b780\", \"index\": 4}'")
            + HelpExampleRpc("getspentinfo", "{\"txid\": \"33990288fb116981260be1de10b8c764f997674545ab14f9240f00346333b780\", \"index\": 4}")
        );

    if (!fExperimentalInsightExplorer) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: getspentinfo is disabled. "
            "Run './zcash-cli help getspentinfo' for instructions on how to enable this feature.");
    }

    UniValue txidValue = find_value(params[0].get_obj(), "txid");
    UniValue indexValue = find_value(params[0].get_obj(), "index");

    if (!txidValue.isStr())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid txid, must be a string");
    if (!indexValue.isNum())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid index, must be an integer");
    uint256 txid = ParseHashV(txidValue, "txid");
    int outputIndex = indexValue.get_int();

    CSpentIndexKey key(txid, outputIndex);
    CSpentIndexValue value;

    {
        LOCK(cs_main);
        if (!GetSpentIndex(key, value)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get spent info");
        }
    }
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("txid", value.txid.GetHex());
    obj.pushKV("index", (int)value.inputIndex);
    obj.pushKV("height", value.blockHeight);

    return obj;
}

static UniValue RPCLockedMemoryInfo()
{
    LockedPool::Stats stats = LockedPoolManager::Instance().stats();
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("used", uint64_t(stats.used)));
    obj.push_back(Pair("free", uint64_t(stats.free)));
    obj.push_back(Pair("total", uint64_t(stats.total)));
    obj.push_back(Pair("locked", uint64_t(stats.locked)));
    obj.push_back(Pair("chunks_used", uint64_t(stats.chunks_used)));
    obj.push_back(Pair("chunks_free", uint64_t(stats.chunks_free)));
    return obj;
}

UniValue getmemoryinfo(const UniValue& params, bool fHelp)
{
    /* Please, avoid using the word "pool" here in the RPC interface or help,
     * as users will undoubtedly confuse it with the other "memory pool"
     */
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmemoryinfo\n"
            "Returns an object containing information about memory usage.\n"
            "\nResult:\n"
            "{\n"
            "  \"locked\": {               (json object) Information about locked memory manager\n"
            "    \"used\": xxxxx,          (numeric) Number of bytes used\n"
            "    \"free\": xxxxx,          (numeric) Number of bytes available in current arenas\n"
            "    \"total\": xxxxxxx,       (numeric) Total number of bytes managed\n"
            "    \"locked\": xxxxxx,       (numeric) Amount of bytes that succeeded locking. If this number is smaller than total, locking pages failed at some point and key data could be swapped to disk.\n"
            "    \"chunks_used\": xxxxx,   (numeric) Number allocated chunks\n"
            "    \"chunks_free\": xxxxx,   (numeric) Number unused chunks\n"
            "  }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmemoryinfo", "")
            + HelpExampleRpc("getmemoryinfo", "")
        );
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("locked", RPCLockedMemoryInfo()));
    return obj;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "control",            "getinfo",                &getinfo,                true  }, /* uses wallet if enabled */
    { "control",            "getmemoryinfo",          &getmemoryinfo,          true  },
    { "util",               "validateaddress",        &validateaddress,        true  }, /* uses wallet if enabled */
    { "util",               "z_validateaddress",      &z_validateaddress,      true  }, /* uses wallet if enabled */
    { "util",               "createmultisig",         &createmultisig,         true  },
    { "util",               "verifymessage",          &verifymessage,          true  },
    { "control",            "getexperimentalfeatures",&getexperimentalfeatures,true  },

    // START insightexplorer
    /* Address index */
    { "addressindex",       "getaddresstxids",        &getaddresstxids,        false }, /* insight explorer */
    { "addressindex",       "getaddressbalance",      &getaddressbalance,      false }, /* insight explorer */
    { "addressindex",       "getaddressdeltas",       &getaddressdeltas,       false }, /* insight explorer */
    { "addressindex",       "getaddressutxos",        &getaddressutxos,        false }, /* insight explorer */
    { "addressindex",       "getaddressmempool",      &getaddressmempool,      true  }, /* insight explorer */
    { "blockchain",         "getspentinfo",           &getspentinfo,           false }, /* insight explorer */
    // END insightexplorer

    /* Not shown in help */
    { "hidden",             "setmocktime",            &setmocktime,            true  },
};

void RegisterMiscRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
