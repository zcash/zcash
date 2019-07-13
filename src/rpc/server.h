// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
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

#ifndef BITCOIN_RPCSERVER_H
#define BITCOIN_RPCSERVER_H

#include "amount.h"
#include "rpc/protocol.h"
#include "uint256.h"

#include <list>
#include <map>
#include <stdint.h>
#include <string>
#include <memory>

#include <boost/function.hpp>

#include <univalue.h>

class AsyncRPCQueue;
class CRPCCommand;

namespace RPCServer
{
    void OnStarted(boost::function<void ()> slot);
    void OnStopped(boost::function<void ()> slot);
    void OnPreCommand(boost::function<void (const CRPCCommand&)> slot);
    void OnPostCommand(boost::function<void (const CRPCCommand&)> slot);
}

class CBlockIndex;
class CNetAddr;

class JSONRequest
{
public:
    UniValue id;
    std::string strMethod;
    UniValue params;

    JSONRequest() { id = NullUniValue; }
    void parse(const UniValue& valRequest);
};

/** Query whether RPC is running */
bool IsRPCRunning();

/** Get the async queue*/
std::shared_ptr<AsyncRPCQueue> getAsyncRPCQueue();


/**
 * Set the RPC warmup status.  When this is done, all RPC calls will error out
 * immediately with RPC_IN_WARMUP.
 */
void SetRPCWarmupStatus(const std::string& newStatus);
/* Mark warmup as done.  RPC calls will be processed from now on.  */
void SetRPCWarmupFinished();

/* returns the current warmup state.  */
bool RPCIsInWarmup(std::string *statusOut);

/**
 * Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
 * the right number of arguments are passed, just that any passed are the correct type.
 * Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
 */
void RPCTypeCheck(const UniValue& params,
                  const std::list<UniValue::VType>& typesExpected, bool fAllowNull=false);

/*
  Check for expected keys/value types in an Object.
  Use like: RPCTypeCheckObj(object, boost::assign::map_list_of("name", str_type)("value", int_type));
*/
void RPCTypeCheckObj(const UniValue& o,
                  const std::map<std::string, UniValue::VType>& typesExpected, bool fAllowNull=false);

/** Opaque base class for timers returned by NewTimerFunc.
 * This provides no methods at the moment, but makes sure that delete
 * cleans up the whole state.
 */
class RPCTimerBase
{
public:
    virtual ~RPCTimerBase() {}
};

/**
 * RPC timer "driver".
 */
class RPCTimerInterface
{
public:
    virtual ~RPCTimerInterface() {}
    /** Implementation name */
    virtual const char *Name() = 0;
    /** Factory function for timers.
     * RPC will call the function to create a timer that will call func in *millis* milliseconds.
     * @note As the RPC mechanism is backend-neutral, it can use different implementations of timers.
     * This is needed to cope with the case in which there is no HTTP server, but
     * only GUI RPC console, and to break the dependency of rpcserver on httprpc.
     */
    virtual RPCTimerBase* NewTimer(boost::function<void(void)>& func, int64_t millis) = 0;
};

/** Register factory function for timers */
void RPCRegisterTimerInterface(RPCTimerInterface *iface);
/** Unregister factory function for timers */
void RPCUnregisterTimerInterface(RPCTimerInterface *iface);

/**
 * Run func nSeconds from now.
 * Overrides previous timer <name> (if any).
 */
void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds);

typedef UniValue(*rpcfn_type)(const UniValue& params, bool fHelp);

class CRPCCommand
{
public:
    std::string category;
    std::string name;
    rpcfn_type actor;
    bool okSafeMode;
};

/**
 * Bitcoin RPC command dispatcher.
 */
class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;
public:
    CRPCTable();
    const CRPCCommand* operator[](const std::string& name) const;
    std::string help(const std::string& name) const;

    /**
     * Execute a method.
     * @param method   Method to execute
     * @param params   UniValue Array of arguments (JSON objects)
     * @returns Result of the call.
     * @throws an exception (UniValue) when an error happens.
     */
    UniValue execute(const std::string &method, const UniValue &params) const;


    /**
     * Appends a CRPCCommand to the dispatch table.
     * Returns false if RPC server is already running (dump concurrency protection).
     * Commands cannot be overwritten (returns false).
     */
    bool appendCommand(const std::string& name, const CRPCCommand* pcmd);
};

