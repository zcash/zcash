// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "rpc/server.h"
#include "clientversion.h"

#include "fs.h"
#include "init.h"
#include "key_io.h"
#include "random.h"
#include "rpc/common.h"
#include "sync.h"
#include "ui_interface.h"
#include "util/system.h"
#include "util/strencodings.h"
#include "asyncrpcqueue.h"

#include <memory>

#include <univalue.h>

#include <boost/bind/bind.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_upper()

#include <tracing.h>

using namespace RPCServer;
using namespace std;
using namespace boost::placeholders;

static bool fRPCRunning = false;
static bool fRPCInWarmup = true;
static std::string rpcWarmupStatus("RPC server started");
static CCriticalSection cs_rpcWarmup;
/* Timer-creating functions */
static std::vector<RPCTimerInterface*> timerInterfaces;
/* Map of name to timer.
 * @note Can be changed to std::unique_ptr when C++11 */
static std::map<std::string, boost::shared_ptr<RPCTimerBase> > deadlineTimers;

static struct CRPCSignals
{
    boost::signals2::signal<void ()> Started;
    boost::signals2::signal<void ()> Stopped;
    boost::signals2::signal<void (const CRPCCommand&)> PreCommand;
} g_rpcSignals;

void RPCServer::OnStarted(std::function<void ()> slot)
{
    g_rpcSignals.Started.connect(slot);
}

void RPCServer::OnStopped(std::function<void ()> slot)
{
    g_rpcSignals.Stopped.connect(slot);
}

void RPCServer::OnPreCommand(std::function<void (const CRPCCommand&)> slot)
{
    g_rpcSignals.PreCommand.connect(boost::bind(slot, _1));
}

