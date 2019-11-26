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

#include "amount.h"
#include "consensus/upgrades.h"
#include "core_io.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpc/server.h"
#include "timedata.h"
#include "transaction_builder.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "primitives/transaction.h"
#include "zcbenchmarks.h"
#include "script/interpreter.h"
#include "zcash/zip32.h"
#include "notaries_staked.h"

#include "utiltime.h"
#include "asyncrpcoperation.h"
#include "asyncrpcqueue.h"
#include "wallet/asyncrpcoperation_mergetoaddress.h"
#include "wallet/asyncrpcoperation_sendmany.h"
#include "wallet/asyncrpcoperation_shieldcoinbase.h"

#include "consensus/upgrades.h"

#include "sodium.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>
//#include <utf8.h>

#include <univalue.h>

#include <numeric>

#include "komodo_defs.h"
#include <string.h>

using namespace std;

using namespace libzcash;

extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern std::string ASSETCHAINS_OVERRIDE_PUBKEY;
const std::string ADDR_TYPE_SPROUT = "sprout";
const std::string ADDR_TYPE_SAPLING = "sapling";
extern UniValue TxJoinSplitToJSON(const CTransaction& tx);
extern int32_t KOMODO_INSYNC;
uint32_t komodo_segid32(char *coinaddr);
int32_t komodo_dpowconfs(int32_t height,int32_t numconfs);
int32_t komodo_isnotaryvout(char *coinaddr,uint32_t tiptime); // from ac_private chains only
CBlockIndex *komodo_getblockindex(uint256 hash);

int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;
std::string CCerror;

// Private method:
UniValue z_getoperationstatus_IMPL(const UniValue&, bool);

#define PLAN_NAME_MAX   8
#define VALID_PLAN_NAME(x)  (strlen(x) <= PLAN_NAME_MAX)
#define THROW_IF_SYNCING(INSYNC)  if (INSYNC == 0) { throw runtime_error(strprintf("%s: Chain still syncing at height %d, aborting to prevent linkability analysis!",__FUNCTION__,chainActive.Tip()->GetHeight())); }

int tx_height( const uint256 &hash );

std::string HelpRequiringPassphrase()
{
    return pwalletMain && pwalletMain->IsCrypted()
        ? "\nRequires wallet passphrase to be set with walletpassphrase call."
        : "";
}

bool EnsureWalletIsAvailable(bool avoidException)
{
    if (!pwalletMain)
    {
        if (!avoidException)
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");
        else
            return false;
    }
    return true;
}

void EnsureWalletIsUnlocked()
{
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
}

void Lock2NSPV(const CPubKey &pk)
{
    if (!pk.IsValid())
    {
        ENTER_CRITICAL_SECTION(cs_main);
        ENTER_CRITICAL_SECTION(pwalletMain->cs_wallet);
    }
}

void Unlock2NSPV(const CPubKey &pk)
{
    if (!pk.IsValid())
    {
        LEAVE_CRITICAL_SECTION(cs_main);
        LEAVE_CRITICAL_SECTION(pwalletMain->cs_wallet);
    }
}

uint64_t komodo_accrued_interest(int32_t *txheightp,uint32_t *locktimep,uint256 hash,int32_t n,int32_t checkheight,uint64_t checkvalue,int32_t tipheight);

void WalletTxToJSON(const CWalletTx& wtx, UniValue& entry)
{
    //int32_t i,n,txheight; uint32_t locktime; uint64_t interest = 0;
    int confirms = wtx.GetDepthInMainChain();
    entry.push_back(Pair("rawconfirmations", confirms));
    if (wtx.IsCoinBase())
        entry.push_back(Pair("generated", true));
    if (confirms > 0)
    {
        entry.push_back(Pair("confirmations", komodo_dpowconfs((int32_t)komodo_blockheight(wtx.hashBlock),confirms)));
        entry.push_back(Pair("blockhash", wtx.hashBlock.GetHex()));
        entry.push_back(Pair("blockindex", wtx.nIndex));
        entry.push_back(Pair("blocktime", (uint64_t)komodo_blocktime(wtx.hashBlock)));
        entry.push_back(Pair("expiryheight", (int64_t)wtx.nExpiryHeight));
    } else entry.push_back(Pair("confirmations", confirms));
    uint256 hash = wtx.GetHash();
    entry.push_back(Pair("txid", hash.GetHex()));
    UniValue conflicts(UniValue::VARR);
    BOOST_FOREACH(const uint256& conflict, wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.push_back(Pair("walletconflicts", conflicts));
    entry.push_back(Pair("time", wtx.GetTxTime()));
    entry.push_back(Pair("timereceived", (int64_t)wtx.nTimeReceived));
    BOOST_FOREACH(const PAIRTYPE(string,string)& item, wtx.mapValue)
        entry.push_back(Pair(item.first, item.second));

    entry.push_back(Pair("vjoinsplit", TxJoinSplitToJSON(wtx)));
}

string AccountFromValue(const UniValue& value)
{
    string strAccount = value.get_str();
    //if (strAccount != "")
    //    throw JSONRPCError(RPC_WALLET_ACCOUNTS_UNSUPPORTED, "Accounts are unsupported");
    return strAccount;
}

char *komodo_chainname()
{
     return(ASSETCHAINS_SYMBOL[0] == 0 ? (char *)"KMD" : ASSETCHAINS_SYMBOL);
}

void OS_randombytes(unsigned char *x,long xlen);

UniValue getnewaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if ( KOMODO_NSPV_FULLNODE && !EnsureWalletIsAvailable(fHelp) )
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewaddress ( \"account\" )\n"
            "\nReturns a new " + strprintf("%s",komodo_chainname()) + " address for receiving payments.\n"
            "\nArguments:\n"
            "1. \"account\"        (string, optional) DEPRECATED. If provided, it MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "\nResult:\n"
            "\"" + strprintf("%s",komodo_chainname()) + "_address\"    (string) The new " + strprintf("%s",komodo_chainname()) + " address\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleRpc("getnewaddress", "")
        );

    if ( KOMODO_NSPV_SUPERLITE )
    {
        UniValue result(UniValue::VOBJ); uint8_t priv32[32];
#ifndef __WIN32
        OS_randombytes(priv32,sizeof(priv32));
#else
        randombytes_buf(priv32,sizeof(priv32));
#endif
        CKey key;
        key.Set(&priv32[0],&priv32[32], true);
        CPubKey pubkey = key.GetPubKey();
        CKeyID vchAddress = pubkey.GetID();
        result.push_back(Pair("wif",EncodeSecret(key)));
        result.push_back(Pair("address",EncodeDestination(vchAddress)));
        result.push_back(Pair("pubkey",HexStr(pubkey)));
        return(result);
    }
    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBook(keyID, strAccount, "receive");

    return EncodeDestination(keyID);
}


CTxDestination GetAccountAddress(std::string strAccount, bool bForceNew=false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    bool bKeyUsed = false;

    // Check if the current key has been used
    if (account.vchPubKey.IsValid())
    {
        CScript scriptPubKey = GetScriptForDestination(account.vchPubKey.GetID());
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && account.vchPubKey.IsValid();
             ++it)
        {
            const CWalletTx& wtx = (*it).second;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    bKeyUsed = true;
        }
    }

    // Generate a new key
    if (!account.vchPubKey.IsValid() || bForceNew || bKeyUsed)
    {
        if (!pwalletMain->GetKeyFromPool(account.vchPubKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

        pwalletMain->SetAddressBook(account.vchPubKey.GetID(), strAccount, "receive");
        walletdb.WriteAccount(strAccount, account);
    }

    return account.vchPubKey.GetID();
}

UniValue getaccountaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccountaddress \"account\"\n"
            "\nDEPRECATED. Returns the current " + strprintf("%s",komodo_chainname()) + " address for receiving payments to this account.\n"
            "\nArguments:\n"
            "1. \"account\"       (string, required) MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "\nResult:\n"
            "\"" + strprintf("%s",komodo_chainname()) + "_address\"   (string) The account " + strprintf("%s",komodo_chainname()) + " address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccountaddress", "")
            + HelpExampleCli("getaccountaddress", "\"\"")
            + HelpExampleCli("getaccountaddress", "\"myaccount\"")
            + HelpExampleRpc("getaccountaddress", "\"myaccount\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    UniValue ret(UniValue::VSTR);

    ret = EncodeDestination(GetAccountAddress(strAccount));
    return ret;
}


UniValue getrawchangeaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getrawchangeaddress\n"
            "\nReturns a new " + strprintf("%s",komodo_chainname()) + " address, for receiving change.\n"
            "This is for use with raw transactions, NOT normal use.\n"
            "\nResult:\n"
            "\"address\"    (string) The address\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawchangeaddress", "")
            + HelpExampleRpc("getrawchangeaddress", "")
       );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    CReserveKey reservekey(pwalletMain);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CKeyID keyID = vchPubKey.GetID();

    return EncodeDestination(keyID);
}


UniValue setaccount(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setaccount \"" + strprintf("%s",komodo_chainname()) + "_address\" \"account\"\n"
            "\nDEPRECATED. Sets the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"" + strprintf("%s",komodo_chainname()) + "_address\"  (string, required) The " + strprintf("%s",komodo_chainname()) + " address to be associated with an account.\n"
            "2. \"account\"         (string, required) MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "\nExamples:\n"
            + HelpExampleCli("setaccount", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" \"tabby\"")
            + HelpExampleRpc("setaccount", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\", \"tabby\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CTxDestination dest = DecodeDestination(params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Komodo address");
    }

    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Only add the account if the address is yours.
    if (IsMine(*pwalletMain, dest)) {
        // Detect when changing the account of an address that is the 'unused current key' of another account:
        if (pwalletMain->mapAddressBook.count(dest)) {
            std::string strOldAccount = pwalletMain->mapAddressBook[dest].name;
            if (dest == GetAccountAddress(strOldAccount)) {
                GetAccountAddress(strOldAccount, true);
            }
        }
        pwalletMain->SetAddressBook(dest, strAccount, "receive");
    }
    else
        throw JSONRPCError(RPC_MISC_ERROR, "setaccount can only be used with own address");

    return NullUniValue;
}


UniValue getaccount(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccount \"" + strprintf("%s",komodo_chainname()) + "_address\"\n"
            "\nDEPRECATED. Returns the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"" + strprintf("%s",komodo_chainname()) + "_address\"  (string, required) The " + strprintf("%s",komodo_chainname()) + " address for account lookup.\n"
            "\nResult:\n"
            "\"accountname\"        (string) the account address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccount", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\"")
            + HelpExampleRpc("getaccount", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CTxDestination dest = DecodeDestination(params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Komodo address");
    }

    std::string strAccount;
    std::map<CTxDestination, CAddressBookData>::iterator mi = pwalletMain->mapAddressBook.find(dest);
    if (mi != pwalletMain->mapAddressBook.end() && !(*mi).second.name.empty()) {
        strAccount = (*mi).second.name;
    }
    return strAccount;
}


UniValue getaddressesbyaccount(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressesbyaccount \"account\"\n"
            "\nDEPRECATED. Returns the list of addresses for the given account.\n"
            "\nArguments:\n"
            "1. \"account\"  (string, required) MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"" + strprintf("%s",komodo_chainname()) + "_address\"  (string) a " + strprintf("%s",komodo_chainname()) + " address associated with the given account\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressesbyaccount", "\"tabby\"")
            + HelpExampleRpc("getaddressesbyaccount", "\"tabby\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount = AccountFromValue(params[0]);

    // Find all addresses that have the given account
    UniValue ret(UniValue::VARR);
    for (const std::pair<CTxDestination, CAddressBookData>& item : pwalletMain->mapAddressBook) {
        const CTxDestination& dest = item.first;
        const std::string& strName = item.second.name;
        if (strName == strAccount) {
            ret.push_back(EncodeDestination(dest));
        }
    }
    return ret;
}

static void SendMoney(const CTxDestination &address, CAmount nValue, bool fSubtractFeeFromAmount, CWalletTx& wtxNew,uint8_t *opretbuf,int32_t opretlen,long int opretValue)
{
    CAmount curBalance = pwalletMain->GetBalance();

    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
//fprintf(stderr,"nValue %.8f vs curBalance %.8f\n",(double)nValue/COIN,(double)curBalance/COIN);
    if (nValue > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    // Parse Zcash address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    std::string strError;
    vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {scriptPubKey, nValue, fSubtractFeeFromAmount};
    vecSend.push_back(recipient);
    if ( opretlen > 0 && opretbuf != 0 )
    {
        CScript opretpubkey; int32_t i; uint8_t *ptr;
        opretpubkey.resize(opretlen);
        for (i=0; i<opretlen; i++)
        {
            opretpubkey[i] = opretbuf[i];
            //printf("%02x",ptr[i]);
        }
        //printf(" opretbuf[%d]\n",opretlen);
        CRecipient opret = { opretpubkey, opretValue, false };
        vecSend.push_back(opret);
    }
    if (!pwalletMain->CreateTransaction(vecSend, wtxNew, reservekey, nFeeRequired, nChangePosRet, strError)) {
        if (!fSubtractFeeFromAmount && nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
}


UniValue sendtoaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendtoaddress \"" + strprintf("%s",komodo_chainname()) + "_address\" amount ( \"comment\" \"comment-to\" subtractfeefromamount )\n"
            "\nSend an amount to a given address. The amount is a real and is rounded to the nearest 0.00000001\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"" + strprintf("%s",komodo_chainname()) + "_address\"  (string, required) The " + strprintf("%s",komodo_chainname()) + " address to send to.\n"
            "2. \"amount\"      (numeric, required) The amount in " + strprintf("%s",komodo_chainname()) + " to send. eg 0.1\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "5. subtractfeefromamount  (boolean, optional, default=false) The fee will be deducted from the amount being sent.\n"
            "                             The recipient will receive less " + strprintf("%s",komodo_chainname()) + " than you enter in the amount field.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" 0.1")
            + HelpExampleCli("sendtoaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleCli("sendtoaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" 0.1 \"\" \"\" true")
            + HelpExampleRpc("sendtoaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\", 0.1, \"donation\", \"seans outpost\"")
        );

    if ( ASSETCHAINS_PRIVATE != 0 && AmountFromValue(params[1]) > 0 )
    {
        if ( komodo_isnotaryvout((char *)params[0].get_str().c_str(),chainActive.LastTip()->nTime) == 0 )
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid " + strprintf("%s",komodo_chainname()) + " address");
        }
    }
    LOCK2(cs_main, pwalletMain->cs_wallet);

    CTxDestination dest = DecodeDestination(params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Komodo address");
    }

    // Amount
    CAmount nAmount = AmountFromValue(params[1]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();

    bool fSubtractFeeFromAmount = false;
    if (params.size() > 4)
        fSubtractFeeFromAmount = params[4].get_bool();

    EnsureWalletIsUnlocked();

    SendMoney(dest, nAmount, fSubtractFeeFromAmount, wtx,0,0,0);

    return wtx.GetHash().GetHex();
}

#include "komodo_defs.h"

#define KOMODO_KVPROTECTED 1
#define KOMODO_KVBINARY 2
#define KOMODO_KVDURATION 1440
#define IGUANA_MAXSCRIPTSIZE 10001
uint64_t PAX_fiatdest(uint64_t *seedp,int32_t tokomodo,char *destaddr,uint8_t pubkey37[37],char *coinaddr,int32_t height,char *base,int64_t fiatoshis);
int32_t komodo_opreturnscript(uint8_t *script,uint8_t type,uint8_t *opret,int32_t opretlen);
#define CRYPTO777_KMDADDR "RXL3YXG2ceaB6C5hfJcN4fvmLH2C34knhA"
extern int32_t KOMODO_PAX;
extern uint64_t KOMODO_INTERESTSUM,KOMODO_WALLETBALANCE;
int32_t komodo_is_issuer();
int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp);
int32_t komodo_isrealtime(int32_t *kmdheightp);
int32_t pax_fiatstatus(uint64_t *available,uint64_t *deposited,uint64_t *issued,uint64_t *withdrawn,uint64_t *approved,uint64_t *redeemed,char *base);
int32_t komodo_kvsearch(uint256 *refpubkeyp,int32_t current_height,uint32_t *flagsp,int32_t *heightp,uint8_t value[IGUANA_MAXSCRIPTSIZE],uint8_t *key,int32_t keylen);
int32_t komodo_kvcmp(uint8_t *refvalue,uint16_t refvaluesize,uint8_t *value,uint16_t valuesize);
uint64_t komodo_kvfee(uint32_t flags,int32_t opretlen,int32_t keylen);
uint256 komodo_kvsig(uint8_t *buf,int32_t len,uint256 privkey);
int32_t komodo_kvduration(uint32_t flags);
uint256 komodo_kvprivkey(uint256 *pubkeyp,char *passphrase);
int32_t komodo_kvsigverify(uint8_t *buf,int32_t len,uint256 _pubkey,uint256 sig);

UniValue kvupdate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    static uint256 zeroes;
    CWalletTx wtx; UniValue ret(UniValue::VOBJ);
    uint8_t keyvalue[IGUANA_MAXSCRIPTSIZE*8],opretbuf[IGUANA_MAXSCRIPTSIZE*8]; int32_t i,coresize,haveprivkey,duration,opretlen,height; uint16_t keylen=0,valuesize=0,refvaluesize=0; uint8_t *key,*value=0; uint32_t flags,tmpflags,n; struct komodo_kv *ptr; uint64_t fee; uint256 privkey,pubkey,refpubkey,sig;
    if (fHelp || params.size() < 3 )
        throw runtime_error(
            "kvupdate key \"value\" days passphrase\n"
            "\nStore a key value. This feature is only available for asset chains.\n"
            "\nArguments:\n"
            "1. key                      (string, required) key\n"
            "2. \"value\"                (string, required) value\n"
            "3. days                     (numeric, required) amount of days(1440 blocks/day) before the key expires. Minimum 1 day\n"
            "4. passphrase               (string, optional) passphrase required to update this key\n"
            "\nResult:\n"
            "{\n"
            "  \"coin\": \"xxxxx\",          (string) chain the key is stored on\n"
            "  \"height\": xxxxx,            (numeric) height the key was stored at\n"
            "  \"expiration\": xxxxx,        (numeric) height the key will expire\n"
            "  \"flags\": x,                 (string) amount of days the key will be stored \n"
            "  \"key\": \"xxxxx\",           (numeric) stored key\n"
            "  \"keylen\": xxxxx,            (numeric) length of the key\n"
            "  \"value\": \"xxxxx\"          (numeric) stored value\n"
            "  \"valuesize\": xxxxx,         (string) length of the stored value\n"
            "  \"fee\": xxxxx                (string) transaction fee paid to store the key\n"
            "  \"txid\": \"xxxxx\"           (string) transaction id\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("kvupdate", "examplekey \"examplevalue\" 2 examplepassphrase")
            + HelpExampleRpc("kvupdate", "\"examplekey\",\"examplevalue\",\"2\",\"examplepassphrase\"")
        );
    if (!EnsureWalletIsAvailable(fHelp))
        return 0;
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
        return(0);
    haveprivkey = 0;
    memset(&sig,0,sizeof(sig));
    memset(&privkey,0,sizeof(privkey));
    memset(&refpubkey,0,sizeof(refpubkey));
    memset(&pubkey,0,sizeof(pubkey));
    if ( (n= (int32_t)params.size()) >= 3 )
    {
        flags = atoi(params[2].get_str().c_str());
        //printf("flags.%d (%s) n.%d\n",flags,params[2].get_str().c_str(),n);
    } else flags = 0;
    if ( n >= 4 )
        privkey = komodo_kvprivkey(&pubkey,(char *)(n >= 4 ? params[3].get_str().c_str() : "password"));
    haveprivkey = 1;
    flags |= 1;
    /*for (i=0; i<32; i++)
        printf("%02x",((uint8_t *)&privkey)[i]);
    printf(" priv, ");
    for (i=0; i<32; i++)
        printf("%02x",((uint8_t *)&pubkey)[i]);
    printf(" pubkey, privkey derived from (%s)\n",(char *)params[3].get_str().c_str());
    */
    LOCK2(cs_main, pwalletMain->cs_wallet);
    if ( (keylen= (int32_t)strlen(params[0].get_str().c_str())) > 0 )
    {
        key = (uint8_t *)params[0].get_str().c_str();
        if ( n >= 2 && params[1].get_str().c_str() != 0 )
        {
            value = (uint8_t *)params[1].get_str().c_str();
            valuesize = (int32_t)strlen(params[1].get_str().c_str());
        }
        memcpy(keyvalue,key,keylen);
        if ( (refvaluesize= komodo_kvsearch(&refpubkey,chainActive.LastTip()->GetHeight(),&tmpflags,&height,&keyvalue[keylen],key,keylen)) >= 0 )
        {
            if ( (tmpflags & KOMODO_KVPROTECTED) != 0 )
            {
                if ( memcmp(&refpubkey,&pubkey,sizeof(refpubkey)) != 0 )
                {
                    ret.push_back(Pair("error",(char *)"cant modify write once key without passphrase"));
                    return ret;
                }
            }
            if ( keylen+refvaluesize <= sizeof(keyvalue) )
            {
                sig = komodo_kvsig(keyvalue,keylen+refvaluesize,privkey);
                if ( komodo_kvsigverify(keyvalue,keylen+refvaluesize,refpubkey,sig) < 0 )
                {
                    ret.push_back(Pair("error",(char *)"error verifying sig, passphrase is probably wrong"));
                    printf("VERIFY ERROR\n");
                    return ret;
                } // else printf("verified immediately\n");
            }
        }
        //for (i=0; i<32; i++)
        //    printf("%02x",((uint8_t *)&sig)[i]);
        //printf(" sig for keylen.%d + valuesize.%d\n",keylen,refvaluesize);
        ret.push_back(Pair("coin",(char *)(ASSETCHAINS_SYMBOL[0] == 0 ? "KMD" : ASSETCHAINS_SYMBOL)));
        height = chainActive.LastTip()->GetHeight();
        if ( memcmp(&zeroes,&refpubkey,sizeof(refpubkey)) != 0 )
            ret.push_back(Pair("owner",refpubkey.GetHex()));
        ret.push_back(Pair("height", (int64_t)height));
        duration = komodo_kvduration(flags); //((flags >> 2) + 1) * KOMODO_KVDURATION;
        ret.push_back(Pair("expiration", (int64_t)(height+duration)));
        ret.push_back(Pair("flags",(int64_t)flags));
        ret.push_back(Pair("key",params[0].get_str()));
        ret.push_back(Pair("keylen",(int64_t)keylen));
        if ( n >= 2 && params[1].get_str().c_str() != 0 )
        {
            ret.push_back(Pair("value",params[1].get_str()));
            ret.push_back(Pair("valuesize",valuesize));
        }
        iguana_rwnum(1,&keyvalue[0],sizeof(keylen),&keylen);
        iguana_rwnum(1,&keyvalue[2],sizeof(valuesize),&valuesize);
        iguana_rwnum(1,&keyvalue[4],sizeof(height),&height);
        iguana_rwnum(1,&keyvalue[8],sizeof(flags),&flags);
        memcpy(&keyvalue[12],key,keylen);
        if ( value != 0 )
            memcpy(&keyvalue[12 + keylen],value,valuesize);
        coresize = (int32_t)(sizeof(flags)+sizeof(height)+sizeof(uint16_t)*2+keylen+valuesize);
        if ( haveprivkey != 0 )
        {
            for (i=0; i<32; i++)
                keyvalue[12 + keylen + valuesize + i] = ((uint8_t *)&pubkey)[i];
            coresize += 32;
            if ( refvaluesize >=0 )
            {
                for (i=0; i<32; i++)
                    keyvalue[12 + keylen + valuesize + 32 + i] = ((uint8_t *)&sig)[i];
                coresize += 32;
            }
        }
        if ( (opretlen= komodo_opreturnscript(opretbuf,'K',keyvalue,coresize)) == 40 )
            opretlen++;
        //for (i=0; i<opretlen; i++)
        //    printf("%02x",opretbuf[i]);
        //printf(" opretbuf keylen.%d valuesize.%d height.%d (%02x %02x %02x)\n",*(uint16_t *)&keyvalue[0],*(uint16_t *)&keyvalue[2],*(uint32_t *)&keyvalue[4],keyvalue[8],keyvalue[9],keyvalue[10]);
        EnsureWalletIsUnlocked();
        fee = komodo_kvfee(flags,opretlen,keylen);
        ret.push_back(Pair("fee",(double)fee/COIN));
        CBitcoinAddress destaddress(CRYPTO777_KMDADDR);
        if (!destaddress.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid dest Bitcoin address");
        SendMoney(destaddress.Get(),10000,false,wtx,opretbuf,opretlen,fee);
        ret.push_back(Pair("txid",wtx.GetHash().GetHex()));
    } else ret.push_back(Pair("error",(char *)"null key"));
    return ret;
}

UniValue paxdeposit(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint64_t available,deposited,issued,withdrawn,approved,redeemed,seed,komodoshis = 0; int32_t height; char destaddr[64]; uint8_t i,pubkey37[33];
    bool fSubtractFeeFromAmount = false;
    if ( KOMODO_PAX == 0 )
    {
        throw runtime_error("paxdeposit disabled without -pax");
    }
    if ( komodo_is_issuer() != 0 )
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "paxdeposit only from KMD");
    if (!EnsureWalletIsAvailable(fHelp))
        throw runtime_error("paxdeposit needs wallet"); //return Value::null;
    if (fHelp || params.size() != 3)
        throw runtime_error("paxdeposit address fiatoshis base");
    LOCK2(cs_main, pwalletMain->cs_wallet);
    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Bitcoin address");
    int64_t fiatoshis = atof(params[1].get_str().c_str()) * COIN;
    std::string base = params[2].get_str();
    std::string dest;
    height = chainActive.LastTip()->GetHeight();
    if ( pax_fiatstatus(&available,&deposited,&issued,&withdrawn,&approved,&redeemed,(char *)base.c_str()) != 0 || available < fiatoshis )
    {
        fprintf(stderr,"available %llu vs fiatoshis %llu\n",(long long)available,(long long)fiatoshis);
        throw runtime_error("paxdeposit not enough available inventory");
    }
    komodoshis = PAX_fiatdest(&seed,0,destaddr,pubkey37,(char *)params[0].get_str().c_str(),height,(char *)base.c_str(),fiatoshis);
    dest.append(destaddr);
    CBitcoinAddress destaddress(CRYPTO777_KMDADDR);
    if (!destaddress.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid dest Bitcoin address");
    for (i=0; i<33; i++)
        fprintf(stderr,"%02x",pubkey37[i]);
    fprintf(stderr," ht.%d srcaddr.(%s) %s fiatoshis.%lld -> dest.(%s) komodoshis.%llu seed.%llx\n",height,(char *)params[0].get_str().c_str(),(char *)base.c_str(),(long long)fiatoshis,destaddr,(long long)komodoshis,(long long)seed);
    EnsureWalletIsUnlocked();
    CWalletTx wtx;
    uint8_t opretbuf[64]; int32_t opretlen; uint64_t fee = komodoshis / 1000;
    if ( fee < 10000 )
        fee = 10000;
    iguana_rwnum(1,&pubkey37[33],sizeof(height),&height);
    opretlen = komodo_opreturnscript(opretbuf,'D',pubkey37,37);
    SendMoney(address.Get(),fee,fSubtractFeeFromAmount,wtx,opretbuf,opretlen,komodoshis);
    return wtx.GetHash().GetHex();
}

UniValue paxwithdraw(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    CWalletTx wtx; std::string dest; int32_t kmdheight; uint64_t seed,komodoshis = 0; char destaddr[64]; uint8_t i,pubkey37[37]; bool fSubtractFeeFromAmount = false;
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
        return(0);
    if (!EnsureWalletIsAvailable(fHelp))
        return 0;
    throw runtime_error("paxwithdraw deprecated");
    if (fHelp || params.size() != 2)
        throw runtime_error("paxwithdraw address fiatamount");
    if ( komodo_isrealtime(&kmdheight) == 0 )
        return(0);
    LOCK2(cs_main, pwalletMain->cs_wallet);
    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Bitcoin address");
    int64_t fiatoshis = atof(params[1].get_str().c_str()) * COIN;
    komodoshis = PAX_fiatdest(&seed,1,destaddr,pubkey37,(char *)params[0].get_str().c_str(),kmdheight,ASSETCHAINS_SYMBOL,fiatoshis);
    dest.append(destaddr);
    CBitcoinAddress destaddress(CRYPTO777_KMDADDR);
    if (!destaddress.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid dest Bitcoin address");
    for (i=0; i<33; i++)
        printf("%02x",pubkey37[i]);
    printf(" kmdheight.%d srcaddr.(%s) %s fiatoshis.%lld -> dest.(%s) komodoshis.%llu seed.%llx\n",kmdheight,(char *)params[0].get_str().c_str(),ASSETCHAINS_SYMBOL,(long long)fiatoshis,destaddr,(long long)komodoshis,(long long)seed);
    EnsureWalletIsUnlocked();
    uint8_t opretbuf[64]; int32_t opretlen; uint64_t fee = fiatoshis / 1000;
    if ( fee < 10000 )
        fee = 10000;
    iguana_rwnum(1,&pubkey37[33],sizeof(kmdheight),&kmdheight);
    opretlen = komodo_opreturnscript(opretbuf,'W',pubkey37,37);
    SendMoney(destaddress.Get(),fee,fSubtractFeeFromAmount,wtx,opretbuf,opretlen,fiatoshis);
    return wtx.GetHash().GetHex();
}

UniValue listaddressgroupings(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp)
        throw runtime_error(
            "listaddressgroupings\n"
            "\nLists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"" + strprintf("%s",komodo_chainname()) + " address\",     (string) The " + strprintf("%s",komodo_chainname()) + " address\n"
            "      amount,                 (numeric) The amount in " + strprintf("%s",komodo_chainname()) + "\n"
            "      \"account\"             (string, optional) The account (DEPRECATED)\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue jsonGroupings(UniValue::VARR);
    std::map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances();
    for (const std::set<CTxDestination>& grouping : pwalletMain->GetAddressGroupings()) {
        UniValue jsonGrouping(UniValue::VARR);
        for (const CTxDestination& address : grouping)
        {
            UniValue addressInfo(UniValue::VARR);
            addressInfo.push_back(EncodeDestination(address));
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                if (pwalletMain->mapAddressBook.find(address) != pwalletMain->mapAddressBook.end()) {
                    addressInfo.push_back(pwalletMain->mapAddressBook.find(address)->second.name);
                }
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

UniValue signmessage(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 2)
        throw runtime_error(
            "signmessage \"t-addr\" \"message\"\n"
            "\nSign a message with the private key of a t-addr"
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"t-addr\"  (string, required) The transparent address to use for the private key.\n"
            "2. \"message\"         (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessage", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\", \"my message\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strMessage = params[1].get_str();

    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    CKey key;
    if (!pwalletMain->GetKey(*keyID, key)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");
    }

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

UniValue getreceivedbyaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaddress \"" + strprintf("%s",komodo_chainname()) + "_address\" ( minconf )\n"
            "\nReturns the total amount received by the given " + strprintf("%s",komodo_chainname()) + " address in transactions with at least minconf confirmations.\n"
            "\nArguments:\n"
            "1. \"" + strprintf("%s",komodo_chainname()) + "_address\"  (string, required) The " + strprintf("%s",komodo_chainname()) + " address for transactions.\n"
            "2. minconf             (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount   (numeric) The total amount in " + strprintf("%s",komodo_chainname()) + " received at this address.\n"
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" 0") +
            "\nThe amount with at least 6 confirmations, very safe\n"
            + HelpExampleCli("getreceivedbyaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\", 6")
       );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Bitcoin address
    CTxDestination dest = DecodeDestination(params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Komodo address");
    }
    CScript scriptPubKey = GetScriptForDestination(dest);
    if (!IsMine(*pwalletMain, scriptPubKey)) {
        return ValueFromAmount(0);
    }

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    CAmount nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            if (txout.scriptPubKey == scriptPubKey) {
                int nDepth    = wtx.GetDepthInMainChain();
                if( nMinDepth > 1 ) {
                    int nHeight    = tx_height(wtx.GetHash());
                    int dpowconfs  = komodo_dpowconfs(nHeight, nDepth);
                    if (dpowconfs >= nMinDepth) {
                        nAmount   += txout.nValue; // komodo_interest?
                    }
                } else {
                    if (nDepth  >= nMinDepth) {
                        nAmount += txout.nValue; // komodo_interest?
                    }
                }
            }
    }

    return  ValueFromAmount(nAmount);
}


UniValue getreceivedbyaccount(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaccount \"account\" ( minconf )\n"
            "\nDEPRECATED. Returns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, required) MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + strprintf("%s",komodo_chainname()) + " received for this account.\n"
            "\nExamples:\n"
            "\nAmount received by the default account with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaccount", "\"\"") +
            "\nAmount received at the tabby account including unconfirmed amounts with zero confirmations\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaccount", "\"tabby\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Get the set of pub keys assigned to account
    string strAccount = AccountFromValue(params[0]);
    set<CTxDestination> setAddress = pwalletMain->GetAccountAddresses(strAccount);

    // Tally
    CAmount nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*pwalletMain, address) && setAddress.count(address))
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue; // komodo_interest?
        }
    }

    return ValueFromAmount(nAmount);
}


