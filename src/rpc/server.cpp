// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019 The Hush developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "rpc/server.h"

#include "init.h"
#include "key_io.h"
#include "random.h"
#include "sync.h"
#include "ui_interface.h"
#include "util.h"
#include "utilstrencodings.h"
#include "asyncrpcqueue.h"

#include <memory>

#include <univalue.h>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_upper()

using namespace RPCServer;
using namespace std;
extern uint16_t ASSETCHAINS_P2PPORT,ASSETCHAINS_RPCPORT;

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
    boost::signals2::signal<void (const CRPCCommand&)> PostCommand;
} g_rpcSignals;

void RPCServer::OnStarted(boost::function<void ()> slot)
{
    g_rpcSignals.Started.connect(slot);
}

void RPCServer::OnStopped(boost::function<void ()> slot)
{
    g_rpcSignals.Stopped.connect(slot);
}

void RPCServer::OnPreCommand(boost::function<void (const CRPCCommand&)> slot)
{
    g_rpcSignals.PreCommand.connect(boost::bind(slot, _1));
}

void RPCServer::OnPostCommand(boost::function<void (const CRPCCommand&)> slot)
{
    g_rpcSignals.PostCommand.connect(boost::bind(slot, _1));
}

void RPCTypeCheck(const UniValue& params,
                  const list<UniValue::VType>& typesExpected,
                  bool fAllowNull)
{
    size_t i = 0;
    BOOST_FOREACH(UniValue::VType t, typesExpected)
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
    BOOST_FOREACH(const PAIRTYPE(string, UniValue::VType)& t, typesExpected)
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

    BOOST_FOREACH(const PAIRTYPE(string, const CRPCCommand*)& command, vCommands)
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
                (*pfn)(params, true, CPubKey());
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

UniValue help(const UniValue& params, bool fHelp, const CPubKey& mypk)
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

extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];

#ifdef ENABLE_WALLET
void GenerateBitcoins(bool b, CWallet *pw, int t);
#else
void GenerateBitcoins(bool b, CWallet *pw);
#endif


UniValue stop(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    char buf[66+128];
   // Accept the deprecated and ignored 'detach' boolean argument
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "stop\n"
            "\nStop Komodo server.");

#ifdef ENABLE_WALLET
    GenerateBitcoins(false, pwalletMain, 0);
#else
    GenerateBitcoins(false, 0);
#endif

    // Shutdown will take long enough that the response should get back
    StartShutdown();
    sprintf(buf,"%s server stopping",ASSETCHAINS_SYMBOL[0] != 0 ? ASSETCHAINS_SYMBOL : "Komodo");
    return buf;
}

/**
 * Call Table
 */
