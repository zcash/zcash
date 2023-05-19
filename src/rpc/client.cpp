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

std::string FormatConversionFailure(const std::string& strMethod, const ConversionFailure& failure)
{
    return examine(failure, match {
            [&](const UnknownRPCMethod&) {
                return tinyformat::format("Unknown RPC method, %s", strMethod);
            },
            [&](const WrongNumberOfParams& err) {
                return tinyformat::format(
                        "%s for method `%s`. Needed %s, but received %u",
                        err.providedParams < err.requiredParams
                        ? "Not enough parameters"
                        : "Too many parameters",
                        strMethod,
                        err.optionalParams == 0
                        ? tinyformat::format("exactly %u", err.requiredParams)
                        : tinyformat::format(
                                "at least %u and at most %u",
                                err.requiredParams,
                                err.requiredParams + err.optionalParams),
                        err.providedParams);
            },
            [](const UnparseableParam& err) {
                return std::string("Error parsing JSON: ") + err.unparsedParam;
            }
        });
}

std::optional<ParameterSpec>
ParamsToConvertFor(const std::string& strMethod)
{
    auto search = rpcCvtTable.find(strMethod);
    if (search != rpcCvtTable.end()) {
        return search->second;
    } else {
        return std::nullopt;
    }
}

std::optional<UniValue> ParseNonRFCJSONValue(const std::string& strVal)
{
    UniValue jVal;
    if (jVal.read(std::string("[")+strVal+std::string("]")) && jVal.isArray() && jVal.size() == 1) {
        return jVal[0];
    } else {
        return std::nullopt;
    }
}

tl::expected<UniValue, ConversionFailure>
RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VARR);
    auto paramsToConvert = ParamsToConvertFor(strMethod);
    if (!paramsToConvert.has_value()) {
        return tl::make_unexpected(UnknownRPCMethod());
    }
    const auto [requiredParams, optionalParams] = paramsToConvert.value();

    if (strParams.size() < requiredParams.size()
        || requiredParams.size() + optionalParams.size() < strParams.size()) {
        return tl::make_unexpected(
                WrongNumberOfParams(requiredParams.size(), optionalParams.size(), strParams.size()));
    }

    std::vector<Conversion> allParams(requiredParams.begin(), requiredParams.end());
    allParams.reserve(requiredParams.size() + optionalParams.size());
    allParams.insert(allParams.end(), optionalParams.begin(), optionalParams.end());

    for (std::vector<std::string>::size_type idx = 0; idx < strParams.size(); idx++) {
        const Conversion conversion = allParams[idx];
        const std::string& strVal = strParams[idx];

        if (conversion == Conversion::None) {
            params.push_back(strVal);
        } else {
            auto parsedParam = ParseNonRFCJSONValue(strVal);

            if (conversion == Conversion::NullableString) {
                if (parsedParam.has_value() && parsedParam.value().isNull()) {
                    params.push_back(parsedParam.value());
                } else {
                    params.push_back(strVal);
                }
            } else if (conversion == Conversion::JSON) {
                if (parsedParam.has_value()) {
                    params.push_back(parsedParam.value());
                } else {
                    return tl::make_unexpected(UnparseableParam(strVal));
                }
            } else {
                assert(false);
            }
        }
    }

    return params;
}