CAmount GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth, const isminefilter& filter)
{
    CAmount nBalance = 0;

    // Tally wallet transactions
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (!CheckFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
            continue;

        CAmount nReceived, nSent, nFee;
        wtx.GetAccountAmounts(strAccount, nReceived, nSent, nFee, filter);

        int nDepth    = wtx.GetDepthInMainChain();
        if( nMinDepth > 1 ) {
            int nHeight    = tx_height(wtx.GetHash());
            int dpowconfs  = komodo_dpowconfs(nHeight, nDepth);
            if (nReceived != 0 && dpowconfs >= nMinDepth) {
                nBalance += nReceived;
            }
        } else {
            if (nReceived != 0 && wtx.GetDepthInMainChain() >= nMinDepth) {
                nBalance += nReceived;
            }
        }
        nBalance -= nSent + nFee;
    }

    // Tally internal accounting entries
    nBalance += walletdb.GetAccountCreditDebit(strAccount);

    return nBalance;
}

CAmount GetAccountBalance(const string& strAccount, int nMinDepth, const isminefilter& filter)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter);
}

UniValue cleanwallettransactions(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1 )
        throw runtime_error(
            "cleanwallettransactions \"txid\"\n"
            "\nRemove all txs that are spent. You can clear all txs bar one, by specifiying a txid.\n"
            "\nPlease backup your wallet.dat before running this command.\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, optional) The transaction id to keep.\n"
            "\nResult:\n"
            "{\n"
            "  \"total_transactions\" : n,         (numeric) Transactions in wallet of " + strprintf("%s",komodo_chainname()) + "\n"
            "  \"remaining_transactions\" : n,     (numeric) Transactions in wallet after clean.\n"
            "  \"removed_transactions\" : n,       (numeric) The number of transactions removed.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("cleanwallettransactions", "")
            + HelpExampleCli("cleanwallettransactions","\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("cleanwallettransactions", "")
            + HelpExampleRpc("cleanwallettransactions","\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);
    UniValue ret(UniValue::VOBJ);
    uint256 exception; int32_t txs = pwalletMain->mapWallet.size();
    std::vector<uint256> TxToRemove;
    if (params.size() == 1)
    {
        exception.SetHex(params[0].get_str());
        uint256 tmp_hash; CTransaction tmp_tx;
        if (GetTransaction(exception,tmp_tx,tmp_hash,false))
        {
            if ( !pwalletMain->IsMine(tmp_tx) )
            {
                throw runtime_error("\nThe transaction is not yours!\n");
            }
            else
            {
                for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
                {
                    const CWalletTx& wtx = (*it).second;
                    if ( wtx.GetHash() != exception )
                    {
                        TxToRemove.push_back(wtx.GetHash());
                    }
                }
            }
        }
        else
        {
            throw runtime_error("\nThe transaction could not be found!\n");
        }
    }
    else
    {
        // get all locked utxos to relock them later.
        vector<COutPoint> vLockedUTXO;
        pwalletMain->ListLockedCoins(vLockedUTXO);
        // unlock all coins so that the following call containes all utxos.
        pwalletMain->UnlockAllCoins();
        // listunspent call... this gets us all the txids that are unspent, we search this list for the oldest tx,
        vector<COutput> vecOutputs;
        assert(pwalletMain != NULL);
        pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
        int32_t oldestTxDepth = 0;
        BOOST_FOREACH(const COutput& out, vecOutputs)
        {
          if ( out.nDepth > oldestTxDepth )
              oldestTxDepth = out.nDepth;
        }
        oldestTxDepth = oldestTxDepth + 1; // add extra block just for safety.
        // lock all the previouly locked coins.
        BOOST_FOREACH(COutPoint &outpt, vLockedUTXO) {
            pwalletMain->LockCoin(outpt);
        }

        // then add all txs in the wallet before this block to the list to remove.
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (wtx.GetDepthInMainChain() > oldestTxDepth)
                TxToRemove.push_back(wtx.GetHash());
        }
    }

    // erase txs
    BOOST_FOREACH (uint256& hash, TxToRemove)
    {
        pwalletMain->EraseFromWallet(hash);
        LogPrintf("Erased %s from wallet.\n",hash.ToString().c_str());
    }

    // build return JSON for stats.
    int remaining = pwalletMain->mapWallet.size();
    ret.push_back(Pair("total_transactons", (int)txs));
    ret.push_back(Pair("remaining_transactons", (int)remaining));
    ret.push_back(Pair("removed_transactions", (int)(txs-remaining)));
    return  (ret);
}

UniValue getbalance(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            "getbalance ( \"account\" minconf includeWatchonly )\n"
            "\nReturns the server's total available balance.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, optional) DEPRECATED. If provided, it MUST be set to the empty string \"\" or to the string \"*\", either of which will give the total available balance. Passing any other string will result in an error.\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + strprintf("%s",komodo_chainname()) + " received for this account.\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("getbalance", "") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getbalance", "\"*\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 0)
        return  ValueFromAmount(pwalletMain->GetBalance());

    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and "getbalance * 1 true" should return the same number
        CAmount nBalance = 0;
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (!CheckFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
                continue;

            CAmount allFee;
            string strSentAccount;
            list<COutputEntry> listReceived;
            list<COutputEntry> listSent;
            wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);

            int nDepth    = wtx.GetDepthInMainChain();
            if( nMinDepth > 1 ) {
                 int nHeight    = tx_height(wtx.GetHash());
                 int dpowconfs  = komodo_dpowconfs(nHeight, nDepth);
                 if (dpowconfs >= nMinDepth) {
                    BOOST_FOREACH(const COutputEntry& r, listReceived)
                        nBalance += r.amount;
                 }
             } else {
                 if (nDepth >= nMinDepth) {
                    BOOST_FOREACH(const COutputEntry& r, listReceived)
                        nBalance += r.amount;
                 }
            }
            BOOST_FOREACH(const COutputEntry& s, listSent)
                nBalance -= s.amount;
            nBalance -= allFee;
        }
        return  ValueFromAmount(nBalance);
    }

    string strAccount = AccountFromValue(params[0]);

    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, filter);

    return ValueFromAmount(nBalance);
}

UniValue getunconfirmedbalance(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 0)
        throw runtime_error(
                "getunconfirmedbalance\n"
                "Returns the server's total unconfirmed balance\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ValueFromAmount(pwalletMain->GetUnconfirmedBalance());
}


UniValue movecmd(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
            "move \"fromaccount\" \"toaccount\" amount ( minconf \"comment\" )\n"
            "\nDEPRECATED. Move a specified amount from one account in your wallet to another.\n"
            "\nArguments:\n"
            "1. \"fromaccount\"   (string, required) MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "2. \"toaccount\"     (string, required) MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "3. amount            (numeric) Quantity of " + strprintf("%s",komodo_chainname()) + " to move between accounts.\n"
            "4. minconf           (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "5. \"comment\"       (string, optional) An optional comment, stored in the wallet only.\n"
            "\nResult:\n"
            "true|false           (boolean) true if successful.\n"
            "\nExamples:\n"
            "\nMove 0.01 " + strprintf("%s",komodo_chainname()) + " from the default account to the account named tabby\n"
            + HelpExampleCli("move", "\"\" \"tabby\" 0.01") +
            "\nMove 0.01 " + strprintf("%s",komodo_chainname()) + " timotei to akiko with a comment and funds have 6 confirmations\n"
            + HelpExampleCli("move", "\"timotei\" \"akiko\" 0.01 6 \"happy birthday!\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("move", "\"timotei\", \"akiko\", 0.01, 6, \"happy birthday!\"")
        );
    if ( ASSETCHAINS_PRIVATE != 0 )
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "cant use transparent addresses in private chain");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strFrom = AccountFromValue(params[0]);
    string strTo = AccountFromValue(params[1]);
    CAmount nAmount = AmountFromValue(params[2]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");
    if (params.size() > 3)
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)params[3].get_int();
    string strComment;
    if (params.size() > 4)
        strComment = params[4].get_str();

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.TxnBegin())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    walletdb.WriteAccountingEntry(debit);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    walletdb.WriteAccountingEntry(credit);

    if (!walletdb.TxnCommit())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    return true;
}


UniValue sendfrom(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error(
            "sendfrom \"fromaccount\" \"to" + strprintf("%s",komodo_chainname()) + "address\" amount ( minconf \"comment\" \"comment-to\" )\n"
            "\nDEPRECATED (use sendtoaddress). Sent an amount from an account to a " + strprintf("%s",komodo_chainname()) + " address.\n"
            "The amount is a real and is rounded to the nearest 0.00000001."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaccount\"       (string, required) MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "2. \"to" + strprintf("%s",komodo_chainname()) + "address\"  (string, required) The " + strprintf("%s",komodo_chainname()) + " address to send funds to.\n"
            "3. amount                (numeric, required) The amount in " + strprintf("%s",komodo_chainname()) + " (transaction fee is added on top).\n"
            "4. minconf               (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "5. \"comment\"           (string, optional) A comment used to store what the transaction is for. \n"
            "                                     This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"        (string, optional) An optional comment to store the name of the person or organization \n"
            "                                     to which you're sending the transaction. This is not part of the transaction, \n"
            "                                     it is just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"        (string) The transaction id.\n"
            "\nExamples:\n"
            "\nSend 0.01 " + strprintf("%s",komodo_chainname()) + " from the default account to the address, must have at least 1 confirmation\n"
            + HelpExampleCli("sendfrom", "\"\" \"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" 0.01") +
            "\nSend 0.01 from the tabby account to the given address, funds must have at least 6 confirmations\n"
            + HelpExampleCli("sendfrom", "\"tabby\" \"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" 0.01 6 \"donation\" \"seans outpost\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendfrom", "\"tabby\", \"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\", 0.01, 6, \"donation\", \"seans outpost\"")
        );
    if ( ASSETCHAINS_PRIVATE != 0 )
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "cant use transparent addresses in private chain");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strAccount = AccountFromValue(params[0]);
    CTxDestination dest = DecodeDestination(params[1].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Komodo address");
    }
    CAmount nAmount = AmountFromValue(params[2]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");
    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && !params[4].isNull() && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && !params[5].isNull() && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, ISMINE_SPENDABLE);
    if (nAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    SendMoney(dest, nAmount, false, wtx, 0, 0, 0);

    return wtx.GetHash().GetHex();
}


UniValue sendmany(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendmany \"fromaccount\" {\"address\":amount,...} ( minconf \"comment\" [\"address\",...] )\n"
            "\nSend multiple times. Amounts are decimal numbers with at most 8 digits of precision."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaccount\"         (string, required) MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"
            "2. \"amounts\"             (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric) The " + strprintf("%s",komodo_chainname()) + " address is the key, the numeric amount in " + strprintf("%s",komodo_chainname()) + " is the value\n"
            "      ,...\n"
            "    }\n"
            "3. minconf                 (numeric, optional, default=1) Only use the balance confirmed at least this many times.\n"
            "4. \"comment\"             (string, optional) A comment\n"
            "5. subtractfeefromamount   (string, optional) A json array with addresses.\n"
            "                           The fee will be equally deducted from the amount of each selected address.\n"
            "                           Those recipients will receive less " + strprintf("%s",komodo_chainname()) + " than you enter in their corresponding amount field.\n"
            "                           If no addresses are specified here, the sender pays the fee.\n"
            "    [\n"
            "      \"address\"            (string) Subtract fee from this address\n"
            "      ,...\n"
            "    ]\n"
            "\nResult:\n"
            "\"transactionid\"          (string) The transaction id for the send. Only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"
            "\nExamples:\n"
            "\nSend two amounts to two different addresses:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPVMY\\\":0.01,\\\"RRyyejME7LRTuvdziWsXkAbSW1fdiohGwK\\\":0.02}\"") +
            "\nSend two amounts to two different addresses setting the confirmation and comment:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPVMY\\\":0.01,\\\"RRyyejME7LRTuvdziWsXkAbSW1fdiohGwK\\\":0.02}\" 6 \"testing\"") +
            "\nSend two amounts to two different addresses, subtract fee from amount:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPVMY\\\":0.01,\\\"RRyyejME7LRTuvdziWsXkAbSW1fdiohGwK\\\":0.02}\" 1 \"\" \"[\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPVMY\\\",\\\"RRyyejME7LRTuvdziWsXkAbSW1fdiohGwK\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendmany", "\"\", {\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPVMY\":0.01,\"RRyyejME7LRTuvdziWsXkAbSW1fdiohGwK\":0.02}, 6, \"testing\"")
        );
    if ( ASSETCHAINS_PRIVATE != 0 )
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "cant use transparent addresses in private chain");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount = AccountFromValue(params[0]);
    UniValue sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();

    UniValue subtractFeeFromAmount(UniValue::VARR);
    if (params.size() > 4)
        subtractFeeFromAmount = params[4].get_array();

    std::vector<CRecipient> vecSend;

    CAmount totalAmount = 0;
    std::vector<std::string> keys = sendTo.getKeys();
    int32_t i = 0;
    for (const std::string& name_ : keys) {
        CTxDestination dest = DecodeDestination(name_);
        if (!IsValidDestination(dest)) {
            CScript tmpspk;
            tmpspk << ParseHex(name_) << OP_CHECKSIG;
            if ( !ExtractDestination(tmpspk, dest, true) )
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Komodo address or pubkey: ") + name_);
        }
        CScript scriptPubKey = GetScriptForDestination(dest);
        
        CAmount nAmount = AmountFromValue(sendTo[i]);
        i++;
        if (nAmount <= 0)
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");
        totalAmount += nAmount;

        bool fSubtractFeeFromAmount = false;
        for (size_t idx = 0; idx < subtractFeeFromAmount.size(); idx++) {
            const UniValue& addr = subtractFeeFromAmount[idx];
            if (addr.get_str() == name_)
                fSubtractFeeFromAmount = true;
        }

        CRecipient recipient = {scriptPubKey, nAmount, fSubtractFeeFromAmount};
        vecSend.push_back(recipient);
    }

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, ISMINE_SPENDABLE);
    if (totalAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    // Send
    CReserveKey keyChange(pwalletMain);
    CAmount nFeeRequired = 0;
    int nChangePosRet = -1;
    string strFailReason;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, nChangePosRet, strFailReason);
    if (!fCreated)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strFailReason);
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");

    return wtx.GetHash().GetHex();
}

// Defined in rpc/misc.cpp
extern CScript _createmultisig_redeemScript(const UniValue& params);

UniValue addmultisigaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 3)
    {
        string msg = "addmultisigaddress nrequired [\"key\",...] ( \"account\" )\n"
            "\nAdd a nrequired-to-sign multisignature address to the wallet.\n"
            "Each key is a " + strprintf("%s",komodo_chainname()) + " address or hex-encoded public key.\n"
            "If 'account' is specified (DEPRECATED), assign address to that account.\n"

            "\nArguments:\n"
            "1. nrequired        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keysobject\"   (string, required) A json array of " + strprintf("%s",komodo_chainname()) + " addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"  (string) " + strprintf("%s",komodo_chainname()) + " address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. \"account\"      (string, optional) DEPRECATED. If provided, MUST be set to the empty string \"\" to represent the default account. Passing any other string will result in an error.\n"

            "\nResult:\n"
            "\"" + strprintf("%s",komodo_chainname()) + "_address\"  (string) A " + strprintf("%s",komodo_chainname()) + " address associated with the keys.\n"

            "\nExamples:\n"
            "\nAdd a multisig address from 2 addresses\n"
            + HelpExampleCli("addmultisigaddress", "2 \"[\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\\\",\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\\\"]\"") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("addmultisigaddress", "2, \"[\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\\\",\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount;
    if (params.size() > 2)
        strAccount = AccountFromValue(params[2]);

    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);
    pwalletMain->AddCScript(inner);

    pwalletMain->SetAddressBook(innerID, strAccount, "send");
    return EncodeDestination(innerID);
}


struct tallyitem
{
    CAmount nAmount;
    int nConf;
    vector<uint256> txids;
    bool fIsWatchonly;
    int nHeight;
    tallyitem()
    {
        nAmount = 0;
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
        nHeight = 0;
    }
};

UniValue ListReceived(const UniValue& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    // Tally
    std::map<CTxDestination, tallyitem> mapTally;
    for (const std::pair<uint256, CWalletTx>& pairWtx : pwalletMain->mapWallet) {
        const CWalletTx& wtx = pairWtx.second;

        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        int nDepth    = wtx.GetDepthInMainChain();
        if( nMinDepth > 1 ) {
            int nHeight   = tx_height(wtx.GetHash());
            int dpowconfs = komodo_dpowconfs(nHeight, nDepth);
            if (dpowconfs < nMinDepth)
                continue;
        } else {
            if (nDepth < nMinDepth)
                continue;
        }

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            isminefilter mine = IsMine(*pwalletMain, address);
            if(!(mine & filter))
                continue;

            tallyitem& item = mapTally[address];
            item.nAmount += txout.nValue; // komodo_interest?
            item.nConf = min(item.nConf, nDepth);
            item.nHeight = komodo_blockheight(wtx.hashBlock);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
                item.fIsWatchonly = true;
        }
    }

    // Reply
    UniValue ret(UniValue::VARR);
    std::map<std::string, tallyitem> mapAccountTally;
    for (const std::pair<CTxDestination, CAddressBookData>& item : pwalletMain->mapAddressBook) {
        const CTxDestination& dest = item.first;
        const std::string& strAccount = item.second.name;
        std::map<CTxDestination, tallyitem>::iterator it = mapTally.find(dest);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        CAmount nAmount = 0;
        int nConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        int nHeight=0;
        if (it != mapTally.end())
        {
            nAmount = (*it).second.nAmount;
            nConf = (*it).second.nConf;
            fIsWatchonly = (*it).second.fIsWatchonly;
            nHeight = (*it).second.nHeight;
        }

        if (fByAccounts)
        {
            tallyitem& item = mapAccountTally[strAccount];
            item.nAmount += nAmount;
            item.nConf = min(item.nConf, nConf);
            item.fIsWatchonly = fIsWatchonly;
        }
        else
        {
            UniValue obj(UniValue::VOBJ);

            if(fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("address",       EncodeDestination(dest)));
            obj.push_back(Pair("account",       strAccount));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("rawconfirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : komodo_dpowconfs(nHeight, nConf))));
            UniValue transactions(UniValue::VARR);
            if (it != mapTally.end())
            {
                BOOST_FOREACH(const uint256& item, (*it).second.txids)
                {
                    transactions.push_back(item.GetHex());
                }
            }
            obj.push_back(Pair("txids", transactions));
            ret.push_back(obj);
        }
    }

    if (fByAccounts)
    {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it)
        {
            CAmount nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            int nHeight = (*it).second.nHeight;
            UniValue obj(UniValue::VOBJ);
            if((*it).second.fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("account",       (*it).first));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("rawconfirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : komodo_dpowconfs(nHeight, nConf))));
            ret.push_back(obj);
        }
    }

    return ret;
}

UniValue listreceivedbyaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listreceivedbyaddress ( minconf includeempty includeWatchonly)\n"
            "\nList balances by receiving address.\n"
            "\nArguments:\n"
            "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty  (numeric, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,        (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"account\" : \"accountname\",       (string) DEPRECATED. The account of the receiving address. The default account is \"\".\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in " + strprintf("%s",komodo_chainname()) + " received by the address\n"
            "    \"confirmations\" : n                (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "6 true")
            + HelpExampleRpc("listreceivedbyaddress", "6, true, true")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ListReceived(params, false);
}

UniValue listreceivedbyaccount(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listreceivedbyaccount ( minconf includeempty includeWatchonly)\n"
            "\nDEPRECATED. List balances by account.\n"
            "\nArguments:\n"
            "1. minconf      (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty (boolean, optional, default=false) Whether to include accounts that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,   (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"account\" : \"accountname\",  (string) The account name of the receiving account\n"
            "    \"amount\" : x.xxx,             (numeric) The total amount received by addresses with this account\n"
            "    \"confirmations\" : n           (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaccount", "")
            + HelpExampleCli("listreceivedbyaccount", "6 true")
            + HelpExampleRpc("listreceivedbyaccount", "6, true, true")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ListReceived(params, true);
}

static void MaybePushAddress(UniValue & entry, const CTxDestination &dest)
{
    if (IsValidDestination(dest)) {
        entry.push_back(Pair("address", EncodeDestination(dest)));
    }
}

void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter)
{
    CAmount nFee;
    string strSentAccount;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;

    bool bIsCoinbase = wtx.IsCoinBase();

    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);

    bool fAllAccounts = (strAccount == string("*"));
    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount))
    {
        BOOST_FOREACH(const COutputEntry& s, listSent)
        {
            UniValue entry(UniValue::VOBJ);
            if(involvesWatchonly || (::IsMine(*pwalletMain, s.destination) & ISMINE_WATCH_ONLY))
                entry.push_back(Pair("involvesWatchonly", true));
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.destination);
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.amount)));
            entry.push_back(Pair("vout", s.vout));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            entry.push_back(Pair("size", static_cast<uint64_t>(GetSerializeSize(static_cast<CTransaction>(wtx), SER_NETWORK, PROTOCOL_VERSION))));
            ret.push_back(entry);
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth)
    {
        BOOST_FOREACH(const COutputEntry& r, listReceived)
        {
            string account;
            //fprintf(stderr,"recv iter %s\n",wtx.GetHash().GetHex().c_str());
            if (pwalletMain->mapAddressBook.count(r.destination))
                account = pwalletMain->mapAddressBook[r.destination].name;
            if (fAllAccounts || (account == strAccount))
            {
                UniValue entry(UniValue::VOBJ);
                if(involvesWatchonly || (::IsMine(*pwalletMain, r.destination) & ISMINE_WATCH_ONLY))
                    entry.push_back(Pair("involvesWatchonly", true));
                entry.push_back(Pair("account", account));

                CTxDestination dest;
                if (CScriptExt::ExtractVoutDestination(wtx, r.vout, dest))
                    MaybePushAddress(entry, dest);
                else
                    MaybePushAddress(entry, r.destination);

                if (bIsCoinbase)
                {
                    int btm;
                    if (wtx.GetDepthInMainChain() < 1)
                        entry.push_back(Pair("category", "orphan"));
                    else if ((btm = wtx.GetBlocksToMaturity()) > 0)
                    {
                        entry.push_back(Pair("category", "immature"));
                        entry.push_back(Pair("blockstomaturity", btm));
                    }
                    else
                        entry.push_back(Pair("category", "generate"));
                }
                else
                {
                    entry.push_back(Pair("category", "receive"));
                }

                entry.push_back(Pair("amount", ValueFromAmount(r.amount)));
                entry.push_back(Pair("vout", r.vout));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                entry.push_back(Pair("size", static_cast<uint64_t>(GetSerializeSize(static_cast<CTransaction>(wtx), SER_NETWORK, PROTOCOL_VERSION))));
                ret.push_back(entry);
            }
        }
    }
}

void AcentryToJSON(const CAccountingEntry& acentry, const string& strAccount, UniValue& ret)
{
    bool fAllAccounts = (strAccount == string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount)
    {
        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}

UniValue listtransactions(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 4)
        throw runtime_error(
            "listtransactions ( \"account\" count from includeWatchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"    (string, optional) DEPRECATED. The account name. Should be \"*\".\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"account\":\"accountname\",       (string) DEPRECATED. The account name associated with the transaction. \n"
            "                                                It will be \"\" for the default account.\n"
            "    \"address\":\"" + strprintf("%s",komodo_chainname()) + "_address\",    (string) The " + strprintf("%s",komodo_chainname()) + " address of the transaction. Not present for \n"
            "                                                move transactions (category = move).\n"
            "    \"category\":\"send|receive|move\", (string) The transaction category. 'move' is a local (off blockchain)\n"
            "                                                transaction between accounts, and not associated with an address,\n"
            "                                                transaction id or block. 'send' and 'receive' transactions are \n"
            "                                                associated with an address, transaction id and block details\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + strprintf("%s",komodo_chainname()) + ". This is negative for the 'send' category, and for the\n"
            "                                         'move' category for moves outbound. It is positive for the 'receive' category,\n"
            "                                         and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + strprintf("%s",komodo_chainname()) + ". This is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"txid\": \"transactionid\", (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"otheraccount\": \"accountname\",  (string) For the 'move' category of transactions, the account the funds came \n"
            "                                          from (for receiving funds, positive amounts), or went to (for sending funds,\n"
            "                                          negative amounts).\n"
            "    \"size\": n,                (numeric) Transaction size in bytes\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listtransactions", "") +
            "\nList transactions 100 to 120\n"
            + HelpExampleCli("listtransactions", "\"*\" 20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listtransactions", "\"*\", 20, 100")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 3)
        if(params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue ret(UniValue::VARR);

    std::list<CAccountingEntry> acentries;
    CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        if (pwtx != 0)
        {
            //fprintf(stderr,"pwtx iter.%d %s\n",(int32_t)pwtx->nOrderPos,pwtx->GetHash().GetHex().c_str());
            ListTransactions(*pwtx, strAccount, 0, true, ret, filter);
        } //else fprintf(stderr,"null pwtx\n");
        CAccountingEntry *const pacentry = (*it).second.second;
        if (pacentry != 0)
            AcentryToJSON(*pacentry, strAccount, ret);

        if ((int)ret.size() >= (nCount+nFrom)) break;
    }
    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    vector<UniValue> arrTmp = ret.getValues();

    vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}

UniValue listaccounts(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listaccounts ( minconf includeWatchonly)\n"
            "\nDEPRECATED. Returns Object that has account names as keys, account balances as values.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) Only include transactions with at least this many confirmations\n"
            "2. includeWatchonly (bool, optional, default=false) Include balances in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "{                      (json object where keys are account names, and values are numeric balances\n"
            "  \"account\": x.xxx,  (numeric) The property name is the account name, and the value is the total balance for the account.\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            "\nList account balances where there at least 1 confirmation\n"
            + HelpExampleCli("listaccounts", "") +
            "\nList account balances including zero confirmation transactions\n"
            + HelpExampleCli("listaccounts", "0") +
            "\nList account balances for 6 or more confirmations\n"
            + HelpExampleCli("listaccounts", "6") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("listaccounts", "6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();
    isminefilter includeWatchonly = ISMINE_SPENDABLE;
    if(params.size() > 1)
        if(params[1].get_bool())
            includeWatchonly = includeWatchonly | ISMINE_WATCH_ONLY;

    map<string, CAmount> mapAccountBalances;
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, CAddressBookData)& entry, pwalletMain->mapAddressBook) {
        if (IsMine(*pwalletMain, entry.first) & includeWatchonly) // This address belongs to me
            mapAccountBalances[entry.second.name] = 0;
    }

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        CAmount nFee;
        string strSentAccount;
        list<COutputEntry> listReceived;
        list<COutputEntry> listSent;
        int nDepth = wtx.GetDepthInMainChain();
        if (wtx.GetBlocksToMaturity() > 0 || nDepth < 0)
            continue;
        wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, includeWatchonly);
        mapAccountBalances[strSentAccount] -= nFee;
        BOOST_FOREACH(const COutputEntry& s, listSent)
            mapAccountBalances[strSentAccount] -= s.amount;
        if (nDepth >= nMinDepth)
        {
            BOOST_FOREACH(const COutputEntry& r, listReceived)
                if (pwalletMain->mapAddressBook.count(r.destination))
                    mapAccountBalances[pwalletMain->mapAddressBook[r.destination].name] += r.amount;
                else
                    mapAccountBalances[""] += r.amount;
        }
    }

    list<CAccountingEntry> acentries;
    CWalletDB(pwalletMain->strWalletFile).ListAccountCreditDebit("*", acentries);
    BOOST_FOREACH(const CAccountingEntry& entry, acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    UniValue ret(UniValue::VOBJ);
    BOOST_FOREACH(const PAIRTYPE(string, CAmount)& accountBalance, mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

UniValue listsinceblock(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp)
        throw runtime_error(
            "listsinceblock ( \"blockhash\" target-confirmations includeWatchonly)\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, optional) The block hash to list transactions since\n"
            "2. target-confirmations:    (numeric, optional) The confirmations required, must be 1 or more\n"
            "3. includeWatchonly:        (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')"
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"account\":\"accountname\",       (string) DEPRECATED. The account name associated with the transaction. Will be \"\" for the default account.\n"
            "    \"address\":\"" + strprintf("%s",komodo_chainname()) + "_address\",    (string) The " + strprintf("%s",komodo_chainname()) + " address of the transaction. Not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",     (string) The transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + strprintf("%s",komodo_chainname()) + ". This is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. It is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + strprintf("%s",komodo_chainname()) + ". This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",     (string) The block hash containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",  (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (Jan 1 1970 GMT). Available for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"to\": \"...\",            (string) If a comment to is associated with the transaction.\n"
             "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the last block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listsinceblock", "")
            + HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBlockIndex *pindex = NULL;
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (params.size() > 0)
    {
        uint256 blockId;

        blockId.SetHex(params[0].get_str());
        BlockMap::iterator it = mapBlockIndex.find(blockId);
        if (it != mapBlockIndex.end())
            pindex = it->second;
    }

    if (params.size() > 1)
    {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
    }

    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    int depth = pindex ? (1 + chainActive.Height() - pindex->GetHeight()) : -1;

    UniValue transactions(UniValue::VARR);

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); it++)
    {
        CWalletTx tx = (*it).second;

        if (depth == -1 || tx.GetDepthInMainChain() < depth)
            ListTransactions(tx, "*", 0, true, transactions, filter);
    }

    CBlockIndex *pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : uint256();

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("transactions", transactions));
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;
}

UniValue gettransaction(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "gettransaction \"txid\" ( includeWatchonly )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "2. \"includeWatchonly\"    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"
            "\nResult:\n"
            "{\n"
            "  \"amount\" : x.xxx,        (numeric) The transaction amount in " + strprintf("%s",komodo_chainname()) + "\n"
            "  \"confirmations\" : n,     (numeric) The number of confirmations\n"
            "  \"blockhash\" : \"hash\",  (string) The block hash\n"
            "  \"blockindex\" : xx,       (numeric) The block index\n"
            "  \"blocktime\" : ttt,       (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id.\n"
            "  \"time\" : ttt,            (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,    (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"account\" : \"accountname\",  (string) DEPRECATED. The account name involved in the transaction, can be \"\" for the default account.\n"
            "      \"address\" : \"" + strprintf("%s",komodo_chainname()) + "_address\",   (string) The " + strprintf("%s",komodo_chainname()) + " address involved in the transaction\n"
            "      \"category\" : \"send|receive\",    (string) The category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx                  (numeric) The amount in " + strprintf("%s",komodo_chainname()) + "\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"vjoinsplit\" : [\n"
            "    {\n"
            "      \"anchor\" : \"treestateref\",          (string) Merkle root of note commitment tree\n"
            "      \"nullifiers\" : [ string, ... ]      (string) Nullifiers of input notes\n"
            "      \"commitments\" : [ string, ... ]     (string) Note commitments for note outputs\n"
            "      \"macs\" : [ string, ... ]            (string) Message authentication tags\n"
            "      \"vpub_old\" : x.xxx                  (numeric) The amount removed from the transparent value pool\n"
            "      \"vpub_new\" : x.xxx,                 (numeric) The amount added to the transparent value pool\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"         (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 1)
        if(params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    UniValue entry(UniValue::VOBJ);
    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    CAmount nCredit = wtx.GetCredit(filter);
    CAmount nDebit = wtx.GetDebit(filter);
    CAmount nNet = nCredit - nDebit;
    CAmount nFee = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

    entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
    if (wtx.IsFromMe(filter))
        entry.push_back(Pair("fee", ValueFromAmount(nFee)));

    WalletTxToJSON(wtx, entry);

    UniValue details(UniValue::VARR);
    ListTransactions(wtx, "*", 0, false, details, filter);
    entry.push_back(Pair("details", details));

    string strHex = EncodeHexTx(static_cast<CTransaction>(wtx));
    entry.push_back(Pair("hex", strHex));

    return entry;
}


UniValue backupwallet(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "backupwallet \"destination\"\n"
            "\nSafely copies wallet.dat to destination filename\n"
            "\nArguments:\n"
            "1. \"destination\"   (string, required) The destination filename, saved in the directory set by -exportdir option.\n"
            "\nResult:\n"
            "\"path\"             (string) The full path of the destination file\n"
            "\nExamples:\n"
            + HelpExampleCli("backupwallet", "\"backupdata\"")
            + HelpExampleRpc("backupwallet", "\"backupdata\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    boost::filesystem::path exportdir;
    try {
        exportdir = GetExportDir();
    } catch (const std::runtime_error& e) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, e.what());
    }
    if (exportdir.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Cannot backup wallet until the -exportdir option has been set");
    }
    std::string unclean = params[0].get_str();
    std::string clean = SanitizeFilename(unclean);
    if (clean.compare(unclean) != 0) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Filename is invalid as only alphanumeric characters are allowed.  Try '%s' instead.", clean));
    }
    boost::filesystem::path exportfilepath = exportdir / clean;

    if (!BackupWallet(*pwalletMain, exportfilepath.string()))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet backup failed!");

    return exportfilepath.string();
}