void RPCTypeCheck(const UniValue& params,
                  const list<UniValue::VType>& typesExpected,
                  bool fAllowNull)
{
    size_t i = 0;
    for (UniValue::VType t : typesExpected)
    {
        if (params.size() <= i)
            break;

        const UniValue& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.isNull()))))
        {
            string err = strprintf("Expected type %s, got %s",
                                   uvTypeName(t), uvTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheckObj(const UniValue& o,
                  const map<string, UniValue::VType>& typesExpected,
                  bool fAllowNull)
{
    for (const std::pair<string, UniValue::VType>& t : typesExpected)
    {
        const UniValue& v = find_value(o, t.first);
        if (!fAllowNull && v.isNull())
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first));

        if (!((v.type() == t.second) || (fAllowNull && (v.isNull()))))
        {
            string err = strprintf("Expected type %s for %s, got %s",
                                   uvTypeName(t.second), t.first, uvTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

CAmount AmountFromValue(const UniValue& value)
{
    if (!value.isNum() && !value.isStr())
        throw JSONRPCError(RPC_TYPE_ERROR, "Amount is not a number or string");
    CAmount amount;
    if (!ParseFixedPoint(value.getValStr(), 8, &amount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    if (!MoneyRange(amount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Amount out of range");
    return amount;
}

UniValue ValueFromAmount(const CAmount& amount)
{
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    return UniValue(UniValue::VNUM,
            strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder));
}

uint256 ParseHashV(const UniValue& v, string strName)
{
    string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    uint256 result;
    result.SetHex(strHex);
    return result;
}
uint256 ParseHashO(const UniValue& o, string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}
vector<unsigned char> ParseHexV(const UniValue& v, string strName)
{
    string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    return ParseHex(strHex);
}
vector<unsigned char> ParseHexO(const UniValue& o, string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}

/**
 * Note: This interface may still be subject to change.
 */

std::string CRPCTable::help(const std::string& strCommand) const
{
    string strRet;
    string category;
    set<rpcfn_type> setDone;
    vector<pair<string, const CRPCCommand*> > vCommands;

    for (map<string, const CRPCCommand*>::const_iterator mi = mapCommands.begin(); mi != mapCommands.end(); ++mi)
        vCommands.push_back(make_pair(mi->second->category + mi->first, mi->second));
    sort(vCommands.begin(), vCommands.end());

    for (const std::pair<string, const CRPCCommand*>& command : vCommands)
    {
        const CRPCCommand *pcmd = command.second;
        string strMethod = pcmd->name;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != string::npos)
            continue;
        if ((strCommand != "" || pcmd->category == "hidden") && strMethod != strCommand)
            continue;
        try
        {
            UniValue params;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (const std::exception& e)
        {
            // Help text is returned in an exception
            string strHelp = string(e.what());
            if (strCommand == "")
            {
                if (strHelp.find('\n') != string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));

                if (category != pcmd->category)
                {
                    if (!category.empty())
                        strRet += "\n";
                    category = pcmd->category;
                    string firstLetter = category.substr(0,1);
                    boost::to_upper(firstLetter);
                    strRet += "== " + firstLetter + category.substr(1) + " ==\n";
                }
            }
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "")
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

UniValue help(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "help ( \"command\" )\n"
            "\nList all commands, or get help for a specified command.\n"
            "\nArguments:\n"
            "1. \"command\"     (string, optional) The command to get help on\n"
            "\nResult:\n"
            "\"text\"     (string) The help text\n"
        );

    string strCommand;
    if (params.size() > 0)
        strCommand = params[0].get_str();

    return tableRPC.help(strCommand);
}


UniValue setlogfilter(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1) {
        throw runtime_error(
            "setlogfilter \"directives\"\n"
            "\nSets the filter to be used for selecting events to log.\n"
            "\nA filter is a comma-separated list of directives.\n"
            "The syntax for each directive is:\n"
            "\n    target[span{field=value}]=level\n"
            "\nThe default filter, derived from the -debug=target flags, is:\n"
            + strprintf("\n    %s", LogConfigFilter()) + "\n"
            "\nPassing a valid filter here will replace the existing filter.\n"
            "Passing an empty string will reset the filter to the default.\n"
            "\nNote that enabling trace-level events should always be considered\n"
            "unsafe, as they can result in sensitive information like decrypted\n"
            "notes and private keys being printed to the log output.\n"
            "\nArguments:\n"
            "1. newFilterDirectives (string, required) The new log filter.\n"
            "\nExamples:\n"
            + HelpExampleCli("setlogfilter", "\"main=info,rpc=info\"")
            + HelpExampleRpc("setlogfilter", "\"main=info,rpc=info\"")
        );
    }

    auto newFilter = params[0].getValStr();
    if (newFilter.empty()) {
        newFilter = LogConfigFilter();
    }

    if (pTracingHandle) {
        TracingInfo("main", "Reloading log filter", "new_filter", newFilter.c_str());

        if (!tracing_reload(pTracingHandle, newFilter.c_str())) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Filter reload failed; check logs");
        }
    }

    return NullUniValue;
}


UniValue stop(const UniValue& params, bool fHelp)
{
    // Accept the deprecated and ignored 'detach' boolean argument
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "stop\n"
            "\nStop " DAEMON_NAME ".");

    // Event loop will exit after current HTTP requests have been handled, so
    // this reply will get back to the client.
    StartShutdown();
    return DAEMON_NAME " stopping";
}

/**
 * Call Table
 */
static const CRPCCommand vRPCCommands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    /* Overall control/query calls */
    { "control",            "help",                   &help,                   true  },
    { "control",            "setlogfilter",           &setlogfilter,           true  },
    { "control",            "stop",                   &stop,                   true  },
};

CRPCTable::CRPCTable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])); vcidx++)
    {
        const CRPCCommand *pcmd;

        pcmd = &vRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](const std::string &name) const
{
    map<string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
        return NULL;
    return (*it).second;
}

bool CRPCTable::appendCommand(const std::string& name, const CRPCCommand* pcmd)
{
    if (IsRPCRunning())
        return false;

    // don't allow overwriting for now
    map<string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it != mapCommands.end())
        return false;

    mapCommands[name] = pcmd;
    return true;
}

bool StartRPC()
{
    LogPrint("rpc", "Starting RPC\n");
    fRPCRunning = true;
    g_rpcSignals.Started();

    // Launch one async rpc worker.  The ability to launch multiple workers is not recommended at present and thus the option is disabled.
    getAsyncRPCQueue()->addWorker();
/*
    int n = GetArg("-rpcasyncthreads", 1);
    if (n<1) {
        LogPrintf("ERROR: Invalid value %d for -rpcasyncthreads.  Must be at least 1.\n", n);
        strerr = strprintf(_("An error occurred while setting up the Async RPC threads, invalid parameter value of %d (must be at least 1)."), n);
        uiInterface.ThreadSafeMessageBox(strerr, "", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return;
    }
    for (int i = 0; i < n; i++)
        getAsyncRPCQueue()->addWorker();
*/
    return true;
}

void InterruptRPC()
{
    LogPrint("rpc", "Interrupting RPC\n");
    // Interrupt e.g. running longpolls
    fRPCRunning = false;
}

void StopRPC()
{
    LogPrint("rpc", "Stopping RPC\n");
    deadlineTimers.clear();
    g_rpcSignals.Stopped();

    // Tells async queue to cancel all operations and shutdown.
    LogPrintf("%s: waiting for async rpc workers to stop\n", __func__);
    getAsyncRPCQueue()->closeAndWait();
}

