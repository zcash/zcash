// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_RPC_CLIENT_H
#define BITCOIN_RPC_CLIENT_H

#include <tl/expected.hpp>
#include <univalue.h>

class UnknownRPCMethod {
 public:
    UnknownRPCMethod() {};
};

class WrongNumberOfArguments {
 public:
    std::vector<bool>::size_type requiredParams;
    std::vector<bool>::size_type optionalParams;
    std::vector<std::string>::size_type providedArgs;

    WrongNumberOfArguments(
            std::vector<bool>::size_type requiredParams,
            std::vector<bool>::size_type optionalParams,
            std::vector<std::string>::size_type providedArgs) :
        requiredParams(requiredParams),
        optionalParams(optionalParams),
        providedArgs(providedArgs) {}
};

class UnparseableArgument {
 public:
    std::string unparsedArg;

    UnparseableArgument(std::string unparsedArg) :
        unparsedArg(unparsedArg) {}
};

typedef std::variant<
    UnknownRPCMethod,
    WrongNumberOfArguments,
    UnparseableArgument
    > ConversionFailure;

// TODO: This should be closer to the leaves, but don’t have a good place for it, since it’s
//       currently shared by bitcoin-cli and tests.
std::string FormatConversionFailure(const std::string& method, const ConversionFailure& failure);

/** Convert strings to command-specific RPC representation */
tl::expected<UniValue, ConversionFailure>
    RPCConvertValues(const std::string& method, const std::vector<std::string>& strArgs);

/** Non-RFC4627 JSON parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
std::optional<UniValue> ParseNonRFCJSONValue(const std::string& strVal);

#endif // BITCOIN_RPC_CLIENT_H
