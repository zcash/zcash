// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RPC_COMMON_H
#define ZCASH_RPC_COMMON_H

#include "rpc/client.h"
#include "rpc/protocol.h"
#include "util/match.h"
#include "util/system.h"

#include <set>
#include <stdint.h>

#include <univalue.h>

using namespace std;

typedef map<string, pair<vector<bool>, vector<bool>>> CRPCConvertTable;

/** A string parameter, should not be converted. */
static const bool s = false;

/** Something other than a string parameter, should be converted. */
static const bool o = true;

/**
 * A table of operation parameters. The first element is the name of an RPC operation. All RPC
 * operations must be in this list. The second element is a pair of vectors – one for the required
 * parameters of the operation, one for the optional parameters. Each vector contains booleans
 * indicating whether the incoming string needs to be parsed into a JSON value (`true`) or not.
 */
static const CRPCConvertTable rpcCvtTable =
{
    // operation {required params, optional params}
    // blockchain
    { "getblockcount",               {{}, {}} },
    { "getbestblockhash",            {{}, {}} },
    { "getdifficulty",               {{}, {}} },
    { "getrawmempool",               {{}, {o}} },
    { "getblockdeltas",              {{s}, {}} },
    { "getblockhashes",              {{o, o}, {o}} },
    { "getblockhash",                {{o}, {}} },
    { "getblockheader",              {{s}, {o}} },
    { "getblock",                    {{s}, {o}} },
    { "gettxoutsetinfo",             {{}, {}} },
    { "gettxout",                    {{s, o}, {o}} },
    { "verifychain",                 {{}, {o, o}} },
    { "getblockchaininfo",           {{}, {}} },
    { "getchaintips",                {{}, {}} },
    { "z_gettreestate",              {{s}, {}} },
    { "getmempoolinfo",              {{}, {}} },
    { "invalidateblock",             {{s}, {}} },
    { "reconsiderblock",             {{s}, {}} },
    // mining
    { "getlocalsolps",               {{}, {}} },
    { "getnetworksolps",             {{}, {o, o}} },
    { "getnetworkhashps",            {{}, {o, o}} },
    { "getgenerate",                 {{}, {}} },
    { "generate",                    {{o}, {}} },
    { "setgenerate",                 {{o}, {o}} },
    { "getmininginfo",               {{}, {}} },
    { "prioritisetransaction",       {{s, o, o}, {}} },
    { "getblocktemplate",            {{}, {o}} },
    { "submitblock",                 {{s}, {o}} },
    { "estimatefee",                 {{o}, {}} },
    { "estimatepriority",            {{o}, {}} },
    { "getblocksubsidy",             {{o}, {}} },
    // misc
    { "getinfo",                     {{}, {}} },
    { "validateaddress",             {{s}, {}} },
    { "z_validateaddress",           {{s}, {}} },
    { "createmultisig",              {{o, o}, {}} },
    { "verifymessage",               {{s, s ,s}, {}} },
    { "setmocktime",                 {{o}, {}} },
    { "getexperimentalfeatures",     {{}, {}} },
    { "getaddressmempool",           {{o}, {}} },
    { "getaddressutxos",             {{o}, {}} },
    { "getaddressdeltas",            {{o}, {}} },
    { "getaddressbalance",           {{o}, {}} },
    { "getaddresstxids",             {{o}, {}} },
    { "getspentinfo",                {{o}, {}} },
    { "getmemoryinfo",               {{}, {}} },
    // net
    { "getconnectioncount",          {{}, {}} },
    { "ping",                        {{}, {}} },
    { "getpeerinfo",                 {{}, {}} },
    { "addnode",                     {{s, s}, {}} },
    { "disconnectnode",              {{s}, {}} },
    { "getaddednodeinfo",            {{o}, {s}} },
    { "getnettotals",                {{}, {}} },
    { "getdeprecationinfo",          {{}, {}} },
    { "getnetworkinfo",              {{}, {}} },
    { "setban",                      {{s, s}, {o, o}} },
    { "listbanned",                  {{}, {}} },
    { "clearbanned",                 {{}, {}} },
    // rawtransaction
    { "getrawtransaction",           {{s}, {o, s}} },
    { "gettxoutproof",               {{o}, {s}} },
    { "verifytxoutproof",            {{s}, {}} },
    { "createrawtransaction",        {{o, o}, {o, o}} },
    { "decoderawtransaction",        {{s}, {}} },
    { "decodescript",                {{s}, {}} },
    { "signrawtransaction",          {{s}, {o, o, s, s}} },
    { "sendrawtransaction",          {{s}, {o}} },
    // rpcdisclosure
    { "z_getpaymentdisclosure",      {{s, o, o}, {s}} },
    { "z_validatepaymentdisclosure", {{s}, {}} },
    // rpcdump
    { "importprivkey",               {{s}, {s, o}} },
    { "importaddress",               {{s}, {s, o, o}} },
    { "importpubkey",                {{s}, {s, o}} },
    { "z_importwallet",              {{s}, {}} },
    { "importwallet",                {{s}, {}} },
    { "dumpprivkey",                 {{s}, {}} },
    { "z_exportwallet",              {{s}, {}} },
    { "z_importkey",                 {{s}, {s, o}} },
    { "z_importviewingkey",          {{s}, {s, o}} },
    { "z_exportkey",                 {{s}, {}} },
    { "z_exportviewingkey",          {{s}, {}} },
    // rpcwallet
    { "getnewaddress",               {{}, {s}} },
    { "getrawchangeaddress",         {{}, {}} },
    { "sendtoaddress",               {{s, o}, {s, s, o}} },
    { "listaddresses",               {{}, {}} },
    { "listaddressgroupings",        {{}, {o}} },
    { "signmessage",                 {{s, s}, {}} },
    { "getreceivedbyaddress",        {{s}, {o, o, o}} },
    { "getbalance",                  {{}, {s, o, o, o, o}} },
    { "sendmany",                    {{s, o}, {o, s, o}} },
    { "addmultisigaddress",          {{o, o}, {s}} },
    { "listreceivedbyaddress",       {{}, {o, o, o, s, o, o}} },
    { "listtransactions",            {{}, {s, o, o, o, o}} },
    { "listsinceblock",              {{}, {s, o, o, o, o, o}} },
    { "gettransaction",              {{s}, {o, o, o}} },
    { "backupwallet",                {{s}, {}} },
    { "keypoolrefill",               {{}, {o}} },
    { "walletpassphrase",            {{s, o}, {}} },
    { "walletpassphrasechange",      {{s, s}, {}} },
    { "walletconfirmbackup",         {{s}, {}} },
    { "walletlock",                  {{}, {}} },
    { "encryptwallet",               {{s}, {}} },
    { "lockunspent",                 {{o, o}, {}} },
    { "listlockunspent",             {{}, {}} },
    { "settxfee",                    {{o}, {}} },
    { "getwalletinfo",               {{}, {o}} },
    { "resendwallettransactions",    {{}, {}} },
    { "listunspent",                 {{}, {o, o, o, o, o, o}} },
    { "z_listunspent",               {{}, {o, o, o, o, o}} },
    { "fundrawtransaction",          {{s}, {o}} },
    { "zcsamplejoinsplit",           {{}, {}} },
    { "zcbenchmark",                 {{s, o}, {}} },
    { "z_getnewaddress",             {{}, {s}} },
    { "z_getnewaccount",             {{}, {}} },
    { "z_getaddressforaccount",      {{o}, {o, o}} },
    { "z_listaccounts",              {{}, {}} },
    { "z_listaddresses",             {{}, {o}} },
    { "z_listunifiedreceivers",      {{s}, {}} },
    { "z_listreceivedbyaddress",     {{s}, {o, o}} },
    { "z_getbalance",                {{s}, {o, o}} },
    { "z_getbalanceforviewingkey",   {{s}, {o, o}} },
    { "z_getbalanceforaccount",      {{o}, {o, o}} },
    { "z_gettotalbalance",           {{}, {o, o}} },
    { "z_viewtransaction",           {{s}, {}} },
    { "z_getoperationresult",        {{}, {o}} },
    { "z_getoperationstatus",        {{}, {o}} },
    { "z_sendmany",                  {{s, o}, {o, o, s}} },
    { "z_setmigration",              {{o}, {}} },
    { "z_getmigrationstatus",        {{}, {o}} },
    { "z_shieldcoinbase",            {{s, s}, {o, o}} },
    { "z_mergetoaddress",            {{o, s}, {o, o, o, s}} },
    { "z_listoperationids",          {{}, {s}} },
    { "z_getnotescount",             {{}, {o, o}} },
    // server
    { "help",                        {{}, {s}} },
    { "setlogfilter",                {{s}, {}} },
    { "stop",                        {{}, {o}} },
};

#endif // ZCASH_RPC_COMMON_H