UniValue keypoolrefill(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "keypoolrefill ( newsize )\n"
            "\nFills the keypool."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize = 0;
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)params[0].get_int();
    }

    EnsureWalletIsUnlocked();
    pwalletMain->TopUpKeyPool(kpSize);

    if (pwalletMain->GetKeyPoolSize() < kpSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return NullUniValue;
}


static void LockWallet(CWallet* pWallet)
{
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = 0;
    pWallet->Lock();
}

UniValue walletpassphrase(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrase \"passphrase\" timeout\n"
            "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
            "This is needed prior to performing transactions related to private keys such as sending " + strprintf("%s",komodo_chainname()) + "\n"
            "\nArguments:\n"
            "1. \"passphrase\"     (string, required) The wallet passphrase\n"
            "2. timeout            (numeric, required) The time to keep the decryption key in seconds.\n"
            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nExamples:\n"
            "\nunlock the wallet for 60 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nLock the wallet again (before 60 seconds)\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() > 0)
    {
        if (!pwalletMain->Unlock(strWalletPass))
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }
    else
        throw runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");

    // No need to check return values, because the wallet was unlocked above
    pwalletMain->UpdateNullifierNoteMap();
    pwalletMain->TopUpKeyPool();

    int64_t nSleepTime = params[1].get_int64();
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = GetTime() + nSleepTime;
    RPCRunLater("lockwallet", boost::bind(LockWallet, pwalletMain), nSleepTime);

    return NullUniValue;
}


UniValue walletpassphrasechange(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrasechange \"oldpassphrase\" \"newpassphrase\"\n"
            "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"
            "\nArguments:\n"
            "1. \"oldpassphrase\"      (string) The current passphrase\n"
            "2. \"newpassphrase\"      (string) The new passphrase\n"
            "\nExamples:\n"
            + HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"")
            + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    return NullUniValue;
}


UniValue walletlock(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error(
            "walletlock\n"
            "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"
            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n"
            + HelpExampleCli("sendtoaddress", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletlock", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");

    {
        LOCK(cs_nWalletUnlockTime);
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return NullUniValue;
}

int32_t komodo_acpublic(uint32_t tiptime);

UniValue encryptwallet(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    string enableArg = "developerencryptwallet";
    int32_t flag = (komodo_acpublic(0) || ASSETCHAINS_SYMBOL[0] == 0);
    auto fEnableWalletEncryption = fExperimentalMode && GetBoolArg("-" + enableArg, flag);

    std::string strWalletEncryptionDisabledMsg = "";
    if (!fEnableWalletEncryption) {
        strWalletEncryptionDisabledMsg = experimentalDisabledHelpMsg("encryptwallet", enableArg);
    }

    if (!pwalletMain->IsCrypted() && (fHelp || params.size() != 1))
        throw runtime_error(
            "encryptwallet \"passphrase\"\n"
            + strWalletEncryptionDisabledMsg +
            "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
            "After this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "Use the walletpassphrase call for this, and then walletlock call.\n"
            "If the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "Note that this will shutdown the server.\n"
            "\nArguments:\n"
            "1. \"passphrase\"    (string) The pass phrase to encrypt the wallet with. It must be at least 1 character, but should be long.\n"
            "\nExamples:\n"
            "\nEncrypt you wallet\n"
            + HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending " + strprintf("%s",komodo_chainname()) + "\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can so something like sign\n"
            + HelpExampleCli("signmessage", "\"" + strprintf("%s",komodo_chainname()) + "_address\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("encryptwallet", "\"my pass phrase\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!fEnableWalletEncryption) {
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: wallet encryption is disabled.");
    }
    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; Komodo server stopping, restart to run with encrypted wallet. The keypool has been flushed, you need to make a new backup.";
}

UniValue lockunspent(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "lockunspent unlock [{\"txid\":\"txid\",\"vout\":n},...]\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending " + strprintf("%s",komodo_chainname()) + ".\n"
            "Locks are stored in memory only. Nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "Also see the listunspent call\n"
            "\nArguments:\n"
            "1. unlock            (boolean, required) Whether to unlock (true) or lock (false) the specified transactions\n"
            "2. \"transactions\"  (string, required) A json array of objects. Each object the txid (string) vout (numeric)\n"
            "     [           (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",    (string) The transaction id\n"
            "         \"vout\": n         (numeric) The output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "true|false    (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 1)
        RPCTypeCheck(params, boost::assign::list_of(UniValue::VBOOL));
    else
        RPCTypeCheck(params, boost::assign::list_of(UniValue::VBOOL)(UniValue::VARR));

    bool fUnlock = params[0].get_bool();

    if (params.size() == 1) {
        if (fUnlock)
            pwalletMain->UnlockAllCoins();
        return true;
    }

    UniValue outputs = params[1].get_array();
    for (size_t idx = 0; idx < outputs.size(); idx++) {
        const UniValue& output = outputs[idx];
        if (!output.isObject())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");
        const UniValue& o = output.get_obj();

        RPCTypeCheckObj(o, boost::assign::map_list_of("txid", UniValue::VSTR)("vout", UniValue::VNUM));

        string txid = find_value(o, "txid").get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        COutPoint outpt(uint256S(txid), nOutput);

        if (fUnlock)
            pwalletMain->UnlockCoin(outpt);
        else
            pwalletMain->LockCoin(outpt);
    }

    return true;
}

UniValue listlockunspent(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 0)
        throw runtime_error(
            "listlockunspent\n"
            "\nReturns list of temporarily unspendable outputs.\n"
            "See the lockunspent call to lock and unlock transactions for spending.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listlockunspent", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    vector<COutPoint> vOutpts;
    pwalletMain->ListLockedCoins(vOutpts);

    UniValue ret(UniValue::VARR);

    BOOST_FOREACH(COutPoint &outpt, vOutpts) {
        UniValue o(UniValue::VOBJ);

        o.push_back(Pair("txid", outpt.hash.GetHex()));
        o.push_back(Pair("vout", (int)outpt.n));
        ret.push_back(o);
    }

    return ret;
}

UniValue settxfee(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "settxfee amount\n"
            "\nSet the transaction fee per kB.\n"
            "\nArguments:\n"
            "1. amount         (numeric, required) The transaction fee in " + strprintf("%s",komodo_chainname()) + "/kB rounded to the nearest 0.00000001\n"
            "\nResult\n"
            "true|false        (boolean) Returns true if successful\n"
            "\nExamples:\n"
            + HelpExampleCli("settxfee", "0.00001")
            + HelpExampleRpc("settxfee", "0.00001")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Amount
    CAmount nAmount = AmountFromValue(params[0]);

    payTxFee = CFeeRate(nAmount, 1000);
    return true;
}

UniValue getwalletinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getwalletinfo\n"
            "Returns an object containing various wallet state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total confirmed balance of the wallet in " + strprintf("%s",komodo_chainname()) + "\n"
            "  \"unconfirmed_balance\": xxx, (numeric) the total unconfirmed balance of the wallet in " + strprintf("%s",komodo_chainname()) + "\n"
            "  \"immature_balance\": xxxxxx, (numeric) the total immature balance of the wallet in " + strprintf("%s",komodo_chainname()) + "\n"
            "  \"txcount\": xxxxxxx,         (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee configuration, set in " + CURRENCY_UNIT + "/kB\n"
            "  \"seedfp\": \"uint256\",        (string) the BLAKE2b-256 hash of the HD seed\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletinfo", "")
            + HelpExampleRpc("getwalletinfo", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("unconfirmed_balance", ValueFromAmount(pwalletMain->GetUnconfirmedBalance())));
    obj.push_back(Pair("immature_balance",    ValueFromAmount(pwalletMain->GetImmatureBalance())));
    obj.push_back(Pair("txcount",       (int)pwalletMain->mapWallet.size()));
    obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    obj.push_back(Pair("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK())));
    uint256 seedFp = pwalletMain->GetHDChain().seedFp;
    if (!seedFp.IsNull())
         obj.push_back(Pair("seedfp", seedFp.GetHex()));
    return obj;
}

UniValue resendwallettransactions(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 0)
        throw runtime_error(
            "resendwallettransactions\n"
            "Immediately re-broadcast unconfirmed wallet transactions to all peers.\n"
            "Intended only for testing; the wallet code periodically re-broadcasts\n"
            "automatically.\n"
            "Returns array of transaction ids that were re-broadcast.\n"
            );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::vector<uint256> txids = pwalletMain->ResendWalletTransactionsBefore(GetTime());
    UniValue result(UniValue::VARR);
    BOOST_FOREACH(const uint256& txid, txids)
    {
        result.push_back(txid.ToString());
    }
    return result;
}

extern uint32_t komodo_segid32(char *coinaddr);

UniValue listunspent(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listunspent ( minconf maxconf  [\"address\",...] )\n"
            "\nReturns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filter to only include txouts paid to specified addresses.\n"
            "Results are an array of Objects, each of which has:\n"
            "{txid, vout, scriptPubKey, amount, confirmations}\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. \"addresses\"    (string) A json array of " + strprintf("%s",komodo_chainname()) + " addresses to filter\n"
            "    [\n"
            "      \"address\"   (string) " + strprintf("%s",komodo_chainname()) + " address\n"
            "      ,...\n"
            "    ]\n"
            "\nResult\n"
            "[                   (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",          (string) the transaction id \n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"generated\" : true|false  (boolean) true if txout is a coinbase transaction output\n"
            "    \"address\" : \"address\",    (string) the Komodo address\n"
            "    \"account\" : \"account\",    (string) DEPRECATED. The associated account, or \"\" for the default account\n"
            "    \"scriptPubKey\" : \"key\",   (string) the script key\n"
            "    \"amount\" : x.xxx,         (numeric) the transaction amount in " + CURRENCY_UNIT + "\n"
            "    \"confirmations\" : n,      (numeric) The number of confirmations\n"
            "    \"redeemScript\" : n        (string) The redeemScript if scriptPubKey is P2SH\n"
            "    \"spendable\" : xxx         (bool) Whether we have the private keys to spend this output\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n"
            + HelpExampleCli("listunspent", "")
            + HelpExampleCli("listunspent", "6 9999999 \"[\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\\\",\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\\\"]\"")
            + HelpExampleRpc("listunspent", "6, 9999999 \"[\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\\\",\\\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\\\"]\"")
        );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM)(UniValue::VNUM)(UniValue::VARR));

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    std::set<CTxDestination> destinations;
    if (params.size() > 2) {
        UniValue inputs = params[2].get_array();
        for (size_t idx = 0; idx < inputs.size(); idx++) {
            const UniValue& input = inputs[idx];
            CTxDestination dest = DecodeDestination(input.get_str());
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Komodo address: ") + input.get_str());
            }
            if (!destinations.insert(dest).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + input.get_str());
            }
        }
    }

    UniValue results(UniValue::VARR);
    vector<COutput> vecOutputs;
    assert(pwalletMain != NULL);
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    BOOST_FOREACH(const COutput& out, vecOutputs) {
        int nDepth    = out.tx->GetDepthInMainChain();
        if( nMinDepth > 1 ) {
            int nHeight    = tx_height(out.tx->GetHash());
            int dpowconfs  = komodo_dpowconfs(nHeight, nDepth);
            if (dpowconfs < nMinDepth || dpowconfs > nMaxDepth)
                continue;
        } else {
            if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
                continue;
        }

        CTxDestination address;
        const CScript& scriptPubKey = out.tx->vout[out.i].scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);

        if (destinations.size() && (!fValidAddress || !destinations.count(address)))
            continue;

        CAmount nValue = out.tx->vout[out.i].nValue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        UniValue entry(UniValue::VOBJ); int32_t txheight = 0;
        entry.push_back(Pair("txid", out.tx->GetHash().GetHex()));
        entry.push_back(Pair("vout", out.i));
        entry.push_back(Pair("generated", out.tx->IsCoinBase()));

        if (fValidAddress) {
            entry.push_back(Pair("address", EncodeDestination(address)));
            entry.push_back(Pair("segid", (int)komodo_segid32((char*)EncodeDestination(address).c_str()) & 0x3f ));

            if (pwalletMain->mapAddressBook.count(address))
                entry.push_back(Pair("account", pwalletMain->mapAddressBook[address].name));

            if (scriptPubKey.IsPayToScriptHash()) {
                const CScriptID& hash = boost::get<CScriptID>(address);
                CScript redeemScript;
                if (pwalletMain->GetCScript(hash, redeemScript))
                    entry.push_back(Pair("redeemScript", HexStr(redeemScript.begin(), redeemScript.end())));
            }
        }
        entry.push_back(Pair("amount", ValueFromAmount(nValue)));
        if ( out.tx->nLockTime != 0 )
        {
            BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
            CBlockIndex *tipindex,*pindex = it->second;
            uint64_t interest; uint32_t locktime;
            if ( pindex != 0 && (tipindex= chainActive.LastTip()) != 0 )
            {
                interest = komodo_accrued_interest(&txheight,&locktime,out.tx->GetHash(),out.i,0,nValue,(int32_t)tipindex->GetHeight());
                //interest = komodo_interest(txheight,nValue,out.tx->nLockTime,tipindex->nTime);
                entry.push_back(Pair("interest",ValueFromAmount(interest)));
            }
            //fprintf(stderr,"nValue %.8f pindex.%p tipindex.%p locktime.%u txheight.%d pindexht.%d\n",(double)nValue/COIN,pindex,chainActive.LastTip(),locktime,txheight,pindex->GetHeight());
        }
        else if ( chainActive.LastTip() != 0 )
            txheight = (chainActive.LastTip()->GetHeight() - out.nDepth - 1);
        entry.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end())));
        entry.push_back(Pair("rawconfirmations",out.nDepth));
        entry.push_back(Pair("confirmations",komodo_dpowconfs(txheight,out.nDepth)));
        entry.push_back(Pair("spendable", out.fSpendable));
        results.push_back(entry);
    }
    return results;
}

uint64_t komodo_interestsum()
{
#ifdef ENABLE_WALLET
    if ( ASSETCHAINS_SYMBOL[0] == 0 && GetBoolArg("-disablewallet", false) == 0 && KOMODO_NSPV_FULLNODE )
    {
        uint64_t interest,sum = 0; int32_t txheight; uint32_t locktime;
        vector<COutput> vecOutputs;
        assert(pwalletMain != NULL);
        LOCK2(cs_main, pwalletMain->cs_wallet);
        pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
        BOOST_FOREACH(const COutput& out,vecOutputs)
        {
            CAmount nValue = out.tx->vout[out.i].nValue;
            if ( out.tx->nLockTime != 0 && out.fSpendable != 0 )
            {
                BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
                CBlockIndex *tipindex,*pindex = it->second;
                if ( pindex != 0 && (tipindex= chainActive.LastTip()) != 0 )
                {
                    interest = komodo_accrued_interest(&txheight,&locktime,out.tx->GetHash(),out.i,0,nValue,(int32_t)tipindex->GetHeight());
                    //interest = komodo_interest(pindex->GetHeight(),nValue,out.tx->nLockTime,tipindex->nTime);
                    sum += interest;
                }
            }
        }
        KOMODO_INTERESTSUM = sum;
        KOMODO_WALLETBALANCE = pwalletMain->GetBalance();
        return(sum);
    }
#endif
    return(0);
}


UniValue z_listunspent(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 4)
        throw runtime_error(
            "z_listunspent ( minconf maxconf includeWatchonly [\"zaddr\",...] )\n"
            "\nReturns array of unspent shielded notes with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filter to only include notes sent to specified addresses.\n"
            "When minconf is 0, unspent notes with zero confirmations are returned, even though they are not immediately spendable.\n"
            "Results are an array of Objects, each of which has:\n"
            "{txid, jsindex, jsoutindex, confirmations, address, amount, memo} (Sprout)\n"
            "{txid, outindex, confirmations, address, amount, memo} (Sapling)\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. includeWatchonly (bool, optional, default=false) Also include watchonly addresses (see 'z_importviewingkey')\n"
            "4. \"addresses\"      (string) A json array of zaddrs (both Sprout and Sapling) to filter on.  Duplicate addresses not allowed.\n"
            "    [\n"
            "      \"address\"     (string) zaddr\n"
            "      ,...\n"
            "    ]\n"
            "\nResult\n"
            "[                             (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",          (string) the transaction id \n"
            "    \"jsindex\" : n             (numeric) the joinsplit index\n"
            "    \"jsoutindex\" (sprout) : n          (numeric) the output index of the joinsplit\n"
            "    \"outindex\" (sapling) : n          (numeric) the output index\n"
            "    \"confirmations\" : n       (numeric) the number of confirmations\n"
            "    \"spendable\" : true|false  (boolean) true if note can be spent by wallet, false if note has zero confirmations, false if address is watchonly\n"
            "    \"address\" : \"address\",    (string) the shielded address\n"
            "    \"amount\": xxxxx,          (numeric) the amount of value in the note\n"
            "    \"memo\": xxxxx,            (string) hexademical string representation of memo field\n"
            "    \"change\": true|false,     (boolean) true if the address that received the note is also one of the sending addresses\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n"
            + HelpExampleCli("z_listunspent", "")
            + HelpExampleCli("z_listunspent", "6 9999999 false \"[\\\"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\\\",\\\"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\\\"]\"")
            + HelpExampleRpc("z_listunspent", "6,9999999,false,[\"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\",\"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\"]")
        );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM)(UniValue::VNUM)(UniValue::VBOOL)(UniValue::VARR));

    int nMinDepth = 1;
    if (params.size() > 0) {
        nMinDepth = params[0].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    int nMaxDepth = 9999999;
    if (params.size() > 1) {
        nMaxDepth = params[1].get_int();
    }
    if (nMaxDepth < nMinDepth) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maximum number of confirmations must be greater or equal to the minimum number of confirmations");
    }

    std::set<libzcash::PaymentAddress> zaddrs = {};

    bool fIncludeWatchonly = false;
    if (params.size() > 2) {
        fIncludeWatchonly = params[2].get_bool();
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // User has supplied zaddrs to filter on
    if (params.size() > 3) {
        UniValue addresses = params[3].get_array();
        if (addresses.size()==0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, addresses array is empty.");

        // Keep track of addresses to spot duplicates
        set<std::string> setAddress;

        // Sources
        for (const UniValue& o : addresses.getValues()) {
            if (!o.isStr()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected string");
            }
            string address = o.get_str();
            auto zaddr = DecodePaymentAddress(address);
            if (!IsValidPaymentAddress(zaddr)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, address is not a valid zaddr: ") + address);
            }
            auto hasSpendingKey = boost::apply_visitor(HaveSpendingKeyForPaymentAddress(pwalletMain), zaddr);
            if (!fIncludeWatchonly && !hasSpendingKey) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, spending key for address does not belong to wallet: ") + address);
            }
            zaddrs.insert(zaddr);

            if (setAddress.count(address)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ") + address);
            }
            setAddress.insert(address);
        }
    }
    else {
        // User did not provide zaddrs, so use default i.e. all addresses
        std::set<libzcash::SproutPaymentAddress> sproutzaddrs = {};
        pwalletMain->GetSproutPaymentAddresses(sproutzaddrs);

        // Sapling support
        std::set<libzcash::SaplingPaymentAddress> saplingzaddrs = {};
        pwalletMain->GetSaplingPaymentAddresses(saplingzaddrs);

        zaddrs.insert(sproutzaddrs.begin(), sproutzaddrs.end());
        zaddrs.insert(saplingzaddrs.begin(), saplingzaddrs.end());
    }

    UniValue results(UniValue::VARR);

    if (zaddrs.size() > 0) {
        std::vector<CSproutNotePlaintextEntry> sproutEntries;
        std::vector<SaplingNoteEntry> saplingEntries;
        pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, zaddrs, nMinDepth, nMaxDepth, true, !fIncludeWatchonly, false);
        std::set<std::pair<PaymentAddress, uint256>> nullifierSet = pwalletMain->GetNullifiersForAddresses(zaddrs);

        for (auto & entry : sproutEntries) {
            UniValue obj(UniValue::VOBJ);

            int nHeight   = tx_height(entry.jsop.hash);
            int dpowconfs = komodo_dpowconfs(nHeight, entry.confirmations);
            // Only return notarized results when minconf>1
            if (nMinDepth > 1 && dpowconfs == 1)
                continue;

            obj.push_back(Pair("txid", entry.jsop.hash.ToString()));
            obj.push_back(Pair("jsindex", (int)entry.jsop.js ));
            obj.push_back(Pair("jsoutindex", (int)entry.jsop.n));
            obj.push_back(Pair("confirmations", dpowconfs));
            obj.push_back(Pair("rawconfirmations", entry.confirmations));
            bool hasSproutSpendingKey = pwalletMain->HaveSproutSpendingKey(boost::get<libzcash::SproutPaymentAddress>(entry.address));
            obj.push_back(Pair("spendable", hasSproutSpendingKey));
            obj.push_back(Pair("address", EncodePaymentAddress(entry.address)));
            obj.push_back(Pair("amount", ValueFromAmount(CAmount(entry.plaintext.value()))));
            std::string data(entry.plaintext.memo().begin(), entry.plaintext.memo().end());
            obj.push_back(Pair("memo", HexStr(data)));
            if (hasSproutSpendingKey) {
                obj.push_back(Pair("change", pwalletMain->IsNoteSproutChange(nullifierSet, entry.address, entry.jsop)));
            }
            results.push_back(obj);
        }

        for (auto & entry : saplingEntries) {
            UniValue obj(UniValue::VOBJ);

            int nHeight   = tx_height(entry.op.hash);
            int dpowconfs = komodo_dpowconfs(nHeight, entry.confirmations);

            // Only return notarized results when minconf>1
            if (nMinDepth > 1 && dpowconfs == 1)
                continue;

            obj.push_back(Pair("txid", entry.op.hash.ToString()));
            obj.push_back(Pair("outindex", (int)entry.op.n));
            obj.push_back(Pair("confirmations", dpowconfs));
            obj.push_back(Pair("rawconfirmations", entry.confirmations));
            libzcash::SaplingIncomingViewingKey ivk;
            libzcash::SaplingFullViewingKey fvk;
            pwalletMain->GetSaplingIncomingViewingKey(boost::get<libzcash::SaplingPaymentAddress>(entry.address), ivk);
            pwalletMain->GetSaplingFullViewingKey(ivk, fvk);
            bool hasSaplingSpendingKey = pwalletMain->HaveSaplingSpendingKey(fvk);
            obj.push_back(Pair("spendable", hasSaplingSpendingKey));
            obj.push_back(Pair("address", EncodePaymentAddress(entry.address)));
            obj.push_back(Pair("amount", ValueFromAmount(CAmount(entry.note.value())))); // note.value() is equivalent to plaintext.value()
            obj.push_back(Pair("memo", HexStr(entry.memo)));
            if (hasSaplingSpendingKey) {
                obj.push_back(Pair("change", pwalletMain->IsNoteSaplingChange(nullifierSet, entry.address, entry.op)));
            }
            results.push_back(obj);
        }
    }

    return results;
}

UniValue fundrawtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "fundrawtransaction \"hexstring\"\n"
                            "\nAdd inputs to a transaction until it has enough in value to meet its out value.\n"
                            "This will not modify existing inputs, and will add one change output to the outputs.\n"
                            "Note that inputs which were signed may need to be resigned after completion since in/outputs have been added.\n"
                            "The inputs added will not be signed, use signrawtransaction for that.\n"
                            "\nArguments:\n"
                            "1. \"hexstring\"    (string, required) The hex string of the raw transaction\n"
                            "\nResult:\n"
                            "{\n"
                            "  \"hex\":       \"value\", (string)  The resulting raw transaction (hex-encoded string)\n"
                            "  \"fee\":       n,         (numeric) The fee added to the transaction\n"
                            "  \"changepos\": n          (numeric) The position of the added change output, or -1\n"
                            "}\n"
                            "\"hex\"             \n"
                            "\nExamples:\n"
                            "\nCreate a transaction with no inputs\n"
                            + HelpExampleCli("createrawtransaction", "\"[]\" \"{\\\"myaddress\\\":0.01}\"") +
                            "\nAdd sufficient unsigned inputs to meet the output value\n"
                            + HelpExampleCli("fundrawtransaction", "\"rawtransactionhex\"") +
                            "\nSign the transaction\n"
                            + HelpExampleCli("signrawtransaction", "\"fundedtransactionhex\"") +
                            "\nSend the transaction\n"
                            + HelpExampleCli("sendrawtransaction", "\"signedtransactionhex\"")
                            );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VSTR));

    // parse hex string from parameter
    CTransaction origTx;
    if (!DecodeHexTx(origTx, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");

    CMutableTransaction tx(origTx);
    CAmount nFee;
    string strFailReason;
    int nChangePos = -1;
    if(!pwalletMain->FundTransaction(tx, nFee, nChangePos, strFailReason))
        throw JSONRPCError(RPC_INTERNAL_ERROR, strFailReason);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hex", EncodeHexTx(tx)));
    result.push_back(Pair("changepos", nChangePos));
    result.push_back(Pair("fee", ValueFromAmount(nFee)));

    return result;
}

UniValue zc_sample_joinsplit(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp) {
        throw runtime_error(
            "zcsamplejoinsplit\n"
            "\n"
            "Perform a joinsplit and return the JSDescription.\n"
            );
    }

    LOCK(cs_main);

    uint256 joinSplitPubKey;
    uint256 anchor = SproutMerkleTree().root();
    JSDescription samplejoinsplit(true,
                                  *pzcashParams,
                                  joinSplitPubKey,
                                  anchor,
                                  {JSInput(), JSInput()},
                                  {JSOutput(), JSOutput()},
                                  0,
                                  0);

    CDataStream ss(SER_NETWORK, SAPLING_TX_VERSION | (1 << 31));
    ss << samplejoinsplit;

    return HexStr(ss.begin(), ss.end());
}

UniValue zc_benchmark(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp)) {
        return NullUniValue;
    }

    if (fHelp || params.size() < 2) {
        throw runtime_error(
            "zcbenchmark benchmarktype samplecount\n"
            "\n"
            "Runs a benchmark of the selected type samplecount times,\n"
            "returning the running times of each sample.\n"
            "\n"
            "Output: [\n"
            "  {\n"
            "    \"runningtime\": runningtime\n"
            "  },\n"
            "  {\n"
            "    \"runningtime\": runningtime\n"
            "  }\n"
            "  ...\n"
            "]\n"
            );
    }

    LOCK(cs_main);

    std::string benchmarktype = params[0].get_str();
    int samplecount = params[1].get_int();

    if (samplecount <= 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid samplecount");
    }

    std::vector<double> sample_times;

    JSDescription samplejoinsplit;

    if (benchmarktype == "verifyjoinsplit") {
        CDataStream ss(ParseHexV(params[2].get_str(), "js"), SER_NETWORK, SAPLING_TX_VERSION | (1 << 31));
        ss >> samplejoinsplit;
    }

    for (int i = 0; i < samplecount; i++) {
        if (benchmarktype == "sleep") {
            sample_times.push_back(benchmark_sleep());
        } else if (benchmarktype == "parameterloading") {
            sample_times.push_back(benchmark_parameter_loading());
        } else if (benchmarktype == "createjoinsplit") {
            if (params.size() < 3) {
                sample_times.push_back(benchmark_create_joinsplit());
            } else {
                int nThreads = params[2].get_int();
                std::vector<double> vals = benchmark_create_joinsplit_threaded(nThreads);
                // Divide by nThreads^2 to get average seconds per JoinSplit because
                // we are running one JoinSplit per thread.
                sample_times.push_back(std::accumulate(vals.begin(), vals.end(), 0.0) / (nThreads*nThreads));
            }
        } else if (benchmarktype == "verifyjoinsplit") {
            sample_times.push_back(benchmark_verify_joinsplit(samplejoinsplit));
#ifdef ENABLE_MINING
        } else if (benchmarktype == "solveequihash") {
            if (params.size() < 3) {
                sample_times.push_back(benchmark_solve_equihash());
            } else {
                int nThreads = params[2].get_int();
                std::vector<double> vals = benchmark_solve_equihash_threaded(nThreads);
                sample_times.insert(sample_times.end(), vals.begin(), vals.end());
            }
#endif
        } else if (benchmarktype == "verifyequihash") {
            sample_times.push_back(benchmark_verify_equihash());
        } else if (benchmarktype == "validatelargetx") {
            // Number of inputs in the spending transaction that we will simulate
            int nInputs = 11130;
            if (params.size() >= 3) {
                nInputs = params[2].get_int();
            }
            sample_times.push_back(benchmark_large_tx(nInputs));
        } else if (benchmarktype == "trydecryptnotes") {
            int nAddrs = params[2].get_int();
            sample_times.push_back(benchmark_try_decrypt_notes(nAddrs));
        } else if (benchmarktype == "incnotewitnesses") {
            int nTxs = params[2].get_int();
            sample_times.push_back(benchmark_increment_note_witnesses(nTxs));
        } else if (benchmarktype == "connectblockslow") {
            if (Params().NetworkIDString() != "regtest") {
                throw JSONRPCError(RPC_TYPE_ERROR, "Benchmark must be run in regtest mode");
            }
            sample_times.push_back(benchmark_connectblock_slow());
        } else if (benchmarktype == "sendtoaddress") {
            if (Params().NetworkIDString() != "regtest") {
                throw JSONRPCError(RPC_TYPE_ERROR, "Benchmark must be run in regtest mode");
            }
            auto amount = AmountFromValue(params[2]);
            sample_times.push_back(benchmark_sendtoaddress(amount));
        } else if (benchmarktype == "loadwallet") {
            if (Params().NetworkIDString() != "regtest") {
                throw JSONRPCError(RPC_TYPE_ERROR, "Benchmark must be run in regtest mode");
            }
            sample_times.push_back(benchmark_loadwallet());
        } else if (benchmarktype == "listunspent") {
            sample_times.push_back(benchmark_listunspent());
        } else if (benchmarktype == "createsaplingspend") {
            sample_times.push_back(benchmark_create_sapling_spend());
        } else if (benchmarktype == "createsaplingoutput") {
            sample_times.push_back(benchmark_create_sapling_output());
        } else if (benchmarktype == "verifysaplingspend") {
            sample_times.push_back(benchmark_verify_sapling_spend());
        } else if (benchmarktype == "verifysaplingoutput") {
            sample_times.push_back(benchmark_verify_sapling_output());
        } else {
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid benchmarktype");
        }
    }

    UniValue results(UniValue::VARR);
    for (auto time : sample_times) {
        UniValue result(UniValue::VOBJ);
        result.push_back(Pair("runningtime", time));
        results.push_back(result);
    }

    return results;
}

UniValue zc_raw_receive(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp)) {
        return NullUniValue;
    }

    if (fHelp || params.size() != 2) {
        throw runtime_error(
            "zcrawreceive zcsecretkey encryptednote\n"
            "\n"
            "DEPRECATED. Decrypts encryptednote and checks if the coin commitments\n"
            "are in the blockchain as indicated by the \"exists\" result.\n"
            "\n"
            "Output: {\n"
            "  \"amount\": value,\n"
            "  \"note\": noteplaintext,\n"
            "  \"exists\": exists\n"
            "}\n"
            );
    }

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VSTR)(UniValue::VSTR));

    LOCK(cs_main);

    auto spendingkey = DecodeSpendingKey(params[0].get_str());
    if (!IsValidSpendingKey(spendingkey)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid spending key");
    }
    if (boost::get<libzcash::SproutSpendingKey>(&spendingkey) == nullptr) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Only works with Sprout spending keys");
    }
    SproutSpendingKey k = boost::get<libzcash::SproutSpendingKey>(spendingkey);

    uint256 epk;
    unsigned char nonce;
    ZCNoteEncryption::Ciphertext ct;
    uint256 h_sig;

    {
        CDataStream ssData(ParseHexV(params[1], "encrypted_note"), SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> nonce;
            ssData >> epk;
            ssData >> ct;
            ssData >> h_sig;
        } catch(const std::exception &) {
            throw runtime_error(
                "encrypted_note could not be decoded"
            );
        }
    }

    ZCNoteDecryption decryptor(k.receiving_key());

    SproutNotePlaintext npt = SproutNotePlaintext::decrypt(
        decryptor,
        ct,
        epk,
        h_sig,
        nonce
    );
    SproutPaymentAddress payment_addr = k.address();
    SproutNote decrypted_note = npt.note(payment_addr);

    assert(pwalletMain != NULL);
    std::vector<boost::optional<SproutWitness>> witnesses;
    uint256 anchor;
    uint256 commitment = decrypted_note.cm();
    pwalletMain->WitnessNoteCommitment(
        {commitment},
        witnesses,
        anchor
    );

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << npt;

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("amount", ValueFromAmount(decrypted_note.value())));
    result.push_back(Pair("note", HexStr(ss.begin(), ss.end())));
    result.push_back(Pair("exists", (bool) witnesses[0]));
    return result;
}



