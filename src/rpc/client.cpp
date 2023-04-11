// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "rpc/client.h"
#include "rpc/common.h"
#include "rpc/protocol.h"
#include "util/match.h"
#include "util/system.h"

#include <set>
#include <stdint.h>

#include <univalue.h>

using namespace std;

std::string FormatConversionFailure(const std::string& method, const ConversionFailure& failure) {
    return std::visit(match {
            [&](const UnknownRPCMethod&) {
                return tinyformat::format("Unknown RPC method, %s", method);
            },
            [&](const WrongNumberOfArguments& err) {
                return tinyformat::format(
                        "%s for method, %s. Needed between %u and %u, but received %u",
                        err.providedArgs < err.requiredParams
                        ? "Not enough arguments"
                        : "Too many arguments",
                        method,
                        err.requiredParams,
                        err.requiredParams + err.optionalParams,
                        err.providedArgs);
            },
            [](const UnparseableArgument& err) {
                return std::string("Error parsing JSON:") + err.unparsedArg;
            }
        }, failure);
}

optional<pair<vector<bool>, vector<bool>>> ParamsToConvertFor(const string& method) {
    auto search = rpcCvtTable.find(method);
    if (search != rpcCvtTable.end()) {
        return search->second;
    } else {
        return nullopt;
    }
}

optional<UniValue> ParseNonRFCJSONValue(const string& strVal)
{
    UniValue jVal;
    if (jVal.read(string("[")+strVal+string("]")) && jVal.isArray() && jVal.size() == 1) {
        return jVal[0];
    } else {
        return nullopt;
    }
}

tl::expected<UniValue, ConversionFailure>
RPCConvertValues(const string &method, const vector<string> &strArgs)
{
    UniValue params(UniValue::VARR);
    auto paramsToConvert = ParamsToConvertFor(method);
    vector<bool> requiredParams{};
    vector<bool> optionalParams{};
    if (paramsToConvert.has_value()) {
        requiredParams = paramsToConvert.value().first;
        optionalParams = paramsToConvert.value().second;
    } else {
        return tl::expected<UniValue, ConversionFailure>(tl::unexpect, UnknownRPCMethod());
    }

    if (strArgs.size() < requiredParams.size()
        || requiredParams.size() + optionalParams.size() < strArgs.size()) {
        return tl::expected<UniValue, ConversionFailure>(
                tl::unexpect,
                WrongNumberOfArguments(requiredParams.size(), optionalParams.size(), strArgs.size()));
    }

    vector<bool> allParams(requiredParams.begin(), requiredParams.end());
    allParams.reserve(requiredParams.size() + optionalParams.size());
    allParams.insert(allParams.end(), optionalParams.begin(), optionalParams.end());

    for (vector<int>::size_type idx = 0; idx < strArgs.size(); idx++) {
        const bool shouldConvert = allParams[idx];
        const string& strVal = strArgs[idx];

        if (shouldConvert) {
            // parse string as JSON, insert bool/number/object/etc. value
            auto parsedArg = ParseNonRFCJSONValue(strVal);
            if (parsedArg.has_value()) {
                params.push_back(parsedArg.value());
            } else {
                return tl::expected<UniValue, ConversionFailure>(
                        tl::unexpect,
                        UnparseableArgument(strVal));
            }
        } else {
            // insert string value directly
            params.push_back(strVal);
        }
    }

    return params;
}
