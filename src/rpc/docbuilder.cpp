// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assert.h"
#include "docbuilder.h"
#include "server.h"
#include "tinyformat.h"

/* Helper functions */
std::string AgrumentTypeString(const std::string& typeString, const bool& isRequired, const std::string& defaultValue, bool showRequired) {
    std::string formattedType = "(" + typeString;
    if (isRequired) {
        if (showRequired) {
            formattedType += ", required";
        }
    } else {
        formattedType += ", optional";
        if (defaultValue.length() > 0) {
            formattedType += ", default=" + defaultValue;
        }
    }
    return formattedType + ")";
}

std::string PrependSpaces(const std::string str, const size_t& spaces) {
    return std::string(spaces, ' ') + str;
}

#include <iostream>
std::string FormattedArgumentString(
    const size_t& spaces,
    const size_t& nameLength,
    const size_t& offset,
    const std::string& formattedName,
    const std::string& formattedType,
    const std::string& description,
    const std::vector<std::string> additionalDescriptions
) {
    size_t paddedNameLength = nameLength - spaces - offset + 2;
    std::string argumentString = PrependSpaces(strprintf("%-*s%s", paddedNameLength, formattedName, formattedType), spaces);
    if (description.length() > 0) {
        argumentString += " " + description;
    }
    for (const std::string& additionalDesc : additionalDescriptions) {
        argumentString += "\n" + PrependSpaces(additionalDesc, nameLength + 2);
    }
    return argumentString;
}

/* Factory methods for aguments */
PrimitiveArgument RpcArgument::Boolean(const std::string& name, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::BOOLEAN, true, "", description);
}

PrimitiveArgument RpcArgument::Boolean(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::BOOLEAN, true, "", description);
}

PrimitiveArgument RpcArgument::OptionalBoolean(const std::string& name, const std::string& defaultValue, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::BOOLEAN, false, defaultValue, description);
}

PrimitiveArgument RpcArgument::OptionalBoolean(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::BOOLEAN, false, "", description);
}

PrimitiveArgument RpcArgument::Integer(const std::string& name, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::INTEGER, true, "", description);
}

PrimitiveArgument RpcArgument::Integer(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::INTEGER, true, "", description);
}

PrimitiveArgument RpcArgument::OptionalInteger(const std::string& name, const std::string& defaultValue, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::INTEGER, false, defaultValue, description);
}

PrimitiveArgument RpcArgument::OptionalInteger(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::INTEGER, false, "", description);
}

PrimitiveArgument RpcArgument::Decimal(const std::string& name, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::DECIMAL, true, "", description);
}

PrimitiveArgument RpcArgument::Decimal(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::DECIMAL, true, "", description);
}

PrimitiveArgument RpcArgument::OptionalDecimal(const std::string& name, const std::string& defaultValue, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::DECIMAL, false, defaultValue, description);
}

PrimitiveArgument RpcArgument::OptionalDecimal(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::DECIMAL, false, "", description);
}

PrimitiveArgument RpcArgument::String(const std::string& name, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::STRING, true, "", description);
}

PrimitiveArgument RpcArgument::String(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::STRING, true, "", description);
}

PrimitiveArgument RpcArgument::OptionalString(const std::string& name, const std::string& defaultValue, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::STRING, false, defaultValue, description);
}

PrimitiveArgument RpcArgument::OptionalString(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::STRING, false, "", description);
}

PrimitiveArgument RpcArgument::HexString(const std::string& name, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::HEX_STRING, true, "", description);
}

PrimitiveArgument RpcArgument::HexString(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::HEX_STRING, true, "", description);
}

PrimitiveArgument RpcArgument::OptionalHexString(const std::string& name, const std::string& defaultValue, const std::string& description) {
    return PrimitiveArgument(name, PrimitiveType::HEX_STRING, false, defaultValue, description);
}

PrimitiveArgument RpcArgument::OptionalHexString(const std::string& description) {
    return PrimitiveArgument("", PrimitiveType::HEX_STRING, false, "", description);
}

ArrayArgument RpcArgument::Array(const std::string name, const std::string description) {
    return ArrayArgument(name, true, description);
}

ArrayArgument RpcArgument::Array(const std::string description) {
    return ArrayArgument("", true, description);
}

ArrayArgument RpcArgument::Array() {
    return ArrayArgument("", true, "");
}

ArrayArgument RpcArgument::OptionalArray(const std::string name, const std::string description) {
    return ArrayArgument(name, false, description);
}

ArrayArgument RpcArgument::OptionalArray(const std::string description) {
    return ArrayArgument("", false, description);
}

ObjectArgument RpcArgument::Object(const std::string name, const std::string description) {
    return ObjectArgument(name, true, description);
}