static const CRPCCommand vRPCCommands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    /* Overall control/query calls */
    { "control",            "help",                   &help,                   true  },
    { "control",            "getiguanajson",          &getiguanajson,          true  },
    { "control",            "getnotarysendmany",      &getnotarysendmany,      true  },
    { "control",            "geterablockheights",     &geterablockheights,     true  },
    { "control",            "stop",                   &stop,                   true  },

    /* P2P networking */
    { "network",            "getnetworkinfo",         &getnetworkinfo,         true  },
    { "network",            "getdeprecationinfo",     &getdeprecationinfo,     true  },
    { "network",            "addnode",                &addnode,                true  },
    { "network",            "disconnectnode",         &disconnectnode,         true  },
    { "network",            "getaddednodeinfo",       &getaddednodeinfo,       true  },
    { "network",            "getconnectioncount",     &getconnectioncount,     true  },
    { "network",            "getnettotals",           &getnettotals,           true  },
    { "network",            "getpeerinfo",            &getpeerinfo,            true  },
    { "network",            "ping",                   &ping,                   true  },
    { "network",            "setban",                 &setban,                 true  },
    { "network",            "listbanned",             &listbanned,             true  },
    { "network",            "clearbanned",            &clearbanned,            true  },

    /* Block chain and UTXO */
    { "blockchain",         "coinsupply",             &coinsupply,             true  },
    { "blockchain",         "getblockchaininfo",      &getblockchaininfo,      true  },
    { "blockchain",         "getbestblockhash",       &getbestblockhash,       true  },
    { "blockchain",         "getblockcount",          &getblockcount,          true  },
    { "blockchain",         "getblock",               &getblock,               true  },
    { "blockchain",         "getblockdeltas",         &getblockdeltas,         false },
    { "blockchain",         "getblockhashes",         &getblockhashes,         true  },
    { "blockchain",         "getblockhash",           &getblockhash,           true  },
    { "blockchain",         "getblockheader",         &getblockheader,         true  },
    { "blockchain",         "getlastsegidstakes",     &getlastsegidstakes,     true  },
    { "blockchain",         "getchaintips",           &getchaintips,           true  },
    { "blockchain",         "getdifficulty",          &getdifficulty,          true  },
    { "blockchain",         "getmempoolinfo",         &getmempoolinfo,         true  },
    { "blockchain",         "getrawmempool",          &getrawmempool,          true  },
    { "blockchain",         "gettxout",               &gettxout,               true  },
    { "blockchain",         "gettxoutproof",          &gettxoutproof,          true  },
    { "blockchain",         "verifytxoutproof",       &verifytxoutproof,       true  },
    { "blockchain",         "gettxoutsetinfo",        &gettxoutsetinfo,        true  },
    { "blockchain",         "verifychain",            &verifychain,            true  },
    { "blockchain",         "getspentinfo",           &getspentinfo,           false },
    //{ "blockchain",         "paxprice",               &paxprice,               true  },
    //{ "blockchain",         "paxpending",             &paxpending,             true  },
    //{ "blockchain",         "paxprices",              &paxprices,              true  },
    { "blockchain",         "notaries",               &notaries,               true  },
    //{ "blockchain",         "height_MoM",             &height_MoM,             true  },
    //{ "blockchain",         "txMoMproof",             &txMoMproof,             true  },
    { "blockchain",         "minerids",               &minerids,               true  },
    { "blockchain",         "kvsearch",               &kvsearch,               true  },
    { "blockchain",         "kvupdate",               &kvupdate,               true  },

    /* Cross chain utilities */
    { "crosschain",         "MoMoMdata",              &MoMoMdata,              true  },
    { "crosschain",         "calc_MoM",               &calc_MoM,               true  },
    { "crosschain",         "height_MoM",             &height_MoM,             true  },
    { "crosschain",         "assetchainproof",        &assetchainproof,        true  },
    { "crosschain",         "crosschainproof",        &crosschainproof,        true  },
    { "crosschain",         "getNotarisationsForBlock", &getNotarisationsForBlock, true },
    { "crosschain",         "scanNotarisationsDB",    &scanNotarisationsDB,    true },
    { "crosschain",         "getimports",             &getimports,             true },
    { "crosschain",         "getwalletburntransactions",  &getwalletburntransactions,             true },
    { "crosschain",         "migrate_converttoexport", &migrate_converttoexport, true  },
    { "crosschain",         "migrate_createburntransaction", &migrate_createburntransaction, true },
    { "crosschain",         "migrate_createimporttransaction", &migrate_createimporttransaction, true  },
    { "crosschain",         "migrate_completeimporttransaction", &migrate_completeimporttransaction, true  },
    { "crosschain",         "migrate_checkburntransactionsource", &migrate_checkburntransactionsource, true },
    { "crosschain",         "migrate_createnotaryapprovaltransaction", &migrate_createnotaryapprovaltransaction, true },
    { "crosschain",         "selfimport", &selfimport, true  },
    { "crosschain",         "importdual", &importdual, true  },
    //ImportGateway
    { "crosschain",       "importgatewayddress",     &importgatewayaddress,      true },
    { "crosschain",       "importgatewayinfo", &importgatewayinfo, true  },
    { "crosschain",       "importgatewaybind", &importgatewaybind, true  },
    { "crosschain",       "importgatewaydeposit", &importgatewaydeposit, true  },
    { "crosschain",       "importgatewaywithdraw",  &importgatewaywithdraw,     true },
    { "crosschain",       "importgatewaypartialsign",  &importgatewaypartialsign,     true },
    { "crosschain",       "importgatewaycompletesigning",  &importgatewaycompletesigning,     true },
    { "crosschain",       "importgatewaymarkdone",  &importgatewaymarkdone,     true },
    { "crosschain",       "importgatewaypendingwithdraws",   &importgatewaypendingwithdraws,      true },
    { "crosschain",       "importgatewayprocessed",   &importgatewayprocessed,  true },



    /* Mining */
    { "mining",             "getblocktemplate",       &getblocktemplate,       true  },
    { "mining",             "getmininginfo",          &getmininginfo,          true  },
    { "mining",             "getlocalsolps",          &getlocalsolps,          true  },
    { "mining",             "getnetworksolps",        &getnetworksolps,        true  },
    { "mining",             "getnetworkhashps",       &getnetworkhashps,       true  },
    { "mining",             "prioritisetransaction",  &prioritisetransaction,  true  },
    { "mining",             "submitblock",            &submitblock,            true  },
    { "mining",             "getblocksubsidy",        &getblocksubsidy,        true  },
    { "mining",             "genminingCSV",           &genminingCSV,           true  },

