#include "test_util.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>
#include <map>
#include <vector>

#include "script/interpreter.h"
#include "rpc/client.h"
#include "rpc/server.h"

static std::map<std::string, unsigned int> mapFlagNames = boost::assign::map_list_of
    (std::string("NONE"), (unsigned int)SCRIPT_VERIFY_NONE)
    (std::string("P2SH"), (unsigned int)SCRIPT_VERIFY_P2SH)
    (std::string("STRICTENC"), (unsigned int)SCRIPT_VERIFY_STRICTENC)
    (std::string("LOW_S"), (unsigned int)SCRIPT_VERIFY_LOW_S)
    (std::string("SIGPUSHONLY"), (unsigned int)SCRIPT_VERIFY_SIGPUSHONLY)
    (std::string("MINIMALDATA"), (unsigned int)SCRIPT_VERIFY_MINIMALDATA)
    (std::string("NULLDUMMY"), (unsigned int)SCRIPT_VERIFY_NULLDUMMY)
    (std::string("DISCOURAGE_UPGRADABLE_NOPS"), (unsigned int)SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS)
    (std::string("CLEANSTACK"), (unsigned int)SCRIPT_VERIFY_CLEANSTACK)
    (std::string("CHECKLOCKTIMEVERIFY"), (unsigned int)SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY);

UniValue
read_json(const std::string& jsondata)
{
    UniValue v;

    if (!v.read(jsondata) || !v.isArray())
    {
        BOOST_ERROR("Parse error.");
        return UniValue(UniValue::VARR);
    }
    return v.get_array();
}

unsigned int ParseScriptFlags(std::string strFlags)
{
    if (strFlags.empty()) {
        return 0;
    }
    unsigned int flags = 0;
    std::vector<std::string> words;
    boost::algorithm::split(words, strFlags, boost::algorithm::is_any_of(","));

    for (std::string word : words)
    {
        if (!mapFlagNames.count(word))
            BOOST_ERROR("Bad test: unknown verification flag '" << word << "'");
        flags |= mapFlagNames[word];
    }

    return flags;
}

std::string FormatScriptFlags(unsigned int flags)
{
    if (flags == 0) {
        return "";
    }
    std::string ret;
    std::map<std::string, unsigned int>::const_iterator it = mapFlagNames.begin();
    while (it != mapFlagNames.end()) {
        if (flags & it->second) {
            ret += it->first + ",";
        }
        it++;
    }
    return ret.substr(0, ret.size() - 1);
}

UniValue createArgs(int nRequired, const char* address1, const char* address2)
{
    UniValue result(UniValue::VARR);
    result.push_back(nRequired);
    UniValue addresses(UniValue::VARR);
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address2);
    result.push_back(addresses);
    return result;
}

UniValue CallRPC(std::string args)
{
    std::vector<std::string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    std::string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    // Handle empty strings the same way as CLI
    for (auto i = 0; i < vArgs.size(); i++) {
        if (vArgs[i] == "\"\"") {
            vArgs[i] = "";
        }
    }
    UniValue params = RPCConvertValues(strMethod, vArgs);

    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        UniValue result = (*method)(params, false);
        return result;
    }
    catch (const UniValue& objError) {
        throw std::runtime_error(find_value(objError, "message").get_str());
    }
}

void CheckRPCThrows(std::string rpcString, std::string expectedErrorMessage) {
    try {
        CallRPC(rpcString);
        // Note: CallRPC catches (const UniValue& objError) and rethrows a runtime_error
        BOOST_FAIL("Should have caused an error");
    } catch (const std::runtime_error& e) {
        BOOST_CHECK_EQUAL(expectedErrorMessage, e.what());
    } catch(const std::exception& e) {
        BOOST_FAIL(std::string("Unexpected exception: ") + typeid(e).name() + ", message=\"" + e.what() + "\"");
    }
}