bool IsRPCRunning()
{
    return fRPCRunning;
}

void SetRPCWarmupStatus(const std::string& newStatus)
{
    LOCK(cs_rpcWarmup);
    rpcWarmupStatus = newStatus;
}

void SetRPCWarmupFinished()
{
    LOCK(cs_rpcWarmup);
    assert(fRPCInWarmup);
    fRPCInWarmup = false;
}

bool RPCIsInWarmup(std::string *outStatus)
{
    LOCK(cs_rpcWarmup);
    if (outStatus)
        *outStatus = rpcWarmupStatus;
    return fRPCInWarmup;
}

void JSONRequest::parse(const UniValue& valRequest)
{
    // Parse request
    if (!valRequest.isObject())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const UniValue& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    UniValue valMethod = find_value(request, "method");
    if (valMethod.isNull())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (!valMethod.isStr())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    LogPrint("rpc", "ThreadRPCServer method=%s\n", SanitizeString(strMethod));

    // Parse params
    UniValue valParams = find_value(request, "params");
    if (valParams.isArray())
        params = valParams.get_array();
    else if (valParams.isNull())
        params = UniValue(UniValue::VARR);
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}

static UniValue JSONRPCExecOne(const UniValue& req)
{
    UniValue rpc_result(UniValue::VOBJ);

    JSONRequest jreq;
    try {
        jreq.parse(req);

        UniValue result = tableRPC.execute(jreq.strMethod, jreq.params);
        rpc_result = JSONRPCReplyObj(result, NullUniValue, jreq.id);
    }
    catch (const UniValue& objError)
    {
        rpc_result = JSONRPCReplyObj(NullUniValue, objError, jreq.id);
    }
    catch (const std::exception& e)
    {
        rpc_result = JSONRPCReplyObj(NullUniValue,
                                     JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

std::string JSONRPCExecBatch(const UniValue& vReq)
{
    UniValue ret(UniValue::VARR);
    for (size_t reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return ret.write() + "\n";
}

UniValue CRPCTable::execute(const std::string &strMethod, const UniValue &params) const
{
    // Return immediately if in warmup
    {
        LOCK(cs_rpcWarmup);
        if (fRPCInWarmup)
            throw JSONRPCError(RPC_IN_WARMUP, rpcWarmupStatus);
    }

    // Find method
    const CRPCCommand *pcmd = tableRPC[strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    g_rpcSignals.PreCommand(*pcmd);

    try
    {
        // Execute
        auto paramRange = rpcCvtTable.find(strMethod);
        if (paramRange != rpcCvtTable.end()) {
            auto numRequired = paramRange->second.first.size();
            auto numOptional = paramRange->second.second.size();
            if (params.size() < numRequired || numRequired + numOptional < params.size()) {
                std::string helpMsg;
                try {
                    // help gets thrown – if it doesn’t throw, then no help message
                    pcmd->actor(params, true);
                } catch (const std::runtime_error& err) {
                    helpMsg = std::string("\n\n") + err.what();
                }
                throw JSONRPCError(
                    RPC_INVALID_PARAMS,
                    strprintf(
                            "%s for method `%s`. Needed %s, but received %u%s",
                            params.size() < numRequired
                            ? "Not enough parameters"
                            : "Too many parameters",
                            strMethod,
                            numOptional == 0
                            ? strprintf("exactly %u", numRequired)
                            : strprintf("at least %u and at most %u", numRequired, numRequired + numOptional),
                            params.size(),
                            helpMsg));
            } else {
                return pcmd->actor(params, false);
            }
        } else {
            throw JSONRPCError(
                    RPC_INTERNAL_ERROR,
                    "Parameters for "
                    + strMethod
                    + " not found – this is an internal error, please report it.");
        }

    }
    catch (const std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}

std::vector<std::string> CRPCTable::listCommands() const
{
    std::vector<std::string> commandList;
    typedef std::map<std::string, const CRPCCommand*> commandMap;

    std::transform( mapCommands.begin(), mapCommands.end(),
                   std::back_inserter(commandList),
                   boost::bind(&commandMap::value_type::first,_1) );
    return commandList;
}

std::string HelpExampleCli(const std::string& methodname, const std::string& args)
{
    return "> " CLI_NAME " " + methodname + " " + args + "\n";
}

std::string HelpExampleRpc(const std::string& methodname, const std::string& args)
{
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", "
        "\"method\": \"" + methodname + "\", \"params\": [" + args + "] }' -H 'content-type: text/plain;' http://127.0.0.1:8232/\n";
}

std::string experimentalDisabledHelpMsg(const std::string& rpc, const std::vector<string>& enableArgs)
{
    std::string cmd, config = "";
    const auto size = enableArgs.size();
    assert(size > 0);

    for (size_t i = 0; i < size; ++i)
    {
        if (size == 1 || i == 0)
        {
            cmd += "-experimentalfeatures and -" + enableArgs.at(i);
            config += "experimentalfeatures=1\n";
            config += enableArgs.at(i) + "=1\n";
        }
        else {
            cmd += " or:\n-experimentalfeatures and -" + enableArgs.at(i);
            config += "\nor:\n\n";
            config += "experimentalfeatures=1\n";
            config += enableArgs.at(i) + "=1\n";
        }
    }
    return "\nWARNING: " + rpc + " is disabled.\n" +
        "To enable it, restart " DAEMON_NAME " with the following command line options:\n"
        + cmd + "\n\n" +
        "Alternatively add these two lines to the zcash.conf file:\n\n"
        + config;
}

std::string asOfHeightMessage(bool hasMinconf) {
    std::string minconfInteraction = hasMinconf
        ? "                    `minconf` must be at least 1 when `asOfHeight` is provided.\n"
        : "";
    return
        "asOfHeight       (numeric, optional, default=-1) Execute the query as if it\n"
        "                    were run when the blockchain was at the height specified by\n"
        "                    this argument. The default is to use the entire blockchain\n"
        "                    that the node is aware of. -1 can be used as in other RPC\n"
        "                    calls to indicate the current height (including the\n"
        "                    mempool), but this does not support negative values in\n"
        "                    general. A “future” height will fall back to the current\n"
        "                    height. Any explicit value will cause the mempool to be\n"
        "                    ignored, meaning no unconfirmed tx will be considered.\n"
        + minconfInteraction;
}

std::optional<int> parseAsOfHeight(const UniValue& params, int index) {
    std::optional<int> asOfHeight;
    if (params.size() > index) {
        auto requestedHeight = params[index].get_int();
        if (requestedHeight == -1) {
            // the default, do nothing
        } else if (requestedHeight < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Can not perform the query as of a negative block height");
        } else if (requestedHeight == 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Can not perform the query as of the genesis block");
        } else {
            asOfHeight = requestedHeight;
        }
    }
    return asOfHeight;
}

int parseMinconf(int defaultValue, const UniValue& params, int index, const std::optional<int>& asOfHeight) {
    int nMinDepth = defaultValue;
    if (params.size() > index) {
        auto requestedDepth = params[index].get_int();
        if (requestedDepth < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
        } else if (requestedDepth == 0 && asOfHeight.has_value()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Require a minimum of 1 confirmation when `asOfHeight` is provided");
        } else {
            nMinDepth = requestedDepth;
        }
    }
    return nMinDepth;
}

void RPCRegisterTimerInterface(RPCTimerInterface *iface)
{
    timerInterfaces.push_back(iface);
}

void RPCUnregisterTimerInterface(RPCTimerInterface *iface)
{
    std::vector<RPCTimerInterface*>::iterator i = std::find(timerInterfaces.begin(), timerInterfaces.end(), iface);
    assert(i != timerInterfaces.end());
    timerInterfaces.erase(i);
}

void RPCRunLater(const std::string& name, std::function<void(void)> func, int64_t nSeconds)
{
    if (timerInterfaces.empty())
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No timer handler registered for RPC");
    deadlineTimers.erase(name);
    RPCTimerInterface* timerInterface = timerInterfaces[0];
    LogPrint("rpc", "queue run of timer %s in %i seconds (using %s)\n", name, nSeconds, timerInterface->Name());
    deadlineTimers.insert(std::make_pair(name, boost::shared_ptr<RPCTimerBase>(timerInterface->NewTimer(func, nSeconds*1000))));
}

CRPCTable tableRPC;

// Return async rpc queue
std::shared_ptr<AsyncRPCQueue> getAsyncRPCQueue()
{
    return AsyncRPCQueue::sharedInstance();
}

void AddMemo(UniValue &obj, const std::optional<libzcash::Memo> &memo)
{
    obj.pushKV("memo", HexStr(libzcash::Memo::ToBytes(memo)));

    if (memo.has_value()) {
        auto interpMemo = memo.value().Interpret();
        // TODO: Indicate when the memo should have been valid UTF8, but wasn’t.
        if (interpMemo.has_value()) {
            examine(interpMemo.value(), match {
                [&](const std::string& memoStr) { obj.pushKV("memoStr", memoStr); },
                [](const auto&) {},
            });
        }
    }
}