#ifdef ENABLE_MINING
    /* Coin generation */
    { "generating",         "getgenerate",            &getgenerate,            true  },
    { "generating",         "setgenerate",            &setgenerate,            true  },
    { "generating",         "generate",               &generate,               true  },
#endif

    /* Raw transactions */
    { "rawtransactions",    "createrawtransaction",   &createrawtransaction,   true  },
    { "rawtransactions",    "decoderawtransaction",   &decoderawtransaction,   true  },
    { "rawtransactions",    "decodescript",           &decodescript,           true  },
    { "rawtransactions",    "getrawtransaction",      &getrawtransaction,      true  },
    { "rawtransactions",    "sendrawtransaction",     &sendrawtransaction,     false },
    { "rawtransactions",    "signrawtransaction",     &signrawtransaction,     false }, /* uses wallet if enabled */
#ifdef ENABLE_WALLET
    { "rawtransactions",    "fundrawtransaction",     &fundrawtransaction,     false },
#endif

    // auction
    { "auction",       "auctionaddress",    &auctionaddress,  true },

    // lotto
    { "lotto",       "lottoaddress",    &lottoaddress,  true },

    // fsm
    { "FSM",       "FSMaddress",   &FSMaddress, true },
    { "FSM", "FSMcreate",    &FSMcreate,  true },
    { "FSM",   "FSMlist",      &FSMlist,    true },
    { "FSM",   "FSMinfo",      &FSMinfo,    true },

    // fsm
    { "nSPV",   "nspv_getinfo",         &nspv_getinfo, true },
    { "nSPV",   "nspv_login",           &nspv_login, true },
    { "nSPV",   "nspv_listunspent",     &nspv_listunspent,  true },
    { "nSPV",   "nspv_mempool",         &nspv_mempool,  true },
    { "nSPV",   "nspv_listtransactions",&nspv_listtransactions,  true },
    { "nSPV",   "nspv_spentinfo",       &nspv_spentinfo,    true },
    { "nSPV",   "nspv_notarizations",   &nspv_notarizations,    true },
    { "nSPV",   "nspv_hdrsproof",       &nspv_hdrsproof,    true },
    { "nSPV",   "nspv_txproof",         &nspv_txproof,    true },
    { "nSPV",   "nspv_spend",           &nspv_spend,    true },
    { "nSPV",   "nspv_broadcast",       &nspv_broadcast,    true },
    { "nSPV",   "nspv_logout",          &nspv_logout,    true },
    { "nSPV",   "nspv_listccmoduleunspent",     &nspv_listccmoduleunspent,  true },

    // rewards
    { "rewards",       "rewardslist",       &rewardslist,     true },
    { "rewards",       "rewardsinfo",       &rewardsinfo,     true },
    { "rewards",       "rewardscreatefunding",       &rewardscreatefunding,     true },
    { "rewards",       "rewardsaddfunding",       &rewardsaddfunding,     true },
    { "rewards",       "rewardslock",       &rewardslock,     true },
    { "rewards",       "rewardsunlock",     &rewardsunlock,   true },
    { "rewards",       "rewardsaddress",    &rewardsaddress,  true },

    // faucet
    { "faucet",       "faucetinfo",      &faucetinfo,         true },
    { "faucet",       "faucetfund",      &faucetfund,         true },
    { "faucet",       "faucetget",       &faucetget,          true },
    { "faucet",       "faucetaddress",   &faucetaddress,      true },

		// Heir
	{ "heir",       "heiraddress",   &heiraddress,      true },
	{ "heir",       "heirfund",   &heirfund,      true },
	{ "heir",       "heiradd",    &heiradd,        true },
	{ "heir",       "heirclaim",  &heirclaim,     true },