ObjectArgument RpcArgument::Object(const std::string description) {
    return ObjectArgument("", true, description);
}

ObjectArgument RpcArgument::Object() {
    return ObjectArgument("", true, "");
}

ObjectArgument RpcArgument::OptionalObject(const std::string name, const std::string description) {
    return ObjectArgument(name, false, description);
}

ObjectArgument RpcArgument::OptionalObject(const std::string description) {
    return ObjectArgument("", false, description);
}

/* Primitive Argument */
std::string PrimitiveArgument::TypeString(bool pluralize) const {
    std::string typeName;
    switch(type) {
    case PrimitiveType::BOOLEAN:
        typeName = "boolean";
        break;
    case PrimitiveType::INTEGER:
    case PrimitiveType::DECIMAL:
        typeName = "numeric";
        break;
    case PrimitiveType::STRING:
    case PrimitiveType::HEX_STRING:
        typeName = "string";
        break;
    }
    if (pluralize) {
        typeName += "s";
    }
    return typeName;
}

std::string FormatAsType(const std::string& value, const PrimitiveType& type) {
    switch(type) {
    case PrimitiveType::BOOLEAN:
    case PrimitiveType::INTEGER:
    case PrimitiveType::DECIMAL:
        return value;
    case PrimitiveType::STRING:
    case PrimitiveType::HEX_STRING:
        // Don't escape empty string or strings that start with '\"'
        return value.empty() || value.find("\"") == 0 ? value : "\"" + value + "\"";
    }
}

std::string ExampleValue(const PrimitiveType& type) {
    switch(type) {
    case PrimitiveType::BOOLEAN:
        return "true|false";
    case PrimitiveType::INTEGER:
        return "n";
    case PrimitiveType::DECIMAL:
        return "x.xxx";
    case PrimitiveType::STRING:
        return "\"xxx\"";
    case PrimitiveType::HEX_STRING:
        return "\"hex\"";
    }
}

PrimitiveArgument& PrimitiveArgument::AddDescription(const std::string& description_) {
    additionalDescriptions.push_back(description_);
    return *this;
}

size_t PrimitiveArgument::ArgumentNameLength(bool argument, bool outer, bool comma, bool object) const {
    // + 2 for '""' + 2 for ': '
    size_t length = object ? name.length() + 2 + ExampleValue(type).length() + 2
        : argument || name.length() > 0 ? FormatAsType(name, type).length() : ExampleValue(type).length();
    if (argument && outer) {
        // + 3 for 'n. '
        length += 3;
    }
    if (comma) {
        length += 1;
    }
    return length;
}

std::string PrimitiveArgument::CliHeaderString(bool object) const {
    std::string headerString = object ? "\"" + name + "\": " + ExampleValue(type) : FormatAsType(name, type);
    return isRequired ? headerString : "(" + headerString + ")";
}

std::string PrimitiveArgument::ArgumentString(size_t spaces, size_t nameLength, bool argument, bool outer, bool comma, bool object) const {
    std::string formattedName = object ? "\"" + name + "\": " + ExampleValue(type)
        : argument || name.length() > 0 ? FormatAsType(name, type) : ExampleValue(type);
    if (comma) {
        formattedName += ",";
    }
    std::string formattedType = AgrumentTypeString(TypeString(), isRequired, FormatAsType(defaultValue, type), argument);
    return FormattedArgumentString(spaces, nameLength, argument && outer ? 3 : 0, formattedName, formattedType, description, additionalDescriptions);
}

/* Array Argument */
ArrayArgument& ArrayArgument::Of(const RpcArgument& innerArgument_) {
    assert(!innerArgument);
    innerArgument = innerArgument_.Clone();
    return *this;
}

ArrayArgument& ArrayArgument::AddDescription(const std::string& description_) {
    additionalDescriptions.push_back(description_);
    return *this;
}

size_t ArrayArgument::ArgumentNameLength(bool argument, bool outer, bool comma, bool object) const {
    // name for outer; '[' for inner
    // or '"name": [' for object
    size_t nameLength = object ? name.length() + 5 : outer ? name.length() : 1;
    // min of 5 for '  ...'
    size_t innerArgumentLength = std::max((size_t) 5, innerArgument->ArgumentNameLength(argument, false, true) + 2);
    if (argument && outer) {
        // outer += 3 for 'n. '; inner += 5 for extra indentation
        nameLength += 3;
        innerArgumentLength += 5;
    }
    return std::max(nameLength, innerArgumentLength);
}

std::string ArrayArgument::TypeString(bool pluralize) const {
    std::string typeName = "json array";
    if (pluralize) {
        typeName += "s";
    }
    return typeName + " of " + innerArgument->TypeString(true);
}