UniValue zc_raw_joinsplit(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp)) {
        return NullUniValue;
    }

    if (fHelp || params.size() != 5) {
        throw runtime_error(
            "zcrawjoinsplit rawtx inputs outputs vpub_old vpub_new\n"
            "  inputs: a JSON object mapping {note: zcsecretkey, ...}\n"
            "  outputs: a JSON object mapping {zcaddr: value, ...}\n"
            "\n"
            "DEPRECATED. Splices a joinsplit into rawtx. Inputs are unilaterally confidential.\n"
            "Outputs are confidential between sender/receiver. The vpub_old and\n"
            "vpub_new values are globally public and move transparent value into\n"
            "or out of the confidential value store, respectively.\n"
            "\n"
            "Note: The caller is responsible for delivering the output enc1 and\n"
            "enc2 to the appropriate recipients, as well as signing rawtxout and\n"
            "ensuring it is mined. (A future RPC call will deliver the confidential\n"
            "payments in-band on the blockchain.)\n"
            "\n"
            "Output: {\n"
            "  \"encryptednote1\": enc1,\n"
            "  \"encryptednote2\": enc2,\n"
            "  \"rawtxn\": rawtxout\n"
            "}\n"
            );
    }

    LOCK(cs_main);

    CTransaction tx;
    if (!DecodeHexTx(tx, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");

    UniValue inputs = params[1].get_obj();
    UniValue outputs = params[2].get_obj();

    CAmount vpub_old(0);
    CAmount vpub_new(0);

    if (params[3].get_real() != 0.0)
        vpub_old = AmountFromValue(params[3]);

    if (params[4].get_real() != 0.0)
        vpub_new = AmountFromValue(params[4]);

    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    std::vector<SproutNote> notes;
    std::vector<SproutSpendingKey> keys;
    std::vector<uint256> commitments;

    for (const string& name_ : inputs.getKeys()) {
        auto spendingkey = DecodeSpendingKey(inputs[name_].get_str());
        if (!IsValidSpendingKey(spendingkey)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid spending key");
        }
        if (boost::get<libzcash::SproutSpendingKey>(&spendingkey) == nullptr) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Only works with Sprout spending keys");
        }
        SproutSpendingKey k = boost::get<libzcash::SproutSpendingKey>(spendingkey);

        keys.push_back(k);

        SproutNotePlaintext npt;

        {
            CDataStream ssData(ParseHexV(name_, "note"), SER_NETWORK, PROTOCOL_VERSION);
            ssData >> npt;
        }

        SproutPaymentAddress addr = k.address();
        SproutNote note = npt.note(addr);
        notes.push_back(note);
        commitments.push_back(note.cm());
    }

    uint256 anchor;
    std::vector<boost::optional<SproutWitness>> witnesses;
    pwalletMain->WitnessNoteCommitment(commitments, witnesses, anchor);

    assert(witnesses.size() == notes.size());
    assert(notes.size() == keys.size());

    {
        for (size_t i = 0; i < witnesses.size(); i++) {
            if (!witnesses[i]) {
                throw runtime_error(
                    "joinsplit input could not be found in tree"
                );
            }

            vjsin.push_back(JSInput(*witnesses[i], notes[i], keys[i]));
        }
    }

    while (vjsin.size() < ZC_NUM_JS_INPUTS) {
        vjsin.push_back(JSInput());
    }

    for (const string& name_ : outputs.getKeys()) {
        auto addrTo = DecodePaymentAddress(name_);
        if (!IsValidPaymentAddress(addrTo)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid recipient address.");
        }
        if (boost::get<libzcash::SproutPaymentAddress>(&addrTo) == nullptr) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Only works with Sprout payment addresses");
        }
        CAmount nAmount = AmountFromValue(outputs[name_]);

        vjsout.push_back(JSOutput(boost::get<libzcash::SproutPaymentAddress>(addrTo), nAmount));
    }

    while (vjsout.size() < ZC_NUM_JS_OUTPUTS) {
        vjsout.push_back(JSOutput());
    }

    // TODO
    if (vjsout.size() != ZC_NUM_JS_INPUTS || vjsin.size() != ZC_NUM_JS_OUTPUTS) {
        throw runtime_error("unsupported joinsplit input/output counts");
    }

    uint256 joinSplitPubKey;
    unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(joinSplitPubKey.begin(), joinSplitPrivKey);

    CMutableTransaction mtx(tx);
    mtx.nVersion = 2;
    mtx.joinSplitPubKey = joinSplitPubKey;

    JSDescription jsdesc(false,
                         *pzcashParams,
                         joinSplitPubKey,
                         anchor,
                         {vjsin[0], vjsin[1]},
                         {vjsout[0], vjsout[1]},
                         vpub_old,
                         vpub_new);

    {
        auto verifier = libzcash::ProofVerifier::Strict();
        assert(jsdesc.Verify(*pzcashParams, verifier, joinSplitPubKey));
    }

    mtx.vjoinsplit.push_back(jsdesc);

    // Empty output script.
    CScript scriptCode;
    CTransaction signTx(mtx);
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);

    // Add the signature
    assert(crypto_sign_detached(&mtx.joinSplitSig[0], NULL,
                         dataToBeSigned.begin(), 32,
                         joinSplitPrivKey
                        ) == 0);

    // Sanity check
    assert(crypto_sign_verify_detached(&mtx.joinSplitSig[0],
                                       dataToBeSigned.begin(), 32,
                                       mtx.joinSplitPubKey.begin()
                                      ) == 0);

    CTransaction rawTx(mtx);

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;

    std::string encryptedNote1;
    std::string encryptedNote2;
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char) 0x00);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[0];
        ss2 << jsdesc.h_sig(*pzcashParams, joinSplitPubKey);

        encryptedNote1 = HexStr(ss2.begin(), ss2.end());
    }
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char) 0x01);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[1];
        ss2 << jsdesc.h_sig(*pzcashParams, joinSplitPubKey);

        encryptedNote2 = HexStr(ss2.begin(), ss2.end());
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("encryptednote1", encryptedNote1));
    result.push_back(Pair("encryptednote2", encryptedNote2));
    result.push_back(Pair("rawtxn", HexStr(ss.begin(), ss.end())));
    return result;
}

UniValue zc_raw_keygen(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp)) {
        return NullUniValue;
    }

    if (fHelp || params.size() != 0) {
        throw runtime_error(
            "zcrawkeygen\n"
            "\n"
            "DEPRECATED. Generate a zcaddr which can send and receive confidential values.\n"
            "\n"
            "Output: {\n"
            "  \"zcaddress\": zcaddr,\n"
            "  \"zcsecretkey\": zcsecretkey,\n"
            "  \"zcviewingkey\": zcviewingkey,\n"
            "}\n"
            );
    }

    auto k = SproutSpendingKey::random();
    auto addr = k.address();
    auto viewing_key = k.viewing_key();

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("zcaddress", EncodePaymentAddress(addr)));
    result.push_back(Pair("zcsecretkey", EncodeSpendingKey(k)));
    result.push_back(Pair("zcviewingkey", EncodeViewingKey(viewing_key)));
    return result;
}


UniValue z_getnewaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    bool allowSapling = (Params().GetConsensus().vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight <= chainActive.LastTip()->GetHeight());

    std::string defaultType;
    if ( GetTime() < KOMODO_SAPLING_ACTIVATION )
        defaultType = ADDR_TYPE_SPROUT;
    else defaultType = ADDR_TYPE_SAPLING;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getnewaddress ( type )\n"
            "\nReturns a new shielded address for receiving payments.\n"
            "\nWith no arguments, returns a Sprout address.\n"
            "\nArguments:\n"
            "1. \"type\"         (string, optional, default=\"" + defaultType + "\") The type of address. One of [\""
            + ADDR_TYPE_SPROUT + "\", \"" + ADDR_TYPE_SAPLING + "\"].\n"
            "\nResult:\n"
            "\"" + strprintf("%s",komodo_chainname()) + "_address\"    (string) The new shielded address.\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getnewaddress", "")
            + HelpExampleCli("z_getnewaddress", ADDR_TYPE_SAPLING)
            + HelpExampleRpc("z_getnewaddress", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    auto addrType = defaultType;
    if (params.size() > 0) {
        addrType = params[0].get_str();
    }

    if (addrType == ADDR_TYPE_SPROUT) {
        if ( GetTime() >= KOMODO_SAPLING_DEADLINE )
            throw JSONRPCError(RPC_INVALID_PARAMETER, "sprout not valid anymore");
        return EncodePaymentAddress(pwalletMain->GenerateNewSproutZKey());
    } else if (addrType == ADDR_TYPE_SAPLING) {
        return EncodePaymentAddress(pwalletMain->GenerateNewSaplingZKey());
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid address type");
    }
}


UniValue z_listaddresses(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_listaddresses ( includeWatchonly )\n"
            "\nReturns the list of Sprout and Sapling shielded addresses belonging to the wallet.\n"
            "\nArguments:\n"
            "1. includeWatchonly (bool, optional, default=false) Also include watchonly addresses (see 'z_importviewingkey')\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"zaddr\"           (string) a zaddr belonging to the wallet\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listaddresses", "")
            + HelpExampleRpc("z_listaddresses", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    bool fIncludeWatchonly = false;
    if (params.size() > 0) {
        fIncludeWatchonly = params[0].get_bool();
    }

    UniValue ret(UniValue::VARR);
    {
        std::set<libzcash::SproutPaymentAddress> addresses;
        pwalletMain->GetSproutPaymentAddresses(addresses);
        for (auto addr : addresses) {
            if (fIncludeWatchonly || pwalletMain->HaveSproutSpendingKey(addr)) {
                ret.push_back(EncodePaymentAddress(addr));
            }
        }
    }
    {
        std::set<libzcash::SaplingPaymentAddress> addresses;
        pwalletMain->GetSaplingPaymentAddresses(addresses);
        libzcash::SaplingIncomingViewingKey ivk;
        libzcash::SaplingFullViewingKey fvk;
        for (auto addr : addresses) {
            if (fIncludeWatchonly || (
                pwalletMain->GetSaplingIncomingViewingKey(addr, ivk) &&
                pwalletMain->GetSaplingFullViewingKey(ivk, fvk) &&
                pwalletMain->HaveSaplingSpendingKey(fvk)
            )) {
                ret.push_back(EncodePaymentAddress(addr));
            }
        }
    }
    return ret;
}

CAmount getBalanceTaddr(std::string transparentAddress, int minDepth=1, bool ignoreUnspendable=true) {
    std::set<CTxDestination> destinations;
    vector<COutput> vecOutputs;
    CAmount balance = 0;

    if (transparentAddress.length() > 0) {
        CTxDestination taddr = DecodeDestination(transparentAddress);
        if (!IsValidDestination(taddr)) {
            throw std::runtime_error("invalid transparent address");
        }
        destinations.insert(taddr);
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);

    BOOST_FOREACH(const COutput& out, vecOutputs) {
        int nDepth    = out.tx->GetDepthInMainChain();
        if( minDepth > 1 ) {
            int nHeight    = tx_height(out.tx->GetHash());
            int dpowconfs  = komodo_dpowconfs(nHeight, nDepth);
            if (dpowconfs < minDepth) {
                continue;
            }
        } else {
            if (out.nDepth < minDepth) {
                continue;
            }
        }

        if (ignoreUnspendable && !out.fSpendable) {
            continue;
        }

        if (destinations.size()) {
            CTxDestination address;
            if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
                continue;
            }

            if (!destinations.count(address)) {
                continue;
            }
        }

        CAmount nValue = out.tx->vout[out.i].nValue; // komodo_interest
        balance += nValue;
    }
    return balance;
}

CAmount getBalanceZaddr(std::string address, int minDepth = 1, bool ignoreUnspendable=true) {
    CAmount balance = 0;
    std::vector<CSproutNotePlaintextEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, address, minDepth, true, ignoreUnspendable);
    for (auto & entry : sproutEntries) {
        balance += CAmount(entry.plaintext.value());
    }
    for (auto & entry : saplingEntries) {
        balance += CAmount(entry.note.value());
    }
    return balance;
}


UniValue z_listreceivedbyaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size()==0 || params.size() >2)
        throw runtime_error(
            "z_listreceivedbyaddress \"address\" ( minconf )\n"
            "\nReturn a list of amounts received by a zaddr belonging to the node’s wallet.\n"
            "\nArguments:\n"
            "1. \"address\"      (string) The private address.\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\": xxxxx,           (string) the transaction id\n"
            "  \"amount\": xxxxx,         (numeric) the amount of value in the note\n"
            "  \"memo\": xxxxx,           (string) hexadecimal string representation of memo field\n"
            "  \"confirmations\" : n,     (numeric) the number of confirmations\n"
            "  \"jsindex\" (sprout) : n,     (numeric) the joinsplit index\n"
            "  \"jsoutindex\" (sprout) : n,     (numeric) the output index of the joinsplit\n"
            "  \"outindex\" (sapling) : n,     (numeric) the output index\n"
            "  \"change\": true|false,    (boolean) true if the address that received the note is also one of the sending addresses\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listreceivedbyaddress", "\"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\"")
            + HelpExampleRpc("z_listreceivedbyaddress", "\"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 1) {
        nMinDepth = params[1].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    // Check that the from address is valid.
    auto fromaddress = params[0].get_str();

    auto zaddr = DecodePaymentAddress(fromaddress);
    if (!IsValidPaymentAddress(zaddr)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid zaddr.");
    }

    // Visitor to support Sprout and Sapling addrs
    if (!boost::apply_visitor(PaymentAddressBelongsToWallet(pwalletMain), zaddr) &&
        !boost::apply_visitor(IncomingViewingKeyBelongsToWallet(pwalletMain), zaddr)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "From address does not belong to this node, zaddr spending key or viewing key not found.");
    }

    UniValue result(UniValue::VARR);
    std::vector<CSproutNotePlaintextEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, fromaddress, nMinDepth, false, false);

    std::set<std::pair<PaymentAddress, uint256>> nullifierSet;
    auto hasSpendingKey = boost::apply_visitor(HaveSpendingKeyForPaymentAddress(pwalletMain), zaddr);
    if (hasSpendingKey) {
        nullifierSet = pwalletMain->GetNullifiersForAddresses({zaddr});
    }

    if (boost::get<libzcash::SproutPaymentAddress>(&zaddr) != nullptr) {
        for (CSproutNotePlaintextEntry & entry : sproutEntries) {
            UniValue obj(UniValue::VOBJ);
            int nHeight   = tx_height(entry.jsop.hash);
            int dpowconfs = komodo_dpowconfs(nHeight, entry.confirmations);
            // Only return notarized results when minconf>1
            if (nMinDepth > 1 && dpowconfs == 1)
                continue;

            obj.push_back(Pair("txid", entry.jsop.hash.ToString()));
            obj.push_back(Pair("amount", ValueFromAmount(CAmount(entry.plaintext.value()))));
            std::string data(entry.plaintext.memo().begin(), entry.plaintext.memo().end());
            obj.push_back(Pair("memo", HexStr(data)));
            obj.push_back(Pair("jsindex", entry.jsop.js));
            obj.push_back(Pair("jsoutindex", entry.jsop.n));
            obj.push_back(Pair("rawconfirmations", entry.confirmations));
            obj.push_back(Pair("confirmations", dpowconfs));
            if (hasSpendingKey) {
                obj.push_back(Pair("change", pwalletMain->IsNoteSproutChange(nullifierSet, entry.address, entry.jsop)));
            }
            result.push_back(obj);
        }
    } else if (boost::get<libzcash::SaplingPaymentAddress>(&zaddr) != nullptr) {
        for (SaplingNoteEntry & entry : saplingEntries) {
            UniValue obj(UniValue::VOBJ);

            int nHeight   = tx_height(entry.op.hash);
            int dpowconfs = komodo_dpowconfs(nHeight, entry.confirmations);
            // Only return notarized results when minconf>1
            if (nMinDepth > 1 && dpowconfs == 1)
                continue;

            obj.push_back(Pair("txid", entry.op.hash.ToString()));
            obj.push_back(Pair("amount", ValueFromAmount(CAmount(entry.note.value()))));
            obj.push_back(Pair("memo", HexStr(entry.memo)));
            obj.push_back(Pair("outindex", (int)entry.op.n));
            obj.push_back(Pair("rawconfirmations", entry.confirmations));
            obj.push_back(Pair("confirmations", dpowconfs));
            if (hasSpendingKey) {
              obj.push_back(Pair("change", pwalletMain->IsNoteSaplingChange(nullifierSet, entry.address, entry.op)));
            }
            result.push_back(obj);
        }
    }
    return result;
}

UniValue z_getbalance(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size()==0 || params.size() >2)
        throw runtime_error(
            "z_getbalance \"address\" ( minconf )\n"
            "\nReturns the balance of a taddr or zaddr belonging to the node’s wallet.\n"
            "\nCAUTION: If the wallet has only an incoming viewing key for this address, then spends cannot be"
            "\ndetected, and so the returned balance may be larger than the actual balance.\n"
            "\nArguments:\n"
            "1. \"address\"      (string) The selected address. It may be a transparent or private address.\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in KMD received for this address.\n"
            "\nExamples:\n"
            "\nThe total amount received by address \"myaddress\"\n"
            + HelpExampleCli("z_getbalance", "\"myaddress\"") +
            "\nThe total amount received by address \"myaddress\" at least 5 blocks confirmed\n"
            + HelpExampleCli("z_getbalance", "\"myaddress\" 5") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("z_getbalance", "\"myaddress\", 5")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 1) {
        nMinDepth = params[1].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    // Check that the from address is valid.
    auto fromaddress = params[0].get_str();
    bool fromTaddr = false;
    CTxDestination taddr = DecodeDestination(fromaddress);
    fromTaddr = IsValidDestination(taddr);
    if (!fromTaddr) {
        auto res = DecodePaymentAddress(fromaddress);
        if (!IsValidPaymentAddress(res)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or zaddr.");
        }
        if (!boost::apply_visitor(PaymentAddressBelongsToWallet(pwalletMain), res)) {
             throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "From address does not belong to this node, spending key or viewing key not found.");
        }
    }

    CAmount nBalance = 0;
    if (fromTaddr) {
        nBalance = getBalanceTaddr(fromaddress, nMinDepth, false);
    } else {
        nBalance = getBalanceZaddr(fromaddress, nMinDepth, false);
    }

    return ValueFromAmount(nBalance);
}


UniValue z_gettotalbalance(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 2)
        throw runtime_error(
            "z_gettotalbalance ( minconf includeWatchonly )\n"
            "\nReturn the total value of funds stored in the node’s wallet.\n"
            "\nCAUTION: If the wallet contains any addresses for which it only has incoming viewing keys,"
            "\nthe returned private balance may be larger than the actual balance, because spends cannot"
            "\nbe detected with incoming viewing keys.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) Only include private and transparent transactions confirmed at least this many times.\n"
            "2. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress' and 'z_importviewingkey')\n"
            "\nResult:\n"
            "{\n"
            "  \"transparent\": xxxxx,     (numeric) the total balance of transparent funds\n"
            "  \"private\": xxxxx,         (numeric) the total balance of private funds (in both Sprout and Sapling addresses)\n"
            "  \"total\": xxxxx,           (numeric) the total balance of both transparent and private funds\n"
            "}\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("z_gettotalbalance", "") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("z_gettotalbalance", "5") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("z_gettotalbalance", "5")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 0) {
        nMinDepth = params[0].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    bool fIncludeWatchonly = false;
    if (params.size() > 1) {
        fIncludeWatchonly = params[1].get_bool();
    }

    // getbalance and "getbalance * 1 true" should return the same number
    // but they don't because wtx.GetAmounts() does not handle tx where there are no outputs
    // pwalletMain->GetBalance() does not accept min depth parameter
    // so we use our own method to get balance of utxos.
    CAmount nBalance = getBalanceTaddr("", nMinDepth, !fIncludeWatchonly);
    CAmount nPrivateBalance = getBalanceZaddr("", nMinDepth, !fIncludeWatchonly);
    uint64_t interest = komodo_interestsum();
    CAmount nTotalBalance = nBalance + nPrivateBalance;
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("transparent", FormatMoney(nBalance)));
    result.push_back(Pair("interest", FormatMoney(interest)));
    result.push_back(Pair("private", FormatMoney(nPrivateBalance)));
    result.push_back(Pair("total", FormatMoney(nTotalBalance)));
    return result;
}

// got bored and copied str4d's work - thanks dude/dudette :)
UniValue z_viewtransaction(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_viewtransaction \"txid\"\n"
            "\nGet detailed shielded information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\" (string, required) The transaction id\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id\n"
            "  \"spends\" : [\n"
            "    {\n"
            "      \"type\" : \"sprout|sapling\",      (string) The type of address\n"
            "      \"js\" : n,                       (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsSpend\" : n,                  (numeric, sprout) the index of the spend within the JSDescription\n"
            "      \"spend\" : n,                    (numeric, sapling) the index of the spend within vShieldedSpend\n"
            "      \"txidPrev\" : \"transactionid\",   (string) The id for the transaction this note was created in\n"
            "      \"jsPrev\" : n,                   (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsOutputPrev\" : n,             (numeric, sprout) the index of the output within the JSDescription\n"
            "      \"outputPrev\" : n,               (numeric, sapling) the index of the output within the vShieldedOutput\n"
            "      \"address\" : \"zcashaddress\",     (string) The Zcash address involved in the transaction\n"
            "      \"value\" : x.xxx                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"valueZat\" : xxxx               (numeric) The amount in zatoshis\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"outputs\" : [\n"
            "    {\n"
            "      \"type\" : \"sprout|sapling\",      (string) The type of address\n"
            "      \"js\" : n,                       (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsOutput\" : n,                 (numeric, sprout) the index of the output within the JSDescription\n"
            "      \"output\" : n,                   (numeric, sapling) the index of the output within the vShieldedOutput\n"
            "      \"address\" : \"zcashaddress\",     (string) The Zcash address involved in the transaction\n"
            "      \"recovered\" : true|false        (boolean, sapling) True if the output is not for an address in the wallet\n"
            "      \"value\" : x.xxx                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"valueZat\" : xxxx               (numeric) The amount in zatoshis\n"
            "      \"memo\" : \"hexmemo\",             (string) Hexademical string representation of the memo field\n"
            "      \"memoStr\" : \"memo\",             (string) Only returned if memo contains valid UTF-8 text.\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("z_viewtransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("z_viewtransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("z_viewtransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());

    UniValue entry(UniValue::VOBJ);
    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    entry.push_back(Pair("txid", hash.GetHex()));

    UniValue spends(UniValue::VARR);
    UniValue outputs(UniValue::VARR);

    // Sprout spends - retained for old sprout addresses and transactions
    for (size_t i = 0; i < wtx.vjoinsplit.size(); ++i) {
        for (size_t j = 0; j < wtx.vjoinsplit[i].nullifiers.size(); ++j) {
            auto nullifier = wtx.vjoinsplit[i].nullifiers[j];

            // Fetch the note that is being spent, if ours
            auto res = pwalletMain->mapSproutNullifiersToNotes.find(nullifier);
            if (res == pwalletMain->mapSproutNullifiersToNotes.end()) {
                continue;
            }
            auto jsop = res->second;
            auto wtxPrev = pwalletMain->mapWallet.at(jsop.hash);

            auto decrypted = wtxPrev.DecryptSproutNote(jsop);
            auto notePt = decrypted.first;
            auto pa = decrypted.second;

            UniValue entry(UniValue::VOBJ);
            entry.push_back(Pair("type", ADDR_TYPE_SPROUT));
            entry.push_back(Pair("js", (int)i));
            entry.push_back(Pair("jsSpend", (int)j));
            entry.push_back(Pair("txidPrev", jsop.hash.GetHex()));
            entry.push_back(Pair("jsPrev", (int)jsop.js));
            entry.push_back(Pair("jsOutputPrev", (int)jsop.n));
            entry.push_back(Pair("address", EncodePaymentAddress(pa)));
            entry.push_back(Pair("value", ValueFromAmount(notePt.value())));
            entry.push_back(Pair("valueZat", notePt.value()));
            outputs.push_back(entry);
        }
    }

    // Sprout outputs
    for (auto & pair : wtx.mapSproutNoteData) {
        JSOutPoint jsop = pair.first;

        auto decrypted = wtx.DecryptSproutNote(jsop);
        auto notePt = decrypted.first;
        auto pa = decrypted.second;
        auto memo = notePt.memo();

        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("type", ADDR_TYPE_SPROUT));
        entry.push_back(Pair("js", (int)jsop.js));
        entry.push_back(Pair("jsOutput", (int)jsop.n));
        entry.push_back(Pair("address", EncodePaymentAddress(pa)));
        entry.push_back(Pair("value", ValueFromAmount(notePt.value())));
        entry.push_back(Pair("valueZat", notePt.value()));
        entry.push_back(Pair("memo", HexStr(memo)));
        if (memo[0] <= 0xf4) {
            auto end = std::find_if(memo.rbegin(), memo.rend(), [](unsigned char v) { return v != 0; });
            std::string memoStr(memo.begin(), end.base());
            //if (utf8::is_valid(memoStr))
            {
                entry.push_back(Pair("memoStr", memoStr));
            }
        }
        outputs.push_back(entry);
    }

    // Sapling spends
    std::set<uint256> ovks;
    for (size_t i = 0; i < wtx.vShieldedSpend.size(); ++i) {
        auto spend = wtx.vShieldedSpend[i];

        // Fetch teh note that is being spent
        auto res = pwalletMain->mapSaplingNullifiersToNotes.find(spend.nullifier);
        if (res == pwalletMain->mapSaplingNullifiersToNotes.end()) {
            continue;
        }
        auto op = res->second;
        auto wtxPrev = pwalletMain->mapWallet.at(op.hash);

        auto decrypted = wtxPrev.DecryptSaplingNote(op).get();
        auto notePt = decrypted.first;
        auto pa = decrypted.second;

        // Store the OutgoingViewingKey for recovering outputs
        libzcash::SaplingFullViewingKey fvk;
        assert(pwalletMain->GetSaplingFullViewingKey(wtxPrev.mapSaplingNoteData.at(op).ivk, fvk));
        ovks.insert(fvk.ovk);

        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("type", ADDR_TYPE_SAPLING));
        entry.push_back(Pair("spend", (int)i));
        entry.push_back(Pair("txidPrev", op.hash.GetHex()));
        entry.push_back(Pair("outputPrev", (int)op.n));
        entry.push_back(Pair("address", EncodePaymentAddress(pa)));
        entry.push_back(Pair("value", ValueFromAmount(notePt.value())));
        entry.push_back(Pair("valueZat", notePt.value()));
        spends.push_back(entry);
    }

    // Sapling outputs
    for (uint32_t i = 0; i < wtx.vShieldedOutput.size(); ++i) {
        auto op = SaplingOutPoint(hash, i);

        SaplingNotePlaintext notePt;
        SaplingPaymentAddress pa;
        bool isRecovered;

        auto decrypted = wtx.DecryptSaplingNote(op);
        if (decrypted) {
            notePt = decrypted->first;
            pa = decrypted->second;
            isRecovered = false;
        } else {
            // Try recovering the output
            auto recovered = wtx.RecoverSaplingNote(op, ovks);
            if (recovered) {
                notePt = recovered->first;
                pa = recovered->second;
                isRecovered = true;
            } else {
                // Unreadable
                continue;
            }
        }
        auto memo = notePt.memo();

        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("type", ADDR_TYPE_SAPLING));
        entry.push_back(Pair("output", (int)op.n));
        entry.push_back(Pair("recovered", isRecovered));
        entry.push_back(Pair("address", EncodePaymentAddress(pa)));
        entry.push_back(Pair("value", ValueFromAmount(notePt.value())));
        entry.push_back(Pair("valueZat", notePt.value()));
        entry.push_back(Pair("memo", HexStr(memo)));
        if (memo[0] <= 0xf4) {
            auto end = std::find_if(memo.rbegin(), memo.rend(), [](unsigned char v) { return v != 0; });
            std::string memoStr(memo.begin(), end.base());
            //if (utf8::is_valid(memoStr))
            {
                entry.push_back(Pair("memoStr", memoStr));
            }
        }
        outputs.push_back(entry);
    }

    entry.push_back(Pair("spends", spends));
    entry.push_back(Pair("outputs", outputs));

    return entry;
}


UniValue z_getoperationresult(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getoperationresult ([\"operationid\", ... ]) \n"
            "\nRetrieve the result and status of an operation which has finished, and then remove the operation from memory."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"operationid\"         (array, optional) A list of operation ids we are interested in.  If not provided, examine all operations known to the node.\n"
            "\nResult:\n"
            "\"    [object, ...]\"      (array) A list of JSON objects\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getoperationresult", "'[\"operationid\", ... ]'")
            + HelpExampleRpc("z_getoperationresult", "'[\"operationid\", ... ]'")
        );

    // This call will remove finished operations
    return z_getoperationstatus_IMPL(params, true);
}

UniValue z_getoperationstatus(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
   if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getoperationstatus ([\"operationid\", ... ]) \n"
            "\nGet operation status and any associated result or error data.  The operation will remain in memory."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"operationid\"         (array, optional) A list of operation ids we are interested in.  If not provided, examine all operations known to the node.\n"
            "\nResult:\n"
            "\"    [object, ...]\"      (array) A list of JSON objects\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getoperationstatus", "'[\"operationid\", ... ]'")
            + HelpExampleRpc("z_getoperationstatus", "'[\"operationid\", ... ]'")
        );

   // This call is idempotent so we don't want to remove finished operations
   return z_getoperationstatus_IMPL(params, false);
}

UniValue z_getoperationstatus_IMPL(const UniValue& params, bool fRemoveFinishedOperations=false)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::set<AsyncRPCOperationId> filter;
    if (params.size()==1) {
        UniValue ids = params[0].get_array();
        for (const UniValue & v : ids.getValues()) {
            filter.insert(v.get_str());
        }
    }
    bool useFilter = (filter.size()>0);

    UniValue ret(UniValue::VARR);
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::vector<AsyncRPCOperationId> ids = q->getAllOperationIds();

    for (auto id : ids) {
        if (useFilter && !filter.count(id))
            continue;

        std::shared_ptr<AsyncRPCOperation> operation = q->getOperationForId(id);
        if (!operation) {
            continue;
            // It's possible that the operation was removed from the internal queue and map during this loop
            // throw JSONRPCError(RPC_INVALID_PARAMETER, "No operation exists for that id.");
        }

        UniValue obj = operation->getStatus();
        std::string s = obj["status"].get_str();
        if (fRemoveFinishedOperations) {
            // Caller is only interested in retrieving finished results
            if ("success"==s || "failed"==s || "cancelled"==s) {
                ret.push_back(obj);
                q->popOperationForId(id);
            }
        } else {
            ret.push_back(obj);
        }
    }

    std::vector<UniValue> arrTmp = ret.getValues();

    // sort results chronologically by creation_time
    std::sort(arrTmp.begin(), arrTmp.end(), [](UniValue a, UniValue b) -> bool {
        const int64_t t1 = find_value(a.get_obj(), "creation_time").get_int64();
        const int64_t t2 = find_value(b.get_obj(), "creation_time").get_int64();
        return t1 < t2;
    });

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}


// JSDescription size depends on the transaction version
#define V3_JS_DESCRIPTION_SIZE    (GetSerializeSize(JSDescription(), SER_NETWORK, (OVERWINTER_TX_VERSION | (1 << 31))))
// Here we define the maximum number of zaddr outputs that can be included in a transaction.
// If input notes are small, we might actually require more than one joinsplit per zaddr output.
// For now though, we assume we use one joinsplit per zaddr output (and the second output note is change).
// We reduce the result by 1 to ensure there is room for non-joinsplit CTransaction data.
#define Z_SENDMANY_MAX_ZADDR_OUTPUTS_BEFORE_SAPLING    ((MAX_TX_SIZE_BEFORE_SAPLING / V3_JS_DESCRIPTION_SIZE) - 1)

// transaction.h comment: spending taddr output requires CTxIn >= 148 bytes and typical taddr txout is 34 bytes
#define CTXIN_SPEND_DUST_SIZE   148
#define CTXOUT_REGULAR_SIZE     34