extern CRPCTable tableRPC;

/**
 * Utilities: convert hex-encoded Values
 * (throws error if not hex).
 */
extern uint256 ParseHashV(const UniValue& v, std::string strName);
extern uint256 ParseHashO(const UniValue& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const UniValue& v, std::string strName);
extern std::vector<unsigned char> ParseHexO(const UniValue& o, std::string strKey);

extern int64_t nWalletUnlockTime;
extern CAmount AmountFromValue(const UniValue& value);
extern UniValue ValueFromAmount(const CAmount& amount);
extern double GetDifficulty(const CBlockIndex* blockindex = NULL);
extern double GetNetworkDifficulty(const CBlockIndex* blockindex = NULL);
extern std::string HelpRequiringPassphrase();
extern std::string HelpExampleCli(const std::string& methodname, const std::string& args);
extern std::string HelpExampleRpc(const std::string& methodname, const std::string& args);

extern void EnsureWalletIsUnlocked();

bool StartRPC();
void InterruptRPC();
void StopRPC();
std::string JSONRPCExecBatch(const UniValue& vReq);

extern std::string experimentalDisabledHelpMsg(const std::string& rpc, const std::string& enableArg);

extern UniValue getconnectioncount(const UniValue& params, bool fHelp); // in rpcnet.cpp
extern UniValue getaddressmempool(const UniValue& params, bool fHelp);
extern UniValue getaddressutxos(const UniValue& params, bool fHelp);
extern UniValue getaddressdeltas(const UniValue& params, bool fHelp);
extern UniValue getaddresstxids(const UniValue& params, bool fHelp);
extern UniValue getsnapshot(const UniValue& params, bool fHelp);
extern UniValue getaddressbalance(const UniValue& params, bool fHelp);
extern UniValue getpeerinfo(const UniValue& params, bool fHelp);
extern UniValue checknotarization(const UniValue& params, bool fHelp);
extern UniValue getnotarypayinfo(const UniValue& params, bool fHelp);
extern UniValue ping(const UniValue& params, bool fHelp);
extern UniValue addnode(const UniValue& params, bool fHelp);
extern UniValue disconnectnode(const UniValue& params, bool fHelp);
extern UniValue getaddednodeinfo(const UniValue& params, bool fHelp);
extern UniValue getnettotals(const UniValue& params, bool fHelp);
extern UniValue setban(const UniValue& params, bool fHelp);
extern UniValue listbanned(const UniValue& params, bool fHelp);
extern UniValue clearbanned(const UniValue& params, bool fHelp);