std::string ArrayArgument::CliHeaderString(bool object) const {
    std::string headerString = "[" + innerArgument->CliHeaderString(false) + ", ...]";
    if (object) {
        headerString = "\"" + name + "\"" + ": " + headerString;
    }
    return isRequired ? headerString : "(" + headerString + ")";
}

std::string ArrayArgument::ArgumentString(size_t spaces, size_t nameLength, bool argument, bool outer, bool comma, bool object) const {
    std::string argumentStr;
    std::string formattedType = AgrumentTypeString(TypeString(), isRequired, defaultValue, argument);
    if (argument && outer) {
        argumentStr += FormattedArgumentString(spaces, nameLength, 3, name, formattedType, description, additionalDescriptions) + "\n";
        spaces += 5;
        argumentStr += PrependSpaces("[", spaces) + "\n";
    } else {
        std::string formattedName = object ? "\"" + name + "\": [" : "[";
        argumentStr += FormattedArgumentString(spaces, nameLength, 0, formattedName, formattedType, description, additionalDescriptions) + "\n";
    }
    argumentStr += innerArgument->ArgumentString(spaces + 2, nameLength, argument, false, true) + "\n"
        + PrependSpaces("...", spaces + 2) + "\n"
        + PrependSpaces("]", spaces);
    if (comma) {
        argumentStr += ",";
    }
    return argumentStr;
}

/* Object argument */
ObjectArgument::ObjectArgument(const ObjectArgument& other)
    : RpcArgument(other.name, other.isRequired, other.defaultValue, other.description, other.additionalDescriptions) {
    for (RpcArgument* argument : other.innerArguments) {
        innerArguments.push_back(argument->Clone());
    }
}

ObjectArgument::~ObjectArgument() {
    for (RpcArgument* argument : innerArguments) {
        delete argument;
    }
}

ObjectArgument& ObjectArgument::With(const RpcArgument& innerArgument) {
    innerArguments.push_back(innerArgument.Clone());
    return *this;
}

ObjectArgument& ObjectArgument::AddDescription(const std::string& description_) {
    additionalDescriptions.push_back(description_);
    return *this;
}

size_t ObjectArgument::ArgumentNameLength(bool argument, bool outer, bool comma, bool object) const {
    // name for outer; '{' for inner
    // or '"name": {' for object
    size_t nameLength = object ? name.length() + 5 : outer ? name.length() : 1;
    size_t maxInnerLength = 0;
    for (size_t i = 0; i < innerArguments.size(); ++i) {
        bool innerComma = i != innerArguments.size() - 1;
        maxInnerLength = std::max(maxInnerLength, innerArguments[i]->ArgumentNameLength(argument, false, innerComma, true) + 2);
    }
    if (argument && outer) {
        // outer += 3 for 'n. '; inner += 5 for extra indentation
        nameLength += 3;
        maxInnerLength += 5;
    }
    return std::max(nameLength, maxInnerLength);
}

std::string ObjectArgument::TypeString(bool pluralize) const {
    std::string typeName = "json object";
    if (pluralize) {
        typeName += "s";
    }
    return typeName;
}

std::string ObjectArgument::CliHeaderString(bool object) const {
    std::string headerString = "{";
    for (size_t i = 0; i < innerArguments.size() - 1; ++i) {
        headerString += innerArguments[i]->CliHeaderString(true) + ", ";
    }
    if (innerArguments.size() > 0) {
        headerString += innerArguments[innerArguments.size() - 1]->CliHeaderString(true);
    }
    headerString += "}";
    if (object) {
        headerString = "\"" + name + "\": " + headerString;
    }
    return isRequired ? headerString : "(" + headerString + ")";
}

std::string ObjectArgument::ArgumentString(size_t spaces, size_t nameLength, bool argument, bool outer, bool comma, bool object) const {
    std::string argumentStr;
    std::string formattedType = AgrumentTypeString(TypeString(), isRequired, defaultValue, argument);
    if (argument && outer) {
        argumentStr += FormattedArgumentString(spaces, nameLength, 3, name, formattedType, description, additionalDescriptions) + "\n";
        spaces += 5;
        argumentStr += PrependSpaces("{", spaces) + "\n";
    } else {
        std::string formattedName = object ? "\"" + name + "\": {" : "{";
        argumentStr += FormattedArgumentString(spaces, nameLength, 0, formattedName, formattedType, description, additionalDescriptions) + "\n";
    }
    for (size_t i = 0; i < innerArguments.size(); ++i) {
        bool innerComma = i != innerArguments.size() - 1;
        argumentStr += innerArguments[i]->ArgumentString(spaces + 2, nameLength, argument, false, innerComma, true) + "\n";
    }
    argumentStr += PrependSpaces("}", spaces);
    if (comma) {
        argumentStr += ",";
    }
    return argumentStr;
}