UniValue z_sendmany(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "z_sendmany \"fromaddress\" [{\"address\":... ,\"amount\":...},...] ( minconf ) ( fee )\n"
            "\nSend multiple times. Amounts are decimal numbers with at most 8 digits of precision."
            "\nChange generated from a taddr flows to a new taddr address, while change generated from a zaddr returns to itself."
            "\nWhen sending coinbase UTXOs to a zaddr, change is not allowed. The entire value of the UTXO(s) must be consumed."
            + strprintf("\nBefore Sapling activates, the maximum number of zaddr outputs is %d due to transaction size limits.\n", Z_SENDMANY_MAX_ZADDR_OUTPUTS_BEFORE_SAPLING)
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaddress\"         (string, required) The taddr or zaddr to send the funds from.\n"
            "2. \"amounts\"             (array, required) An array of json objects representing the amounts to send.\n"
            "    [{\n"
            "      \"address\":address  (string, required) The address is a taddr or zaddr\n"
            "      \"amount\":amount    (numeric, required) The numeric amount in KMD is the value\n"
            "      \"memo\":memo        (string, optional) If the address is a zaddr, raw data represented in hexadecimal string format\n"
            "    }, ... ]\n"
            "3. minconf               (numeric, optional, default=1) Only use funds confirmed at least this many times.\n"
            "4. fee                   (numeric, optional, default="
            + strprintf("%s", FormatMoney(ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE)) + ") The fee amount to attach to this transaction.\n"
            "\nResult:\n"
            "\"operationid\"          (string) An operationid to pass to z_getoperationstatus to get the result of the operation.\n"
            "\nExamples:\n"
            + HelpExampleCli("z_sendmany", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" '[{\"address\": \"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\" ,\"amount\": 5.0}]'")
            + HelpExampleRpc("z_sendmany", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\", [{\"address\": \"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\" ,\"amount\": 5.0}]")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    THROW_IF_SYNCING(KOMODO_INSYNC);

    // Check that the from address is valid.
    auto fromaddress = params[0].get_str();
    bool fromTaddr = false;
    bool fromSapling = false;

    uint32_t branchId = CurrentEpochBranchId(chainActive.Height(), Params().GetConsensus());

    CTxDestination taddr = DecodeDestination(fromaddress);
    fromTaddr = IsValidDestination(taddr);
    if (!fromTaddr) {
        auto res = DecodePaymentAddress(fromaddress);
        if (!IsValidPaymentAddress(res, branchId)) {
            // invalid
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or zaddr.");
        }

        // Check that we have the spending key
        if (!boost::apply_visitor(HaveSpendingKeyForPaymentAddress(pwalletMain), res)) {
             throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "From address does not belong to this node, zaddr spending key not found.");
        }

        // Remember whether this is a Sprout or Sapling address
        fromSapling = boost::get<libzcash::SaplingPaymentAddress>(&res) != nullptr;
    }
    // This logic will need to be updated if we add a new shielded pool
    bool fromSprout = !(fromTaddr || fromSapling);

    UniValue outputs = params[1].get_array();

    if (outputs.size()==0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, amounts array is empty.");

    // Keep track of addresses to spot duplicates
    set<std::string> setAddress;

    // Track whether we see any Sprout addresses
    bool noSproutAddrs = !fromSprout;

    // Recipients
    std::vector<SendManyRecipient> taddrRecipients;
    std::vector<SendManyRecipient> zaddrRecipients;
    CAmount nTotalOut = 0;

    bool containsSproutOutput = false;
    bool containsSaplingOutput = false;

    for (const UniValue& o : outputs.getValues()) {
        if (!o.isObject())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");

        // sanity check, report error if unknown key-value pairs
        for (const string& name_ : o.getKeys()) {
            std::string s = name_;
            if (s != "address" && s != "amount" && s!="memo")
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown key: ")+s);
        }

        string address = find_value(o, "address").get_str();
        bool isZaddr = false;
        CTxDestination taddr = DecodeDestination(address);
        if (!IsValidDestination(taddr)) {
            auto res = DecodePaymentAddress(address);
            if (IsValidPaymentAddress(res, branchId)) {
                isZaddr = true;

                bool toSapling = boost::get<libzcash::SaplingPaymentAddress>(&res) != nullptr;
                bool toSprout = !toSapling;
                noSproutAddrs = noSproutAddrs && toSapling;

                containsSproutOutput |= toSprout;
                containsSaplingOutput |= toSapling;

                // Sending to both Sprout and Sapling is currently unsupported using z_sendmany
                if (containsSproutOutput && containsSaplingOutput) {
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Cannot send to both Sprout and Sapling addresses using z_sendmany");
                }
                if ( GetTime() > KOMODO_SAPLING_DEADLINE )
                {
                    if ( fromSprout || toSprout )
                        throw JSONRPCError(RPC_INVALID_PARAMETER,"Sprout usage has expired");
                }
                if ( toSapling && ASSETCHAINS_SYMBOL[0] == 0 )
                    throw JSONRPCError(RPC_INVALID_PARAMETER,"Sprout usage will expire soon");

                // If we are sending from a shielded address, all recipient
                // shielded addresses must be of the same type.
                if ((fromSprout && toSapling) || (fromSapling && toSprout)) {
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                                       "Cannot send between Sprout and Sapling addresses using z_sendmany");
                }
            } else {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown address format: ")+address );
            }
        }
        //else if ( ASSETCHAINS_PRIVATE != 0 && komodo_isnotaryvout((char *)address.c_str()) == 0 )
        //    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "cant use transparent addresses in private chain");

        // Allowing duplicate receivers helps various HushList protocol operations
        //if (setAddress.count(address))
        //    throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+address);
        setAddress.insert(address);

        UniValue memoValue = find_value(o, "memo");
        string memo;
        if (!memoValue.isNull()) {
            memo = memoValue.get_str();
            if (!isZaddr) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Memo cannot be used with a taddr.  It can only be used with a zaddr.");
            } else if (!IsHex(memo)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected memo data in hexadecimal format.");
            }
            if (memo.length() > ZC_MEMO_SIZE*2) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,  strprintf("Invalid parameter, size of memo is larger than maximum allowed %d", ZC_MEMO_SIZE ));
            }
        }

        UniValue av = find_value(o, "amount");
        CAmount nAmount = AmountFromValue( av );
        if (nAmount < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, amount must be positive");

        if (isZaddr) {
            zaddrRecipients.push_back( SendManyRecipient(address, nAmount, memo) );
        } else {
            taddrRecipients.push_back( SendManyRecipient(address, nAmount, memo) );
        }

        nTotalOut += nAmount;
    }

    int nextBlockHeight = chainActive.Height() + 1;
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nVersion = SAPLING_TX_VERSION;
    unsigned int max_tx_size = MAX_TX_SIZE_AFTER_SAPLING;
    if (!NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_SAPLING)) {
        if (NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER)) {
            mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
            mtx.nVersion = OVERWINTER_TX_VERSION;
        } else {
            mtx.fOverwintered = false;
            mtx.nVersion = 2;
        }

        max_tx_size = MAX_TX_SIZE_BEFORE_SAPLING;

        // Check the number of zaddr outputs does not exceed the limit.
        if (zaddrRecipients.size() > Z_SENDMANY_MAX_ZADDR_OUTPUTS_BEFORE_SAPLING)  {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, too many zaddr outputs");
        }
    }

    // If Sapling is not active, do not allow sending from or sending to Sapling addresses.
    if (!NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_SAPLING)) {
        if (fromSapling || containsSaplingOutput) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
        }
    }

    // As a sanity check, estimate and verify that the size of the transaction will be valid.
    // Depending on the input notes, the actual tx size may turn out to be larger and perhaps invalid.
    size_t txsize = 0;
    for (int i = 0; i < zaddrRecipients.size(); i++) {
        auto address = std::get<0>(zaddrRecipients[i]);
        auto res = DecodePaymentAddress(address);
        bool toSapling = boost::get<libzcash::SaplingPaymentAddress>(&res) != nullptr;
        if (toSapling) {
            mtx.vShieldedOutput.push_back(OutputDescription());
        } else {
            JSDescription jsdesc;
            if (mtx.fOverwintered && (mtx.nVersion >= SAPLING_TX_VERSION)) {
                jsdesc.proof = GrothProof();
            }
            mtx.vjoinsplit.push_back(jsdesc);
        }
    }
    CTransaction tx(mtx);
    txsize += GetSerializeSize(tx, SER_NETWORK, tx.nVersion);
    if (fromTaddr) {
        txsize += CTXIN_SPEND_DUST_SIZE;
        txsize += CTXOUT_REGULAR_SIZE;      // There will probably be taddr change
    }
    txsize += CTXOUT_REGULAR_SIZE * taddrRecipients.size();
    if (txsize > max_tx_size) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Too many outputs, size of raw transaction would be larger than limit of %d bytes", max_tx_size ));
    }

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 2) {
        nMinDepth = params[2].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    // Fee in Zatoshis, not currency format)
    CAmount nFee        = ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE;
    CAmount nDefaultFee = nFee;

    if (params.size() > 3) {
        if (params[3].get_real() == 0.0) {
            nFee = 0;
        } else {
            nFee = AmountFromValue( params[3] );
        }

        // Check that the user specified fee is not absurd.
        // This allows amount=0 (and all amount < nDefaultFee) transactions to use the default network fee
        // or anything less than nDefaultFee instead of being forced to use a custom fee and leak metadata
        if (nTotalOut < nDefaultFee) {
            if (nFee > nDefaultFee) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Small transaction amount %s has fee %s that is greater than the default fee %s", FormatMoney(nTotalOut), FormatMoney(nFee), FormatMoney(nDefaultFee)));
            }
        } else {
            // Check that the user specified fee is not absurd.
            if (nFee > nTotalOut) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Fee %s is greater than the sum of outputs %s and also greater than the default fee", FormatMoney(nFee), FormatMoney(nTotalOut)));
            }
	}
    }

    // Use input parameters as the optional context info to be returned by z_getoperationstatus and z_getoperationresult.
    UniValue o(UniValue::VOBJ);
    o.push_back(Pair("fromaddress", params[0]));
    o.push_back(Pair("amounts", params[1]));
    o.push_back(Pair("minconf", nMinDepth));
    o.push_back(Pair("fee", std::stod(FormatMoney(nFee))));
    UniValue contextInfo = o;

    // Builder (used if Sapling addresses are involved)
    boost::optional<TransactionBuilder> builder;
    if (noSproutAddrs) {
        builder = TransactionBuilder(Params().GetConsensus(), nextBlockHeight, pwalletMain);
    }

    // Contextual transaction we will build on
    // (used if no Sapling addresses are involved)
    CMutableTransaction contextualTx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), nextBlockHeight);
    bool isShielded = !fromTaddr || zaddrRecipients.size() > 0;
    if (contextualTx.nVersion == 1 && isShielded) {
        contextualTx.nVersion = 2; // Tx format should support vjoinsplits
    }

    // Create operation and add to global queue
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_sendmany(builder, contextualTx, fromaddress, taddrRecipients, zaddrRecipients, nMinDepth, nFee, contextInfo) );
    q->addOperation(operation);
    AsyncRPCOperationId operationId = operation->getId();
    return operationId;
}


/**
When estimating the number of coinbase utxos we can shield in a single transaction:
1. Joinsplit description is 1802 bytes.
2. Transaction overhead ~ 100 bytes
3. Spending a typical P2PKH is >=148 bytes, as defined in CTXIN_SPEND_DUST_SIZE.
4. Spending a multi-sig P2SH address can vary greatly:
   https://github.com/bitcoin/bitcoin/blob/c3ad56f4e0b587d8d763af03d743fdfc2d180c9b/src/main.cpp#L517
   In real-world coinbase utxos, we consider a 3-of-3 multisig, where the size is roughly:
    (3*(33+1))+3 = 105 byte redeem script
    105 + 1 + 3*(73+1) = 328 bytes of scriptSig, rounded up to 400 based on testnet experiments.
*/
#define CTXIN_SPEND_P2SH_SIZE 400

#define SHIELD_COINBASE_DEFAULT_LIMIT 50

UniValue z_shieldcoinbase(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "z_shieldcoinbase \"fromaddress\" \"tozaddress\" ( fee ) ( limit )\n"
            "\nShield transparent coinbase funds by sending to a shielded zaddr.  This is an asynchronous operation and utxos"
            "\nselected for shielding will be locked.  If there is an error, they are unlocked.  The RPC call `listlockunspent`"
            "\ncan be used to return a list of locked utxos.  The number of coinbase utxos selected for shielding can be limited"
            "\nby the caller.  If the limit parameter is set to zero, and Overwinter is not yet active, the -mempooltxinputlimit"
            "\noption will determine the number of uxtos.  Any limit is constrained by the consensus rule defining a maximum"
            "\ntransaction size of "
            + strprintf("%d bytes before Sapling, and %d bytes once Sapling activates.", MAX_TX_SIZE_BEFORE_SAPLING, MAX_TX_SIZE_AFTER_SAPLING)
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaddress\"         (string, required) The address is a taddr or \"*\" for all taddrs belonging to the wallet.\n"
            "2. \"toaddress\"           (string, required) The address is a zaddr.\n"
            "3. fee                   (numeric, optional, default="
            + strprintf("%s", FormatMoney(SHIELD_COINBASE_DEFAULT_MINERS_FEE)) + ") The fee amount to attach to this transaction.\n"
            "4. limit                 (numeric, optional, default="
            + strprintf("%d", SHIELD_COINBASE_DEFAULT_LIMIT) + ") Limit on the maximum number of utxos to shield.  Set to 0 to use node option -mempooltxinputlimit (before Overwinter), or as many as will fit in the transaction (after Overwinter).\n"
            "\nResult:\n"
            "{\n"
            "  \"remainingUTXOs\": xxx       (numeric) Number of coinbase utxos still available for shielding.\n"
            "  \"remainingValue\": xxx       (numeric) Value of coinbase utxos still available for shielding.\n"
            "  \"shieldingUTXOs\": xxx        (numeric) Number of coinbase utxos being shielded.\n"
            "  \"shieldingValue\": xxx        (numeric) Value of coinbase utxos being shielded.\n"
            "  \"opid\": xxx          (string) An operationid to pass to z_getoperationstatus to get the result of the operation.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_shieldcoinbase", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\" \"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\"")
            + HelpExampleRpc("z_shieldcoinbase", "\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\", \"zs14d8tc0hl9q0vg5l28uec5vk6sk34fkj2n8s7jalvw5fxpy6v39yn4s2ga082lymrkjk0x2nqg37\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    THROW_IF_SYNCING(KOMODO_INSYNC);

    // Validate the from address
    auto fromaddress = params[0].get_str();
    bool isFromWildcard = fromaddress == "*";
    CTxDestination taddr;
    if (!isFromWildcard) {
        taddr = DecodeDestination(fromaddress);
        if (!IsValidDestination(taddr)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or \"*\".");
        }
    }

    // Validate the destination address
    auto destaddress = params[1].get_str();
    if (!IsValidPaymentAddressString(destaddress, CurrentEpochBranchId(chainActive.Height(), Params().GetConsensus()))) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown address format: ") + destaddress );
    }

    // Convert fee from currency format to zatoshis
    CAmount nFee = SHIELD_COINBASE_DEFAULT_MINERS_FEE;
    if (params.size() > 2) {
        if (params[2].get_real() == 0.0) {
            nFee = 0;
        } else {
            nFee = AmountFromValue( params[2] );
        }
    }

    int nLimit = SHIELD_COINBASE_DEFAULT_LIMIT;
    if (params.size() > 3) {
        nLimit = params[3].get_int();
        if (nLimit < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of utxos cannot be negative");
        }
    }

    int nextBlockHeight = chainActive.Height() + 1;
    bool overwinterActive = NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER);
    unsigned int max_tx_size = MAX_TX_SIZE_AFTER_SAPLING;
    if (!NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_SAPLING)) {
        max_tx_size = MAX_TX_SIZE_BEFORE_SAPLING;
    }

    // If Sapling is not active, do not allow sending to a Sapling address.
    if (!NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_SAPLING)) {
        auto res = DecodePaymentAddress(destaddress);
        if (IsValidPaymentAddress(res)) {
            bool toSapling = boost::get<libzcash::SaplingPaymentAddress>(&res) != nullptr;
            if (toSapling) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
            }
        } else {
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown address format: ") + destaddress );
        }
    }

    // Prepare to get coinbase utxos
    std::vector<ShieldCoinbaseUTXO> inputs;
    CAmount shieldedValue = 0;
    CAmount remainingValue = 0;
    size_t estimatedTxSize = 2000;  // 1802 joinsplit description + tx overhead + wiggle room

    #ifdef __LP64__
    uint64_t utxoCounter = 0;
    #else
    size_t utxoCounter = 0;
    #endif

    bool maxedOutFlag = false;
    size_t mempoolLimit = (nLimit != 0) ? nLimit : (overwinterActive ? 0 : (size_t)GetArg("-mempooltxinputlimit", 0));

    // Set of addresses to filter utxos by
    std::set<CTxDestination> destinations = {};
    if (!isFromWildcard) {
        destinations.insert(taddr);
    }

    // Get available utxos
    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, true, NULL, false, true);

    // Find unspent coinbase utxos and update estimated size
    BOOST_FOREACH(const COutput& out, vecOutputs) {
        if (!out.fSpendable) {
            continue;
        }

        CTxDestination address;
        if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
            continue;
        }

        // If taddr is not wildcard "*", filter utxos
        if (destinations.size() > 0 && !destinations.count(address)) {
            continue;
        }

        if (!out.tx->IsCoinBase()) {
            continue;
        }

        utxoCounter++;
        auto scriptPubKey = out.tx->vout[out.i].scriptPubKey;
        CAmount nValue = out.tx->vout[out.i].nValue;

        if (!maxedOutFlag) {
            size_t increase = (boost::get<CScriptID>(&address) != nullptr) ? CTXIN_SPEND_P2SH_SIZE : CTXIN_SPEND_DUST_SIZE;
            if (estimatedTxSize + increase >= max_tx_size ||
                (mempoolLimit > 0 && utxoCounter > mempoolLimit))
            {
                maxedOutFlag = true;
            } else {
                estimatedTxSize += increase;
                ShieldCoinbaseUTXO utxo = {out.tx->GetHash(), out.i, scriptPubKey, nValue};
                inputs.push_back(utxo);
                shieldedValue += nValue;
            }
        }

        if (maxedOutFlag) {
            remainingValue += nValue;
        }
    }

    #ifdef __LP64__
    uint64_t numUtxos = inputs.size();
    #else
    size_t numUtxos = inputs.size();
    #endif

    if (numUtxos == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Could not find any coinbase funds to shield.");
    }

    if (shieldedValue < nFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
            strprintf("Insufficient coinbase funds, have %s, which is less than miners fee %s",
            FormatMoney(shieldedValue), FormatMoney(nFee)));
    }

    // Check that the user specified fee is sane (if too high, it can result in error -25 absurd fee)
    CAmount netAmount = shieldedValue - nFee;
    if (nFee > netAmount) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Fee %s is greater than the net amount to be shielded %s", FormatMoney(nFee), FormatMoney(netAmount)));
    }

    // Keep record of parameters in context object
    UniValue contextInfo(UniValue::VOBJ);
    contextInfo.push_back(Pair("fromaddress", params[0]));
    contextInfo.push_back(Pair("toaddress", params[1]));
    contextInfo.push_back(Pair("fee", ValueFromAmount(nFee)));

    // Builder (used if Sapling addresses are involved)
    TransactionBuilder builder = TransactionBuilder(
        Params().GetConsensus(), nextBlockHeight, pwalletMain);

    // Contextual transaction we will build on
    int blockHeight = chainActive.LastTip()->GetHeight();
    nextBlockHeight = blockHeight + 1;
    // (used if no Sapling addresses are involved)
    CMutableTransaction contextualTx = CreateNewContextualCMutableTransaction(
        Params().GetConsensus(), nextBlockHeight);
    contextualTx.nLockTime = chainActive.LastTip()->GetHeight();

    if (contextualTx.nVersion == 1) {
        contextualTx.nVersion = 2; // Tx format should support vjoinsplits
    }

    // Create operation and add to global queue
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_shieldcoinbase(builder, contextualTx, inputs, destaddress, nFee, contextInfo) );
    q->addOperation(operation);
    AsyncRPCOperationId operationId = operation->getId();

    // Return continuation information
    UniValue o(UniValue::VOBJ);
    o.push_back(Pair("remainingUTXOs", static_cast<uint64_t>(utxoCounter - numUtxos)));
    o.push_back(Pair("remainingValue", ValueFromAmount(remainingValue)));
    o.push_back(Pair("shieldingUTXOs", static_cast<uint64_t>(numUtxos)));
    o.push_back(Pair("shieldingValue", ValueFromAmount(shieldedValue)));
    o.push_back(Pair("opid", operationId));
    return o;
}

#define MERGE_TO_ADDRESS_DEFAULT_TRANSPARENT_LIMIT 50
#define MERGE_TO_ADDRESS_DEFAULT_SPROUT_LIMIT 10
#define MERGE_TO_ADDRESS_DEFAULT_SAPLING_LIMIT 90

#define JOINSPLIT_SIZE GetSerializeSize(JSDescription(), SER_NETWORK, PROTOCOL_VERSION)
#define OUTPUTDESCRIPTION_SIZE GetSerializeSize(OutputDescription(), SER_NETWORK, PROTOCOL_VERSION)
#define SPENDDESCRIPTION_SIZE GetSerializeSize(SpendDescription(), SER_NETWORK, PROTOCOL_VERSION)

UniValue z_mergetoaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    string enableArg = "zmergetoaddress";
    auto fEnableMergeToAddress = fExperimentalMode && GetBoolArg("-" + enableArg, true);
    std::string strDisabledMsg = "";
    if (!fEnableMergeToAddress) {
        strDisabledMsg = experimentalDisabledHelpMsg("z_mergetoaddress", enableArg);
    }

    if (fHelp || params.size() < 2 || params.size() > 7)
        throw runtime_error(
            "z_mergetoaddress [\"fromaddress\", ... ] \"toaddress\" ( fee ) ( transparent_limit ) ( shielded_limit ) ( memo )\n"
            + strDisabledMsg +
            "\nMerge multiple UTXOs and notes into a single UTXO or note.  Coinbase UTXOs are ignored; use `z_shieldcoinbase`"
            "\nto combine those into a single note."
            "\n\nThis is an asynchronous operation, and UTXOs selected for merging will be locked.  If there is an error, they"
            "\nare unlocked.  The RPC call `listlockunspent` can be used to return a list of locked UTXOs."
            "\n\nThe number of UTXOs and notes selected for merging can be limited by the caller.  If the transparent limit"
            "\nparameter is set to zero, and Overwinter is not yet active, the -mempooltxinputlimit option will determine the"
            "\nnumber of UTXOs.  Any limit is constrained by the consensus rule defining a maximum transaction size of"
            + strprintf("\n%d bytes before Sapling, and %d bytes once Sapling activates.", MAX_TX_SIZE_BEFORE_SAPLING, MAX_TX_SIZE_AFTER_SAPLING)
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. fromaddresses         (string, required) A JSON array with addresses.\n"
            "                         The following special strings are accepted inside the array:\n"
            "                             - \"*\": Merge both UTXOs and notes from all addresses belonging to the wallet.\n"
            "                             - \"ANY_TADDR\": Merge UTXOs from all t-addrs belonging to the wallet.\n"
            "                             - \"ANY_ZADDR\": Merge notes from all z-addrs belonging to the wallet.\n"
            "                         If a special string is given, any given addresses of that type will be ignored.\n"
            "    [\n"
            "      \"address\"          (string) Can be a t-addr or a z-addr\n"
            "      ,...\n"
            "    ]\n"
            "2. \"toaddress\"           (string, required) The t-addr or z-addr to send the funds to.\n"
            "3. fee                   (numeric, optional, default="
            + strprintf("%s", FormatMoney(MERGE_TO_ADDRESS_OPERATION_DEFAULT_MINERS_FEE)) + ") The fee amount to attach to this transaction.\n"
            "4. transparent_limit     (numeric, optional, default="
            + strprintf("%d", MERGE_TO_ADDRESS_DEFAULT_TRANSPARENT_LIMIT) + ") Limit on the maximum number of UTXOs to merge.  Set to 0 to use node option -mempooltxinputlimit (before Overwinter), or as many as will fit in the transaction (after Overwinter).\n"
            "4. shielded_limit        (numeric, optional, default="
            + strprintf("%d Sprout or %d Sapling Notes", MERGE_TO_ADDRESS_DEFAULT_SPROUT_LIMIT, MERGE_TO_ADDRESS_DEFAULT_SAPLING_LIMIT) + ") Limit on the maximum number of notes to merge.  Set to 0 to merge as many as will fit in the transaction.\n"
            "5. maximum_utxo_size       (numeric, optional) eg, 0.0001 anything under 10000 satoshies will be merged, ignores 10,000 sat p2pk utxo that iguana uses, and merges coinbase utxo.\n"
            "6. \"memo\"                (string, optional) Encoded as hex. When toaddress is a z-addr, this will be stored in the memo field of the new note.\n"

            "\nResult:\n"
            "{\n"
            "  \"remainingUTXOs\": xxx               (numeric) Number of UTXOs still available for merging.\n"
            "  \"remainingTransparentValue\": xxx    (numeric) Value of UTXOs still available for merging.\n"
            "  \"remainingNotes\": xxx               (numeric) Number of notes still available for merging.\n"
            "  \"remainingShieldedValue\": xxx       (numeric) Value of notes still available for merging.\n"
            "  \"mergingUTXOs\": xxx                 (numeric) Number of UTXOs being merged.\n"
            "  \"mergingTransparentValue\": xxx      (numeric) Value of UTXOs being merged.\n"
            "  \"mergingNotes\": xxx                 (numeric) Number of notes being merged.\n"
            "  \"mergingShieldedValue\": xxx         (numeric) Value of notes being merged.\n"
            "  \"opid\": xxx          (string) An operationid to pass to z_getoperationstatus to get the result of the operation.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_mergetoaddress", "'[\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\"]' ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf")
            + HelpExampleRpc("z_mergetoaddress", "[\"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPV\"], \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
        );

    if (!fEnableMergeToAddress) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: z_mergetoaddress is disabled.");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    THROW_IF_SYNCING(KOMODO_INSYNC);

    bool useAnyUTXO = false;
    bool useAnySprout = false;
    bool useAnySapling = false;
    std::set<CTxDestination> taddrs = {};
    std::set<libzcash::PaymentAddress> zaddrs = {};

    UniValue addresses = params[0].get_array();
    if (addresses.size()==0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, fromaddresses array is empty.");

    // Keep track of addresses to spot duplicates
    std::set<std::string> setAddress;

    // Sources
    for (const UniValue& o : addresses.getValues()) {
        if (!o.isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected string");

        std::string address = o.get_str();

        if (address == "ANY_TADDR") {
            useAnyUTXO = true;
        } else if (address == "ANY_SPROUT") {
            useAnySprout = true;
        } else if (address == "ANY_SAPLING") {
            useAnySapling = true;
        } else {
            CTxDestination taddr = DecodeDestination(address);
            if (IsValidDestination(taddr)) {
                taddrs.insert(taddr);
            } else {
                auto zaddr = DecodePaymentAddress(address);
                if (IsValidPaymentAddress(zaddr)) {
                    zaddrs.insert(zaddr);
                } else {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, string("Unknown address format: ") + address);
                }
            }
        }

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ") + address);
        setAddress.insert(address);
    }

    if (useAnyUTXO && taddrs.size() > 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot specify specific t-addrs when using \"ANY_TADDR\"");
    }
    if ((useAnySprout || useAnySapling) && zaddrs.size() > 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot specify specific z-addrs when using \"ANY_SPROUT\" or \"ANY_SAPLING\"");
    }

    const int nextBlockHeight = chainActive.Height() + 1;
    const bool overwinterActive = NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER);
    const bool saplingActive = NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_SAPLING);

    // Validate the destination address
    auto destaddress = params[1].get_str();
    bool isToSproutZaddr = false;
    bool isToSaplingZaddr = false;
    CTxDestination taddr = DecodeDestination(destaddress);
    if (!IsValidDestination(taddr)) {
        auto decodeAddr = DecodePaymentAddress(destaddress);
        if (IsValidPaymentAddress(decodeAddr)) {
            if (boost::get<libzcash::SaplingPaymentAddress>(&decodeAddr) != nullptr) {
                isToSaplingZaddr = true;
                // If Sapling is not active, do not allow sending to a sapling addresses.
                if (!saplingActive) {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
                }
            } else {
                isToSproutZaddr = true;
            }
        } else {
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown address format: ") + destaddress );
        }
    }

    // Convert fee from currency format to zatoshis
    CAmount nFee = SHIELD_COINBASE_DEFAULT_MINERS_FEE;
    if (params.size() > 2) {
        if (params[2].get_real() == 0.0) {
            nFee = 0;
        } else {
            nFee = AmountFromValue( params[2] );
        }
    }

    int nUTXOLimit = MERGE_TO_ADDRESS_DEFAULT_TRANSPARENT_LIMIT;
    if (params.size() > 3) {
        nUTXOLimit = params[3].get_int();
        if (nUTXOLimit < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of UTXOs cannot be negative");
        }
    }

    int sproutNoteLimit = MERGE_TO_ADDRESS_DEFAULT_SPROUT_LIMIT;
    int saplingNoteLimit = MERGE_TO_ADDRESS_DEFAULT_SAPLING_LIMIT;
    if (params.size() > 4) {
        int nNoteLimit = params[4].get_int();
        if (nNoteLimit < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of notes cannot be negative");
        }
        sproutNoteLimit = nNoteLimit;
        saplingNoteLimit = nNoteLimit;
    }

    CAmount maximum_utxo_size;
    if (params.size() > 5) {
      maximum_utxo_size = AmountFromValue( params[5] );
      if (maximum_utxo_size < 10) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maximum size must be bigger than 0.00000010.");
      }
    } else {
      maximum_utxo_size = 0;
    }

    std::string memo;
    if (params.size() > 6) {
        memo = params[6].get_str();
        if (!(isToSproutZaddr || isToSaplingZaddr)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Memo can not be used with a taddr.  It can only be used with a zaddr.");
        } else if (!IsHex(memo)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected memo data in hexadecimal format.");
        }
        if (memo.length() > ZC_MEMO_SIZE*2) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,  strprintf("Invalid parameter, size of memo is larger than maximum allowed %d", ZC_MEMO_SIZE ));
        }
    }

    MergeToAddressRecipient recipient(destaddress, memo);

    // Prepare to get UTXOs and notes
    std::vector<MergeToAddressInputUTXO> utxoInputs;
    std::vector<MergeToAddressInputSproutNote> sproutNoteInputs;
    std::vector<MergeToAddressInputSaplingNote> saplingNoteInputs;
    CAmount mergedUTXOValue = 0;
    CAmount mergedNoteValue = 0;
    CAmount remainingUTXOValue = 0;
    CAmount remainingNoteValue = 0;
    size_t utxoCounter = 0;
    size_t noteCounter = 0;
    bool maxedOutUTXOsFlag = false;
    bool maxedOutNotesFlag = false;
    size_t mempoolLimit = (nUTXOLimit != 0) ? nUTXOLimit : (overwinterActive ? 0 : (size_t)GetArg("-mempooltxinputlimit", 0));

    unsigned int max_tx_size = saplingActive ? MAX_TX_SIZE_AFTER_SAPLING : MAX_TX_SIZE_BEFORE_SAPLING;
    size_t estimatedTxSize = 200;  // tx overhead + wiggle room
    if (isToSproutZaddr) {
        estimatedTxSize += JOINSPLIT_SIZE;
    } else if (isToSaplingZaddr) {
        estimatedTxSize += OUTPUTDESCRIPTION_SIZE;
    }

    if (useAnyUTXO || taddrs.size() > 0) {
        // Get available utxos
        vector<COutput> vecOutputs;
        pwalletMain->AvailableCoins(vecOutputs, true, NULL, false, maximum_utxo_size != 0 ? true : false);

        // Find unspent utxos and update estimated size
        for (const COutput& out : vecOutputs) {
            if (!out.fSpendable) {
                continue;
            }

            CScript scriptPubKey = out.tx->vout[out.i].scriptPubKey;

            CTxDestination address;
            if (!ExtractDestination(scriptPubKey, address)) {
                continue;
            }
            // If taddr is not wildcard "*", filter utxos
            if (taddrs.size() > 0 && !taddrs.count(address)) {
                continue;
            }

            CAmount nValue = out.tx->vout[out.i].nValue;

            if (maximum_utxo_size != 0) 
            {
                //fprintf(stderr, "utxo txid.%s vout.%i nValue.%li scriptpubkeylength.%i\n",out.tx->GetHash().ToString().c_str(),out.i,nValue,out.tx->vout[out.i].scriptPubKey.size());
                if (nValue > maximum_utxo_size) 
                    continue;
                if (nValue == 10000 && out.tx->vout[out.i].scriptPubKey.size() == 35)
                    continue;
            }

            utxoCounter++;
            if (!maxedOutUTXOsFlag) {
                size_t increase = (boost::get<CScriptID>(&address) != nullptr) ? CTXIN_SPEND_P2SH_SIZE : CTXIN_SPEND_DUST_SIZE;
                if (estimatedTxSize + increase >= max_tx_size ||
                    (mempoolLimit > 0 && utxoCounter > mempoolLimit))
                {
                    maxedOutUTXOsFlag = true;
                } else {
                    estimatedTxSize += increase;
                    COutPoint utxo(out.tx->GetHash(), out.i);
                    utxoInputs.emplace_back(utxo, nValue, scriptPubKey);
                    mergedUTXOValue += nValue;
                }
            }

            if (maxedOutUTXOsFlag) {
                remainingUTXOValue += nValue;
            }
        }
    }

    if (useAnySprout || useAnySapling || zaddrs.size() > 0) {
        // Get available notes
        std::vector<CSproutNotePlaintextEntry> sproutEntries;
        //std::vector<SaplingNoteEntry> saplingEntries;
        //pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, zaddrs);
        std::vector<SaplingNoteEntry> saplingEntries,skipsapling;
        pwalletMain->GetFilteredNotes(sproutEntries, useAnySprout == 0 ? saplingEntries : skipsapling, zaddrs);
        // If Sapling is not active, do not allow sending from a sapling addresses.
        if (!saplingActive && saplingEntries.size() > 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
        }
        // Sending from both Sprout and Sapling is currently unsupported using z_mergetoaddress
        if (sproutEntries.size() > 0 && saplingEntries.size() > 0) {
            throw JSONRPCError(
                               RPC_INVALID_PARAMETER,
                               "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress");
        }
        // If sending between shielded addresses, they must be the same type
        if ((saplingEntries.size() > 0 && isToSproutZaddr) || (sproutEntries.size() > 0 && isToSaplingZaddr)) {
            throw JSONRPCError(
                               RPC_INVALID_PARAMETER,
                               "Cannot send between Sprout and Sapling addresses using z_mergetoaddress");
        }

        // Find unspent notes and update estimated size
        for (const CSproutNotePlaintextEntry& entry : sproutEntries) {
            noteCounter++;
            CAmount nValue = entry.plaintext.value();

            if (!maxedOutNotesFlag) {
                // If we haven't added any notes yet and the merge is to a
                // z-address, we have already accounted for the first JoinSplit.
                size_t increase = (sproutNoteInputs.empty() && !isToSproutZaddr) || (sproutNoteInputs.size() % 2 == 0) ? JOINSPLIT_SIZE : 0;
                if (estimatedTxSize + increase >= max_tx_size ||
                    (sproutNoteLimit > 0 && noteCounter > sproutNoteLimit))
                {
                    maxedOutNotesFlag = true;
                } else {
                    estimatedTxSize += increase;
                    auto zaddr = entry.address;
                    SproutSpendingKey zkey;
                    pwalletMain->GetSproutSpendingKey(zaddr, zkey);
                    sproutNoteInputs.emplace_back(entry.jsop, entry.plaintext.note(zaddr), nValue, zkey);
                    mergedNoteValue += nValue;
                }
            }

            if (maxedOutNotesFlag) {
                remainingNoteValue += nValue;
            }
        }

        for (const SaplingNoteEntry& entry : saplingEntries) {
            noteCounter++;
            CAmount nValue = entry.note.value();
            if (!maxedOutNotesFlag) {
                size_t increase = SPENDDESCRIPTION_SIZE;
                if (estimatedTxSize + increase >= max_tx_size ||
                    (saplingNoteLimit > 0 && noteCounter > saplingNoteLimit))
                {
                    maxedOutNotesFlag = true;
                } else {
                    estimatedTxSize += increase;
                    libzcash::SaplingExtendedSpendingKey extsk;
                    if (!pwalletMain->GetSaplingExtendedSpendingKey(entry.address, extsk)) {
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Could not find spending key for payment address.");
                    }
                    saplingNoteInputs.emplace_back(entry.op, entry.note, nValue, extsk.expsk);
                    mergedNoteValue += nValue;
                }
            }

            if (maxedOutNotesFlag) {
                remainingNoteValue += nValue;
            }
        }
    }

    size_t numUtxos = utxoInputs.size();
    size_t numNotes = sproutNoteInputs.size() + saplingNoteInputs.size();

    //fprintf(stderr, "num utxos.%li\n", numUtxos);
    if (numUtxos < 2 && numNotes == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Could not find any funds to merge.");
    }
    
    // Sanity check: Don't do anything if:
    // - We only have one from address
    // - It's equal to toaddress
    // - The address only contains a single UTXO or note
    if (setAddress.size() == 1 && setAddress.count(destaddress) && (numUtxos + numNotes) == 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Destination address is also the only source address, and all its funds are already merged.");
    }

    CAmount mergedValue = mergedUTXOValue + mergedNoteValue;
    if (mergedValue < nFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                           strprintf("Insufficient funds, have %s, which is less than miners fee %s",
                                     FormatMoney(mergedValue), FormatMoney(nFee)));
    }

    // Check that the user specified fee is sane (if too high, it can result in error -25 absurd fee)
    CAmount netAmount = mergedValue - nFee;
    if (nFee > netAmount) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Fee %s is greater than the net amount to be shielded %s", FormatMoney(nFee), FormatMoney(netAmount)));
    }

    // Keep record of parameters in context object
    UniValue contextInfo(UniValue::VOBJ);
    contextInfo.push_back(Pair("fromaddresses", params[0]));
    contextInfo.push_back(Pair("toaddress", params[1]));
    contextInfo.push_back(Pair("fee", ValueFromAmount(nFee)));

    // Contextual transaction we will build on
    CMutableTransaction contextualTx = CreateNewContextualCMutableTransaction(
                                                                              Params().GetConsensus(),
                                                                              nextBlockHeight);
    bool isSproutShielded = sproutNoteInputs.size() > 0 || isToSproutZaddr;
    if (contextualTx.nVersion == 1 && isSproutShielded) {
        contextualTx.nVersion = 2; // Tx format should support vjoinsplit
    }

    // Builder (used if Sapling addresses are involved)
    boost::optional<TransactionBuilder> builder;
    if (isToSaplingZaddr || saplingNoteInputs.size() > 0) {
        builder = TransactionBuilder(Params().GetConsensus(), nextBlockHeight, pwalletMain);
    } else 
        contextualTx.nExpiryHeight = 0; // set non z-tx to have no expiry height.

    // Create operation and add to global queue
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation(
                                                 new AsyncRPCOperation_mergetoaddress(builder, contextualTx, utxoInputs, sproutNoteInputs, saplingNoteInputs, recipient, nFee, contextInfo) );
    q->addOperation(operation);
    AsyncRPCOperationId operationId = operation->getId();

    // Return continuation information
    UniValue o(UniValue::VOBJ);
    o.push_back(Pair("remainingUTXOs", static_cast<uint64_t>(utxoCounter - numUtxos)));
    o.push_back(Pair("remainingTransparentValue", ValueFromAmount(remainingUTXOValue)));
    o.push_back(Pair("remainingNotes", static_cast<uint64_t>(noteCounter - numNotes)));
    o.push_back(Pair("remainingShieldedValue", ValueFromAmount(remainingNoteValue)));
    o.push_back(Pair("mergingUTXOs", static_cast<uint64_t>(numUtxos)));
    o.push_back(Pair("mergingTransparentValue", ValueFromAmount(mergedUTXOValue)));
    o.push_back(Pair("mergingNotes", static_cast<uint64_t>(numNotes)));
    o.push_back(Pair("mergingShieldedValue", ValueFromAmount(mergedNoteValue)));
    o.push_back(Pair("opid", operationId));
    return o;
}

