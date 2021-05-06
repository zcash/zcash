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
#include <pubkey.h>

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

typedef UniValue(*rpcfn_type)(const UniValue& params, bool fHelp, const CPubKey& mypk);

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

extern UniValue getconnectioncount(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcnet.cpp
extern UniValue getaddressmempool(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getaddressutxos(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getaddressdeltas(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getaddresstxids(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getsnapshot(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getaddressbalance(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getpeerinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue checknotarization(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getnotarypayinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue ping(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue addnode(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue disconnectnode(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getaddednodeinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getnettotals(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue setban(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue listbanned(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue clearbanned(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue dumpprivkey(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdump.cpp
extern UniValue importprivkey(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue dumpwallet(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importwallet(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue getgenerate(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcmining.cpp
extern UniValue setgenerate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue generate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getlocalsolps(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getnetworksolps(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getnetworkhashps(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getmininginfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue prioritisetransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getblocktemplate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue submitblock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue estimatefee(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue estimatepriority(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue coinsupply(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokeninfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenlist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenorders(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue mytokenorders(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenbalance(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue assetsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokencreate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokentransfer(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenbid(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokencancelbid(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenfillbid(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenask(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokencancelask(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenfillask(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue tokenconvert(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue heiraddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue heirfund(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue heiradd(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue heirclaim(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue heirinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue heirlist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue channelsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclesaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oracleslist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclesinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclescreate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclesfund(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclesregister(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclessubscribe(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclesdata(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclessample(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue oraclessamples(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricesaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue priceslist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue mypriceslist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricesinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmaraaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmara_poolpayout(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmara_receive(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmara_issue(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmara_transfer(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmara_info(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmara_creditloop(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmara_settlement(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue marmara_lock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue paymentsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_release(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_fund(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_merge(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_txidopret(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_create(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_airdrop(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_airdroptokens(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_info(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue payments_list(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue cclibaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue cclibinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue cclib(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewayslist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysdumpprivkey(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysexternaladdress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysbind(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysdeposit(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysclaim(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewayswithdraw(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewayspartialsign(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewayscompletesigning(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysmarkdone(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewayspendingdeposits(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewayspendingwithdraws(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gatewaysprocessed(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue channelslist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue channelsinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue channelsopen(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue channelspayment(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue channelsclose(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue channelsrefund(const UniValue& params, bool fHelp, const CPubKey& mypk);
//extern UniValue tokenswapask(const UniValue& params, bool fHelp, const CPubKey& mypk);
//extern UniValue tokenfillswap(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue faucetfund(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue faucetget(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue faucetaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue faucetinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue rewardsinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue rewardslist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue rewardsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue rewardscreatefunding(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue rewardsaddfunding(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue rewardslock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue rewardsunlock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue diceaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue dicefund(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue dicelist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue diceinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue diceaddfunds(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue dicebet(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue dicefinish(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue dicestatus(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue lottoaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue FSMaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue FSMcreate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue FSMlist(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue FSMinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue auctionaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegscreate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsfund(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsget(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsredeem(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsliquidate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsexchange(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsaccounthistory(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsaccountinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsworstaccounts(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pegsinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue getnewaddress(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
//extern UniValue getnewaddress64(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue getaccountaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getrawchangeaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue setaccount(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getaccount(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getaddressesbyaccount(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue sendtoaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue signmessage(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue verifymessage(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getreceivedbyaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getreceivedbyaccount(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue cleanwallettransactions(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getbalance(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getbalance64(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getunconfirmedbalance(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue movecmd(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue sendfrom(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue sendmany(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue addmultisigaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue createmultisig(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue listreceivedbyaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue listreceivedbyaccount(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue listtransactions(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue listaddressgroupings(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue listaccounts(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue listsinceblock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gettransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue backupwallet(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue keypoolrefill(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue walletpassphrase(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue walletpassphrasechange(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue walletlock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue encryptwallet(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue validateaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue txnotarizedconfirmed(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue decodeccopret(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getiguanajson(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getnotarysendmany(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue geterablockheights(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue setpubkey(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue setstakingsplit(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getwalletinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getblockchaininfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getnetworkinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getdeprecationinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue setmocktime(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue resendwallettransactions(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue zc_benchmark(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue zc_raw_keygen(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue zc_raw_joinsplit(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue zc_raw_receive(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue zc_sample_joinsplit(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue jumblr_deposit(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue jumblr_secret(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue jumblr_pause(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue jumblr_resume(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue getrawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rcprawtransaction.cpp
extern UniValue listunspent(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue lockunspent(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue listlockunspent(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue createrawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue decoderawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue decodescript(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue fundrawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue signrawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue sendrawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gettxoutproof(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue verifytxoutproof(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue getblockcount(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcblockchain.cpp
extern UniValue getbestblockhash(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getdifficulty(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue settxfee(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getmempoolinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getrawmempool(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getblockhashes(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getblockdeltas(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getblockhash(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getblockheader(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getlastsegidstakes(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getblock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gettxoutsetinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue gettxout(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue verifychain(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getchaintips(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue invalidateblock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue reconsiderblock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getspentinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue selfimport(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importdual(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewayaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewayinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewaybind(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewaydeposit(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewaywithdraw(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewaypartialsign(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewaycompletesigning(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewaymarkdone(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewaypendingwithdraws(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importgatewayprocessed(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue genminingCSV(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue nspv_getinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_login(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_listtransactions(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_mempool(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_listunspent(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_spentinfo(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_notarizations(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_hdrsproof(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_txproof(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_spend(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_broadcast(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_logout(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue nspv_listccmoduleunspent(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue getblocksubsidy(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue z_exportkey(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdump.cpp
extern UniValue z_importkey(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdump.cpp
extern UniValue z_exportviewingkey(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdump.cpp
extern UniValue z_importviewingkey(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdump.cpp
extern UniValue z_getnewaddress(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_listaddresses(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_exportwallet(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdump.cpp
extern UniValue z_importwallet(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdump.cpp
extern UniValue z_listreceivedbyaddress(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_getbalance(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_gettotalbalance(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_mergetoaddress(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_sendmany(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_shieldcoinbase(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_getoperationstatus(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_getoperationresult(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_listoperationids(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue opreturn_burn(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcwallet.cpp
extern UniValue z_validateaddress(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcmisc.cpp
extern UniValue z_getpaymentdisclosure(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdisclosure.cpp
extern UniValue z_validatepaymentdisclosure(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdisclosure.cpp

extern UniValue MoMoMdata(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue calc_MoM(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue height_MoM(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue assetchainproof(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue crosschainproof(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getNotarisationsForBlock(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue scanNotarisationsDB(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getimports(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue getwalletburntransactions(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue migrate_converttoexport(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue migrate_createburntransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue migrate_createimporttransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue migrate_completeimporttransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue migrate_checkburntransactionsource(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue migrate_createnotaryapprovaltransaction(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue notaries(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue minerids(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue kvsearch(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue kvupdate(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue paxprice(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue paxpending(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue paxprices(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue paxdeposit(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue paxwithdraw(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue prices(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricesbet(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricessetcostbasis(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricescashout(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricesrekt(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricesaddfunding(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricesgetorderbook(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue pricesrefillfund(const UniValue& params, bool fHelp, const CPubKey& mypk);



#endif // BITCOIN_RPCSERVER_H