extern UniValue dumpprivkey(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue importprivkey(const UniValue& params, bool fHelp);
extern UniValue importaddress(const UniValue& params, bool fHelp);
extern UniValue dumpwallet(const UniValue& params, bool fHelp);
extern UniValue importwallet(const UniValue& params, bool fHelp);

extern UniValue getgenerate(const UniValue& params, bool fHelp); // in rpcmining.cpp
extern UniValue setgenerate(const UniValue& params, bool fHelp);
extern UniValue generate(const UniValue& params, bool fHelp);
extern UniValue getlocalsolps(const UniValue& params, bool fHelp);
extern UniValue getnetworksolps(const UniValue& params, bool fHelp);
extern UniValue getnetworkhashps(const UniValue& params, bool fHelp);
extern UniValue getmininginfo(const UniValue& params, bool fHelp);
extern UniValue prioritisetransaction(const UniValue& params, bool fHelp);
extern UniValue getblocktemplate(const UniValue& params, bool fHelp);
extern UniValue submitblock(const UniValue& params, bool fHelp);
extern UniValue estimatefee(const UniValue& params, bool fHelp);
extern UniValue estimatepriority(const UniValue& params, bool fHelp);
extern UniValue coinsupply(const UniValue& params, bool fHelp);
extern UniValue tokeninfo(const UniValue& params, bool fHelp);
extern UniValue tokenlist(const UniValue& params, bool fHelp);
extern UniValue tokenorders(const UniValue& params, bool fHelp);
extern UniValue mytokenorders(const UniValue& params, bool fHelp);
extern UniValue tokenbalance(const UniValue& params, bool fHelp);
extern UniValue assetsaddress(const UniValue& params, bool fHelp);
extern UniValue tokenaddress(const UniValue& params, bool fHelp);
extern UniValue tokencreate(const UniValue& params, bool fHelp);
extern UniValue tokentransfer(const UniValue& params, bool fHelp);
extern UniValue tokenbid(const UniValue& params, bool fHelp);
extern UniValue tokencancelbid(const UniValue& params, bool fHelp);
extern UniValue tokenfillbid(const UniValue& params, bool fHelp);
extern UniValue tokenask(const UniValue& params, bool fHelp);
extern UniValue tokencancelask(const UniValue& params, bool fHelp);
extern UniValue tokenfillask(const UniValue& params, bool fHelp);
extern UniValue tokenconvert(const UniValue& params, bool fHelp);
extern UniValue heiraddress(const UniValue& params, bool fHelp);
extern UniValue heirfund(const UniValue& params, bool fHelp);
extern UniValue heiradd(const UniValue& params, bool fHelp);
extern UniValue heirclaim(const UniValue& params, bool fHelp);
extern UniValue heirinfo(const UniValue& params, bool fHelp);
extern UniValue heirlist(const UniValue& params, bool fHelp);
extern UniValue channelsaddress(const UniValue& params, bool fHelp);
extern UniValue oraclesaddress(const UniValue& params, bool fHelp);
extern UniValue oracleslist(const UniValue& params, bool fHelp);
extern UniValue oraclesinfo(const UniValue& params, bool fHelp);
extern UniValue oraclescreate(const UniValue& params, bool fHelp);
extern UniValue oraclesfund(const UniValue& params, bool fHelp);
extern UniValue oraclesregister(const UniValue& params, bool fHelp);
extern UniValue oraclessubscribe(const UniValue& params, bool fHelp);
extern UniValue oraclesdata(const UniValue& params, bool fHelp);
extern UniValue oraclessample(const UniValue& params, bool fHelp);
extern UniValue oraclessamples(const UniValue& params, bool fHelp);
extern UniValue pricesaddress(const UniValue& params, bool fHelp);
extern UniValue priceslist(const UniValue& params, bool fHelp);
extern UniValue mypriceslist(const UniValue& params, bool fHelp);
extern UniValue pricesinfo(const UniValue& params, bool fHelp);
extern UniValue pegsaddress(const UniValue& params, bool fHelp);
extern UniValue marmaraaddress(const UniValue& params, bool fHelp);
extern UniValue marmara_poolpayout(const UniValue& params, bool fHelp);
extern UniValue marmara_receive(const UniValue& params, bool fHelp);
extern UniValue marmara_issue(const UniValue& params, bool fHelp);
extern UniValue marmara_transfer(const UniValue& params, bool fHelp);
extern UniValue marmara_info(const UniValue& params, bool fHelp);
extern UniValue marmara_creditloop(const UniValue& params, bool fHelp);
extern UniValue marmara_settlement(const UniValue& params, bool fHelp);
extern UniValue marmara_lock(const UniValue& params, bool fHelp);
extern UniValue paymentsaddress(const UniValue& params, bool fHelp);
extern UniValue payments_release(const UniValue& params, bool fHelp);
extern UniValue payments_fund(const UniValue& params, bool fHelp);
extern UniValue payments_merge(const UniValue& params, bool fHelp);
extern UniValue payments_txidopret(const UniValue& params, bool fHelp);
extern UniValue payments_create(const UniValue& params, bool fHelp);
extern UniValue payments_airdrop(const UniValue& params, bool fHelp);
extern UniValue payments_airdroptokens(const UniValue& params, bool fHelp);
extern UniValue payments_info(const UniValue& params, bool fHelp);
extern UniValue payments_list(const UniValue& params, bool fHelp);

extern UniValue cclibaddress(const UniValue& params, bool fHelp);
extern UniValue cclibinfo(const UniValue& params, bool fHelp);
extern UniValue cclib(const UniValue& params, bool fHelp);
extern UniValue gatewaysaddress(const UniValue& params, bool fHelp);
extern UniValue gatewayslist(const UniValue& params, bool fHelp);
extern UniValue gatewaysinfo(const UniValue& params, bool fHelp);
extern UniValue gatewaysdumpprivkey(const UniValue& params, bool fHelp);
extern UniValue gatewaysexternaladdress(const UniValue& params, bool fHelp);
extern UniValue gatewaysbind(const UniValue& params, bool fHelp);
extern UniValue gatewaysdeposit(const UniValue& params, bool fHelp);
extern UniValue gatewaysclaim(const UniValue& params, bool fHelp);
extern UniValue gatewayswithdraw(const UniValue& params, bool fHelp);
extern UniValue gatewayspartialsign(const UniValue& params, bool fHelp);
extern UniValue gatewayscompletesigning(const UniValue& params, bool fHelp);
extern UniValue gatewaysmarkdone(const UniValue& params, bool fHelp);
extern UniValue gatewayspendingdeposits(const UniValue& params, bool fHelp);
extern UniValue gatewayspendingwithdraws(const UniValue& params, bool fHelp);
extern UniValue gatewaysprocessed(const UniValue& params, bool fHelp);
extern UniValue channelslist(const UniValue& params, bool fHelp);
extern UniValue channelsinfo(const UniValue& params, bool fHelp);
extern UniValue channelsopen(const UniValue& params, bool fHelp);
extern UniValue channelspayment(const UniValue& params, bool fHelp);
extern UniValue channelsclose(const UniValue& params, bool fHelp);
extern UniValue channelsrefund(const UniValue& params, bool fHelp);
//extern UniValue tokenswapask(const UniValue& params, bool fHelp);
//extern UniValue tokenfillswap(const UniValue& params, bool fHelp);
extern UniValue faucetfund(const UniValue& params, bool fHelp);
extern UniValue faucetget(const UniValue& params, bool fHelp);
extern UniValue faucetaddress(const UniValue& params, bool fHelp);
extern UniValue faucetinfo(const UniValue& params, bool fHelp);
extern UniValue rewardsinfo(const UniValue& params, bool fHelp);
extern UniValue rewardslist(const UniValue& params, bool fHelp);
extern UniValue rewardsaddress(const UniValue& params, bool fHelp);
extern UniValue rewardscreatefunding(const UniValue& params, bool fHelp);
extern UniValue rewardsaddfunding(const UniValue& params, bool fHelp);
extern UniValue rewardslock(const UniValue& params, bool fHelp);
extern UniValue rewardsunlock(const UniValue& params, bool fHelp);
extern UniValue diceaddress(const UniValue& params, bool fHelp);
extern UniValue dicefund(const UniValue& params, bool fHelp);
extern UniValue dicelist(const UniValue& params, bool fHelp);
extern UniValue diceinfo(const UniValue& params, bool fHelp);
extern UniValue diceaddfunds(const UniValue& params, bool fHelp);
extern UniValue dicebet(const UniValue& params, bool fHelp);
extern UniValue dicefinish(const UniValue& params, bool fHelp);
extern UniValue dicestatus(const UniValue& params, bool fHelp);
extern UniValue lottoaddress(const UniValue& params, bool fHelp);
extern UniValue FSMaddress(const UniValue& params, bool fHelp);
extern UniValue FSMcreate(const UniValue& params, bool fHelp);
extern UniValue FSMlist(const UniValue& params, bool fHelp);
extern UniValue FSMinfo(const UniValue& params, bool fHelp);
extern UniValue auctionaddress(const UniValue& params, bool fHelp);
extern UniValue pegscreate(const UniValue& params, bool fHelp);
extern UniValue pegsfund(const UniValue& params, bool fHelp);
extern UniValue pegsget(const UniValue& params, bool fHelp);
extern UniValue pegsredeem(const UniValue& params, bool fHelp);
extern UniValue pegsliquidate(const UniValue& params, bool fHelp);
extern UniValue pegsexchange(const UniValue& params, bool fHelp);
extern UniValue pegsaccounthistory(const UniValue& params, bool fHelp);
extern UniValue pegsaccountinfo(const UniValue& params, bool fHelp);
extern UniValue pegsworstaccounts(const UniValue& params, bool fHelp);
extern UniValue pegsinfo(const UniValue& params, bool fHelp);

extern UniValue getnewaddress(const UniValue& params, bool fHelp); // in rpcwallet.cpp
//extern UniValue getnewaddress64(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue getaccountaddress(const UniValue& params, bool fHelp);
extern UniValue getrawchangeaddress(const UniValue& params, bool fHelp);
extern UniValue setaccount(const UniValue& params, bool fHelp);
extern UniValue getaccount(const UniValue& params, bool fHelp);
extern UniValue getaddressesbyaccount(const UniValue& params, bool fHelp);
extern UniValue sendtoaddress(const UniValue& params, bool fHelp);
extern UniValue signmessage(const UniValue& params, bool fHelp);
extern UniValue verifymessage(const UniValue& params, bool fHelp);
extern UniValue getreceivedbyaddress(const UniValue& params, bool fHelp);
extern UniValue getreceivedbyaccount(const UniValue& params, bool fHelp);
extern UniValue cleanwallettransactions(const UniValue& params, bool fHelp);
extern UniValue getbalance(const UniValue& params, bool fHelp);
extern UniValue getbalance64(const UniValue& params, bool fHelp);
extern UniValue getunconfirmedbalance(const UniValue& params, bool fHelp);
extern UniValue movecmd(const UniValue& params, bool fHelp);
extern UniValue sendfrom(const UniValue& params, bool fHelp);
extern UniValue sendmany(const UniValue& params, bool fHelp);
extern UniValue addmultisigaddress(const UniValue& params, bool fHelp);
extern UniValue createmultisig(const UniValue& params, bool fHelp);
extern UniValue listreceivedbyaddress(const UniValue& params, bool fHelp);
extern UniValue listreceivedbyaccount(const UniValue& params, bool fHelp);
extern UniValue listtransactions(const UniValue& params, bool fHelp);
extern UniValue listaddressgroupings(const UniValue& params, bool fHelp);
extern UniValue listaccounts(const UniValue& params, bool fHelp);
extern UniValue listsinceblock(const UniValue& params, bool fHelp);
extern UniValue gettransaction(const UniValue& params, bool fHelp);
extern UniValue backupwallet(const UniValue& params, bool fHelp);
extern UniValue keypoolrefill(const UniValue& params, bool fHelp);
extern UniValue walletpassphrase(const UniValue& params, bool fHelp);
extern UniValue walletpassphrasechange(const UniValue& params, bool fHelp);
extern UniValue walletlock(const UniValue& params, bool fHelp);
extern UniValue encryptwallet(const UniValue& params, bool fHelp);
extern UniValue validateaddress(const UniValue& params, bool fHelp);
extern UniValue txnotarizedconfirmed(const UniValue& params, bool fHelp);
extern UniValue decodeccopret(const UniValue& params, bool fHelp);
extern UniValue getinfo(const UniValue& params, bool fHelp);
extern UniValue getiguanajson(const UniValue& params, bool fHelp);
extern UniValue getnotarysendmany(const UniValue& params, bool fHelp);
extern UniValue geterablockheights(const UniValue& params, bool fHelp);
extern UniValue setpubkey(const UniValue& params, bool fHelp);
extern UniValue getwalletinfo(const UniValue& params, bool fHelp);
extern UniValue getblockchaininfo(const UniValue& params, bool fHelp);
extern UniValue getnetworkinfo(const UniValue& params, bool fHelp);
extern UniValue getdeprecationinfo(const UniValue& params, bool fHelp);
extern UniValue setmocktime(const UniValue& params, bool fHelp);
extern UniValue resendwallettransactions(const UniValue& params, bool fHelp);
extern UniValue zc_benchmark(const UniValue& params, bool fHelp);
extern UniValue zc_raw_keygen(const UniValue& params, bool fHelp);
extern UniValue zc_raw_joinsplit(const UniValue& params, bool fHelp);
extern UniValue zc_raw_receive(const UniValue& params, bool fHelp);
extern UniValue zc_sample_joinsplit(const UniValue& params, bool fHelp);

extern UniValue jumblr_deposit(const UniValue& params, bool fHelp);
extern UniValue jumblr_secret(const UniValue& params, bool fHelp);
extern UniValue jumblr_pause(const UniValue& params, bool fHelp);
extern UniValue jumblr_resume(const UniValue& params, bool fHelp);

extern UniValue getrawtransaction(const UniValue& params, bool fHelp); // in rcprawtransaction.cpp
extern UniValue listunspent(const UniValue& params, bool fHelp);
extern UniValue lockunspent(const UniValue& params, bool fHelp);
extern UniValue listlockunspent(const UniValue& params, bool fHelp);
extern UniValue createrawtransaction(const UniValue& params, bool fHelp);
extern UniValue decoderawtransaction(const UniValue& params, bool fHelp);
extern UniValue decodescript(const UniValue& params, bool fHelp);
extern UniValue fundrawtransaction(const UniValue& params, bool fHelp);
extern UniValue signrawtransaction(const UniValue& params, bool fHelp);
extern UniValue sendrawtransaction(const UniValue& params, bool fHelp);
extern UniValue gettxoutproof(const UniValue& params, bool fHelp);
extern UniValue verifytxoutproof(const UniValue& params, bool fHelp);

extern UniValue getblockcount(const UniValue& params, bool fHelp); // in rpcblockchain.cpp
extern UniValue getbestblockhash(const UniValue& params, bool fHelp);
extern UniValue getdifficulty(const UniValue& params, bool fHelp);
extern UniValue settxfee(const UniValue& params, bool fHelp);
extern UniValue getmempoolinfo(const UniValue& params, bool fHelp);
extern UniValue getrawmempool(const UniValue& params, bool fHelp);
extern UniValue getblockhashes(const UniValue& params, bool fHelp);
extern UniValue getblockdeltas(const UniValue& params, bool fHelp);
extern UniValue getblockhash(const UniValue& params, bool fHelp);
extern UniValue getblockheader(const UniValue& params, bool fHelp);
extern UniValue getlastsegidstakes(const UniValue& params, bool fHelp);
extern UniValue getblock(const UniValue& params, bool fHelp);
extern UniValue gettxoutsetinfo(const UniValue& params, bool fHelp);
extern UniValue gettxout(const UniValue& params, bool fHelp);
extern UniValue verifychain(const UniValue& params, bool fHelp);
extern UniValue getchaintips(const UniValue& params, bool fHelp);
extern UniValue invalidateblock(const UniValue& params, bool fHelp);
extern UniValue reconsiderblock(const UniValue& params, bool fHelp);
extern UniValue getspentinfo(const UniValue& params, bool fHelp);
extern UniValue selfimport(const UniValue& params, bool fHelp);
extern UniValue importdual(const UniValue& params, bool fHelp);
extern UniValue importgatewayaddress(const UniValue& params, bool fHelp);
extern UniValue importgatewayinfo(const UniValue& params, bool fHelp);
extern UniValue importgatewaybind(const UniValue& params, bool fHelp);
extern UniValue importgatewaydeposit(const UniValue& params, bool fHelp);
extern UniValue importgatewaywithdraw(const UniValue& params, bool fHelp);
extern UniValue importgatewaypartialsign(const UniValue& params, bool fHelp);
extern UniValue importgatewaycompletesigning(const UniValue& params, bool fHelp);
extern UniValue importgatewaymarkdone(const UniValue& params, bool fHelp);
extern UniValue importgatewaypendingdeposits(const UniValue& params, bool fHelp);
extern UniValue importgatewaypendingwithdraws(const UniValue& params, bool fHelp);
extern UniValue importgatewayprocessed(const UniValue& params, bool fHelp);

extern UniValue nspv_getinfo(const UniValue& params, bool fHelp);
extern UniValue nspv_login(const UniValue& params, bool fHelp);
extern UniValue nspv_listtransactions(const UniValue& params, bool fHelp);
extern UniValue nspv_listunspent(const UniValue& params, bool fHelp);
extern UniValue nspv_spentinfo(const UniValue& params, bool fHelp);
extern UniValue nspv_notarizations(const UniValue& params, bool fHelp);
extern UniValue nspv_hdrsproof(const UniValue& params, bool fHelp);
extern UniValue nspv_txproof(const UniValue& params, bool fHelp);
extern UniValue nspv_spend(const UniValue& params, bool fHelp);
extern UniValue nspv_broadcast(const UniValue& params, bool fHelp);
extern UniValue nspv_logout(const UniValue& params, bool fHelp);

extern UniValue getblocksubsidy(const UniValue& params, bool fHelp);

extern UniValue z_exportkey(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue z_importkey(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue z_exportviewingkey(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue z_importviewingkey(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue z_getnewaddress(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_listaddresses(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_exportwallet(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue z_importwallet(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue z_listreceivedbyaddress(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_getbalance(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_gettotalbalance(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_mergetoaddress(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_sendmany(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_shieldcoinbase(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_getoperationstatus(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_getoperationresult(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_listoperationids(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue opreturn_burn(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue z_validateaddress(const UniValue& params, bool fHelp); // in rpcmisc.cpp
extern UniValue z_getpaymentdisclosure(const UniValue& params, bool fHelp); // in rpcdisclosure.cpp
extern UniValue z_validatepaymentdisclosure(const UniValue &params, bool fHelp); // in rpcdisclosure.cpp

extern UniValue MoMoMdata(const UniValue& params, bool fHelp);
extern UniValue calc_MoM(const UniValue& params, bool fHelp);
extern UniValue height_MoM(const UniValue& params, bool fHelp);
extern UniValue assetchainproof(const UniValue& params, bool fHelp);
extern UniValue crosschainproof(const UniValue& params, bool fHelp);
extern UniValue getNotarisationsForBlock(const UniValue& params, bool fHelp);
extern UniValue scanNotarisationsDB(const UniValue& params, bool fHelp);
extern UniValue getimports(const UniValue& params, bool fHelp);
extern UniValue getwalletburntransactions(const UniValue& params, bool fHelp);
extern UniValue migrate_converttoexport(const UniValue& params, bool fHelp);
extern UniValue migrate_createburntransaction(const UniValue& params, bool fHelp);
extern UniValue migrate_createimporttransaction(const UniValue& params, bool fHelp);
extern UniValue migrate_completeimporttransaction(const UniValue& params, bool fHelp);
extern UniValue migrate_checkburntransactionsource(const UniValue& params, bool fHelp);
extern UniValue migrate_createnotaryapprovaltransaction(const UniValue& params, bool fHelp);

extern UniValue notaries(const UniValue& params, bool fHelp);
extern UniValue minerids(const UniValue& params, bool fHelp);
extern UniValue kvsearch(const UniValue& params, bool fHelp);
extern UniValue kvupdate(const UniValue& params, bool fHelp);
extern UniValue paxprice(const UniValue& params, bool fHelp);
extern UniValue paxpending(const UniValue& params, bool fHelp);
extern UniValue paxprices(const UniValue& params, bool fHelp);
extern UniValue paxdeposit(const UniValue& params, bool fHelp);
extern UniValue paxwithdraw(const UniValue& params, bool fHelp);

extern UniValue prices(const UniValue& params, bool fHelp);
extern UniValue pricesbet(const UniValue& params, bool fHelp);
extern UniValue pricessetcostbasis(const UniValue& params, bool fHelp);
extern UniValue pricescashout(const UniValue& params, bool fHelp);
extern UniValue pricesrekt(const UniValue& params, bool fHelp);
extern UniValue pricesaddfunding(const UniValue& params, bool fHelp);
extern UniValue pricesgetorderbook(const UniValue& params, bool fHelp);
extern UniValue pricesrefillfund(const UniValue& params, bool fHelp);



#endif // BITCOIN_RPCSERVER_H