UniValue z_listoperationids(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_listoperationids\n"
            "\nReturns the list of operation ids currently known to the wallet.\n"
            "\nArguments:\n"
            "1. \"status\"         (string, optional) Filter result by the operation's state e.g. \"success\".\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"operationid\"       (string) an operation id belonging to the wallet\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listoperationids", "")
            + HelpExampleRpc("z_listoperationids", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string filter;
    bool useFilter = false;
    if (params.size()==1) {
        filter = params[0].get_str();
        useFilter = true;
    }

    UniValue ret(UniValue::VARR);
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::vector<AsyncRPCOperationId> ids = q->getAllOperationIds();
    for (auto id : ids) {
        std::shared_ptr<AsyncRPCOperation> operation = q->getOperationForId(id);
        if (!operation) {
            continue;
        }
        std::string state = operation->getStateAsString();
        if (useFilter && filter.compare(state)!=0)
            continue;
        ret.push_back(id);
    }

    return ret;
}


#include "script/sign.h"
int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex);
extern std::string NOTARY_PUBKEY;

int32_t komodo_notaryvin(CMutableTransaction &txNew,uint8_t *notarypub33, void *pTr)
{
    set<CBitcoinAddress> setAddress; uint8_t *script,utxosig[128]; uint256 utxotxid; uint64_t utxovalue; int32_t i,siglen=0,nMinDepth = 0,nMaxDepth = 9999999; vector<COutput> vecOutputs; uint32_t utxovout,eligible,earliest = 0; CScript best_scriptPubKey; bool fNegative,fOverflow;
    bool signSuccess; SignatureData sigdata; uint64_t txfee; uint8_t *ptr;
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    if (!EnsureWalletIsAvailable(0))
        return 0;

    assert(pwalletMain != NULL);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    utxovalue = 0;
    memset(&utxotxid,0,sizeof(utxotxid));
    memset(&utxovout,0,sizeof(utxovout));
    memset(utxosig,0,sizeof(utxosig));
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    BOOST_FOREACH(const COutput& out, vecOutputs)
    {
        if ( out.nDepth < nMinDepth || out.nDepth > nMaxDepth )
            continue;
        if ( setAddress.size() )
        {
            CTxDestination address;
            if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
                continue;
            if (!setAddress.count(address))
                continue;
        }
        CAmount nValue = out.tx->vout[out.i].nValue;
        if ( nValue != 10000 )
            continue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        CTxDestination address;
        if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
        {
            //entry.push_back(Pair("address", CBitcoinAddress(address).ToString()));
            //if (pwalletMain->mapAddressBook.count(address))
            //    entry.push_back(Pair("account", pwalletMain->mapAddressBook[address].name));
        }
        script = (uint8_t *)&out.tx->vout[out.i].scriptPubKey[0];
        if ( out.tx->vout[out.i].scriptPubKey.size() != 35 || script[0] != 33 || script[34] != OP_CHECKSIG || memcmp(notarypub33,script+1,33) != 0 )
        {
            //fprintf(stderr,"scriptsize.%d [0] %02x\n",(int32_t)out.tx->vout[out.i].scriptPubKey.size(),script[0]);
            continue;
        }
        utxovalue = (uint64_t)nValue;
        //decode_hex((uint8_t *)&utxotxid,32,(char *)out.tx->GetHash().GetHex().c_str());
        utxotxid = out.tx->GetHash();
        utxovout = out.i;
        best_scriptPubKey = out.tx->vout[out.i].scriptPubKey;
        //fprintf(stderr,"check %s/v%d %llu\n",(char *)utxotxid.GetHex().c_str(),utxovout,(long long)utxovalue);

        txNew.vin.resize(1);
        txNew.vout.resize((pTr!=0)+1);
        txfee = utxovalue / 2;
        //for (i=0; i<32; i++)
        //    ((uint8_t *)&revtxid)[i] = ((uint8_t *)&utxotxid)[31 - i];
        txNew.vin[0].prevout.hash = utxotxid; //revtxid;
        txNew.vin[0].prevout.n = utxovout;
        txNew.vout[0].nValue = utxovalue - txfee;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex(CRYPTO777_PUBSECPSTR) << OP_CHECKSIG;
        if ( pTr != 0 )
        {
            void **p = (void**)pTr;
            txNew.vout[1].nValue = 0;
            txNew.vout[1].scriptPubKey = *(CScript*)p[0];
            txNew.nLockTime = (uint32_t)(unsigned long long)p[1];
        }
        CTransaction txNewConst(txNew);
        signSuccess = ProduceSignature(TransactionSignatureCreator(&keystore, &txNewConst, 0, utxovalue, SIGHASH_ALL), best_scriptPubKey, sigdata, consensusBranchId);
        if (!signSuccess)
            fprintf(stderr,"notaryvin failed to create signature\n");
        else
        {
            UpdateTransaction(txNew,0,sigdata);
            ptr = (uint8_t *)&sigdata.scriptSig[0];
            siglen = sigdata.scriptSig.size();
            for (i=0; i<siglen; i++)
                utxosig[i] = ptr[i];//, fprintf(stderr,"%02x",ptr[i]);
            //fprintf(stderr," siglen.%d notaryvin %s/v%d\n",siglen,utxotxid.GetHex().c_str(),utxovout);
            break;
        }
    }
    return(siglen);
}

int32_t verus_staked(CBlock *pBlock, CMutableTransaction &txNew, uint32_t &nBits, arith_uint256 &hashResult, uint8_t *utxosig, CPubKey &pk)
{
    return pwalletMain->VerusStakeTransaction(pBlock, txNew, nBits, hashResult, utxosig, pk);
}


#include "../cc/CCfaucet.h"
#include "../cc/CCassets.h"
#include "../cc/CCrewards.h"
#include "../cc/CCdice.h"
#include "../cc/CCfsm.h"
#include "../cc/CCauction.h"
#include "../cc/CClotto.h"
#include "../cc/CCchannels.h"
#include "../cc/CCOracles.h"
#include "../cc/CCGateways.h"
#include "../cc/CCPrices.h"
#include "../cc/CCHeir.h"
#include "../cc/CCMarmara.h"
#include "../cc/CCPayments.h"
#include "../cc/CCPegs.h"

int32_t ensure_CCrequirements(uint8_t evalcode)
{
    CCerror = "";
    if ( ASSETCHAINS_CCDISABLES[evalcode] != 0 || (evalcode == EVAL_MARMARA && ASSETCHAINS_MARMARA == 0) )
    {
        // check if a height activation has been set. 
        fprintf(stderr, "evalcode.%i activates at height. %i current height.%i\n", evalcode, mapHeightEvalActivate[evalcode], komodo_currentheight());
        if ( mapHeightEvalActivate[evalcode] == 0 || komodo_currentheight() == 0 || mapHeightEvalActivate[evalcode] > komodo_currentheight() )
        {
            fprintf(stderr,"evalcode %d disabled\n",evalcode);
            return(-1);
        }
    }
    if ( NOTARY_PUBKEY33[0] == 0 )
    {
        fprintf(stderr,"no -pubkey set\n");
        return(-1);
    }
    else if ( GetBoolArg("-addressindex", DEFAULT_ADDRESSINDEX) == 0 )
    {
        fprintf(stderr,"no -addressindex\n");
        return(-1);
    }
    else if ( GetBoolArg("-spentindex", DEFAULT_SPENTINDEX) == 0 )
    {
        fprintf(stderr,"no -spentindex\n");
        return(-1);
    }
    else return(0);
}

UniValue CCaddress(struct CCcontract_info *cp,char *name,std::vector<unsigned char> &pubkey)
{
    UniValue result(UniValue::VOBJ); char destaddr[64],str[64]; CPubKey mypk,pk;
    pk = GetUnspendable(cp,0);
    GetCCaddress(cp,destaddr,pk);
    if ( strcmp(destaddr,cp->unspendableCCaddr) != 0 )
    {
        uint8_t priv[32];
        Myprivkey(priv); // it is assumed the CC's normal address'es -pubkey was used
        fprintf(stderr,"fix mismatched CCaddr %s -> %s\n",cp->unspendableCCaddr,destaddr);
        strcpy(cp->unspendableCCaddr,destaddr);
        memset(priv,0,32);
    }
    result.push_back(Pair("result", "success"));
    sprintf(str,"%sCCAddress",name);
    result.push_back(Pair(str,cp->unspendableCCaddr));
    sprintf(str,"%sCCBalance",name);
    result.push_back(Pair(str,ValueFromAmount(CCaddress_balance(cp->unspendableCCaddr,1))));
    sprintf(str,"%sNormalAddress",name);
    result.push_back(Pair(str,cp->normaladdr));
    sprintf(str,"%sNormalBalance",name);
    result.push_back(Pair(str,ValueFromAmount(CCaddress_balance(cp->normaladdr,0))));
    if (strcmp(name,"Gateways")==0) result.push_back(Pair("GatewaysPubkey","03ea9c062b9652d8eff34879b504eda0717895d27597aaeb60347d65eed96ccb40"));
    if ((strcmp(name,"Channels")==0 || strcmp(name,"Heir")==0) && pubkey.size() == 33)
    {
        sprintf(str,"%sCC1of2Address",name);
        mypk = pubkey2pk(Mypubkey());
        GetCCaddress1of2(cp,destaddr,mypk,pubkey2pk(pubkey));
        result.push_back(Pair(str,destaddr));
        if (GetTokensCCaddress1of2(cp,destaddr,mypk,pubkey2pk(pubkey))>0)
        {
            sprintf(str,"%sCC1of2TokensAddress",name);
            result.push_back(Pair(str,destaddr));
        }
    }
    else if (strcmp(name,"Tokens")!=0)
    {
        if (GetTokensCCaddress(cp,destaddr,pk)>0)
        {
            sprintf(str,"%sCCTokensAddress",name);
            result.push_back(Pair(str,destaddr));
        }
    }
    if ( pubkey.size() == 33 )
    {
        if ( GetCCaddress(cp,destaddr,pubkey2pk(pubkey)) != 0 )
        {
            sprintf(str,"PubkeyCCaddress(%s)",name);
            result.push_back(Pair(str,destaddr));
            sprintf(str,"PubkeyCCbalance(%s)",name);
            result.push_back(Pair(str,ValueFromAmount(CCaddress_balance(destaddr,0))));
        }
    }
    if ( GetCCaddress(cp,destaddr,pubkey2pk(Mypubkey())) != 0 )
    {
        sprintf(str,"myCCAddress(%s)",name);
        result.push_back(Pair(str,destaddr));
        sprintf(str,"myCCbalance(%s)",name);
        result.push_back(Pair(str,ValueFromAmount(CCaddress_balance(destaddr,1))));
    }
    if ( Getscriptaddress(destaddr,(CScript() << Mypubkey() << OP_CHECKSIG)) != 0 )
    {
        result.push_back(Pair("myaddress",destaddr));
        result.push_back(Pair("mybalance",ValueFromAmount(CCaddress_balance(destaddr,0))));
    }
    return(result);
}

bool pubkey2addr(char *destaddr,uint8_t *pubkey33);

UniValue setpubkey(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    if ( fHelp || params.size() > 1 )
        throw runtime_error(
        "setpubkey\n"
        "\nSets the -pubkey if the daemon was not started with it, if it was already set, it returns the pubkey, and its Raddress.\n"
        "\nArguments:\n"
        "1. \"pubkey\"         (string) pubkey to set.\n"
        "\nResult:\n"
        "  {\n"
        "    \"pubkey\" : \"pubkey\",     (string) The pubkey\n"
        "    \"ismine\" : \"true/false\",     (bool)\n"
        "    \"R-address\" : \"R address\",     (string) The pubkey\n"
        "  }\n"
        "\nExamples:\n"
        + HelpExampleCli("setpubkey", "02f7597468703c1c5c8465dd6d43acaae697df9df30bed21494d193412a1ea193e")
        + HelpExampleRpc("setpubkey", "02f7597468703c1c5c8465dd6d43acaae697df9df30bed21494d193412a1ea193e")
      );

    LOCK2(cs_main, pwalletMain ? &pwalletMain->cs_wallet : NULL);

    char Raddress[64];
    uint8_t pubkey33[33];
    if ( NOTARY_PUBKEY33[0] == 0 ) 
    {
        if (strlen(params[0].get_str().c_str()) == 66) 
        {
            decode_hex(pubkey33,33,(char *)params[0].get_str().c_str());
            pubkey2addr((char *)Raddress,(uint8_t *)pubkey33);
            CBitcoinAddress address(Raddress);
            bool isValid = address.IsValid();
            if (isValid)
            {
                CTxDestination dest = address.Get();
                isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;
                if ( mine == ISMINE_NO ) 
                    result.push_back(Pair("WARNING", "privkey for this pubkey is not imported to wallet!"));
                else 
                {
                    result.push_back(Pair("ismine", "true"));
                    std::string notaryname;
                    if ( (IS_STAKED_NOTARY= StakedNotaryID(notaryname, Raddress)) > -1 ) 
                    {
                        result.push_back(Pair("IsNotary", notaryname));
                        IS_KOMODO_NOTARY = 0;
                    }
                }
                NOTARY_PUBKEY = params[0].get_str();
                decode_hex(NOTARY_PUBKEY33,33,(char *)NOTARY_PUBKEY.c_str());
                USE_EXTERNAL_PUBKEY = 1;
                NOTARY_ADDRESS = address.ToString();
            }
            else
                result.push_back(Pair("error", "pubkey entered is invalid."));
        }
        else
            result.push_back(Pair("error", "pubkey is wrong length, must be 66 char hex string."));
    }
    else 
    {
        if ( NOTARY_ADDRESS.empty() ) 
        {
          pubkey2addr((char *)Raddress,(uint8_t *)NOTARY_PUBKEY33);
          NOTARY_ADDRESS.assign(Raddress);
        }
        result.push_back(Pair("error", "Can only set pubkey once, to change it you need to restart your daemon."));
    }
    if ( NOTARY_PUBKEY33[0] != 0 && !NOTARY_ADDRESS.empty() ) 
    {
        result.push_back(Pair("address", NOTARY_ADDRESS));
        result.push_back(Pair("pubkey", NOTARY_PUBKEY));
    }
    return result;
}

UniValue setstakingsplit(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    if ( fHelp || params.size() > 1 )
        throw runtime_error(
        "setstakingsplit\n"
        "\nSets the split ratio as a percentage for the PoS64 staker. Sends entered % of staking tx value to the mined coinbase.\n"
        "\nArguments:\n"
        "1. \"split_percentage\"         (numeric) split ratio range 0-100.\n"
        "\nResult:\n"
        "  {\n"
        "    \"split_percentage\" : \"split_percentage\"     (numeric) range 0-100\n"
        "  }\n"
        "\nExamples:\n"
        + HelpExampleCli("setstakingsplit", "0")
        + HelpExampleRpc("setstakingsplit", "100")
    );
    
    LOCK(cs_main);
    if ( komodo_newStakerActive(chainActive.Height(),(uint32_t)time(NULL)) != 1 ) 
    {
        throw runtime_error("New PoS64 staker not active yet\n");
    }
    if ( params.size() == 0 )
    {
        result.push_back(Pair("split_percentage", ASSETCHAINS_STAKED_SPLIT_PERCENTAGE));
    }
    else
    {
        std::string strperc = params[0].get_str();
        int32_t perc = std::stoi(strperc);
        if ( perc >= 0 && perc <= 100 ) 
        {
            
            ASSETCHAINS_STAKED_SPLIT_PERCENTAGE = perc;
            result.push_back(Pair("split_percentage", perc));
        }
        else 
        {
            throw runtime_error("must be between 0 and 100 inclusive.\n");
        }
    }
    return result;
}

UniValue channelsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;

    cp = CCinit(&C,EVAL_CHANNELS);
    if ( fHelp || params.size() != 1 )
        throw runtime_error("channelsaddress pubkey\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Channels",pubkey));
}

UniValue cclibaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey; uint8_t evalcode = EVAL_FIRSTUSER;
    if ( fHelp || params.size() > 2 )
        throw runtime_error("cclibaddress [evalcode] [pubkey]\n");
    if ( params.size() >= 1 )
    {
        evalcode = atoi(params[0].get_str().c_str());
        if ( evalcode < EVAL_FIRSTUSER || evalcode > EVAL_LASTUSER )
            throw runtime_error("evalcode not between EVAL_FIRSTUSER and EVAL_LASTUSER\n");
        if ( params.size() == 2 )
            pubkey = ParseHex(params[1].get_str().c_str());
    }
    cp = CCinit(&C,evalcode);
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( cp == 0 )
        throw runtime_error("error creating *cp\n");
    return(CCaddress(cp,(char *)"CClib",pubkey));
}

UniValue cclibinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; uint8_t evalcode = EVAL_FIRSTUSER;
    if ( fHelp || params.size() > 0 )
        throw runtime_error("cclibinfo\n");
    if ( ensure_CCrequirements(0) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    cp = CCinit(&C,evalcode);
    return(CClib_info(cp));
}

UniValue cclib(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; char *method,*jsonstr=0; uint8_t evalcode = EVAL_FIRSTUSER;
    std::string vobjJsonSerialized;

    if ( fHelp || params.size() > 3 )
        throw runtime_error("cclib method [evalcode] [JSON params]\n");
    if ( ASSETCHAINS_CCLIB.size() == 0 )
        throw runtime_error("no -ac_cclib= specified\n");
    if ( ensure_CCrequirements(0) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    method = (char *)params[0].get_str().c_str();
    if ( params.size() >= 2 )
    {
        evalcode = atoi(params[1].get_str().c_str());
        if ( evalcode < EVAL_FIRSTUSER || evalcode > EVAL_LASTUSER )
        {
            //printf("evalcode.%d vs (%d, %d)\n",evalcode,EVAL_FIRSTUSER,EVAL_LASTUSER);
            throw runtime_error("evalcode not between EVAL_FIRSTUSER and EVAL_LASTUSER\n");
        }
        if ( params.size() == 3 )
        {
            if (params[2].getType() == UniValue::VOBJ) {
                vobjJsonSerialized = params[2].write(0, 0);
                jsonstr = (char *)vobjJsonSerialized.c_str();
            }
            else  // VSTR assumed
                jsonstr = (char *)params[2].get_str().c_str();
            //fprintf(stderr,"params.(%s %s %s)\n",params[0].get_str().c_str(),params[1].get_str().c_str(),jsonstr);
        }
    }
    cp = CCinit(&C,evalcode);
    return(CClib(cp,method,jsonstr));
}

UniValue payments_release(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("paymentsrelease \"[%22createtxid%22,amount,(skipminimum)]\"\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsRelease(cp,(char *)params[0].get_str().c_str()));
}

UniValue payments_fund(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("paymentsfund \"[%22createtxid%22,amount(,useopret)]\"\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsFund(cp,(char *)params[0].get_str().c_str()));
}

UniValue payments_merge(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("paymentsmerge \"[%22createtxid%22]\"\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsMerge(cp,(char *)params[0].get_str().c_str()));
}

UniValue payments_txidopret(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("paymentstxidopret \"[allocation,%22scriptPubKey%22(,%22destopret%22)]\"\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsTxidopret(cp,(char *)params[0].get_str().c_str()));
}

UniValue payments_create(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("paymentscreate \"[lockedblocks,minamount,%22paytxid0%22,...,%22paytxidN%22]\"\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsCreate(cp,(char *)params[0].get_str().c_str()));
}

UniValue payments_airdrop(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("paymentsairdrop \"[lockedblocks,minamount,mintoaddress,top,bottom,fixedFlag,%22excludeAddress%22,...,%22excludeAddressN%22]\"\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsAirdrop(cp,(char *)params[0].get_str().c_str()));
}

UniValue payments_airdroptokens(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("payments_airdroptokens \"[%22tokenid%22,lockedblocks,minamount,mintoaddress,top,bottom,fixedFlag,%22excludePubKey%22,...,%22excludePubKeyN%22]\"\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsAirdropTokens(cp,(char *)params[0].get_str().c_str()));
}

UniValue payments_info(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("paymentsinfo \"[%22createtxid%22]\"\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsInfo(cp,(char *)params[0].get_str().c_str()));
}

UniValue payments_list(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C;
    if ( fHelp || params.size() != 0 )
        throw runtime_error("paymentslist\n");
    if ( ensure_CCrequirements(EVAL_PAYMENTS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    cp = CCinit(&C,EVAL_PAYMENTS);
    return(PaymentsList(cp,(char *)""));
}

UniValue oraclesaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_ORACLES);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("oraclesaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Oracles",pubkey));
}

UniValue pricesaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); struct CCcontract_info *cp,C,*assetscp,C2; std::vector<unsigned char> pubkey; CPubKey pk,planpk,pricespk; char myaddr[64],houseaddr[64],exposureaddr[64];
    cp = CCinit(&C,EVAL_PRICES);
    assetscp = CCinit(&C2,EVAL_PRICES);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("pricesaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    result = CCaddress(cp,(char *)"Prices",pubkey);
    if (mypk.IsValid()) pk=mypk;
    else pk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    GetCCaddress(assetscp,myaddr,pk);
    GetCCaddress1of2(assetscp,houseaddr,pricespk,planpk);
    GetCCaddress1of2(assetscp,exposureaddr,pricespk,pricespk);
    result.push_back(Pair("myaddr",myaddr)); // for holding my asssets
    result.push_back(Pair("houseaddr",houseaddr)); // globally accessible house assets
    result.push_back(Pair("exposureaddr",exposureaddr)); // tracking of exposure
    return(result);
}

UniValue pegsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_PEGS);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("pegssaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Pegs",pubkey));
}

UniValue marmaraaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_MARMARA);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("Marmaraaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Marmara",pubkey));
}

UniValue paymentsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_PAYMENTS);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("paymentsaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Payments",pubkey));
}

UniValue gatewaysaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("gatewaysaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Gateways",pubkey));
}

UniValue heiraddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
	struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
	cp = CCinit(&C,EVAL_HEIR);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("heiraddress pubkey\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    pubkey = ParseHex(params[0].get_str().c_str());
	return(CCaddress(cp,(char *)"Heir",pubkey));
}

UniValue lottoaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_LOTTO);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("lottoaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Lotto",pubkey));
}

UniValue FSMaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_FSM);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("FSMaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"FSM",pubkey));
}

UniValue auctionaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_AUCTION);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("auctionaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Auction",pubkey));
}

UniValue diceaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_DICE);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("diceaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Dice",pubkey));
}

UniValue faucetaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    int error;
    cp = CCinit(&C,EVAL_FAUCET);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("faucetaddress [pubkey]\n");
    error = ensure_CCrequirements(cp->evalcode);
    if ( error < 0 )
        throw runtime_error(strprintf("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet. ERR=%d\n", error));
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Faucet",pubkey));
}

UniValue rewardsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_REWARDS);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("rewardsaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Rewards",pubkey));
}

UniValue assetsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
	struct CCcontract_info *cp, C; std::vector<unsigned char> pubkey;
	cp = CCinit(&C, EVAL_ASSETS);
	if (fHelp || params.size() > 1)
		throw runtime_error("assetsaddress [pubkey]\n");
	if (ensure_CCrequirements(cp->evalcode) < 0)
		throw runtime_error(CC_REQUIREMENTS_MSG);
	if (params.size() == 1)
		pubkey = ParseHex(params[0].get_str().c_str());
	return(CCaddress(cp, (char *)"Assets", pubkey));
}

UniValue tokenaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_TOKENS);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("tokenaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Tokens", pubkey));
}

UniValue importgatewayaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("importgatewayddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"ImportGateway", pubkey));
}

