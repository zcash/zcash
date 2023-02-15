// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "rpc/client.h"
#include "rpc/protocol.h"
#include "util/system.h"

#include <set>
#include <stdint.h>

#include <univalue.h>

using namespace std;

typedef map<string, set<int>> CRPCConvertTable;

static const CRPCConvertTable rpcCvtTable =
{
    { "stop", {0} },
    { "setmocktime", {0} },
    { "getaddednodeinfo", {0} },
    { "setgenerate", {0, 1} },
    { "generate", {0} },
    { "getnetworkhashps", {0, 1} },
    { "getnetworksolps", {0, 1} },
    { "sendtoaddress", {1, 4} },
    { "settxfee", {0} },
    { "getreceivedbyaddress", {1, 2} },
    { "listreceivedbyaddress", {0, 1, 2} },
    { "getbalance", {1, 2, 3} },
    { "getblockhash", {0} },
    { "listtransactions", {1, 2, 3} },
    { "walletpassphrase", {1} },
    { "getblocktemplate", {0} },
    { "listsinceblock", {1, 2} },
    { "sendmany", {1, 2, 4} },
    { "addmultisigaddress", {0, 1} },
    { "createmultisig", {0, 1} },
    { "listunspent", {0, 1, 2} },
    { "getblock", {1} },
    { "getblockheader", {1} },
    { "gettransaction", {1} },
    { "getrawtransaction", {1} },
    { "createrawtransaction", {0, 1, 2, 3} },
    { "signrawtransaction", {1, 2} },
    { "sendrawtransaction", {1} },
    { "fundrawtransaction", {1} },
    { "gettxout", {1, 2} },
    { "gettxoutproof", {0} },
    { "lockunspent", {0, 1} },
    { "importprivkey", {2} },
    { "importaddress", {2, 3} },
    { "importpubkey", {2} },
    { "verifychain", {0, 1} },
    { "keypoolrefill", {0} },
    { "getrawmempool", {0} },
    { "estimatefee", {0} },
    { "estimatepriority", {0} },
    { "prioritisetransaction", {1, 2} },
    { "setban", {2, 3} },
    { "getspentinfo", {0} },
    { "getaddresstxids", {0} },
    { "getaddressbalance", {0} },
    { "getaddressdeltas", {0} },
    { "getaddressutxos", {0} },
    { "getaddressmempool", {0} },
    { "getblockhashes", {0, 1, 2} },
    { "getblockdeltas", {0} },
    { "zcbenchmark", {1, 2} },
    { "getblocksubsidy", {0} },
    { "z_listaddresses", {0} },
    { "z_listreceivedbyaddress", {1} },
    { "z_listunspent", {0, 1, 3} },
    { "z_getaddressforaccount", {0, 1, 2} },
    { "z_getbalance", {1, 2} },
    { "z_getbalanceforaccount", {0, 1} },
    { "z_getbalanceforaddress", {1} },
    { "z_gettotalbalance", {0, 1, 2} },
    { "z_mergetoaddress", {0, 2, 3, 4} },
    { "z_sendmany", {1, 2, 3} },
    { "z_shieldcoinbase", {2, 3} },
    { "z_getoperationstatus", {0} },
    { "z_getoperationresult", {0} },
    { "z_importkey", {2} },
    { "z_importviewingkey", {2} },
    { "z_getpaymentdisclosure", {1, 2} },
    { "z_setmigration", {0} },
    { "z_getnotescount", {0} },
};

set<int> paramsToConvertFor(const string& method) {
    auto search = rpcCvtTable.find(method);
    if (search != rpcCvtTable.end()) {
        return search->second;
    } else {
        return set<int>();
    }
}

/** Non-RFC4627 JSON parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
UniValue ParseNonRFCJSONValue(const string& strVal)
{
    UniValue jVal;
    if (!jVal.read(string("[")+strVal+string("]")) ||
        !jVal.isArray() || jVal.size()!=1)
        throw runtime_error(string("Error parsing JSON:")+strVal);
    return jVal[0];
}

/** Convert strings to command-specific RPC representation */
UniValue RPCConvertValues(const string &strMethod, const vector<string> &strParams)
{
    UniValue params(UniValue::VARR);

    auto paramsToConvert = paramsToConvertFor(strMethod);
    for (vector<int>::size_type idx = 0; idx < strParams.size(); idx++) {
        const string& strVal = strParams[idx];

        params.push_back(
                paramsToConvert.count(idx) != 0
                // parse string as JSON, insert bool/number/object/etc. value
                ? ParseNonRFCJSONValue(strVal)
                // insert string value directly
                : strVal);
    }

    return params;
}