/*	{ "heir",       "heirfundtokens",   &heirfundtokens,      true },
	{ "heir",       "heiraddtokens",    &heiraddtokens,        true },
	{ "heir",       "heirclaimtokens",  &heirclaimtokens,     true },*/
	{ "heir",       "heirinfo",   &heirinfo,      true },
	{ "heir",       "heirlist",   &heirlist,      true },

    // Channels
    { "channels",       "channelsaddress",   &channelsaddress,   true },
    { "channels",       "channelslist",      &channelslist,      true },
    { "channels",       "channelsinfo",      &channelsinfo,      true },
    { "channels",       "channelsopen",      &channelsopen,      true },
    { "channels",       "channelspayment",   &channelspayment,   true },
    { "channels",       "channelsclose",     &channelsclose,      true },
    { "channels",       "channelsrefund",    &channelsrefund,    true },

    // Oracles
    { "oracles",       "oraclesaddress",   &oraclesaddress,     true },
    { "oracles",       "oracleslist",      &oracleslist,        true },
    { "oracles",       "oraclesinfo",      &oraclesinfo,        true },
    { "oracles",       "oraclescreate",    &oraclescreate,      true },
    { "oracles",       "oraclesfund",  &oraclesfund,    true },
    { "oracles",       "oraclesregister",  &oraclesregister,    true },
    { "oracles",       "oraclessubscribe", &oraclessubscribe,   true },
    { "oracles",       "oraclesdata",      &oraclesdata,        true },
    { "oracles",       "oraclessample",   &oraclessample,     true },
    { "oracles",       "oraclessamples",   &oraclessamples,     true },

    // Prices
    { "prices",       "prices",      &prices,      true },
    { "prices",       "pricesaddress",      &pricesaddress,      true },
    { "prices",       "priceslist",         &priceslist,         true },
    { "prices",       "mypriceslist",         &mypriceslist,         true },
    { "prices",       "pricesinfo",         &pricesinfo,         true },
    { "prices",       "pricesbet",         &pricesbet,         true },
    { "prices",       "pricessetcostbasis",         &pricessetcostbasis,         true },
    { "prices",       "pricescashout",         &pricescashout,         true },
    { "prices",       "pricesrekt",         &pricesrekt,         true },
    { "prices",       "pricesaddfunding",         &pricesaddfunding,         true },
    { "prices",       "pricesgetorderbook",         &pricesgetorderbook,         true },
    { "prices",       "pricesrefillfund",         &pricesrefillfund,         true },

    // Pegs
    { "pegs",       "pegsaddress",   &pegsaddress,      true },

    // Marmara
    { "marmara",       "marmaraaddress",   &marmaraaddress,      true },
    { "marmara",       "marmarapoolpayout",   &marmara_poolpayout,      true },
    { "marmara",       "marmarareceive",   &marmara_receive,      true },
    { "marmara",       "marmaraissue",   &marmara_issue,      true },
    { "marmara",       "marmaratransfer",   &marmara_transfer,      true },
    { "marmara",       "marmarainfo",   &marmara_info,      true },
    { "marmara",       "marmaracreditloop",   &marmara_creditloop,      true },
    { "marmara",       "marmarasettlement",   &marmara_settlement,      true },
    { "marmara",       "marmaralock",   &marmara_lock,      true },

    // Payments
    { "payments",       "paymentsaddress",   &paymentsaddress,       true },
    { "payments",       "paymentstxidopret", &payments_txidopret,    true },
    { "payments",       "paymentscreate",    &payments_create,       true },
    { "payments",       "paymentsairdrop",   &payments_airdrop,      true },
    { "payments",       "paymentsairdroptokens",   &payments_airdroptokens,      true },
    { "payments",       "paymentslist",      &payments_list,         true },
    { "payments",       "paymentsinfo",      &payments_info,         true },
    { "payments",       "paymentsfund",      &payments_fund,         true },
    { "payments",       "paymentsmerge",     &payments_merge,        true },
    { "payments",       "paymentsrelease",   &payments_release,      true },

    { "CClib",       "cclibaddress",   &cclibaddress,      true },
    { "CClib",       "cclibinfo",   &cclibinfo,      true },
    { "CClib",       "cclib",   &cclib,      true },

    // Gateways
    { "gateways",       "gatewaysaddress",   &gatewaysaddress,      true },
    { "gateways",       "gatewayslist",      &gatewayslist,         true },
    { "gateways",       "gatewaysexternaladdress",      &gatewaysexternaladdress,         true },
    { "gateways",       "gatewaysdumpprivkey",      &gatewaysdumpprivkey,         true },
    { "gateways",       "gatewaysinfo",      &gatewaysinfo,         true },
    { "gateways",       "gatewaysbind",      &gatewaysbind,         true },
    { "gateways",       "gatewaysdeposit",   &gatewaysdeposit,      true },
    { "gateways",       "gatewaysclaim",     &gatewaysclaim,        true },
    { "gateways",       "gatewayswithdraw",  &gatewayswithdraw,     true },
    { "gateways",       "gatewayspartialsign",  &gatewayspartialsign,     true },
    { "gateways",       "gatewayscompletesigning",  &gatewayscompletesigning,     true },
    { "gateways",       "gatewaysmarkdone",  &gatewaysmarkdone,     true },
    { "gateways",       "gatewayspendingdeposits",   &gatewayspendingdeposits,      true },
    { "gateways",       "gatewayspendingwithdraws",   &gatewayspendingwithdraws,      true },
    { "gateways",       "gatewaysprocessed",   &gatewaysprocessed,  true },

    // dice
    { "dice",       "dicelist",      &dicelist,         true },
    { "dice",       "diceinfo",      &diceinfo,         true },
    { "dice",       "dicefund",      &dicefund,         true },
    { "dice",       "diceaddfunds",  &diceaddfunds,     true },
    { "dice",       "dicebet",       &dicebet,          true },
    { "dice",       "dicefinish",    &dicefinish,       true },
    { "dice",       "dicestatus",    &dicestatus,       true },
    { "dice",       "diceaddress",   &diceaddress,      true },

    // tokens & assets
	{ "tokens",       "assetsaddress",     &assetsaddress,      true },
    { "tokens",       "tokeninfo",        &tokeninfo,         true },
    { "tokens",       "tokenlist",        &tokenlist,         true },
    { "tokens",       "tokenorders",      &tokenorders,       true },
    { "tokens",       "mytokenorders",    &mytokenorders,     true },
    { "tokens",       "tokenaddress",     &tokenaddress,      true },
    { "tokens",       "tokenbalance",     &tokenbalance,      true },
    { "tokens",       "tokencreate",      &tokencreate,       true },
    { "tokens",       "tokentransfer",    &tokentransfer,     true },
    { "tokens",       "tokenbid",         &tokenbid,          true },
    { "tokens",       "tokencancelbid",   &tokencancelbid,    true },
    { "tokens",       "tokenfillbid",     &tokenfillbid,      true },
    { "tokens",       "tokenask",         &tokenask,          true },
    //{ "tokens",       "tokenswapask",     &tokenswapask,      true },
    { "tokens",       "tokencancelask",   &tokencancelask,    true },
    { "tokens",       "tokenfillask",     &tokenfillask,      true },
    //{ "tokens",       "tokenfillswap",    &tokenfillswap,     true },
    { "tokens",       "tokenconvert", &tokenconvert, true },

    // pegs
    { "pegs",       "pegscreate",     &pegscreate,      true },
    { "pegs",       "pegsfund",         &pegsfund,      true },
    { "pegs",       "pegsget",         &pegsget,        true },
    { "pegs",       "pegsredeem",         &pegsredeem,        true },
    { "pegs",       "pegsliquidate",         &pegsliquidate,        true },
    { "pegs",       "pegsexchange",         &pegsexchange,        true },
    { "pegs",       "pegsaccounthistory", &pegsaccounthistory,      true },
    { "pegs",       "pegsaccountinfo", &pegsaccountinfo,      true },
    { "pegs",       "pegsworstaccounts",         &pegsworstaccounts,      true },
    { "pegs",       "pegsinfo",         &pegsinfo,      true },

    /* Address index */
    { "addressindex",       "getaddressmempool",      &getaddressmempool,      true  },
    { "addressindex",       "getaddressutxos",        &getaddressutxos,        false },
    { "addressindex",       "checknotarization",      &checknotarization,      false },
    { "addressindex",       "getnotarypayinfo",       &getnotarypayinfo,       false },
    { "addressindex",       "getaddressdeltas",       &getaddressdeltas,       false },
    { "addressindex",       "getaddresstxids",        &getaddresstxids,        false },
    { "addressindex",       "getaddressbalance",      &getaddressbalance,      false },
    { "addressindex",       "getsnapshot",            &getsnapshot,            false },

    /* Utility functions */
    { "util",               "createmultisig",         &createmultisig,         true  },
    { "util",               "validateaddress",        &validateaddress,        true  }, /* uses wallet if enabled */
    { "util",               "verifymessage",          &verifymessage,          true  },
    { "util",               "txnotarizedconfirmed",   &txnotarizedconfirmed,   true  },
    { "util",               "decodeccopret",   &decodeccopret,   true  },
    { "util",               "estimatefee",            &estimatefee,            true  },
    { "util",               "estimatepriority",       &estimatepriority,       true  },
    { "util",               "z_validateaddress",      &z_validateaddress,      true  }, /* uses wallet if enabled */
    { "util",               "jumblr_deposit",       &jumblr_deposit,       true  },
    { "util",               "jumblr_secret",        &jumblr_secret,       true  },
    { "util",               "jumblr_pause",        &jumblr_pause,       true  },
    { "util",               "jumblr_resume",        &jumblr_resume,       true  },

    { "util",             "invalidateblock",        &invalidateblock,        true  },
    { "util",             "reconsiderblock",        &reconsiderblock,        true  },
    /* Not shown in help */
    { "hidden",             "setmocktime",            &setmocktime,            true  },