UniValue marmara_poolpayout(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t firstheight; double perc; char *jsonstr;
    if ( fHelp || params.size() != 3 )
    {
        // marmarapoolpayout 0.5 2 '[["024131032ed90941e714db8e6dd176fe5a86c9d873d279edecf005c06f773da686",1000],["02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92",100]]';
        //marmarapoolpayout 0 2 '[["024131032ed90941e714db8e6dd176fe5a86c9d873d279edecf005c06f773da686",1000]]'
        throw runtime_error("marmarapoolpayout perc firstheight \"[[\\\"pubkey\\\":shares], ...]\"\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    perc = atof(params[0].get_str().c_str()) / 100.;
    firstheight = atol(params[1].get_str().c_str());
    jsonstr = (char *)params[2].get_str().c_str();
    return(MarmaraPoolPayout(0,firstheight,perc,jsonstr)); // [[pk0, shares0], [pk1, shares1], ...]
}

UniValue marmara_receive(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 batontxid; std::vector<uint8_t> senderpub; int64_t amount; int32_t matures; std::string currency;
    if ( fHelp || (params.size() != 5 && params.size() != 4) )
    {
        // automatic flag -> lsb of matures
        // 1st marmarareceive 028076d42eb20efc10007fafb5ca66a2052523c0d2221e607adf958d1a332159f6 7.5 MARMARA 1440
        // after marmarareceive 039433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775 7.5 MARMARA 1168 d72d87aa0d50436de695c93e2bf3d7273c63c92ef6307913aa01a6ee6a16548b
        throw runtime_error("marmarareceive senderpk amount currency matures batontxid\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    memset(&batontxid,0,sizeof(batontxid));
    senderpub = ParseHex(params[0].get_str().c_str());
    if (senderpub.size()!= 33)
    {
        ERR_RESULT("invalid sender pubkey");
        return result;
    }
    amount = atof(params[1].get_str().c_str()) * COIN + 0.00000000499999;
    currency = params[2].get_str();
    if ( params.size() == 5 )
    {
        matures = atol(params[3].get_str().c_str());
        batontxid = Parseuint256((char *)params[4].get_str().c_str());
    } else matures = atol(params[3].get_str().c_str()) + chainActive.LastTip()->GetHeight() + 1;
    return(MarmaraReceive(0,pubkey2pk(senderpub),amount,currency,matures,batontxid,true));
}

UniValue marmara_issue(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 approvaltxid; std::vector<uint8_t> receiverpub; int64_t amount; int32_t matures; std::string currency;
    if ( fHelp || params.size() != 5 )
    {
        // marmaraissue 039433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775 7.5 MARMARA 1168 32da4cb3e886ee42de90b4a15042d71169077306badf909099c5c5c692df3f27
        // marmaraissue 039433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775 700 MARMARA 2629 11fe8bf1de80c2ef69124d08907f259aef7f41e3a632ca2d48ad072a8c8f3078 -> 335df3a5dd6b92a3d020c9465d4d76e0d8242126106b83756dcecbad9813fdf3

        throw runtime_error("marmaraissue receiverpk amount currency matures approvaltxid\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    receiverpub = ParseHex(params[0].get_str().c_str());
    if (receiverpub.size()!= 33)
    {
        ERR_RESULT("invalid receiverpub pubkey");
        return result;
    }
    amount = atof(params[1].get_str().c_str()) * COIN + 0.00000000499999;
    currency = params[2].get_str();
    matures = atol(params[3].get_str().c_str());
    approvaltxid = Parseuint256((char *)params[4].get_str().c_str());
    return(MarmaraIssue(0,'I',pubkey2pk(receiverpub),amount,currency,matures,approvaltxid,zeroid));
}

UniValue marmara_transfer(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 approvaltxid,batontxid; std::vector<uint8_t> receiverpub; int64_t amount; int32_t matures; std::string currency; std::vector<uint256> creditloop;
    if ( fHelp || params.size() != 5 )
    {
        // marmaratransfer 028076d42eb20efc10007fafb5ca66a2052523c0d2221e607adf958d1a332159f6 7.5 MARMARA 1168 1506c774e4b2804a6e25260920840f4cfca8d1fb400e69fe6b74b8e593dbedc5
        throw runtime_error("marmaratransfer receiverpk amount currency matures approvaltxid\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    receiverpub = ParseHex(params[0].get_str().c_str());
    if (receiverpub.size()!= 33)
    {
        ERR_RESULT("invalid receiverpub pubkey");
        return result;
    }
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    amount = atof(params[1].get_str().c_str()) * COIN + 0.00000000499999;
    currency = params[2].get_str();
    matures = atol(params[3].get_str().c_str());
    approvaltxid = Parseuint256((char *)params[4].get_str().c_str());
    if ( MarmaraGetbatontxid(creditloop,batontxid,approvaltxid) < 0 )
        throw runtime_error("couldnt find batontxid\n");
    return(MarmaraIssue(0,'T',pubkey2pk(receiverpub),amount,currency,matures,approvaltxid,batontxid));
}

UniValue marmara_info(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); CPubKey issuerpk; std::vector<uint8_t> issuerpub; int64_t minamount,maxamount; int32_t firstheight,lastheight; std::string currency;
    if ( fHelp || params.size() < 4 || params.size() > 6 )
    {
        throw runtime_error("marmarainfo firstheight lastheight minamount maxamount [currency issuerpk]\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    firstheight = atol(params[0].get_str().c_str());
    lastheight = atol(params[1].get_str().c_str());
    minamount = atof(params[2].get_str().c_str()) * COIN + 0.00000000499999;
    maxamount = atof(params[3].get_str().c_str()) * COIN + 0.00000000499999;
    if ( params.size() >= 5 )
        currency = params[4].get_str();
    if ( params.size() == 6 )
    {
        issuerpub = ParseHex(params[5].get_str().c_str());
        if ( issuerpub.size()!= 33 )
        {
            ERR_RESULT("invalid issuer pubkey");
            return result;
        }
        issuerpk = pubkey2pk(issuerpub);
    }
    result = MarmaraInfo(issuerpk,firstheight,lastheight,minamount,maxamount,currency);
    return(result);
}

UniValue marmara_creditloop(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 txid;
    if ( fHelp || params.size() != 1 )
    {
        // marmaracreditloop 010ff7f9256cefe3b5dee3d72c0eeae9fc6f34884e6f32ffe5b60916df54a9be
        throw runtime_error("marmaracreditloop txid\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    result = MarmaraCreditloop(txid);
    return(result);
}

UniValue marmara_settlement(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 batontxid;
    if ( fHelp || params.size() != 1 )
    {
        // marmarasettlement 010ff7f9256cefe3b5dee3d72c0eeae9fc6f34884e6f32ffe5b60916df54a9be
        // marmarasettlement ff3e259869196f3da9b5ea3f9e088a76c4fc063cf36ab586b652e121d441a603
        throw runtime_error("marmarasettlement batontxid\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    batontxid = Parseuint256((char *)params[0].get_str().c_str());
    result = MarmaraSettlement(0,batontxid);
    return(result);
}

UniValue marmara_lock(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t amount; int32_t height;
    if ( fHelp || params.size() > 2 || params.size() == 0 )
    {
        throw runtime_error("marmaralock amount unlockht\n");
    }
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    amount = atof(params[0].get_str().c_str()) * COIN + 0.00000000499999;
    if ( params.size() == 2 )
        height = atol(params[1].get_str().c_str());
    else height = chainActive.LastTip()->GetHeight() + 1;
    return(MarmaraLock(0,amount,height));
}

UniValue channelslist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if ( fHelp || params.size() > 0 )
        throw runtime_error("channelslist\n");
    if ( ensure_CCrequirements(EVAL_CHANNELS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    return(ChannelsList(mypk));
}

UniValue channelsinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 opentxid;
    if ( fHelp || params.size() > 1 )
        throw runtime_error("channelsinfo [opentxid]\n");
    if ( ensure_CCrequirements(EVAL_CHANNELS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    opentxid=zeroid;
    if (params.size() > 0 && !params[0].isNull() && !params[0].get_str().empty())
        opentxid = Parseuint256((char *)params[0].get_str().c_str());
    return(ChannelsInfo(mypk,opentxid));
}

UniValue channelsopen(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int32_t numpayments; int64_t payment; std::vector<unsigned char> destpub; struct CCcontract_info *cp,C;
    uint256 tokenid=zeroid;

    cp = CCinit(&C,EVAL_CHANNELS);
    if ( fHelp || params.size() < 3 || params.size() > 4)
        throw runtime_error("channelsopen destpubkey numpayments payment [tokenid]\n");
    if ( ensure_CCrequirements(EVAL_CHANNELS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    destpub = ParseHex(params[0].get_str().c_str());
    numpayments = atoi(params[1].get_str().c_str());
    payment = atol(params[2].get_str().c_str());
    if (params.size()==4)
    {
        tokenid=Parseuint256((char *)params[3].get_str().c_str());
    }
    result = ChannelOpen(mypk,0,pubkey2pk(destpub),numpayments,payment,tokenid);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);    
    return(result);
}

UniValue channelspayment(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); struct CCcontract_info *cp,C; uint256 opentxid,secret=zeroid; int32_t n; int64_t amount;
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( fHelp || params.size() < 2 ||  params.size() >3 )
        throw runtime_error("channelspayment opentxid amount [secret]\n");
    if ( ensure_CCrequirements(EVAL_CHANNELS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    opentxid = Parseuint256((char *)params[0].get_str().c_str());
    amount = atoi((char *)params[1].get_str().c_str());
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
    {
        secret = Parseuint256((char *)params[2].get_str().c_str());
    }
    result = ChannelPayment(mypk,0,opentxid,amount,secret);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue channelsclose(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); struct CCcontract_info *cp,C; uint256 opentxid;
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( fHelp || params.size() != 1 )
        throw runtime_error("channelsclose opentxid\n");
    if ( ensure_CCrequirements(EVAL_CHANNELS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    opentxid = Parseuint256((char *)params[0].get_str().c_str());
    result = ChannelClose(mypk,0,opentxid);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue channelsrefund(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); struct CCcontract_info *cp,C; uint256 opentxid,closetxid;
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( fHelp || params.size() != 2 )
        throw runtime_error("channelsrefund opentxid closetxid\n");
    if ( ensure_CCrequirements(EVAL_CHANNELS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    opentxid = Parseuint256((char *)params[0].get_str().c_str());
    closetxid = Parseuint256((char *)params[1].get_str().c_str());
    result = ChannelRefund(mypk,0,opentxid,closetxid);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue rewardscreatefunding(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); char *name; int64_t funds,APR,minseconds,maxseconds,mindeposit; std::string hex;
    if ( fHelp || params.size() > 6 || params.size() < 2 )
        throw runtime_error("rewardscreatefunding name amount APR mindays maxdays mindeposit\n");
    if ( ensure_CCrequirements(EVAL_REWARDS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
   // default to OOT params
    APR = 5 * COIN;
    minseconds = maxseconds = 60 * 3600 * 24;
    mindeposit = 100 * COIN;
    name = (char *)params[0].get_str().c_str();
    funds = atof(params[1].get_str().c_str()) * COIN + 0.00000000499999;

    if (!VALID_PLAN_NAME(name)) {
        ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
        return(result);
    }

    if ( funds <= 0 ) {
        ERR_RESULT("funds must be positive");
        return result;
    }
    if ( params.size() > 2 )
    {
        APR = atof(params[2].get_str().c_str()) * COIN;
        if ( APR > REWARDSCC_MAXAPR )
        {
            ERR_RESULT("25% APR is maximum");
            return result;
        }
        if ( params.size() > 3 )
        {
            minseconds = atol(params[3].get_str().c_str()) * 3600 * 24;
            if ( minseconds < 0 ) {
                ERR_RESULT("mindays must be non-negative");
                return result;
            }
            if ( params.size() > 4 )
            {
                maxseconds = atol(params[4].get_str().c_str()) * 3600 * 24;
                if ( maxseconds <= 0 ) {
                    ERR_RESULT("maxdays must be positive");
                    return result;
                }
                if ( maxseconds < minseconds ) {
                    ERR_RESULT("maxdays must be greater than mindays");
                    return result;
                }
                if ( params.size() > 5 )
                    mindeposit = atof(params[5].get_str().c_str()) * COIN + 0.00000000499999;
                    if ( mindeposit <= 0 ) {
                        ERR_RESULT("mindeposit must be positive");
                        return result;
                    }
            }
        }
    }
    hex = RewardsCreateFunding(0,name,funds,APR,minseconds,maxseconds,mindeposit);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt create rewards funding transaction");
    return(result);
}

UniValue rewardslock(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); char *name; uint256 fundingtxid; int64_t amount; std::string hex;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("rewardslock name fundingtxid amount\n");
    if ( ensure_CCrequirements(EVAL_REWARDS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = (char *)params[0].get_str().c_str();
    fundingtxid = Parseuint256((char *)params[1].get_str().c_str());
    amount = atof(params[2].get_str().c_str()) * COIN + 0.00000000499999;
    hex = RewardsLock(0,name,fundingtxid,amount);

    if (!VALID_PLAN_NAME(name)) {
            ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
            return(result);
    }
    if ( CCerror != "" ){
        ERR_RESULT(CCerror);
    } else if ( amount > 0 ) {
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT( "couldnt create rewards lock transaction");
    } else ERR_RESULT("amount must be positive");
    return(result);
}

UniValue rewardsaddfunding(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); char *name; uint256 fundingtxid; int64_t amount; std::string hex;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("rewardsaddfunding name fundingtxid amount\n");
    if ( ensure_CCrequirements(EVAL_REWARDS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = (char *)params[0].get_str().c_str();
    fundingtxid = Parseuint256((char *)params[1].get_str().c_str());
    amount = atof(params[2].get_str().c_str()) * COIN + 0.00000000499999;
    hex = RewardsAddfunding(0,name,fundingtxid,amount);

    if (!VALID_PLAN_NAME(name)) {
            ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
            return(result);
    }
    if (CCerror != "") {
        ERR_RESULT(CCerror);
    } else if (amount > 0) {
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "couldnt create rewards addfunding transaction"));
        }
    } else {
            ERR_RESULT("funding amount must be positive");
    }
    return(result);
}

UniValue rewardsunlock(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex; char *name; uint256 fundingtxid,txid;
    if ( fHelp || params.size() > 3 || params.size() < 2 )
        throw runtime_error("rewardsunlock name fundingtxid [txid]\n");
    if ( ensure_CCrequirements(EVAL_REWARDS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = (char *)params[0].get_str().c_str();
    fundingtxid = Parseuint256((char *)params[1].get_str().c_str());

    if (!VALID_PLAN_NAME(name)) {
            ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
            return(result);
    }
    if ( params.size() > 2 )
        txid = Parseuint256((char *)params[2].get_str().c_str());
    else memset(&txid,0,sizeof(txid));
    hex = RewardsUnlock(0,name,fundingtxid,txid);
    if (CCerror != "") {
        ERR_RESULT(CCerror);
    } else if ( hex.size() > 0 ) {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt create rewards unlock transaction");
    return(result);
}

UniValue rewardslist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if ( fHelp || params.size() > 0 )
        throw runtime_error("rewardslist\n");
    if ( ensure_CCrequirements(EVAL_REWARDS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    return(RewardsList());
}

UniValue rewardsinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 fundingtxid;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("rewardsinfo fundingtxid\n");
    if ( ensure_CCrequirements(EVAL_REWARDS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    fundingtxid = Parseuint256((char *)params[0].get_str().c_str());
    return(RewardsInfo(fundingtxid));
}

UniValue gatewayslist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if ( fHelp || params.size() > 0 )
        throw runtime_error("gatewayslist\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    return(GatewaysList());
}

UniValue gatewaysexternaladdress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid; CPubKey pubkey;

    if ( fHelp || params.size() != 2)
        throw runtime_error("gatewaysexternaladdress bindtxid pubkey\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    pubkey = ParseHex(params[1].get_str().c_str());
    return(GatewaysExternalAddress(bindtxid,pubkey));
}

UniValue gatewaysdumpprivkey(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid;

    if ( fHelp || params.size() != 2)
        throw runtime_error("gatewaysdumpprivkey bindtxid address\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    std::string strAddress = params[1].get_str();
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid transparent address");
    }
    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    }
    CKey vchSecret;
    if (!pwalletMain->GetKey(*keyID, vchSecret)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    }
    return(GatewaysDumpPrivKey(bindtxid,vchSecret));
}

UniValue gatewaysinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 txid;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("gatewaysinfo bindtxid\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    return(GatewaysInfo(txid));
}

UniValue gatewaysbind(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 tokenid,oracletxid; int32_t i; int64_t totalsupply; std::vector<CPubKey> pubkeys;
    uint8_t M,N,p1,p2,p3,p4=0; std::string coin; std::vector<unsigned char> pubkey;

    if ( fHelp || params.size() < 10 )
        throw runtime_error("gatewaysbind tokenid oracletxid coin tokensupply M N pubkey(s) pubtype p2shtype wiftype [taddr]\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    oracletxid = Parseuint256((char *)params[1].get_str().c_str());
    coin = params[2].get_str();
    totalsupply = atol((char *)params[3].get_str().c_str());
    M = atoi((char *)params[4].get_str().c_str());
    N = atoi((char *)params[5].get_str().c_str());
    if ( M > N || N == 0 || N > 15 || totalsupply < COIN/100 || tokenid == zeroid )
    {
        Unlock2NSPV(mypk);
        throw runtime_error("illegal M or N > 15 or tokensupply or invalid tokenid\n");
    }
    if ( params.size() < 6+N+3 )
    {
        Unlock2NSPV(mypk);
        throw runtime_error("not enough parameters for N pubkeys\n");
    }
    for (i=0; i<N; i++)
    {       
        pubkey = ParseHex(params[6+i].get_str().c_str());
        if (pubkey.size()!= 33)
        {
            Unlock2NSPV(mypk);
            throw runtime_error("invalid destination pubkey");
        }
        pubkeys.push_back(pubkey2pk(pubkey));
    }
    p1 = atoi((char *)params[6+N].get_str().c_str());
    p2 = atoi((char *)params[6+N+1].get_str().c_str());
    p3 = atoi((char *)params[6+N+2].get_str().c_str());
    if (params.size() == 9+N+1) p4 = atoi((char *)params[9+N].get_str().c_str());
    result = GatewaysBind(mypk,0,coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,p1,p2,p3,p4);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue gatewaysdeposit(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int32_t i,claimvout,height; int64_t amount; std::string coin,deposithex; uint256 bindtxid,cointxid; std::vector<uint8_t>proof,destpub,pubkey;
    if ( fHelp || params.size() != 9 )
        throw runtime_error("gatewaysdeposit bindtxid height coin cointxid claimvout deposithex proof destpub amount\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    height = atoi((char *)params[1].get_str().c_str());
    coin = params[2].get_str();
    cointxid = Parseuint256((char *)params[3].get_str().c_str());
    claimvout = atoi((char *)params[4].get_str().c_str());
    deposithex = params[5].get_str();
    proof = ParseHex(params[6].get_str());
    destpub = ParseHex(params[7].get_str());
    amount = atof((char *)params[8].get_str().c_str()) * COIN + 0.00000000499999;
    if ( amount <= 0 || claimvout < 0 )
    {
        Unlock2NSPV(mypk);
        throw runtime_error("invalid param: amount, numpks or claimvout\n");
    }
    if (destpub.size()!= 33)
    {
        Unlock2NSPV(mypk);
        throw runtime_error("invalid destination pubkey");
    }
    result = GatewaysDeposit(mypk,0,bindtxid,height,coin,cointxid,claimvout,deposithex,proof,pubkey2pk(destpub),amount);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue gatewaysclaim(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string coin; uint256 bindtxid,deposittxid; std::vector<uint8_t>destpub; int64_t amount;
    if ( fHelp || params.size() != 5 )
        throw runtime_error("gatewaysclaim bindtxid coin deposittxid destpub amount\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    deposittxid = Parseuint256((char *)params[2].get_str().c_str());
    destpub = ParseHex(params[3].get_str());
    amount = atof((char *)params[4].get_str().c_str()) * COIN + 0.00000000499999;
    if (destpub.size()!= 33)
    {
        Unlock2NSPV(mypk);
        throw runtime_error("invalid destination pubkey");
    }
    result = GatewaysClaim(mypk,0,bindtxid,coin,deposittxid,pubkey2pk(destpub),amount);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue gatewayswithdraw(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 bindtxid; int64_t amount; std::string coin; std::vector<uint8_t> withdrawpub;
    if ( fHelp || params.size() != 4 )
        throw runtime_error("gatewayswithdraw bindtxid coin withdrawpub amount\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    withdrawpub = ParseHex(params[2].get_str());
    amount = atof((char *)params[3].get_str().c_str()) * COIN + 0.00000000499999;
    if (withdrawpub.size()!= 33)
    {
        Unlock2NSPV(mypk);
        throw runtime_error("invalid destination pubkey");
    }
    result = GatewaysWithdraw(mypk,0,bindtxid,coin,pubkey2pk(withdrawpub),amount);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Lock2NSPV(mypk);
    return(result);
}

UniValue gatewayspartialsign(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string coin,parthex; uint256 txid;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("gatewayspartialsign txidaddr refcoin hex\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    parthex = params[2].get_str();
    result = GatewaysPartialSign(mypk,0,txid,coin,parthex);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue gatewayscompletesigning(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 withdrawtxid; std::string txhex,coin;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("gatewayscompletesigning withdrawtxid coin hex\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    withdrawtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    txhex = params[2].get_str();
    result = GatewaysCompleteSigning(mypk,0,withdrawtxid,coin,txhex);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue gatewaysmarkdone(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 completetxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("gatewaysmarkdone completesigningtx coin\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    completetxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    result = GatewaysMarkDone(mypk,0,completetxid,coin);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue gatewayspendingdeposits(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("gatewayspendingdeposits bindtxid coin\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    return(GatewaysPendingDeposits(mypk,bindtxid,coin));
}

UniValue gatewayspendingwithdraws(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("gatewayspendingwithdraws bindtxid coin\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    return(GatewaysPendingWithdraws(mypk,bindtxid,coin));
}

UniValue gatewaysprocessed(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("gatewaysprocessed bindtxid coin\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    return(GatewaysProcessedWithdraws(mypk,bindtxid,coin));
}

UniValue oracleslist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if ( fHelp || params.size() > 0 )
        throw runtime_error("oracleslist\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    return(OraclesList());
}

UniValue oraclesinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 txid;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("oraclesinfo oracletxid\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    return(OracleInfo(txid));
}

UniValue oraclesfund(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 txid;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("oraclesfund oracletxid\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    result = OracleFund(mypk,0,txid);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue oraclesregister(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 txid; int64_t datafee;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("oraclesregister oracletxid datafee\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    if ( (datafee= atol((char *)params[1].get_str().c_str())) == 0 )
        datafee = atof((char *)params[1].get_str().c_str()) * COIN + 0.00000000499999;
    result = OracleRegister(mypk,0,txid,datafee);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue oraclessubscribe(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 txid; int64_t amount; std::vector<unsigned char> pubkey;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("oraclessubscribe oracletxid publisher amount\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    pubkey = ParseHex(params[1].get_str().c_str());
    amount = atof((char *)params[2].get_str().c_str()) * COIN + 0.00000000499999;
    result = OracleSubscribe(mypk,0,txid,pubkey2pk(pubkey),amount);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue oraclessample(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 oracletxid,txid; int32_t num; char *batonaddr;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("oraclessample oracletxid txid\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    oracletxid = Parseuint256((char *)params[0].get_str().c_str());
    txid = Parseuint256((char *)params[1].get_str().c_str());
    return(OracleDataSample(oracletxid,txid));
}

UniValue oraclessamples(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 txid; int32_t num; char *batonaddr;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("oraclessamples oracletxid batonaddress num\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    batonaddr = (char *)params[1].get_str().c_str();
    num = atoi((char *)params[2].get_str().c_str());
    return(OracleDataSamples(txid,batonaddr,num));
}

UniValue oraclesdata(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 txid; std::vector<unsigned char> data;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("oraclesdata oracletxid hexstr\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    data = ParseHex(params[1].get_str().c_str());
    result = OracleData(mypk,0,txid,data);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue oraclescreate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string name,description,format;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("oraclescreate name description format\n");
    if ( ensure_CCrequirements(EVAL_ORACLES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    name = params[0].get_str();
    description = params[1].get_str();
    format = params[2].get_str();
    result = OracleCreate(mypk,0,name,description,format);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue FSMcreate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string name,states,hex;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("FSMcreate name states\n");
    if ( ensure_CCrequirements(EVAL_FSM) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = params[0].get_str();
    states = params[1].get_str();
    hex = FSMCreate(0,name,states);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else result.push_back(Pair("error", "couldnt create FSM transaction"));
    return(result);
}

UniValue FSMlist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    if ( fHelp || params.size() > 0 )
        throw runtime_error("FSMlist\n");
    if ( ensure_CCrequirements(EVAL_FSM) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    return(FSMList());
}

UniValue FSMinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 FSMtxid;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("FSMinfo fundingtxid\n");
    if ( ensure_CCrequirements(EVAL_FSM) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    FSMtxid = Parseuint256((char *)params[0].get_str().c_str());
    return(FSMInfo(FSMtxid));
}

UniValue faucetinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 fundingtxid;
    if ( fHelp || params.size() != 0 )
        throw runtime_error("faucetinfo\n");
    if ( ensure_CCrequirements(EVAL_FAUCET) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    return(FaucetInfo());
}

UniValue faucetfund(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t funds; std::string hex;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("faucetfund amount\n");
    funds = atof(params[0].get_str().c_str()) * COIN + 0.00000000499999;
    if ( (0) && KOMODO_NSPV_SUPERLITE )
    {
        char coinaddr[64]; struct CCcontract_info *cp,C; CTxOut v;
        cp = CCinit(&C,EVAL_FAUCET);
        v = MakeCC1vout(EVAL_FAUCET,funds,GetUnspendable(cp,0));
        Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pubkey2pk(Mypubkey()))) << OP_CHECKSIG);
        return(NSPV_spend(coinaddr,(char *)HexStr(v.scriptPubKey.begin()+1,v.scriptPubKey.end()-1).c_str(),funds));
    }
    if ( ensure_CCrequirements(EVAL_FAUCET) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);

    //const CKeyStore& keystore = *pwalletMain;
    //LOCK2(cs_main, pwalletMain->cs_wallet);

    bool lockWallet = false;
    if (!mypk.IsValid())   // if mypk is not set then it is a local call, use local wallet in AddNormalInputs
        lockWallet = true;

    if (funds > 0) 
    {
        if (lockWallet)
        {
            ENTER_CRITICAL_SECTION(cs_main);
            ENTER_CRITICAL_SECTION(pwalletMain->cs_wallet);
        }
        result = FaucetFund(mypk, 0,(uint64_t) funds);
        if (lockWallet)
        {
            LEAVE_CRITICAL_SECTION(pwalletMain->cs_wallet);
            LEAVE_CRITICAL_SECTION(cs_main);
        }

        if ( result[JSON_HEXTX].getValStr().size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            //result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt create faucet funding transaction");
    } else ERR_RESULT( "funding amount must be positive");
    return(result);
}

UniValue faucetget(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex;
    if ( fHelp || params.size() !=0 )
        throw runtime_error("faucetget\n");
    if ( ensure_CCrequirements(EVAL_FAUCET) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);

    bool lockWallet = false;
    if (!mypk.IsValid())   // if mypk is not set then it is a local call, use wallet in AddNormalInputs (see check for this there)
        lockWallet = true;

    //const CKeyStore& keystore = *pwalletMain;
    //LOCK2(cs_main, pwalletMain->cs_wallet);

    if (lockWallet)
    {
        // use this instead LOCK2 because we need conditional wallet lock
        ENTER_CRITICAL_SECTION(cs_main);
        ENTER_CRITICAL_SECTION(pwalletMain->cs_wallet);
    }
    result = FaucetGet(mypk, 0);
    if (lockWallet)
    {
        LEAVE_CRITICAL_SECTION(pwalletMain->cs_wallet);
        LEAVE_CRITICAL_SECTION(cs_main);
    }

    if (result[JSON_HEXTX].getValStr().size() > 0 ) {
        result.push_back(Pair("result", "success"));
        //result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt create faucet get transaction");
    return(result);
}

uint32_t pricesGetParam(UniValue param) {
    uint32_t filter = 0;
    if (STR_TOLOWER(param.get_str()) == "all")
        filter = 0;
    else if (STR_TOLOWER(param.get_str()) == "open")
        filter = 1;
    else if (STR_TOLOWER(param.get_str()) == "closed")
        filter = 2;
    else
        throw runtime_error("incorrect parameter\n");
    return filter;
}

UniValue priceslist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if ( fHelp || params.size() != 0 && params.size() != 1)
        throw runtime_error("priceslist [all|open|closed]\n");
    if ( ensure_CCrequirements(EVAL_PRICES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    uint32_t filter = 0;
    if (params.size() == 1) 
        filter = pricesGetParam(params[0]);
    
    CPubKey emptypk;

    return(PricesList(filter, emptypk));
}

UniValue mypriceslist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 0 && params.size() != 1)
        throw runtime_error("mypriceslist [all|open|closed]\n");
    if (ensure_CCrequirements(EVAL_PRICES) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

    uint32_t filter = 0;
    if (params.size() == 1)
        filter = pricesGetParam(params[0]);
    CPubKey pk;
    if (mypk.IsValid()) pk=mypk;
    else pk = pubkey2pk(Mypubkey());

    return(PricesList(filter, pk));
}

UniValue pricesinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bettxid; int32_t height;
    if ( fHelp || params.size() != 1 && params.size() != 2)
        throw runtime_error("pricesinfo bettxid [height]\n");
    if ( ensure_CCrequirements(EVAL_PRICES) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bettxid = Parseuint256((char *)params[0].get_str().c_str());
    height = 0;
    if (params.size() == 2)
        height = atoi(params[1].get_str().c_str());
    return(PricesInfo(bettxid, height));
}

UniValue dicefund(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t funds,minbet,maxbet,maxodds,timeoutblocks; std::string hex; char *name;
    if ( fHelp || params.size() != 6 )
        throw runtime_error("dicefund name funds minbet maxbet maxodds timeoutblocks\n");
    if ( ensure_CCrequirements(EVAL_DICE) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = (char *)params[0].get_str().c_str();
    funds = atof(params[1].get_str().c_str()) * COIN + 0.00000000499999;
    minbet = atof(params[2].get_str().c_str()) * COIN + 0.00000000499999;
    maxbet = atof(params[3].get_str().c_str()) * COIN + 0.00000000499999;
    maxodds = atol(params[4].get_str().c_str());
    timeoutblocks = atol(params[5].get_str().c_str());

    if (!VALID_PLAN_NAME(name)) {
        ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
        return(result);
    }

    hex = DiceCreateFunding(0,name,funds,minbet,maxbet,maxodds,timeoutblocks);
    if (CCerror != "") {
        ERR_RESULT(CCerror);
    } else if ( hex.size() > 0 ) {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else  {
        ERR_RESULT( "couldnt create dice funding transaction");
    }
    return(result);
}

UniValue diceaddfunds(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); char *name; uint256 fundingtxid; int64_t amount; std::string hex;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("diceaddfunds name fundingtxid amount\n");
    if ( ensure_CCrequirements(EVAL_DICE) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = (char *)params[0].get_str().c_str();
    fundingtxid = Parseuint256((char *)params[1].get_str().c_str());
    amount = atof(params[2].get_str().c_str()) * COIN + 0.00000000499999;
    if (!VALID_PLAN_NAME(name)) {
        ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
        return(result);
    }
    if ( amount > 0 ) {
        hex = DiceAddfunding(0,name,fundingtxid,amount);
        if (CCerror != "") {
            ERR_RESULT(CCerror);
        } else if ( hex.size() > 0 ) {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt create dice addfunding transaction");
    } else ERR_RESULT("amount must be positive");
    return(result);
}

UniValue dicebet(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex,error; uint256 fundingtxid; int64_t amount,odds; char *name;
    if ( fHelp || params.size() != 4 )
        throw runtime_error("dicebet name fundingtxid amount odds\n");
    if ( ensure_CCrequirements(EVAL_DICE) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = (char *)params[0].get_str().c_str();
    fundingtxid = Parseuint256((char *)params[1].get_str().c_str());
    amount = atof(params[2].get_str().c_str()) * COIN + 0.00000000499999;
    odds = atol(params[3].get_str().c_str());

    if (!VALID_PLAN_NAME(name)) {
        ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
        return(result);
    }
    if (amount > 0 && odds > 0) {
        hex = DiceBet(0,name,fundingtxid,amount,odds);
        RETURN_IF_ERROR(CCerror);
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        }
    } else {
        ERR_RESULT("amount and odds must be positive");
    }
    return(result);
}

UniValue dicefinish(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint8_t funcid; char *name; uint256 entropyused,fundingtxid,bettxid; std::string hex; int32_t r,entropyvout;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("dicefinish name fundingtxid bettxid\n");
    if ( ensure_CCrequirements(EVAL_DICE) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = (char *)params[0].get_str().c_str();
    if (!VALID_PLAN_NAME(name)) {
        ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
        return(result);
    }
    fundingtxid = Parseuint256((char *)params[1].get_str().c_str());
    bettxid = Parseuint256((char *)params[2].get_str().c_str());
    hex = DiceBetFinish(funcid,entropyused,entropyvout,&r,0,name,fundingtxid,bettxid,1,zeroid,-1);
    if ( CCerror != "" )
    {
        ERR_RESULT(CCerror);
    } else if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
        if ( funcid != 0 )
        {
            char funcidstr[2];
            funcidstr[0] = funcid;
            funcidstr[1] = 0;
            result.push_back(Pair("funcid", funcidstr));
        }
    } else ERR_RESULT( "couldnt create dicefinish transaction");
    return(result);
}

UniValue dicestatus(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); char *name; uint256 fundingtxid,bettxid; std::string status,error; double winnings;
    if ( fHelp || (params.size() != 2 && params.size() != 3) )
        throw runtime_error("dicestatus name fundingtxid bettxid\n");
    if ( ensure_CCrequirements(EVAL_DICE) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    name = (char *)params[0].get_str().c_str();
    if (!VALID_PLAN_NAME(name)) {
        ERR_RESULT(strprintf("Plan name can be at most %d ASCII characters",PLAN_NAME_MAX));
        return(result);
    }
    fundingtxid = Parseuint256((char *)params[1].get_str().c_str());
    memset(&bettxid,0,sizeof(bettxid));
    if ( params.size() == 3 )
        bettxid = Parseuint256((char *)params[2].get_str().c_str());
    winnings = DiceStatus(0,name,fundingtxid,bettxid);
    RETURN_IF_ERROR(CCerror);

    result.push_back(Pair("result", "success"));
    if ( winnings >= 0. )
    {
        if ( winnings > 0. )
        {
            if ( params.size() == 3 )
            {
                int64_t val;
                val = winnings * COIN + 0.00000000499999;
                result.push_back(Pair("status", "win"));
                result.push_back(Pair("won", ValueFromAmount(val)));
            }
            else
            {
                result.push_back(Pair("status", "finalized"));
                result.push_back(Pair("n", (int64_t)winnings));
            }
        }
        else
        {
            if ( params.size() == 3 )
                result.push_back(Pair("status", "loss"));
            else result.push_back(Pair("status", "no pending bets"));
        }
    } else result.push_back(Pair("status", "bet still pending"));
    return(result);
}

UniValue dicelist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if ( fHelp || params.size() > 0 )
        throw runtime_error("dicelist\n");
    if ( ensure_CCrequirements(EVAL_DICE) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    return(DiceList());
}

UniValue diceinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 fundingtxid;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("diceinfo fundingtxid\n");
    if ( ensure_CCrequirements(EVAL_DICE) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    fundingtxid = Parseuint256((char *)params[0].get_str().c_str());
    return(DiceInfo(fundingtxid));
}

UniValue tokenlist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    if ( fHelp || params.size() > 0 )
        throw runtime_error("tokenlist\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    return(TokenList());
}

UniValue tokeninfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("tokeninfo tokenid\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    return(TokenInfo(tokenid));
}

UniValue tokenorders(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    if ( fHelp || params.size() > 1 )
        throw runtime_error("tokenorders [tokenid]\n"
                            "returns token orders for the tokenid or all available token orders if tokenid is not set\n"
                            "(this rpc supports only fungible tokens)\n" "\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
	if (params.size() == 1) {
		tokenid = Parseuint256((char *)params[0].get_str().c_str());
		if (tokenid == zeroid) 
			throw runtime_error("incorrect tokenid\n");
        return AssetOrders(tokenid, CPubKey(), 0);
	}
    else {
        // throw runtime_error("no tokenid\n");
        return AssetOrders(zeroid, CPubKey(), 0);
    }
}


UniValue mytokenorders(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    if (fHelp || params.size() > 1)
        throw runtime_error("mytokenorders [evalcode]\n"
                            "returns all the token orders for mypubkey\n"
                            "if evalcode is set then returns mypubkey token orders for non-fungible tokens with this evalcode\n" "\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    uint8_t additionalEvalCode = 0;
    if (params.size() == 1)
        additionalEvalCode = strtol(params[0].get_str().c_str(), NULL, 0);  // supports also 0xEE-like values

    return AssetOrders(zeroid, Mypubkey(), additionalEvalCode);
}

UniValue tokenbalance(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 tokenid; uint64_t balance; std::vector<unsigned char> pubkey; struct CCcontract_info *cp,C;
	CCerror.clear();

    if ( fHelp || params.size() > 2 )
        throw runtime_error("tokenbalance tokenid [pubkey]\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
	LOCK(cs_main);

    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    if ( params.size() == 2 )
        pubkey = ParseHex(params[1].get_str().c_str());
    else 
		pubkey = Mypubkey();

    balance = GetTokenBalance(pubkey2pk(pubkey),tokenid);

	if (CCerror.empty()) {
		char destaddr[64];

		result.push_back(Pair("result", "success"));
        cp = CCinit(&C,EVAL_TOKENS);
		if (GetCCaddress(cp, destaddr, pubkey2pk(pubkey)) != 0)
			result.push_back(Pair("CCaddress", destaddr));

		result.push_back(Pair("tokenid", params[0].get_str()));
		result.push_back(Pair("balance", (int64_t)balance));
	}
	else {
		ERR_RESULT(CCerror);
	}

    return(result);
}

UniValue tokencreate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    std::string name, description, hextx; 
    std::vector<uint8_t> nonfungibleData;
    int64_t supply; // changed from uin64_t to int64_t for this 'if ( supply <= 0 )' to work as expected

    CCerror.clear();

    if ( fHelp || params.size() > 4 || params.size() < 2 )
        throw runtime_error("tokencreate name supply [description][data]\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    name = params[0].get_str();
    if (name.size() == 0 || name.size() > 32)   {
        ERR_RESULT("Token name must not be empty and up to 32 characters");
        return(result);
    }

    supply = atof(params[1].get_str().c_str()) * COIN + 0.00000000499999;   // what for is this '+0.00000000499999'? it will be lost while converting double to int64_t (dimxy)
    if (supply <= 0)    {
        ERR_RESULT("Token supply must be positive");
        return(result);
    }
    
    if (params.size() >= 3)     {
        description = params[2].get_str();
        if (description.size() > 4096)   {
            ERR_RESULT("Token description must be <= 4096 characters");
            return(result);
        }
    }
    
    if (params.size() == 4)    {
        nonfungibleData = ParseHex(params[3].get_str());
        if (nonfungibleData.size() > IGUANA_MAXSCRIPTSIZE) // opret limit
        {
            ERR_RESULT("Non-fungible data size must be <= " + std::to_string(IGUANA_MAXSCRIPTSIZE));
            return(result);
        }
        if( nonfungibleData.empty() ) {
            ERR_RESULT("Non-fungible data incorrect");
            return(result);
        }
    }

    hextx = CreateToken(0, supply, name, description, nonfungibleData);
    if( hextx.size() > 0 )     {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hextx));
    } 
    else 
        ERR_RESULT(CCerror);
    return(result);
}

UniValue tokentransfer(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); 
    std::string hex; 
    int64_t amount; 
    uint256 tokenid;
    
    CCerror.clear();

    if ( fHelp || params.size() != 3)
        throw runtime_error("tokentransfer tokenid destpubkey amount\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    std::vector<unsigned char> pubkey(ParseHex(params[1].get_str().c_str()));
    //amount = atol(params[2].get_str().c_str());
	amount = atoll(params[2].get_str().c_str()); // dimxy changed to prevent loss of significance
    if( tokenid == zeroid )    {
        ERR_RESULT("invalid tokenid");
        return(result);
    }
    if( amount <= 0 )    {
        ERR_RESULT("amount must be positive");
        return(result);
    }

    hex = TokenTransfer(0, tokenid, pubkey, amount);

    if( !CCerror.empty() )   {
        ERR_RESULT(CCerror);
    }
    else {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    }
    return(result);
}

UniValue tokenconvert(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex; int32_t evalcode; int64_t amount; uint256 tokenid;
    if ( fHelp || params.size() != 4 )
        throw runtime_error("tokenconvert evalcode tokenid pubkey amount\n");
    if ( ensure_CCrequirements(EVAL_ASSETS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    evalcode = atoi(params[0].get_str().c_str());
    tokenid = Parseuint256((char *)params[1].get_str().c_str());
    std::vector<unsigned char> pubkey(ParseHex(params[2].get_str().c_str()));
    //amount = atol(params[3].get_str().c_str());
	amount = atoll(params[3].get_str().c_str()); // dimxy changed to prevent loss of significance
    if ( tokenid == zeroid )
    {
        ERR_RESULT("invalid tokenid");
        return(result);
    }
    if ( amount <= 0 )
    {
        ERR_RESULT("amount must be positive");
        return(result);
    }

	ERR_RESULT("deprecated");
	return(result);

/*    hex = AssetConvert(0,tokenid,pubkey,amount,evalcode);
    if (amount > 0) {
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt convert tokens");
    } else {
        ERR_RESULT("amount must be positive");
    }
    return(result); */
}

UniValue tokenbid(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t bidamount,numtokens; std::string hex; double price; uint256 tokenid;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("tokenbid numtokens tokenid price\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    //numtokens = atoi(params[0].get_str().c_str());
	numtokens = atoll(params[0].get_str().c_str());  // dimxy changed to prevent loss of significance
    tokenid = Parseuint256((char *)params[1].get_str().c_str());
    price = atof(params[2].get_str().c_str());
    bidamount = (price * numtokens) * COIN + 0.0000000049999;
    if ( price <= 0 )
    {
        ERR_RESULT("price must be positive");
        return(result);
    }
    if ( tokenid == zeroid )
    {
        ERR_RESULT("invalid tokenid");
        return(result);
    }
    if ( bidamount <= 0 )
    {
        ERR_RESULT("bid amount must be positive");
        return(result);
    }
    hex = CreateBuyOffer(0,bidamount,tokenid,numtokens);
    if (price > 0 && numtokens > 0) {
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt create bid");
    } else {
        ERR_RESULT("price and numtokens must be positive");
    }
    return(result);
}

UniValue tokencancelbid(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex; int32_t i; uint256 tokenid,bidtxid;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("tokencancelbid tokenid bidtxid\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    bidtxid = Parseuint256((char *)params[1].get_str().c_str());
    if ( tokenid == zeroid || bidtxid == zeroid )
    {
        result.push_back(Pair("error", "invalid parameter"));
        return(result);
    }
    hex = CancelBuyOffer(0,tokenid,bidtxid);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt cancel bid");
    return(result);
}

UniValue tokenfillbid(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t fillamount; std::string hex; uint256 tokenid,bidtxid;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("tokenfillbid tokenid bidtxid fillamount\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    bidtxid = Parseuint256((char *)params[1].get_str().c_str());
    // fillamount = atol(params[2].get_str().c_str());
	fillamount = atoll(params[2].get_str().c_str());		// dimxy changed to prevent loss of significance
    if ( fillamount <= 0 )
    {
        ERR_RESULT("fillamount must be positive");
        return(result);
    }
    if ( tokenid == zeroid || bidtxid == zeroid )
    {
        ERR_RESULT("must provide tokenid and bidtxid");
        return(result);
    }
    hex = FillBuyOffer(0,tokenid,bidtxid,fillamount);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt fill bid");
    return(result);
}

UniValue tokenask(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t askamount,numtokens; std::string hex; double price; uint256 tokenid;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("tokenask numtokens tokenid price\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    //numtokens = atoi(params[0].get_str().c_str());
	numtokens = atoll(params[0].get_str().c_str());			// dimxy changed to prevent loss of significance
    tokenid = Parseuint256((char *)params[1].get_str().c_str());
    price = atof(params[2].get_str().c_str());
    askamount = (price * numtokens) * COIN + 0.0000000049999;
	//std::cerr << std::boolalpha << "tokenask(): (tokenid == zeroid) is "  << (tokenid == zeroid) << " (numtokens <= 0) is " << (numtokens <= 0) << " (price <= 0) is " << (price <= 0) << " (askamount <= 0) is " << (askamount <= 0) << std::endl;
    if ( tokenid == zeroid || numtokens <= 0 || price <= 0 || askamount <= 0 )
    {
        ERR_RESULT("invalid parameter");
        return(result);
    }
    hex = CreateSell(0,numtokens,tokenid,askamount);
    if (price > 0 && numtokens > 0) {
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt create ask");
    } else {
        ERR_RESULT("price and numtokens must be positive");
    }
    return(result);
}

UniValue tokenswapask(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    static uint256 zeroid;
    UniValue result(UniValue::VOBJ); int64_t askamount,numtokens; std::string hex; double price; uint256 tokenid,otherid;
    if ( fHelp || params.size() != 4 )
        throw runtime_error("tokenswapask numtokens tokenid otherid price\n");
    if ( ensure_CCrequirements(EVAL_ASSETS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    //numtokens = atoi(params[0].get_str().c_str());
	numtokens = atoll(params[0].get_str().c_str());			// dimxy changed to prevent loss of significance
    tokenid = Parseuint256((char *)params[1].get_str().c_str());
    otherid = Parseuint256((char *)params[2].get_str().c_str());
    price = atof(params[3].get_str().c_str());
    askamount = (price * numtokens);
    hex = CreateSwap(0,numtokens,tokenid,otherid,askamount);
    if (price > 0 && numtokens > 0) {
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt create swap");
    } else {
        ERR_RESULT("price and numtokens must be positive");
    }
    return(result);
}

UniValue tokencancelask(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex; int32_t i; uint256 tokenid,asktxid;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("tokencancelask tokenid asktxid\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    asktxid = Parseuint256((char *)params[1].get_str().c_str());
    if ( tokenid == zeroid || asktxid == zeroid )
    {
        result.push_back(Pair("error", "invalid parameter"));
        return(result);
    }
    hex = CancelSell(0,tokenid,asktxid);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt cancel ask");
    return(result);
}

UniValue tokenfillask(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t fillunits; std::string hex; uint256 tokenid,asktxid;
    if ( fHelp || params.size() != 3 )
        throw runtime_error("tokenfillask tokenid asktxid fillunits\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    asktxid = Parseuint256((char *)params[1].get_str().c_str());
    //fillunits = atol(params[2].get_str().c_str());
	fillunits = atoll(params[2].get_str().c_str());	 // dimxy changed to prevent loss of significance
    if ( fillunits <= 0 )
    {
        ERR_RESULT("fillunits must be positive");
        return(result);
    }
    if ( tokenid == zeroid || asktxid == zeroid )
    {
        result.push_back(Pair("error", "invalid parameter"));
        return(result);
    }
    hex = FillSell(0,tokenid,zeroid,asktxid,fillunits);
    if (fillunits > 0) {
        if (CCerror != "") {
            ERR_RESULT(CCerror);
        } else if ( hex.size() > 0) {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else {
            ERR_RESULT("couldnt fill ask");
        }
    } else {
        ERR_RESULT("fillunits must be positive");
    }
    return(result);
}

UniValue tokenfillswap(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    static uint256 zeroid;
    UniValue result(UniValue::VOBJ); int64_t fillunits; std::string hex; uint256 tokenid,otherid,asktxid;
    if ( fHelp || params.size() != 4 )
        throw runtime_error("tokenfillswap tokenid otherid asktxid fillunits\n");
    if ( ensure_CCrequirements(EVAL_ASSETS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    otherid = Parseuint256((char *)params[1].get_str().c_str());
    asktxid = Parseuint256((char *)params[2].get_str().c_str());
    //fillunits = atol(params[3].get_str().c_str());
	fillunits = atoll(params[3].get_str().c_str());  // dimxy changed to prevent loss of significance
    hex = FillSell(0,tokenid,otherid,asktxid,fillunits);
    if (fillunits > 0) {
        if ( hex.size() > 0 ) {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt fill bid");
    } else {
        ERR_RESULT("fillunits must be positive");
    }
    return(result);
}

UniValue getbalance64(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    set<CBitcoinAddress> setAddress; vector<COutput> vecOutputs;
    UniValue ret(UniValue::VOBJ); UniValue a(UniValue::VARR),b(UniValue::VARR); CTxDestination address;
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    const CKeyStore& keystore = *pwalletMain;
    CAmount nValues[64],nValues2[64],nValue,total,total2; int32_t i,segid;
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    if (params.size() > 0)
        throw runtime_error("getbalance64\n");
    total = total2 = 0;
    memset(nValues,0,sizeof(nValues));
    memset(nValues2,0,sizeof(nValues2));
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    BOOST_FOREACH(const COutput& out, vecOutputs)
    {
        nValue = out.tx->vout[out.i].nValue;
        if ( ExtractDestination(out.tx->vout[out.i].scriptPubKey, address) )
        {
            segid = (komodo_segid32((char *)CBitcoinAddress(address).ToString().c_str()) & 0x3f);
            if ( out.nDepth < 100 )
                nValues2[segid] += nValue, total2 += nValue;
            else nValues[segid] += nValue, total += nValue;
            //fprintf(stderr,"%s %.8f depth.%d segid.%d\n",(char *)CBitcoinAddress(address).ToString().c_str(),(double)nValue/COIN,(int32_t)out.nDepth,segid);
        } else fprintf(stderr,"no destination\n");
    }
    ret.push_back(Pair("mature",(double)total/COIN));
    ret.push_back(Pair("immature",(double)total2/COIN));
    for (i=0; i<64; i++)
    {
        a.push_back((uint64_t)nValues[i]);
        b.push_back((uint64_t)nValues2[i]);
    }
    ret.push_back(Pair("staking", a));
    ret.push_back(Pair("notstaking", b));
    return ret;
}


// heir contract functions for coins and tokens
UniValue heirfund(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
	UniValue result(UniValue::VOBJ);
	uint256 tokenid = zeroid;
	int64_t amount;
	int64_t inactivitytime;
	std::string hex;
	std::vector<unsigned char> pubkey;
	std::string name, memo;

	if (!EnsureWalletIsAvailable(fHelp))
	    return NullUniValue;

	if (fHelp || params.size() != 5 && params.size() != 6)
		throw runtime_error("heirfund funds heirname heirpubkey inactivitytime memo [tokenid]\n");
	if (ensure_CCrequirements(EVAL_HEIR) < 0)
		throw runtime_error(CC_REQUIREMENTS_MSG);

	const CKeyStore& keystore = *pwalletMain;
	LOCK2(cs_main, pwalletMain->cs_wallet);

	if (params.size() == 6)	// tokens in satoshis:
		amount = atoll(params[0].get_str().c_str());
    	else { // coins:
        	amount = 0;   
        	if (!ParseFixedPoint(params[0].get_str(), 8, &amount))  // using ParseFixedPoint instead atof to avoid small round errors
            		amount = -1; // set error
    	}
	if (amount <= 0) {
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "incorrect amount"));
		return result;
	}

	name = params[1].get_str();

	pubkey = ParseHex(params[2].get_str().c_str());
	if (!pubkey2pk(pubkey).IsValid()) {
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "incorrect pubkey"));
		return result;
	}

	inactivitytime = atoll(params[3].get_str().c_str());
	if (inactivitytime <= 0) {
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "incorrect inactivity time"));
		return result;
	}

	memo = params[4].get_str();

	if (params.size() == 6) {
		tokenid = Parseuint256((char*)params[5].get_str().c_str());
		if (tokenid == zeroid) {
			result.push_back(Pair("result", "error"));
			result.push_back(Pair("error", "incorrect tokenid"));
			return result;
		}
	}

	if( tokenid == zeroid )
		result = HeirFundCoinCaller(0, amount, name, pubkey2pk(pubkey), inactivitytime, memo);
	else
		result = HeirFundTokenCaller(0, amount, name, pubkey2pk(pubkey), inactivitytime, memo, tokenid);

	return result;
}

UniValue heiradd(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
	UniValue result; 
	uint256 fundingtxid;
	int64_t amount;
	int64_t inactivitytime;
	std::string hex;
	std::vector<unsigned char> pubkey;
	std::string name;

	if (!EnsureWalletIsAvailable(fHelp))
	    return NullUniValue;

	if (fHelp || params.size() != 2)
		throw runtime_error("heiradd funds fundingtxid\n");
	if (ensure_CCrequirements(EVAL_HEIR) < 0)
		throw runtime_error(CC_REQUIREMENTS_MSG);

	const CKeyStore& keystore = *pwalletMain;
	LOCK2(cs_main, pwalletMain->cs_wallet);

	std::string strAmount = params[0].get_str();
	fundingtxid = Parseuint256((char*)params[1].get_str().c_str());

	result = HeirAddCaller(fundingtxid, 0, strAmount);
	return result;
}

UniValue heirclaim(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
	UniValue result; uint256 fundingtxid;

	if (!EnsureWalletIsAvailable(fHelp))
	    return NullUniValue;
	if (fHelp || params.size() != 2)
		throw runtime_error("heirclaim funds fundingtxid\n");
	if (ensure_CCrequirements(EVAL_HEIR) < 0)
		throw runtime_error(CC_REQUIREMENTS_MSG);

	const CKeyStore& keystore = *pwalletMain;
	LOCK2(cs_main, pwalletMain->cs_wallet);

    	std::string strAmount = params[0].get_str();
	fundingtxid = Parseuint256((char*)params[1].get_str().c_str());
	result = HeirClaimCaller(fundingtxid, 0, strAmount);
	return result;
}

UniValue heirinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
	uint256 fundingtxid;
	if (fHelp || params.size() != 1) 
		throw runtime_error("heirinfo fundingtxid\n");
    if ( ensure_CCrequirements(EVAL_HEIR) < 0 )
	    throw runtime_error(CC_REQUIREMENTS_MSG);
	fundingtxid = Parseuint256((char*)params[0].get_str().c_str());
	return (HeirInfo(fundingtxid));
}

UniValue heirlist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
	if (fHelp || params.size() != 0) 
		throw runtime_error("heirlist\n");
    if ( ensure_CCrequirements(EVAL_HEIR) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
	return (HeirList());
}

UniValue pegscreate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int32_t i; std::vector<uint256> txids;
    uint8_t N; uint256 txid; int64_t amount;

    if ( fHelp || params.size()<3)
        throw runtime_error("pegscreate amount N bindtxid1 [bindtxid2 ...]\n");
    if ( ensure_CCrequirements(EVAL_PEGS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    Lock2NSPV(mypk);
    amount = atof((char *)params[0].get_str().c_str()) * COIN + 0.00000000499999;
    N = atoi((char *)params[1].get_str().c_str());
    if ( params.size() < N+1 )
    {
        Unlock2NSPV(mypk);
        throw runtime_error("not enough parameters for N pegscreate\n");
    }
    for (i=0; i<N; i++)
    {       
        txid = Parseuint256(params[i+2].get_str().c_str());
        txids.push_back(txid);
    }
    result = PegsCreate(mypk,0,amount,txids);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue pegsfund(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 pegstxid,tokenid; int64_t amount;


    if ( fHelp || params.size()!=3)
        throw runtime_error("pegsfund pegstxid tokenid amount\n");
    if ( ensure_CCrequirements(EVAL_PEGS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    Lock2NSPV(mypk);
    pegstxid = Parseuint256(params[0].get_str().c_str());
    tokenid = Parseuint256(params[1].get_str().c_str());
    amount = atof((char *)params[2].get_str().c_str()) * COIN + 0.00000000499999;
    result = PegsFund(mypk,0,pegstxid,tokenid,amount);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue pegsget(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 pegstxid,tokenid; int64_t amount;

    if ( fHelp || params.size()!=3)
        throw runtime_error("pegsget pegstxid tokenid amount\n");
    if ( ensure_CCrequirements(EVAL_PEGS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    pegstxid = Parseuint256(params[0].get_str().c_str());
    tokenid = Parseuint256(params[1].get_str().c_str());
    amount = atof((char *)params[2].get_str().c_str()) * COIN + 0.00000000499999;
    result = PegsGet(mypk,0,pegstxid,tokenid,amount);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue pegsredeem(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 pegstxid,tokenid; int64_t amount;

    if ( fHelp || params.size()!=2)
        throw runtime_error("pegsredeem pegstxid tokenid\n");
    if ( ensure_CCrequirements(EVAL_PEGS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    pegstxid = Parseuint256(params[0].get_str().c_str());
    tokenid = Parseuint256(params[1].get_str().c_str());
    result = PegsRedeem(mypk,0,pegstxid,tokenid);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue pegsliquidate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 pegstxid,tokenid,accounttxid;

    if ( fHelp || params.size()!=3)
        throw runtime_error("pegsliquidate pegstxid tokenid accounttxid\n");
    if ( ensure_CCrequirements(EVAL_PEGS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    pegstxid = Parseuint256(params[0].get_str().c_str());
    tokenid = Parseuint256(params[1].get_str().c_str());
    accounttxid = Parseuint256(params[2].get_str().c_str());
    result = PegsLiquidate(mypk,0,pegstxid,tokenid,accounttxid);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue pegsexchange(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 pegstxid,tokenid,accounttxid; int64_t amount;

    if ( fHelp || params.size()!=3)
        throw runtime_error("pegsexchange pegstxid tokenid amount\n");
    if ( ensure_CCrequirements(EVAL_PEGS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    Lock2NSPV(mypk);
    pegstxid = Parseuint256(params[0].get_str().c_str());
    tokenid = Parseuint256(params[1].get_str().c_str());
    amount = atof((char *)params[2].get_str().c_str()) * COIN + 0.00000000499999;
    result = PegsExchange(mypk,0,pegstxid,tokenid,amount);
    if ( result[JSON_HEXTX].getValStr().size() > 0  )
    {
        result.push_back(Pair("result", "success"));
    }
    Unlock2NSPV(mypk);
    return(result);
}

UniValue pegsaccounthistory(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 pegstxid;

    if ( fHelp || params.size() != 1 )
        throw runtime_error("pegsaccounthistory pegstxid\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    pegstxid = Parseuint256((char *)params[0].get_str().c_str());
    return(PegsAccountHistory(mypk,pegstxid));
}

UniValue pegsaccountinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 pegstxid;

    if ( fHelp || params.size() != 1 )
        throw runtime_error("pegsaccountinfo pegstxid\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    pegstxid = Parseuint256((char *)params[0].get_str().c_str());
    return(PegsAccountInfo(mypk,pegstxid));
}

UniValue pegsworstaccounts(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 pegstxid;

    if ( fHelp || params.size() != 1 )
        throw runtime_error("pegsworstaccounts pegstxid\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    pegstxid = Parseuint256((char *)params[0].get_str().c_str());
    return(PegsWorstAccounts(pegstxid));
}

UniValue pegsinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 pegstxid;

    if ( fHelp || params.size() != 1 )
        throw runtime_error("pegsinfo pegstxid\n");
    if ( ensure_CCrequirements(EVAL_GATEWAYS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    pegstxid = Parseuint256((char *)params[0].get_str().c_str());
    return(PegsInfo(pegstxid));
}

extern UniValue dumpprivkey(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdump.cpp
extern UniValue convertpassphrase(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importprivkey(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importaddress(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue dumpwallet(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue importwallet(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue z_exportkey(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue z_importkey(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue z_exportviewingkey(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue z_importviewingkey(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue z_exportwallet(const UniValue& params, bool fHelp, const CPubKey& mypk);
extern UniValue z_importwallet(const UniValue& params, bool fHelp, const CPubKey& mypk);

extern UniValue z_getpaymentdisclosure(const UniValue& params, bool fHelp, const CPubKey& mypk); // in rpcdisclosure.cpp
extern UniValue z_validatepaymentdisclosure(const UniValue& params, bool fHelp, const CPubKey& mypk);

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)           okSafeMode
    //  --------------------- ------------------------    -----------------------    ----------
    { "rawtransactions",    "fundrawtransaction",       &fundrawtransaction,       false },
    { "hidden",             "resendwallettransactions", &resendwallettransactions, true  },
    { "wallet",             "addmultisigaddress",       &addmultisigaddress,       true  },
    { "wallet",             "backupwallet",             &backupwallet,             true  },
    { "wallet",             "dumpprivkey",              &dumpprivkey,              true  },
    { "wallet",             "dumpwallet",               &dumpwallet,               true  },
    { "wallet",             "encryptwallet",            &encryptwallet,            true  },
    { "wallet",             "getaccountaddress",        &getaccountaddress,        true  },
    { "wallet",             "getaccount",               &getaccount,               true  },
    { "wallet",             "getaddressesbyaccount",    &getaddressesbyaccount,    true  },
    { "wallet",             "getbalance",               &getbalance,               false },
    { "wallet",             "getnewaddress",            &getnewaddress,            true  },
    { "wallet",             "getrawchangeaddress",      &getrawchangeaddress,      true  },
    { "wallet",             "getreceivedbyaccount",     &getreceivedbyaccount,     false },
    { "wallet",             "getreceivedbyaddress",     &getreceivedbyaddress,     false },
    { "wallet",             "gettransaction",           &gettransaction,           false },
    { "wallet",             "getunconfirmedbalance",    &getunconfirmedbalance,    false },
    { "wallet",             "getwalletinfo",            &getwalletinfo,            false },
    { "wallet",             "convertpassphrase",        &convertpassphrase,        true  },
    { "wallet",             "importprivkey",            &importprivkey,            true  },
    { "wallet",             "importwallet",             &importwallet,             true  },
    { "wallet",             "importaddress",            &importaddress,            true  },
    { "wallet",             "keypoolrefill",            &keypoolrefill,            true  },
    { "wallet",             "listaccounts",             &listaccounts,             false },
    { "wallet",             "listaddressgroupings",     &listaddressgroupings,     false },
    { "wallet",             "listlockunspent",          &listlockunspent,          false },
    { "wallet",             "listreceivedbyaccount",    &listreceivedbyaccount,    false },
    { "wallet",             "listreceivedbyaddress",    &listreceivedbyaddress,    false },
    { "wallet",             "listsinceblock",           &listsinceblock,           false },
    { "wallet",             "listtransactions",         &listtransactions,         false },
    { "wallet",             "listunspent",              &listunspent,              false },
    { "wallet",             "lockunspent",              &lockunspent,              true  },
    { "wallet",             "move",                     &movecmd,                  false },
    { "wallet",             "sendfrom",                 &sendfrom,                 false },
    { "wallet",             "sendmany",                 &sendmany,                 false },
    { "wallet",             "sendtoaddress",            &sendtoaddress,            false },
    { "wallet",             "setaccount",               &setaccount,               true  },
    { "wallet",             "settxfee",                 &settxfee,                 true  },
    { "wallet",             "signmessage",              &signmessage,              true  },
    { "wallet",             "walletlock",               &walletlock,               true  },
    { "wallet",             "walletpassphrasechange",   &walletpassphrasechange,   true  },
    { "wallet",             "walletpassphrase",         &walletpassphrase,         true  },
    { "wallet",             "zcbenchmark",              &zc_benchmark,             true  },
    { "wallet",             "zcrawkeygen",              &zc_raw_keygen,            true  },
    { "wallet",             "zcrawjoinsplit",           &zc_raw_joinsplit,         true  },
    { "wallet",             "zcrawreceive",             &zc_raw_receive,           true  },
    { "wallet",             "zcsamplejoinsplit",        &zc_sample_joinsplit,      true  },
    { "wallet",             "z_listreceivedbyaddress",  &z_listreceivedbyaddress,  false },
    { "wallet",             "z_listunspent",            &z_listunspent,            false },
    { "wallet",             "z_getbalance",             &z_getbalance,             false },
    { "wallet",             "z_gettotalbalance",        &z_gettotalbalance,        false },
    { "wallet",             "z_mergetoaddress",         &z_mergetoaddress,         false },
    { "wallet",             "z_sendmany",               &z_sendmany,               false },
    { "wallet",             "z_shieldcoinbase",         &z_shieldcoinbase,         false },
    { "wallet",             "z_getoperationstatus",     &z_getoperationstatus,     true  },
    { "wallet",             "z_getoperationresult",     &z_getoperationresult,     true  },
    { "wallet",             "z_listoperationids",       &z_listoperationids,       true  },
    { "wallet",             "z_getnewaddress",          &z_getnewaddress,          true  },
    { "wallet",             "z_listaddresses",          &z_listaddresses,          true  },
    { "wallet",             "z_exportkey",              &z_exportkey,              true  },
    { "wallet",             "z_importkey",              &z_importkey,              true  },
    { "wallet",             "z_exportviewingkey",       &z_exportviewingkey,       true  },
    { "wallet",             "z_importviewingkey",       &z_importviewingkey,       true  },
    { "wallet",             "z_exportwallet",           &z_exportwallet,           true  },
    { "wallet",             "z_importwallet",           &z_importwallet,           true  },
    { "wallet",             "z_viewtransaction",        &z_viewtransaction,        true  },
    // TODO: rearrange into another category
    { "disclosure",         "z_getpaymentdisclosure",   &z_getpaymentdisclosure,   true  },
    { "disclosure",         "z_validatepaymentdisclosure", &z_validatepaymentdisclosure, true }
};

void RegisterWalletRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

UniValue opreturn_burn(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    std::vector<uint8_t> vHexStr; CScript opret; int32_t txfee = 10000;CPubKey myPubkey;
    if (fHelp || (params.size() < 2) || (params.size() > 4) )
    {
        throw runtime_error(
        "opreturn_burn burn_amount hexstring ( txfee )\n"
        "\nBurn the specified amount of coins via OP_RETURN. Returns unsigned transaction raw hex that must then be signed via signrawtransaction and broadcast via sendrawtransaction rpc\n"
        "\nArguments:\n"
        "1. \"burn_amount\"       (numeric, required) Amount of coins to burn.\n"
        "2. \"hexstring\"         (string, required) Hex string to include in OP_RETURN data.\n"
        "3. \"txfee\"             (numeric, optional, default=0.0001) Transaction fee.\n"
        "\nResult:\n"
        "  {\n"
        "    \"hex\" : \"hexstring\",     (string) raw hex of transaction \n"
        "  }\n"
        "\nExamples:\n"
        "\nBurn 10 coins with OP_RETURN data \"deadbeef\"\n"
        + HelpExampleCli("opreturn_burn", "\"10\" \"deadbeef\"") 
        + HelpExampleRpc("opreturn_burn", "\"10\", \"deadbeef\"") +
        "\nBurn 10 coins with OP_RETURN data \"deadbeef\" with 0.00005 txfee\n"
        + HelpExampleCli("opreturn_burn", "\"10\" \"deadbeef\" \"0.00005\"") 
        + HelpExampleRpc("opreturn_burn", "\"10\", \"deadbeef\", 0.00005")
      );
    }
    UniValue ret(UniValue::VOBJ);

	CAmount nAmount = AmountFromValue(params[0]);
    vHexStr = ParseHex(params[1].get_str());
    if ( vHexStr.size() == 0 )
        throw JSONRPCError(RPC_TYPE_ERROR, "hexstring is not valid.");

    if ( params.size() > 2 )
        txfee = AmountFromValue(params[2]);

    if (!EnsureWalletIsAvailable(fHelp))
        throw JSONRPCError(RPC_TYPE_ERROR, "wallet is locked or unavailable.");
    EnsureWalletIsUnlocked();
    CReserveKey reservekey(pwalletMain);
    if (!reservekey.GetReservedKey(myPubkey))
    {
        throw JSONRPCError(RPC_TYPE_ERROR, "keypool error.");
    }

	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

	int64_t normalInputs = AddNormalinputs(mtx, myPubkey, nAmount+txfee, 60);
	if (normalInputs < nAmount)
		throw runtime_error("insufficient funds\n");
    
	opret << OP_RETURN << E_MARSHAL(ss << vHexStr);
    
    mtx.vout.push_back(CTxOut(nAmount,opret));
    ret.push_back(Pair("hex", EncodeHexTx(mtx)));
	return(ret);
}
