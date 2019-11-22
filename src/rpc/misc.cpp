// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "clientversion.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpc/server.h"
#include "timedata.h"
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
            "  \"timeoffset\": xxxxx,        (numeric) the time offset\n"
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
    obj.push_back(Pair("version", CLIENT_VERSION));
    obj.push_back(Pair("protocolversion", PROTOCOL_VERSION));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
        obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    }
#endif
    obj.push_back(Pair("blocks",        (int)chainActive.Height()));
    obj.push_back(Pair("timeoffset",    GetTimeOffset()));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("proxy",         (proxy.IsValid() ? proxy.proxy.ToStringIPPort() : string())));
    obj.push_back(Pair("difficulty",    (double)GetDifficulty()));
    obj.push_back(Pair("testnet",       Params().TestnetToBeDeprecatedFieldRPC()));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
        obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
    }
    if (pwalletMain && pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    obj.push_back(Pair("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK())));
#endif
    obj.push_back(Pair("relayfee",      ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
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
        obj.push_back(Pair("isscript", false));
        if (pwalletMain && pwalletMain->GetPubKey(keyID, vchPubKey)) {
            obj.push_back(Pair("pubkey", HexStr(vchPubKey)));
            obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        }
        return obj;
    }

    UniValue operator()(const CScriptID &scriptID) const {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        if (pwalletMain && pwalletMain->GetCScript(scriptID, subscript)) {
            std::vector<CTxDestination> addresses;
            txnouttype whichType;
            int nRequired;
            ExtractDestinations(subscript, whichType, addresses, nRequired);
            obj.push_back(Pair("script", GetTxnOutputType(whichType)));
            obj.push_back(Pair("hex", HexStr(subscript.begin(), subscript.end())));
            UniValue a(UniValue::VARR);
            for (const CTxDestination& addr : addresses) {
                a.push_back(EncodeDestination(addr));
            }
            obj.push_back(Pair("addresses", a));
            if (whichType == TX_MULTISIG)
                obj.push_back(Pair("sigsrequired", nRequired));
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

    CTxDestination dest = DecodeDestination(params[0].get_str());
    bool isValid = IsValidDestination(dest);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        std::string currentAddress = EncodeDestination(dest);
        ret.push_back(Pair("address", currentAddress));

        CScript scriptPubKey = GetScriptForDestination(dest);
        ret.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end())));

#ifdef ENABLE_WALLET
        isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;
        ret.push_back(Pair("ismine", (mine & ISMINE_SPENDABLE) ? true : false));
        ret.push_back(Pair("iswatchonly", (mine & ISMINE_WATCH_ONLY) ? true: false));
        UniValue detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
        ret.pushKVs(detail);
        if (pwalletMain && pwalletMain->mapAddressBook.count(dest))
            ret.push_back(Pair("account", pwalletMain->mapAddressBook[dest].name));
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
        obj.push_back(Pair("type", "sprout"));
        obj.push_back(Pair("payingkey", zaddr.a_pk.GetHex()));
        obj.push_back(Pair("transmissionkey", zaddr.pk_enc.GetHex()));
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            obj.push_back(Pair("ismine", pwalletMain->HaveSproutSpendingKey(zaddr)));
        }
#endif
        return obj;
    }

    UniValue operator()(const libzcash::SaplingPaymentAddress &zaddr) const {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("type", "sapling"));
        obj.push_back(Pair("diversifier", HexStr(zaddr.d)));
        obj.push_back(Pair("diversifiedtransmissionkey", zaddr.pk_d.GetHex()));
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            libzcash::SaplingIncomingViewingKey ivk;
            libzcash::SaplingFullViewingKey fvk;
            bool isMine = pwalletMain->GetSaplingIncomingViewingKey(zaddr, ivk) &&
                pwalletMain->GetSaplingFullViewingKey(ivk, fvk) &&
                pwalletMain->HaveSaplingSpendingKey(fvk);
            obj.push_back(Pair("ismine", isMine));
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

    string strAddress = params[0].get_str();
    auto address = DecodePaymentAddress(strAddress);
    bool isValid = IsValidPaymentAddress(address);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        ret.push_back(Pair("address", strAddress));
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
    std::vector<CPubKey> pubkeys;
    pubkeys.resize(keys.size());
    for (unsigned int i = 0; i < keys.size(); i++)
    {
        const std::string& ks = keys[i].get_str();
#ifdef ENABLE_WALLET
        // Case 1: Bitcoin address and we have full public key:
        CTxDestination dest = DecodeDestination(ks);
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

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("address", EncodeDestination(innerID)));
    result.push_back(Pair("redeemScript", HexStr(inner.begin(), inner.end())));

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

    CTxDestination destination = DecodeDestination(strAddress);
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

// insightexplorer
static bool getAddressFromIndex(
    int type, const uint160 &hash, std::string &address)
{
    if (type == CScript::P2SH) {
        address = EncodeDestination(CScriptID(hash));
    } else if (type == CScript::P2PKH) {
        address = EncodeDestination(CKeyID(hash));
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
    if (dest.type() == typeid(CKeyID)) {
        auto x = boost::get<CKeyID>(&dest);
        memcpy(&hashBytes, x->begin(), 20);
        type = CScript::P2PKH;
        return true;
    }
    if (dest.type() == typeid(CScriptID)) {
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
    for (const auto& it : param_addresses) {
        CTxDestination address = DecodeDestination(it);
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
    std::string enableArg = "insightexplorer";
    bool enabled = fExperimentalMode && fInsightExplorer;
    std::string disabledMsg = "";
    if (!enabled) {
        disabledMsg = experimentalDisabledHelpMsg("getaddressmempool", enableArg);
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

    if (!enabled) {
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
        delta.push_back(Pair("address", address));
        delta.push_back(Pair("txid", it.first.txhash.GetHex()));
        delta.push_back(Pair("index", (int)it.first.index));
        delta.push_back(Pair("satoshis", it.second.amount));
        delta.push_back(Pair("timestamp", it.second.time));
        if (it.second.amount < 0) {
            delta.push_back(Pair("prevtxid", it.second.prevhash.GetHex()));
            delta.push_back(Pair("prevout", (int)it.second.prevout));
        }
        result.push_back(delta);
    }
    return result;
}

// insightexplorer
UniValue getaddressutxos(const UniValue& params, bool fHelp)
{
    std::string enableArg = "insightexplorer";
    bool enabled = fExperimentalMode && fInsightExplorer;
    std::string disabledMsg = "";
    if (!enabled) {
        disabledMsg = experimentalDisabledHelpMsg("getaddressutxos", enableArg);
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

    if (!enabled) {
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

        output.push_back(Pair("address", address));
        output.push_back(Pair("txid", it.first.txhash.GetHex()));
        output.push_back(Pair("outputIndex", (int)it.first.index));
        output.push_back(Pair("script", HexStr(it.second.script.begin(), it.second.script.end())));
        output.push_back(Pair("satoshis", it.second.satoshis));
        output.push_back(Pair("height", it.second.blockHeight));
        utxos.push_back(output);
    }

    if (!includeChainInfo)
        return utxos;

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("utxos", utxos));

    LOCK(cs_main);  // for chainActive
    result.push_back(Pair("hash", chainActive.Tip()->GetBlockHash().GetHex()));
    result.push_back(Pair("height", (int)chainActive.Height()));
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
    std::string enableArg = "insightexplorer";
    bool enabled = fExperimentalMode && fInsightExplorer;
    std::string disabledMsg = "";
    if (!enabled) {
        disabledMsg = experimentalDisabledHelpMsg("getaddressdeltas", enableArg);
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

    if (!enabled) {
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
        delta.push_back(Pair("address", address));
        delta.push_back(Pair("blockindex", (int)it.first.txindex));
        delta.push_back(Pair("height", it.first.blockHeight));
        delta.push_back(Pair("index", (int)it.first.index));
        delta.push_back(Pair("satoshis", it.second));
        delta.push_back(Pair("txid", it.first.txhash.GetHex()));
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
        startInfo.push_back(Pair("hash", chainActive[start]->GetBlockHash().GetHex()));
        endInfo.push_back(Pair("hash", chainActive[end]->GetBlockHash().GetHex()));
    }
    startInfo.push_back(Pair("height", start));
    endInfo.push_back(Pair("height", end));

    result.push_back(Pair("deltas", deltas));
    result.push_back(Pair("start", startInfo));
    result.push_back(Pair("end", endInfo));

    return result;
}

// insightexplorer
UniValue getaddressbalance(const UniValue& params, bool fHelp)
{
    std::string enableArg = "insightexplorer";
    bool enabled = fExperimentalMode && fInsightExplorer;
    std::string disabledMsg = "";
    if (!enabled) {
        disabledMsg = experimentalDisabledHelpMsg("getaddressbalance", enableArg);
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

    if (!enabled) {
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
    result.push_back(Pair("balance", balance));
    result.push_back(Pair("received", received));
    return result;
}

// insightexplorer
UniValue getaddresstxids(const UniValue& params, bool fHelp)
{
    std::string enableArg = "insightexplorer";
    bool enabled = fExperimentalMode && fInsightExplorer;
    std::string disabledMsg = "";
    if (!enabled) {
        disabledMsg = experimentalDisabledHelpMsg("getaddresstxids", enableArg);
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

    if (!enabled) {
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
    std::string enableArg = "insightexplorer";
    bool enabled = fExperimentalMode && fInsightExplorer;
    std::string disabledMsg = "";
    if (!enabled) {
        disabledMsg = experimentalDisabledHelpMsg("getspentinfo", enableArg);
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

    if (!enabled) {
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

    if (!GetSpentIndex(key, value)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get spent info");
    }
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("txid", value.txid.GetHex()));
    obj.push_back(Pair("index", (int)value.inputIndex));
    obj.push_back(Pair("height", value.blockHeight));

    return obj;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "control",            "getinfo",                &getinfo,                true  }, /* uses wallet if enabled */
    { "util",               "validateaddress",        &validateaddress,        true  }, /* uses wallet if enabled */
    { "util",               "z_validateaddress",      &z_validateaddress,      true  }, /* uses wallet if enabled */
    { "util",               "createmultisig",         &createmultisig,         true  },
    { "util",               "verifymessage",          &verifymessage,          true  },

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