#ifdef ENABLE_WALLET
    /* Wallet */
    { "wallet",             "resendwallettransactions", &resendwallettransactions, true},
    { "wallet",             "addmultisigaddress",     &addmultisigaddress,     true  },
    { "wallet",             "backupwallet",           &backupwallet,           true  },
    { "wallet",             "dumpprivkey",            &dumpprivkey,            true  },
    { "wallet",             "dumpwallet",             &dumpwallet,             true  },
    { "wallet",             "encryptwallet",          &encryptwallet,          true  },
    { "wallet",             "getaccountaddress",      &getaccountaddress,      true  },
    { "wallet",             "getaccount",             &getaccount,             true  },
    { "wallet",             "getaddressesbyaccount",  &getaddressesbyaccount,  true  },
    { "wallet",             "cleanwallettransactions", &cleanwallettransactions, false },
    { "wallet",             "getbalance",             &getbalance,             false },
    { "wallet",             "getbalance64",           &getbalance64,             false },
    { "wallet",             "getnewaddress",          &getnewaddress,          true  },
//    { "wallet",             "getnewaddress64",        &getnewaddress64,          true  },
    { "wallet",             "getrawchangeaddress",    &getrawchangeaddress,    true  },
    { "wallet",             "getreceivedbyaccount",   &getreceivedbyaccount,   false },
    { "wallet",             "getreceivedbyaddress",   &getreceivedbyaddress,   false },
    { "wallet",             "gettransaction",         &gettransaction,         false },
    { "wallet",             "getunconfirmedbalance",  &getunconfirmedbalance,  false },
    { "wallet",             "getwalletinfo",          &getwalletinfo,          false },
    { "wallet",             "importprivkey",          &importprivkey,          true  },
    { "wallet",             "importwallet",           &importwallet,           true  },
    { "wallet",             "importaddress",          &importaddress,          true  },
    { "wallet",             "keypoolrefill",          &keypoolrefill,          true  },
    { "wallet",             "listaccounts",           &listaccounts,           false },
    { "wallet",             "listaddressgroupings",   &listaddressgroupings,   false },
    { "wallet",             "listlockunspent",        &listlockunspent,        false },
    { "wallet",             "listreceivedbyaccount",  &listreceivedbyaccount,  false },
    { "wallet",             "listreceivedbyaddress",  &listreceivedbyaddress,  false },
    { "wallet",             "listsinceblock",         &listsinceblock,         false },
    { "wallet",             "listtransactions",       &listtransactions,       false },
    { "wallet",             "listunspent",            &listunspent,            false },
    { "wallet",             "lockunspent",            &lockunspent,            true  },
    { "wallet",             "move",                   &movecmd,                false },
    { "wallet",             "sendfrom",               &sendfrom,               false },
    { "wallet",             "sendmany",               &sendmany,               false },
    { "wallet",             "sendtoaddress",          &sendtoaddress,          false },
    { "wallet",             "setaccount",             &setaccount,             true  },
    { "wallet",             "setpubkey",              &setpubkey,              true  },
    { "wallet",             "setstakingsplit",        &setstakingsplit,        true  },
    { "wallet",             "settxfee",               &settxfee,               true  },
    { "wallet",             "signmessage",            &signmessage,            true  },
    { "wallet",             "walletlock",             &walletlock,             true  },
    { "wallet",             "walletpassphrasechange", &walletpassphrasechange, true  },
    { "wallet",             "walletpassphrase",       &walletpassphrase,       true  },
    { "wallet",             "zcbenchmark",            &zc_benchmark,           true  },
    { "wallet",             "zcrawkeygen",            &zc_raw_keygen,          true  },
    { "wallet",             "zcrawjoinsplit",         &zc_raw_joinsplit,       true  },
    { "wallet",             "zcrawreceive",           &zc_raw_receive,         true  },
    { "wallet",             "zcsamplejoinsplit",      &zc_sample_joinsplit,    true  },
    { "wallet",             "z_listreceivedbyaddress",&z_listreceivedbyaddress,false },
    { "wallet",             "z_getbalance",           &z_getbalance,           false },
    { "wallet",             "z_gettotalbalance",      &z_gettotalbalance,      false },
    { "wallet",             "z_mergetoaddress",       &z_mergetoaddress,       false },
    { "wallet",             "z_sendmany",             &z_sendmany,             false },
    { "wallet",             "z_shieldcoinbase",       &z_shieldcoinbase,       false },
    { "wallet",             "z_getoperationstatus",   &z_getoperationstatus,   true  },
    { "wallet",             "z_getoperationresult",   &z_getoperationresult,   true  },
    { "wallet",             "z_listoperationids",     &z_listoperationids,     true  },
    { "wallet",             "z_getnewaddress",        &z_getnewaddress,        true  },
    { "wallet",             "z_listaddresses",        &z_listaddresses,        true  },
    { "wallet",             "z_exportkey",            &z_exportkey,            true  },
    { "wallet",             "z_importkey",            &z_importkey,            true  },
    { "wallet",             "z_exportviewingkey",     &z_exportviewingkey,     true  },
    { "wallet",             "z_importviewingkey",     &z_importviewingkey,     true  },
    { "wallet",             "z_exportwallet",         &z_exportwallet,         true  },
    { "wallet",             "z_importwallet",         &z_importwallet,         true  },
    { "wallet",             "opreturn_burn",          &opreturn_burn,          true  },

    // TODO: rearrange into another category
    { "disclosure",         "z_getpaymentdisclosure", &z_getpaymentdisclosure, true  },
    { "disclosure",         "z_validatepaymentdisclosure", &z_validatepaymentdisclosure, true }
