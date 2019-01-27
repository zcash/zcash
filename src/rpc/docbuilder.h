// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOCBUILDER_H
#define DOCBUILDER_H

#include <string>
#include <vector>

class PrimitiveArgument;
class ArrayArgument;
class ObjectArgument;
class MappingArgument;

enum class ParentArgumentType {
    NONE,
    ARRAY,
    OBJECT,
    MAPPING
};

class RpcArgument
{
protected:
    const std::string name;
    const bool isRequired;
    const std::string defaultValue;
    const std::string description;
    std::vector<std::string> additionalDescriptions;
public:
    RpcArgument(
        const std::string& name_,
        const bool& isRequired_,
        const std::string defaultValue_,
        const std::string description_,
        std::vector<std::string> additionalDescriptions_
    ) : name(name_), isRequired(isRequired_), defaultValue(defaultValue_), description(description_), additionalDescriptions(additionalDescriptions_) {}
    virtual ~RpcArgument() {};

    virtual RpcArgument* Clone() const = 0;
    
    virtual RpcArgument& AddDescription(const std::string& description_) = 0;
    virtual size_t ArgumentNameLength(bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const = 0;
    virtual std::string TypeString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const = 0;
    virtual std::string CliHeaderString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const = 0;
    virtual std::string ArgumentString(size_t spaces, size_t nameLength, bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const = 0;

    /* Factory methods for derived classes */
    static PrimitiveArgument Boolean(const std::string& name, const std::string& description);
    static PrimitiveArgument Boolean(const std::string& description);
    static PrimitiveArgument OptionalBoolean(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalBoolean(const std::string& description);
    static PrimitiveArgument Integer(const std::string& name, const std::string& description);
    static PrimitiveArgument Integer(const std::string& description);
    static PrimitiveArgument OptionalInteger(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalInteger(const std::string& description);
    static PrimitiveArgument Decimal(const std::string& name, const std::string& description);
    static PrimitiveArgument Decimal(const std::string& description);
    static PrimitiveArgument OptionalDecimal(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalDecimal(const std::string& description);
    static PrimitiveArgument String(const std::string& name, const std::string& description);
    static PrimitiveArgument String(const std::string& description);
    static PrimitiveArgument OptionalString(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalString(const std::string& description);
    static PrimitiveArgument HexString(const std::string& name, const std::string& description);
    static PrimitiveArgument HexString(const std::string& description);
    static PrimitiveArgument OptionalHexString(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalHexString(const std::string& description);

    static PrimitiveArgument Amount(const std::string& name, const std::string& description);
    static PrimitiveArgument Amount(const std::string& description);
    static PrimitiveArgument OptionalAmount(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalAmount(const std::string& description);
    static PrimitiveArgument Timestamp(const std::string& name, const std::string& description);
    static PrimitiveArgument Timestamp(const std::string& description);
    static PrimitiveArgument OptionalTimestamp(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalTimestamp(const std::string& description);
    static PrimitiveArgument Asm(const std::string& name, const std::string& description);
    static PrimitiveArgument Asm(const std::string& description);
    static PrimitiveArgument IPAddress(const std::string& name, const std::string& description);
    static PrimitiveArgument IPAddress(const std::string& description);
    static PrimitiveArgument OptionalIPAddress(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalIPAddress(const std::string& description);
    static PrimitiveArgument SocketAddress(const std::string& name, const std::string& description);
    static PrimitiveArgument SocketAddress(const std::string& description);
    static PrimitiveArgument OptionalSocketAddress(const std::string& name, const std::string& defaultValue, const std::string& description);
    static PrimitiveArgument OptionalSocketAddress(const std::string& description);

    static ArrayArgument Array(const std::string name, const std::string description);
    static ArrayArgument Array(const std::string description);
    static ArrayArgument Array();
    static ArrayArgument OptionalArray(const std::string name, const std::string description);
    static ArrayArgument OptionalArray(const std::string description);

    static ObjectArgument Object(const std::string name, const std::string description);
    static ObjectArgument Object(const std::string description);
    static ObjectArgument Object();
    static ObjectArgument OptionalObject(const std::string name, const std::string description);
    static ObjectArgument OptionalObject(const std::string description);

    static MappingArgument Mapping(const std::string name, const std::string description);
    static MappingArgument Mapping(const std::string description);
    static MappingArgument Mapping();
};

enum class PrimitiveType {
    BOOLEAN,
    INTEGER,
    DECIMAL,
    STRING,
    HEX_STRING,
    AMOUNT,
    TIMESTAMP,
    ASM,
    IP_ADDRESS,
    SOCKET_ADDRESS
};

class PrimitiveArgument : public RpcArgument
{
private:
    const PrimitiveType type;
public:
    PrimitiveArgument(const std::string& name_, const PrimitiveType& type_, const bool& isRequired_, const std::string& defaultValue_, const std::string description_)
        : RpcArgument(name_, isRequired_, defaultValue_, description_, std::vector<std::string>()), type(type_) {}
    PrimitiveArgument(const PrimitiveArgument& other)
        : RpcArgument(other.name, other.isRequired, other.defaultValue, other.description, other.additionalDescriptions), type(other.type) {}
    ~PrimitiveArgument() {}

    PrimitiveArgument* Clone() const { return new PrimitiveArgument(*this); }

    PrimitiveArgument& AddDescription(const std::string& description_);
    size_t ArgumentNameLength(bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const;
    std::string TypeString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const;
    std::string CliHeaderString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const;
    std::string ArgumentString(size_t spaces, size_t nameLength, bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const;
};

class ArrayArgument : public RpcArgument {
private:
    RpcArgument* innerArgument;
public:
    ArrayArgument(const std::string& name_, const bool& isRequired, const std::string description_)
        : RpcArgument(name_, isRequired, "", description_, std::vector<std::string>()), innerArgument(nullptr)  {}
    ArrayArgument(const ArrayArgument& other)
        : RpcArgument(other.name, other.isRequired, other.defaultValue, other.description, other.additionalDescriptions),
        innerArgument(other.innerArgument ? other.innerArgument->Clone() : nullptr) {}
    ~ArrayArgument() { delete innerArgument; }

    ArrayArgument& Of(const RpcArgument& innerArgument_);

    ArrayArgument* Clone() const { return new ArrayArgument(*this); }

    ArrayArgument& AddDescription(const std::string& description_);
    size_t ArgumentNameLength(bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const;
    std::string TypeString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const;
    std::string CliHeaderString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const;
    std::string ArgumentString(size_t spaces, size_t nameLength, bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const;
};

class ObjectArgument : public RpcArgument {
private:
    std::vector<RpcArgument*> innerArguments;
public:
    ObjectArgument(const std::string& name_, const bool& isRequired, const std::string description_)
        : RpcArgument(name_, isRequired, "", description_, std::vector<std::string>()) {}
    ObjectArgument(const ObjectArgument& other);
    ~ObjectArgument();

    ObjectArgument& With(const RpcArgument& innerArgument);

    ObjectArgument* Clone() const { return new ObjectArgument(*this); }

    ObjectArgument& AddDescription(const std::string& description_);
    size_t ArgumentNameLength(bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const;
    std::string TypeString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const;
    std::string CliHeaderString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const;
    std::string ArgumentString(size_t spaces, size_t nameLength, bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const;
};

class MappingArgument : public RpcArgument {
private:
    RpcArgument* valueArgument;
public:
    MappingArgument(const std::string& name_, const bool& isRequired, const std::string description_)
        : RpcArgument(name_, isRequired, "", description_, std::vector<std::string>()), valueArgument(nullptr)  {}
    MappingArgument(const MappingArgument& other)
        : RpcArgument(other.name, other.isRequired, other.defaultValue, other.description, other.additionalDescriptions),
        valueArgument(other.valueArgument ? other.valueArgument->Clone() : nullptr) {}
    ~MappingArgument() { delete valueArgument; }

    MappingArgument& Of(const RpcArgument& valueArgument_);

    MappingArgument* Clone() const { return new MappingArgument(*this); }

    MappingArgument& AddDescription(const std::string& description_);
    size_t ArgumentNameLength(bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const;
    std::string TypeString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const;
    std::string CliHeaderString(const ParentArgumentType& parentType=ParentArgumentType::NONE) const;
    std::string ArgumentString(size_t spaces, size_t nameLength, bool argument, const ParentArgumentType& parentType=ParentArgumentType::NONE, bool comma=false) const;
};

class RpcDocBuilder
{
private:
    const std::string name;
    std::string description;
    std::vector<RpcArgument*> arguments;
    std::vector<std::pair<std::string, RpcArgument*>> results;
    std::vector<std::pair<std::string, std::string>> cliHelps;
    std::vector<std::pair<std::string, std::string>> rpcHelps;
public:
    RpcDocBuilder(const std::string& name_)
        : name(name_) {}
    RpcDocBuilder(const RpcDocBuilder& other);
    ~RpcDocBuilder();

    RpcDocBuilder& SetDescription(const std::string& description);
    RpcDocBuilder& AddArgument(const RpcArgument& argument);
    RpcDocBuilder& AddResult(const std::string& resultDescription, const RpcArgument& result);
    RpcDocBuilder& AddResult(const RpcArgument& result);
    RpcDocBuilder& AddHelpExampleCli(const std::string& cliHelpDescription, const std::string& argsString);
    RpcDocBuilder& AddHelpExampleCli(const std::string& argsString);
    RpcDocBuilder& AddHelpExampleRpc(const std::string& rpcHelpDescription, const std::string& argsString);
    RpcDocBuilder& AddHelpExampleRpc(const std::string& argsString);

    std::string ToString() const;
};

#endif // DOCBUILDER_H