/* RpcDocBuilder */
RpcDocBuilder::RpcDocBuilder(const RpcDocBuilder& other)
    : name(other.name), description(other.description), cliHelps(other.cliHelps), rpcHelps(other.rpcHelps) {
    for (const RpcArgument* argument : other.arguments) {
        arguments.push_back(argument->Clone());
    }
    for (const std::pair<std::string, RpcArgument*>& result : other.results) {
        results.push_back(std::make_pair(result.first, result.second->Clone()));
    }
}

RpcDocBuilder::~RpcDocBuilder() {
    for (RpcArgument* argument : arguments) {
        delete argument;
    }
    for (std::pair<std::string, RpcArgument*> result : results) {
        delete result.second;
    }
}

RpcDocBuilder& RpcDocBuilder::SetDescription(const std::string& description) {
    this->description = description;
    return *this;
}

RpcDocBuilder& RpcDocBuilder::AddArgument(const RpcArgument& argument) {
    arguments.push_back(argument.Clone());
    return *this;
}

RpcDocBuilder& RpcDocBuilder::AddResult(const std::string& resultDescription, const RpcArgument& result) {
    results.push_back(std::make_pair(resultDescription, result.Clone()));
    return *this;
}

RpcDocBuilder& RpcDocBuilder::AddResult(const RpcArgument& result) {
    results.push_back(std::make_pair("", result.Clone()));
    return *this;
}

RpcDocBuilder& RpcDocBuilder::AddHelpExampleCli(const std::string& cliHelpDescription, const std::string& cliHelpArgs) {
    cliHelps.push_back(std::make_pair(cliHelpDescription, cliHelpArgs));
    return *this;
}

RpcDocBuilder& RpcDocBuilder::AddHelpExampleCli(const std::string& cliHelpArgs) {
    cliHelps.push_back(std::make_pair("", cliHelpArgs));
    return *this;
}

RpcDocBuilder& RpcDocBuilder::AddHelpExampleRpc(const std::string& rpcHelpDescription, const std::string& rpcHelpArgs) {
    rpcHelps.push_back(std::make_pair(rpcHelpDescription, rpcHelpArgs));
    return *this;
}

RpcDocBuilder& RpcDocBuilder::AddHelpExampleRpc(const std::string& rpcHelpArgs) {
    rpcHelps.push_back(std::make_pair("", rpcHelpArgs));
    return *this;
}

std::string HelpExamples(const std::string& rpcName, const std::vector<std::pair<std::string, std::string>>& helps, bool isCli) {
    std::string help;
    for (const std::pair<std::string, std::string>& cliHelp : helps) {
        std::string helpDescription = cliHelp.first;
        std::string helpArgs = cliHelp.second;
        if (helpDescription.length() > 0) {
            help += "\n" + helpDescription + "\n";
        }
        help += isCli ? HelpExampleCli(rpcName, helpArgs) : HelpExampleRpc(rpcName, helpArgs);
    }
    return help;
}

std::string RpcDocBuilder::ToString() const {
    size_t maxArgNameLength = 0;
    size_t maxResNameLength = 0;
    std::string cliString;
    // Header
    cliString += name;
    for (const auto& argument : arguments) {
        cliString += " " + argument->CliHeaderString();
        maxArgNameLength = std::max(maxArgNameLength, argument->ArgumentNameLength(true));
    }
    // Description
    cliString += "\n\n" + description + "\n";
    // Arguments
    if (arguments.size() > 0) {
        cliString += "\nArguments:\n";
    }
    for (size_t i = 0; i < arguments.size(); ++i) {
        cliString += std::to_string(i + 1) + ". " + arguments[i]->ArgumentString(0, maxArgNameLength, true) + "\n";
    }
    // Results
    for (const std::pair<std::string, RpcArgument*>& result : results) {
        std::string resultDescription = result.first;
        cliString += "\nResult" + (resultDescription.empty() ? ":\n" : " (" + resultDescription + "):\n");
        RpcArgument* resultArg = result.second;
        cliString += resultArg->ArgumentString(0, resultArg->ArgumentNameLength(false), false) + "\n";
    }
    // Examples
    if (cliHelps.size() > 0 || rpcHelps.size() > 0){
        cliString += "\nExamples:\n";
        cliString += HelpExamples(name, cliHelps, true);
        cliString += HelpExamples(name, rpcHelps, false);
    }
    return cliString;
}