#endif // ENABLE_WALLET
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
    if (strMethod != "getblocktemplate")
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

    //printf("RPC call: %s\n", strMethod.c_str());

    // Find method
    const CRPCCommand *pcmd = tableRPC[strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    g_rpcSignals.PreCommand(*pcmd);

    try
    {
        // Execute
        return pcmd->actor(params, false, CPubKey());
    }
    catch (const std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }

    g_rpcSignals.PostCommand(*pcmd);
}

std::string HelpExampleCli(const std::string& methodname, const std::string& args)
{
    if ( ASSETCHAINS_SYMBOL[0] == 0 ) {
        return "> komodo-cli " + methodname + " " + args + "\n";
    } else if ((strncmp(ASSETCHAINS_SYMBOL, "HUSH3", 5) == 0) ) {
        return "> hush-cli " + methodname + " " + args + "\n";
    } else {
        return "> komodo-cli -ac_name=" + strprintf("%s", ASSETCHAINS_SYMBOL) + " " + methodname + " " + args + "\n";
    }
}

std::string HelpExampleRpc(const std::string& methodname, const std::string& args)
{
    return  "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", "
        "\"method\": \"" + methodname + "\", \"params\": [" + args + "] }' -H 'content-type: text/plain;' http://127.0.0.1:" + to_string(ASSETCHAINS_RPCPORT) + "/\n";
}

string experimentalDisabledHelpMsg(const string& rpc, const string& enableArg)
{
    string daemon = ASSETCHAINS_SYMBOL[0] == 0 ? "komodod" : "hushd";
    string ticker = ASSETCHAINS_SYMBOL[0] == 0 ? "komodo" : ASSETCHAINS_SYMBOL;

    return "\nWARNING: " + rpc + " is disabled.\n"
        "To enable it, restart " + daemon + " with the -experimentalfeatures and\n"
        "-" + enableArg + " commandline options, or add these two lines\n"
        "to the " + ticker + ".conf file:\n\n"
        "experimentalfeatures=1\n"
        + enableArg + "=1\n";
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

void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds)
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
