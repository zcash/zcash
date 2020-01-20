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

#include "clientversion.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpc/server.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "notaries_staked.h"
#include "cc/eval.h"
#include "cc/CCinclude.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include <univalue.h>

#include "zcash/Address.hpp"

using namespace std;

/**
 * @note Do not add or change anything in the information returned by this
 * method. `getinfo` exists for backwards-compatibility only. It combines
 * information from wildly different sources in the program, which is a mess,
 * and is thus planned to be deprecated eventually.
 *
 * Based on the source of the information, new information should be added to:
 * - `getblockchaininfo`,
 * - `getnetworkinfo` or
 * - `getwalletinfo`
 *
 * Or alternatively, create a specific query method for the information.
 **/

int32_t Jumblr_depositaddradd(char *depositaddr);
int32_t Jumblr_secretaddradd(char *secretaddr);
uint64_t komodo_interestsum();
int32_t komodo_longestchain();
int32_t komodo_notarized_height(int32_t *prevMoMheightp,uint256 *hashp,uint256 *txidp);
bool komodo_txnotarizedconfirmed(uint256 txid);
uint32_t komodo_chainactive_timestamp();
int32_t komodo_whoami(char *pubkeystr,int32_t height,uint32_t timestamp);
extern uint64_t KOMODO_INTERESTSUM,KOMODO_WALLETBALANCE;
extern int32_t KOMODO_LASTMINED,JUMBLR_PAUSE,KOMODO_LONGESTCHAIN,IS_STAKED_NOTARY,IS_KOMODO_NOTARY,STAKED_ERA,KOMODO_INSYNC;
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
uint32_t komodo_segid32(char *coinaddr);
int64_t komodo_coinsupply(int64_t *zfundsp,int64_t *sproutfundsp,int32_t height);
int32_t notarizedtxid_height(char *dest,char *txidstr,int32_t *kmdnotarized_heightp);
int8_t StakedNotaryID(std::string &notaryname, char *Raddress);
uint64_t komodo_notarypayamount(int32_t nHeight, int64_t notarycount);
int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);

#define KOMODO_VERSION "0.5.2"
#define VERUS_VERSION "0.4.0g"
extern uint16_t ASSETCHAINS_P2PPORT,ASSETCHAINS_RPCPORT;
extern uint32_t ASSETCHAINS_CC;
extern uint32_t ASSETCHAINS_MAGIC,ASSETCHAINS_ALGO;
extern uint64_t ASSETCHAINS_COMMISSION,ASSETCHAINS_SUPPLY;
extern int32_t ASSETCHAINS_LWMAPOS,ASSETCHAINS_SAPLING,ASSETCHAINS_STAKED;
extern uint64_t ASSETCHAINS_ENDSUBSIDY[],ASSETCHAINS_REWARD[],ASSETCHAINS_HALVING[],ASSETCHAINS_DECAY[],ASSETCHAINS_NOTARY_PAY[];
extern std::string NOTARY_PUBKEY,NOTARY_ADDRESS; extern uint8_t NOTARY_PUBKEY33[];

int32_t getera(int timestamp)
{
    for (int32_t i = 0; i < NUM_STAKED_ERAS; i++) {
        if ( timestamp <= STAKED_NOTARIES_TIMESTAMP[i] ) {
            return(i);
        }
    }
    return(0);
}

UniValue getiguanajson(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 0)
      throw runtime_error("getiguanajson\nreturns json for iguana, for the current ERA.");

    UniValue json(UniValue::VOBJ);
    UniValue seeds(UniValue::VARR);
    UniValue notaries(UniValue::VARR);
    // get the current era, use local time for now.
    // should ideally take blocktime of last known block?
    int now = time(NULL);
    int32_t era = getera(now);

    // loop over seeds array and push back to json array for seeds
    for (int8_t i = 0; i < 8; i++) {
        seeds.push_back(iguanaSeeds[i][0]);
    }

    // loop over era's notaries and push back each pair to the notary array
    for (int8_t i = 0; i < num_notaries_STAKED[era]; i++) {
        UniValue notary(UniValue::VOBJ);
        notary.push_back(Pair(notaries_STAKED[era][i][0],notaries_STAKED[era][i][1]));
        notaries.push_back(notary);
    }

    // get the min sigs .. this always rounds UP so min sigs in iguana is +1 min sigs in komodod, due to some rounding error.
    int minsigs;
    if ( num_notaries_STAKED[era]/5 > overrideMinSigs )
        minsigs = (num_notaries_STAKED[era] / 5) + 1;
    else
        minsigs = overrideMinSigs;

    json.push_back(Pair("port",iguanaPort));
    json.push_back(Pair("BTCminsigs",BTCminsigs));
    json.push_back(Pair("minsigs",minsigs));
    json.push_back(Pair("seeds",seeds));
    json.push_back(Pair("notaries",notaries));
    return json;
}

UniValue getnotarysendmany(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 1)
      throw runtime_error(
          "getnotarysendmany\n"
          "Returns a sendmany JSON array with all current notaries Raddress's.\n"
          "\nExamples:\n"
          + HelpExampleCli("getnotarysendmany", "10")
          + HelpExampleRpc("getnotarysendmany", "10")
      );
    int amount = 0;
    if ( params.size() == 1 ) {
        amount = params[0].get_int();
    }

    int era = getera(time(NULL));

    UniValue ret(UniValue::VOBJ);
    for (int i = 0; i<num_notaries_STAKED[era]; i++)
    {
        char Raddress[18]; uint8_t pubkey33[33];
        decode_hex(pubkey33,33,(char *)notaries_STAKED[era][i][1]);
        pubkey2addr((char *)Raddress,(uint8_t *)pubkey33);
        ret.push_back(Pair(Raddress,amount));
    }
    return ret;
}

UniValue geterablockheights(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 0)
      throw runtime_error(
          "geterablockheights\n"
          "Returns a JSON object with the first block in each era.\n"
          );
      
    CBlockIndex *pindex; int8_t lastera,era = 0; UniValue ret(UniValue::VOBJ);

    for (size_t i = 1; i < chainActive.LastTip()->GetHeight(); i++)
    {
        pindex = chainActive[i];
        era = getera(pindex->nTime)+1;
        if ( era > lastera )
        {
            char str[16];
            sprintf(str, "%d", era);
            ret.push_back(Pair(str,(int64_t)i));
            lastera = era;
        }
    }
    
    return(ret);
}

UniValue getinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 notarized_hash,notarized_desttxid; int32_t prevMoMheight,notarized_height,longestchain,kmdnotarized_height,txid_height;
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getinfo\n"
            "Returns an object containing various state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,           (numeric) the server version\n"
            "  \"protocolversion\": xxxxx,   (numeric) the protocol version\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total Komodo balance of the wallet\n"
            "  \"blocks\": xxxxxx,           (numeric) the current number of blocks processed in the server\n"
            "  \"timeoffset\": xxxxx,        (numeric) the time offset\n"
            "  \"connections\": xxxxx,       (numeric) the number of connections\n"
            "  \"proxy\": \"host:port\",     (string, optional) the proxy used by the server\n"
            "  \"difficulty\": xxxxxx,       (numeric) the current difficulty\n"
            "  \"testnet\": true|false,      (boolean) if the server is using testnet or not\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee set in " + CURRENCY_UNIT + "/kB\n"
            "  \"relayfee\": x.xxxx,         (numeric) minimum relay fee for non-free transactions in " + CURRENCY_UNIT + "/kB\n"
            "  \"errors\": \"...\"           (string) any error messages\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getinfo", "")
            + HelpExampleRpc("getinfo", "")
        );
    //#ifdef ENABLE_WALLET
    //    LOCK2(cs_main, pwalletMain ? &pwalletMain->cs_wallet : NULL);
    //#else
    LOCK(cs_main);
    //#endif
    
    proxyType proxy;
    GetProxy(NET_IPV4, proxy);
    notarized_height = komodo_notarized_height(&prevMoMheight,&notarized_hash,&notarized_desttxid);
    //fprintf(stderr,"after notarized_height %u\n",(uint32_t)time(NULL));
    
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("version", CLIENT_VERSION));
    obj.push_back(Pair("protocolversion", PROTOCOL_VERSION));
    obj.push_back(Pair("KMDversion", KOMODO_VERSION));
    obj.push_back(Pair("synced", KOMODO_INSYNC!=0));
    //obj.push_back(Pair("VRSCversion", VERUS_VERSION));
    obj.push_back(Pair("notarized", notarized_height));
    obj.push_back(Pair("prevMoMheight", prevMoMheight));
    obj.push_back(Pair("notarizedhash", notarized_hash.ToString()));
    obj.push_back(Pair("notarizedtxid", notarized_desttxid.ToString()));
    if ( KOMODO_NSPV_FULLNODE )
    {
        txid_height = notarizedtxid_height(ASSETCHAINS_SYMBOL[0] != 0 ? (char *)"KMD" : (char *)"BTC",(char *)notarized_desttxid.ToString().c_str(),&kmdnotarized_height);
        if ( txid_height > 0 )
            obj.push_back(Pair("notarizedtxid_height", txid_height));
        else obj.push_back(Pair("notarizedtxid_height", "mempool"));
        if ( ASSETCHAINS_SYMBOL[0] != 0 )
            obj.push_back(Pair("KMDnotarized_height", kmdnotarized_height));
        obj.push_back(Pair("notarized_confirms", txid_height < kmdnotarized_height ? (kmdnotarized_height - txid_height + 1) : 0));
        //fprintf(stderr,"after notarized_confirms %u\n",(uint32_t)time(NULL));
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
            if ( ASSETCHAINS_SYMBOL[0] == 0 )
            {
                obj.push_back(Pair("interest",       ValueFromAmount(KOMODO_INTERESTSUM)));
                obj.push_back(Pair("balance",       ValueFromAmount(KOMODO_WALLETBALANCE))); //pwalletMain->GetBalance()
            }
            else
            {
                obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance()))); //
            }
        }
#endif
        //fprintf(stderr,"after wallet %u\n",(uint32_t)time(NULL));
        obj.push_back(Pair("blocks",        (int)chainActive.Height()));
        if ( (longestchain= KOMODO_LONGESTCHAIN) != 0 && chainActive.Height() > longestchain )
            longestchain = chainActive.Height();
        //fprintf(stderr,"after longestchain %u\n",(uint32_t)time(NULL));
        obj.push_back(Pair("longestchain",        longestchain));
        if ( chainActive.LastTip() != 0 )
            obj.push_back(Pair("tiptime", (int)chainActive.LastTip()->nTime));
        obj.push_back(Pair("difficulty",    (double)GetDifficulty()));
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
            obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
        }
        if (pwalletMain && pwalletMain->IsCrypted())
            obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
        obj.push_back(Pair("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK())));
#endif
        obj.push_back(Pair("sapling", ASSETCHAINS_SAPLING));
    }
    obj.push_back(Pair("timeoffset",    GetTimeOffset()));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("proxy",         (proxy.IsValid() ? proxy.proxy.ToStringIPPort() : string())));
    obj.push_back(Pair("testnet",       Params().TestnetToBeDeprecatedFieldRPC()));
    obj.push_back(Pair("relayfee",      ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
     if ( NOTARY_PUBKEY33[0] != 0 ) {
        char pubkeystr[65]; int32_t notaryid; std::string notaryname;
        if ( (notaryid= StakedNotaryID(notaryname, (char *)NOTARY_ADDRESS.c_str())) != -1 ) {
            obj.push_back(Pair("notaryid",        notaryid));
            obj.push_back(Pair("notaryname",      notaryname));
        } else if( (notaryid= komodo_whoami(pubkeystr,(int32_t)chainActive.LastTip()->GetHeight(),komodo_chainactive_timestamp())) >= 0 )  {
            obj.push_back(Pair("notaryid",        notaryid));
            if ( KOMODO_LASTMINED != 0 )
                obj.push_back(Pair("lastmined", KOMODO_LASTMINED));
        }
        obj.push_back(Pair("pubkey", NOTARY_PUBKEY));
    }
    if ( ASSETCHAINS_CC != 0 )
        obj.push_back(Pair("CCid",        (int)ASSETCHAINS_CC));
    obj.push_back(Pair("name",        ASSETCHAINS_SYMBOL[0] == 0 ? "KMD" : ASSETCHAINS_SYMBOL));

    obj.push_back(Pair("p2pport",        ASSETCHAINS_P2PPORT));
    obj.push_back(Pair("rpcport",        ASSETCHAINS_RPCPORT));
    if ( ASSETCHAINS_SYMBOL[0] != 0 )
    {
        if ( is_STAKED(ASSETCHAINS_SYMBOL) != 0 )
            obj.push_back(Pair("StakedEra",        STAKED_ERA));
        //obj.push_back(Pair("name",        ASSETCHAINS_SYMBOL));
        obj.push_back(Pair("magic",        (int)ASSETCHAINS_MAGIC));
        obj.push_back(Pair("premine",        ASSETCHAINS_SUPPLY));

        if ( ASSETCHAINS_REWARD[0] != 0 || ASSETCHAINS_LASTERA > 0 )
        {
            std::string acReward = "", acHalving = "", acDecay = "", acEndSubsidy = "", acNotaryPay = "";
            for (int i = 0; i <= ASSETCHAINS_LASTERA; i++)
            {
                if (i == 0)
                {
                    acReward = std::to_string(ASSETCHAINS_REWARD[i]);
                    acHalving = std::to_string(ASSETCHAINS_HALVING[i]);
                    acDecay = std::to_string(ASSETCHAINS_DECAY[i]);
                    acEndSubsidy = std::to_string(ASSETCHAINS_ENDSUBSIDY[i]);
                    acNotaryPay = std::to_string(ASSETCHAINS_NOTARY_PAY[i]);
                }
                else
                {
                    acReward += "," + std::to_string(ASSETCHAINS_REWARD[i]);
                    acHalving += "," + std::to_string(ASSETCHAINS_HALVING[i]);
                    acDecay += "," + std::to_string(ASSETCHAINS_DECAY[i]);
                    acEndSubsidy += "," + std::to_string(ASSETCHAINS_ENDSUBSIDY[i]);
                    acNotaryPay += "," + std::to_string(ASSETCHAINS_NOTARY_PAY[i]);
                }
            }
            if (ASSETCHAINS_LASTERA > 0)
                obj.push_back(Pair("eras", (int64_t)(ASSETCHAINS_LASTERA + 1)));
            obj.push_back(Pair("reward", acReward));
            obj.push_back(Pair("halving", acHalving));
            obj.push_back(Pair("decay", acDecay));
            obj.push_back(Pair("endsubsidy", acEndSubsidy));
            obj.push_back(Pair("notarypay", acNotaryPay));
        }

        if ( ASSETCHAINS_COMMISSION != 0 )
            obj.push_back(Pair("commission",        ASSETCHAINS_COMMISSION));
        if ( ASSETCHAINS_STAKED != 0 )
            obj.push_back(Pair("staked",        ASSETCHAINS_STAKED));
        if ( ASSETCHAINS_LWMAPOS != 0 )
            obj.push_back(Pair("veruspos", ASSETCHAINS_LWMAPOS));
        if ( ASSETCHAINS_ALGO != ASSETCHAINS_EQUIHASH )
            obj.push_back(Pair("algo",ASSETCHAINS_ALGORITHMS[ASSETCHAINS_ALGO]));
    }
    return obj;
}

#ifdef ENABLE_WALLET
class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    UniValue operator()(const CNoDestination &dest) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const CKeyID &keyID) const {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        obj.push_back(Pair("isscript", false));
        if (pwalletMain && pwalletMain->GetPubKey(keyID, vchPubKey)) {
            obj.push_back(Pair("pubkey", HexStr(vchPubKey))); // should return pubkeyhash, but not sure about compatibility impact
            obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        }
        return obj;
    }

    UniValue operator()(const CPubKey &key) const {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("isscript", false));
        if (pwalletMain && key.IsValid()) {
            obj.push_back(Pair("pubkey", HexStr(key)));
            obj.push_back(Pair("iscompressed", key.IsCompressed()));
        }
        else
        {
            obj.push_back(Pair("pubkey", "invalid"));
        }
        return obj;
    }

    UniValue operator()(const CScriptID &scriptID) const {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        if (pwalletMain && pwalletMain->GetCScript(scriptID, subscript)) {
            std::vector<CTxDestination> addresses;
            txnouttype whichType;
            int nRequired;
            ExtractDestinations(subscript, whichType, addresses, nRequired);
            obj.push_back(Pair("script", GetTxnOutputType(whichType)));
            obj.push_back(Pair("hex", HexStr(subscript.begin(), subscript.end())));
            UniValue a(UniValue::VARR);
            for (const CTxDestination& addr : addresses) {
                a.push_back(EncodeDestination(addr));
            }
            obj.push_back(Pair("addresses", a));
            if (whichType == TX_MULTISIG)
                obj.push_back(Pair("sigsrequired", nRequired));
        }
        return obj;
    }
};
#endif

UniValue coinsupply(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t height = 0; int32_t currentHeight; int64_t blocks_per_year,zf1,zf3,zf12,sf1,sf3,sf12,sproutfunds,zfunds,supply1,supply3,supply12,supply = 0; UniValue result(UniValue::VOBJ);
    if (fHelp || params.size() > 1)
        throw runtime_error("coinsupply <height>\n"
            "\nReturn coin supply information at a given block height. If no height is given, the current height is used.\n"
            "\nArguments:\n"
            "1. \"height\"     (integer, optional) Block height\n"
            "\nResult:\n"
            "{\n"
            "  \"result\" : \"success\",         (string) If the request was successful.\n"
            "  \"coin\" : \"KMD\",               (string) The currency symbol of the coin for asset chains, otherwise KMD.\n"
            "  \"height\" : 420,               (integer) The height of this coin supply data\n"
            "  \"supply\" : \"777.0\",           (float) The transparent coin supply\n"
            "  \"zfunds\" : \"0.777\",           (float) The shielded coin supply (in zaddrs)\n"
            "  \"sprout\" : \"0.077\",           (float) The sprout coin supply (in zcaddrs)\n"
           "  \"total\" :  \"777.777\",         (float) The total coin supply, i.e. sum of supply + zfunds\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("coinsupply", "420")
            + HelpExampleRpc("coinsupply", "420")
        );
    if ( params.size() == 0 )
        height = chainActive.Height();
    else height = atoi(params[0].get_str());
    currentHeight = chainActive.Height();

    if (height >= 0 && height <= currentHeight) {
        if ( (supply= komodo_coinsupply(&zfunds,&sproutfunds,height)) > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("coin", ASSETCHAINS_SYMBOL[0] == 0 ? "KMD" : ASSETCHAINS_SYMBOL));
            result.push_back(Pair("height", (int)height));
            result.push_back(Pair("supply", ValueFromAmount(supply)));
            result.push_back(Pair("zfunds", ValueFromAmount(zfunds)));
            result.push_back(Pair("sprout", ValueFromAmount(sproutfunds)));
            result.push_back(Pair("total", ValueFromAmount(zfunds + supply)));
            if ( ASSETCHAINS_BLOCKTIME > 0 )
            {
                blocks_per_year = 24*3600*365 / ASSETCHAINS_BLOCKTIME;
                if ( height > blocks_per_year )
                {
                    supply1 = komodo_coinsupply(&zf1,&sf1,height - blocks_per_year/12);
                    supply3 = komodo_coinsupply(&zf3,&sf3,height - blocks_per_year/4);
                    supply12 = komodo_coinsupply(&zf12,&sf12,height - blocks_per_year);
                    if ( supply1 != 0 && supply3 != 0 && supply12 != 0 )
                    {
                        result.push_back(Pair("lastmonth", ValueFromAmount(supply1+zf1)));
                        result.push_back(Pair("monthcoins", ValueFromAmount(zfunds + supply - supply1-zf1)));
                        result.push_back(Pair("lastquarter", ValueFromAmount(supply3+zf3)));
                        result.push_back(Pair("quartercoins", ValueFromAmount(zfunds + supply - supply3-zf3)));
                        result.push_back(Pair("lastyear", ValueFromAmount(supply12+zf12)));
                        result.push_back(Pair("yearcoins", ValueFromAmount(zfunds + supply - supply12-zf12)));
                        result.push_back(Pair("inflation", 100. * (((double)(zfunds + supply)/(supply12+zf12))-1.)));
                        result.push_back(Pair("blocksperyear", (int64_t)blocks_per_year));
                    }
                }
            }
        } else result.push_back(Pair("error", "couldnt calculate supply"));
    } else {
        result.push_back(Pair("error", "invalid height"));
    }
    return(result);
}

UniValue jumblr_deposit(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t retval; UniValue result(UniValue::VOBJ);
    if (fHelp || params.size() != 1)
        throw runtime_error("jumblr_deposit \"depositaddress\"\n");
    CBitcoinAddress address(params[0].get_str());
    bool isValid = address.IsValid();
    if ( isValid != 0 )
    {
        string addr = params[0].get_str();
        if ( (retval= Jumblr_depositaddradd((char *)addr.c_str())) >= 0 )
        {
            result.push_back(Pair("result", retval));
            JUMBLR_PAUSE = 0;
        }
        else result.push_back(Pair("error", retval));
    } else result.push_back(Pair("error", "invalid address"));
    return(result);
}

UniValue jumblr_secret(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t retval; UniValue result(UniValue::VOBJ);
    if (fHelp || params.size() != 1)
        throw runtime_error("jumblr_secret \"secretaddress\"\n");
    CBitcoinAddress address(params[0].get_str());
    bool isValid = address.IsValid();
    if ( isValid != 0 )
    {
        string addr = params[0].get_str();
        retval = Jumblr_secretaddradd((char *)addr.c_str());
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("num", retval));
        JUMBLR_PAUSE = 0;
    } else result.push_back(Pair("error", "invalid address"));
    return(result);
}

UniValue jumblr_pause(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t retval; UniValue result(UniValue::VOBJ);
    if (fHelp )
        throw runtime_error("jumblr_pause\n");
    JUMBLR_PAUSE = 1;
    result.push_back(Pair("result", "paused"));
    return(result);
}

UniValue jumblr_resume(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t retval; UniValue result(UniValue::VOBJ);
    if (fHelp )
        throw runtime_error("jumblr_resume\n");
    JUMBLR_PAUSE = 0;
    result.push_back(Pair("result", "resumed"));
    return(result);
}

UniValue validateaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "validateaddress \"komodoaddress\"\n"
            "\nReturn information about the given Komodo address.\n"
            "\nArguments:\n"
            "1. \"komodoaddress\"     (string, required) The Komodo address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,         (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"komodoaddress\",   (string) The Komodo address validated\n"
            "  \"scriptPubKey\" : \"hex\",       (string) The hex encoded scriptPubKey generated by the address\n"
            "  \"ismine\" : true|false,          (boolean) If the address is yours or not\n"
            "  \"isscript\" : true|false,        (boolean) If the key is a script\n"
            "  \"pubkey\" : \"publickeyhex\",    (string) The hex value of the raw public key\n"
            "  \"iscompressed\" : true|false,    (boolean) If the address is compressed\n"
            "  \"account\" : \"account\"         (string) DEPRECATED. The account associated with the address, \"\" is the default account\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validateaddress", "\"RTZMZHDFSTFQst8XmX2dR4DaH87cEUs3gC\"")
            + HelpExampleRpc("validateaddress", "\"RTZMZHDFSTFQst8XmX2dR4DaH87cEUs3gC\"")
        );

#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwalletMain ? &pwalletMain->cs_wallet : NULL);
#else
    LOCK(cs_main);
#endif

    CTxDestination dest = DecodeDestination(params[0].get_str());
    bool isValid = IsValidDestination(dest);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        std::string currentAddress = EncodeDestination(dest);
        ret.push_back(Pair("address", currentAddress));

        CScript scriptPubKey = GetScriptForDestination(dest);
        ret.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end())));
        ret.push_back(Pair("segid", (int32_t)komodo_segid32((char *)params[0].get_str().c_str()) & 0x3f));
#ifdef ENABLE_WALLET
        isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;
        ret.push_back(Pair("ismine", (mine & ISMINE_SPENDABLE) ? true : false));
        ret.push_back(Pair("iswatchonly", (mine & ISMINE_WATCH_ONLY) ? true: false));
        UniValue detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
        ret.pushKVs(detail);
        if (pwalletMain && pwalletMain->mapAddressBook.count(dest))
            ret.push_back(Pair("account", pwalletMain->mapAddressBook[dest].name));
#endif
    }
    return ret;
}


class DescribePaymentAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    UniValue operator()(const libzcash::InvalidEncoding &zaddr) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const libzcash::SproutPaymentAddress &zaddr) const {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("type", "sprout"));
        obj.push_back(Pair("payingkey", zaddr.a_pk.GetHex()));
        obj.push_back(Pair("transmissionkey", zaddr.pk_enc.GetHex()));
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            obj.push_back(Pair("ismine", pwalletMain->HaveSproutSpendingKey(zaddr)));
        }
#endif
        return obj;
    }

    UniValue operator()(const libzcash::SaplingPaymentAddress &zaddr) const {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("type", "sapling"));
        obj.push_back(Pair("diversifier", HexStr(zaddr.d)));
        obj.push_back(Pair("diversifiedtransmissionkey", zaddr.pk_d.GetHex()));
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            libzcash::SaplingIncomingViewingKey ivk;
            libzcash::SaplingFullViewingKey fvk;
            bool isMine = pwalletMain->GetSaplingIncomingViewingKey(zaddr, ivk) &&
                pwalletMain->GetSaplingFullViewingKey(ivk, fvk) &&
                pwalletMain->HaveSaplingSpendingKey(fvk);
            obj.push_back(Pair("ismine", isMine));
        }
#endif
        return obj;
    }
};

UniValue z_validateaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_validateaddress \"zaddr\"\n"
            "\nReturn information about the given z address.\n"
            "\nArguments:\n"
            "1. \"zaddr\"     (string, required) The z address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,      (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"zaddr\",         (string) The z address validated\n"
            "  \"type\" : \"xxxx\",             (string) \"sprout\" or \"sapling\"\n"
            "  \"ismine\" : true|false,       (boolean) If the address is yours or not\n"
            "  \"payingkey\" : \"hex\",         (string) [sprout] The hex value of the paying key, a_pk\n"
            "  \"transmissionkey\" : \"hex\",   (string) [sprout] The hex value of the transmission key, pk_enc\n"
            "  \"diversifier\" : \"hex\",       (string) [sapling] The hex value of the diversifier, d\n"
            "  \"diversifiedtransmissionkey\" : \"hex\", (string) [sapling] The hex value of pk_d\n"

            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_validateaddress", "\"zcWsmqT4X2V4jgxbgiCzyrAfRT1vi1F4sn7M5Pkh66izzw8Uk7LBGAH3DtcSMJeUb2pi3W4SQF8LMKkU2cUuVP68yAGcomL\"")
            + HelpExampleRpc("z_validateaddress", "\"zcWsmqT4X2V4jgxbgiCzyrAfRT1vi1F4sn7M5Pkh66izzw8Uk7LBGAH3DtcSMJeUb2pi3W4SQF8LMKkU2cUuVP68yAGcomL\"")
        );


#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwalletMain->cs_wallet);
#else
    LOCK(cs_main);
#endif

    string strAddress = params[0].get_str();
    auto address = DecodePaymentAddress(strAddress);
    bool isValid = IsValidPaymentAddress(address);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        ret.push_back(Pair("address", strAddress));
        UniValue detail = boost::apply_visitor(DescribePaymentAddressVisitor(), address);
        ret.pushKVs(detail);
    }
    return ret;
}


/**
 * Used by addmultisigaddress / createmultisig:
 */
CScript _createmultisig_redeemScript(const UniValue& params)
{
    int nRequired = params[0].get_int();
    const UniValue& keys = params[1].get_array();

    // Gather public keys
    if (nRequired < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if ((int)keys.size() < nRequired)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %u keys, but need at least %d to redeem)", keys.size(), nRequired));
    if (keys.size() > 16)
        throw runtime_error("Number of addresses involved in the multisignature address creation > 16\nReduce the number");
    std::vector<CPubKey> pubkeys;
    pubkeys.resize(keys.size());
    for (unsigned int i = 0; i < keys.size(); i++)
    {
        const std::string& ks = keys[i].get_str();
#ifdef ENABLE_WALLET
        // Case 1: Bitcoin address and we have full public key:
        CTxDestination dest = DecodeDestination(ks);
        if (pwalletMain && IsValidDestination(dest)) {
            const CKeyID *keyID = boost::get<CKeyID>(&dest);
            if (!keyID) {
                throw std::runtime_error(strprintf("%s does not refer to a key", ks));
            }
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(*keyID, vchPubKey)) {
                throw std::runtime_error(strprintf("no full public key for address %s", ks));
            }
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }

        // Case 2: hex public key
        else
#endif
        if (IsHex(ks))
        {
            CPubKey vchPubKey(ParseHex(ks));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }
        else
        {
            throw runtime_error(" Invalid public key: "+ks);
        }
    }
    CScript result = GetScriptForMultisig(nRequired, pubkeys);

    if (result.size() > MAX_SCRIPT_ELEMENT_SIZE)
        throw runtime_error(
                strprintf("redeemScript exceeds size limit: %d > %d", result.size(), MAX_SCRIPT_ELEMENT_SIZE));

    return result;
}

UniValue createmultisig(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() < 2 || params.size() > 2)
    {
        string msg = "createmultisig nrequired [\"key\",...]\n"
            "\nCreates a multi-signature address with n signature of m keys required.\n"
            "It returns a json object with the address and redeemScript.\n"

            "\nArguments:\n"
            "1. nrequired      (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keys\"       (string, required) A json array of keys which are Komodo addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"key\"    (string) Komodo address or hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",  (string) The value of the new multisig address.\n"
            "  \"redeemScript\":\"script\"       (string) The string value of the hex-encoded redemption script.\n"
            "}\n"

            "\nExamples:\n"
            "\nCreate a multisig address from 2 addresses\n"
            + HelpExampleCli("createmultisig", "2 \"[\\\"RTZMZHDFSTFQst8XmX2dR4DaH87cEUs3gC\\\",\\\"RNKiEBduBru6Siv1cZRVhp4fkZNyPska6z\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("createmultisig", "2, \"[\\\"RTZMZHDFSTFQst8XmX2dR4DaH87cEUs3gC\\\",\\\"RNKiEBduBru6Siv1cZRVhp4fkZNyPska6z\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("address", EncodeDestination(innerID)));
    result.push_back(Pair("redeemScript", HexStr(inner.begin(), inner.end())));

    return result;
}

UniValue verifymessage(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "verifymessage \"komodoaddress\" \"signature\" \"message\"\n"
            "\nVerify a signed message\n"
            "\nArguments:\n"
            "1. \"komodoaddress\"    (string, required) The Komodo address to use for the signature.\n"
            "2. \"signature\"       (string, required) The signature provided by the signer in base 64 encoding (see signmessage).\n"
            "3. \"message\"         (string, required) The message that was signed.\n"
            "\nResult:\n"
            "true|false   (boolean) If the signature is verified or not.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"RNKiEBduBru6Siv1cZRVhp4fkZNyPska6z\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"RNKiEBduBru6Siv1cZRVhp4fkZNyPska6z\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("verifymessage", "\"RNKiEBduBru6Siv1cZRVhp4fkZNyPska6z\", \"signature\", \"my message\"")
        );

    LOCK(cs_main);

    string strAddress  = params[0].get_str();
    string strSign     = params[1].get_str();
    string strMessage  = params[2].get_str();

    CTxDestination destination = DecodeDestination(strAddress);
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = boost::get<CKeyID>(&destination);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == *keyID);
}

UniValue setmocktime(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "setmocktime timestamp\n"
            "\nSet the local time to given timestamp (-regtest only)\n"
            "\nArguments:\n"
            "1. timestamp  (integer, required) Unix seconds-since-epoch timestamp\n"
            "   Pass 0 to go back to using the system time."
        );

    if (!Params().MineBlocksOnDemand())
        throw runtime_error("setmocktime for regression testing (-regtest mode) only");

    // cs_vNodes is locked and node send/receive times are updated
    // atomically with the time change to prevent peers from being
    // disconnected because we think we haven't communicated with them
    // in a long time.
    LOCK2(cs_main, cs_vNodes);

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));
    SetMockTime(params[0].get_int64());

    uint64_t t = GetTime();
    BOOST_FOREACH(CNode* pnode, vNodes) {
        pnode->nLastSend = pnode->nLastRecv = t;
    }

    return NullUniValue;
}

bool getAddressFromIndex(const int &type, const uint160 &hash, std::string &address)
{
    if (type == 2) {
        address = CBitcoinAddress(CScriptID(hash)).ToString();
    } else if (type == 1) {
        address = CBitcoinAddress(CKeyID(hash)).ToString();
    } else if (type == 3) {
        address = CBitcoinAddress(CKeyID(hash)).ToString();
    } else {
        return false;
    }
    return true;
}

bool getAddressesFromParams(const UniValue& params, std::vector<std::pair<uint160, int> > &addresses)
{
    bool ccVout = false;
    if (params.size() == 2)
        ccVout = true;
    if (params[0].isStr()) {
        CBitcoinAddress address(params[0].get_str());
        uint160 hashBytes;
        int type = 0;
        if (!address.GetIndexKey(hashBytes, type, ccVout)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
        }
        addresses.push_back(std::make_pair(hashBytes, type));
    } else if (params[0].isObject()) {

        UniValue addressValues = find_value(params[0].get_obj(), "addresses");
        if (!addressValues.isArray()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Addresses is expected to be an array");
        }

        std::vector<UniValue> values = addressValues.getValues();

        for (std::vector<UniValue>::iterator it = values.begin(); it != values.end(); ++it) {

            CBitcoinAddress address(it->get_str());
            uint160 hashBytes;
            int type = 0;
            if (!address.GetIndexKey(hashBytes, type, ccVout)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid addresses");
            }
            addresses.push_back(std::make_pair(hashBytes, type));
        }
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid addresse");
    }

    return true;
}

bool heightSort(std::pair<CAddressUnspentKey, CAddressUnspentValue> a,
                std::pair<CAddressUnspentKey, CAddressUnspentValue> b) {
    return a.second.blockHeight < b.second.blockHeight;
}

bool timestampSort(std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> a,
                   std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> b) {
    return a.second.time < b.second.time;
}

UniValue getaddressmempool(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 2 || params.size() == 0)
        throw runtime_error(
            "getaddressmempool\n"
            "\nReturns all mempool deltas for an address (requires addressindex to be enabled).\n"
            "\nArguments:\n"
            "{\n"
            "  \"addresses\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "}\n"
            "\nCCvout (optional) Return CCvouts instead of normal vouts\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\"  (string) The base58check encoded address\n"
            "    \"txid\"  (string) The related txid\n"
            "    \"index\"  (number) The related input or output index\n"
            "    \"satoshis\"  (number) The difference of satoshis\n"
            "    \"timestamp\"  (number) The time the transaction entered the mempool (seconds)\n"
            "    \"prevtxid\"  (string) The previous txid (if spending)\n"
            "    \"prevout\"  (string) The previous transaction output index (if spending)\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressmempool", "'{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]}' (ccvout)")
            + HelpExampleRpc("getaddressmempool", "{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]} (ccvout)")
        );

    std::vector<std::pair<uint160, int> > addresses;

    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> > indexes;

    if (!mempool.getAddressIndex(addresses, indexes)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
    }

    std::sort(indexes.begin(), indexes.end(), timestampSort);

    UniValue result(UniValue::VARR);

    for (std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> >::iterator it = indexes.begin();
         it != indexes.end(); it++) {

        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.addressBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        UniValue delta(UniValue::VOBJ);
        delta.push_back(Pair("address", address));
        delta.push_back(Pair("txid", it->first.txhash.GetHex()));
        delta.push_back(Pair("index", (int)it->first.index));
        delta.push_back(Pair("satoshis", it->second.amount));
        delta.push_back(Pair("timestamp", it->second.time));
        if (it->second.amount < 0) {
            delta.push_back(Pair("prevtxid", it->second.prevhash.GetHex()));
            delta.push_back(Pair("prevout", (int)it->second.prevout));
        }
        result.push_back(delta);
    }

    return result;
}

UniValue getaddressutxos(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 2 || params.size() == 0)
        throw runtime_error(
            "getaddressutxos\n"
            "\nReturns all unspent outputs for an address (requires addressindex to be enabled).\n"
            "\nArguments:\n"
            "{\n"
            "  \"addresses\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ],\n"
            "  \"chainInfo\"  (boolean) Include chain info with results\n"
            "}\n"
            "\nCCvout (optional) Return CCvouts instead of normal vouts\n"
            "\nResult\n"
            "[\n"
            "  {\n"
            "    \"address\"  (string) The address base58check encoded\n"
            "    \"txid\"  (string) The output txid\n"
            "    \"height\"  (number) The block height\n"
            "    \"outputIndex\"  (number) The output index\n"
            "    \"script\"  (strin) The script hex encoded\n"
            "    \"satoshis\"  (number) The number of satoshis of the output\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressutxos", "'{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]}' (ccvout)")
            + HelpExampleRpc("getaddressutxos", "{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]} (ccvout)")
            );

    bool includeChainInfo = false;
    if (params[0].isObject()) {
        UniValue chainInfo = find_value(params[0].get_obj(), "chainInfo");
        if (chainInfo.isBool()) {
            includeChainInfo = chainInfo.get_bool();
        }
    }

    std::vector<std::pair<uint160, int> > addresses;

    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    UniValue utxos(UniValue::VARR);

    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        UniValue output(UniValue::VOBJ);
        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        output.push_back(Pair("address", address));
        output.push_back(Pair("txid", it->first.txhash.GetHex()));
        output.push_back(Pair("outputIndex", (int)it->first.index));
        output.push_back(Pair("script", HexStr(it->second.script.begin(), it->second.script.end())));
        output.push_back(Pair("satoshis", it->second.satoshis));
        output.push_back(Pair("height", it->second.blockHeight));
        utxos.push_back(output);
    }

    if (includeChainInfo) {
        UniValue result(UniValue::VOBJ);
        result.push_back(Pair("utxos", utxos));

        LOCK(cs_main);
        result.push_back(Pair("hash", chainActive.LastTip()->GetBlockHash().GetHex()));
        result.push_back(Pair("height", (int)chainActive.Height()));
        return result;
    } else {
        return utxos;
    }
}

UniValue getaddressdeltas(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 2 || params.size() == 0 || !params[0].isObject())
        throw runtime_error(
            "getaddressdeltas\n"
            "\nReturns all changes for an address (requires addressindex to be enabled).\n"
            "\nArguments:\n"
            "{\n"
            "  \"addresses\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "  \"start\" (number) The start block height\n"
            "  \"end\" (number) The end block height\n"
            "  \"chainInfo\" (boolean) Include chain info in results, only applies if start and end specified\n"
            "}\n"
            "\nCCvout (optional) Return CCvouts instead of normal vouts\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"satoshis\"  (number) The difference of satoshis\n"
            "    \"txid\"  (string) The related txid\n"
            "    \"index\"  (number) The related input or output index\n"
            "    \"height\"  (number) The block height\n"
            "    \"address\"  (string) The base58check encoded address\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressdeltas", "'{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]}' (ccvout)")
            + HelpExampleRpc("getaddressdeltas", "{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]} (ccvout)")
        );


    UniValue startValue = find_value(params[0].get_obj(), "start");
    UniValue endValue = find_value(params[0].get_obj(), "end");

    UniValue chainInfo = find_value(params[0].get_obj(), "chainInfo");
    bool includeChainInfo = false;
    if (chainInfo.isBool()) {
        includeChainInfo = chainInfo.get_bool();
    }

    int start = 0;
    int end = 0;

    if (startValue.isNum() && endValue.isNum()) {
        start = startValue.get_int();
        end = endValue.get_int();
        if (start <= 0 || end <= 0) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Start and end is expected to be greater than zero");
        }
        if (end < start) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "End value is expected to be greater than start");
        }
    }

    std::vector<std::pair<uint160, int> > addresses;

    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (start > 0 && end > 0) {
            if (!GetAddressIndex((*it).first, (*it).second, addressIndex, start, end)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        } else {
            if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        }
    }

    UniValue deltas(UniValue::VARR);

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        UniValue delta(UniValue::VOBJ);
        delta.push_back(Pair("satoshis", it->second));
        delta.push_back(Pair("txid", it->first.txhash.GetHex()));
        delta.push_back(Pair("index", (int)it->first.index));
        delta.push_back(Pair("blockindex", (int)it->first.txindex));
        delta.push_back(Pair("height", it->first.blockHeight));
        delta.push_back(Pair("address", address));
        deltas.push_back(delta);
    }

    UniValue result(UniValue::VOBJ);

    if (includeChainInfo && start > 0 && end > 0) {
        LOCK(cs_main);

        if (start > chainActive.Height() || end > chainActive.Height()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Start or end is outside chain range");
        }

        CBlockIndex* startIndex = chainActive[start];
        CBlockIndex* endIndex = chainActive[end];

        UniValue startInfo(UniValue::VOBJ);
        UniValue endInfo(UniValue::VOBJ);

        startInfo.push_back(Pair("hash", startIndex->GetBlockHash().GetHex()));
        startInfo.push_back(Pair("height", start));

        endInfo.push_back(Pair("hash", endIndex->GetBlockHash().GetHex()));
        endInfo.push_back(Pair("height", end));

        result.push_back(Pair("deltas", deltas));
        result.push_back(Pair("start", startInfo));
        result.push_back(Pair("end", endInfo));

        return result;
    } else {
        return deltas;
    }
}

CAmount checkburnaddress(CAmount &received, int64_t &nNotaryPay, int32_t &height, std::string sAddress)
{
    CBitcoinAddress address(sAddress);
    uint160 hashBytes; int type = 0; CAmount balance = 0;
    if (address.GetIndexKey(hashBytes, type, false))
    {
        std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
        if (GetAddressIndex(hashBytes, type, addressIndex))
        {
            for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
            {
                if (it->second > 0)
                    received += it->second;
                balance += it->second;
            }
            // Get notary pay from current chain tip
            CBlockIndex* pindex = chainActive.LastTip();
            nNotaryPay = pindex->nNotaryPay;
            height = pindex->GetHeight();
        }
    }
    return balance;
}

UniValue checknotarization(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "checknotarization\n"
            "\nReturns true if burn address balance is greater than total notary pay. (requires addressindex to be enabled).\n"
        );

    UniValue result(UniValue::VOBJ); CAmount balance = 0, received = 0; int64_t nNotaryPay = 0; int32_t height;
    
    // helper to test burn address's
    /*uint8_t priv[32] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t pub[33] =  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    char coinaddr[64]; uint8_t buf33[33];
    //pubkey2addr(coinaddr, pub);
    priv2addr(coinaddr,buf33,priv);
    fprintf(stderr, "what.%s\n", coinaddr);
    result.push_back(Pair("address", coinaddr));
    return result;
    */
    
    if ( ASSETCHAINS_NOTARY_PAY[0] == 0 )
        throw runtime_error("only works for ac_notarypay chains");
    // pubkey 020000000000000000000000000000000
    balance = checkburnaddress(received, nNotaryPay, height, "REDVp3ox1pbcWYCzySadfHhk8UU3HM4k5x");
    if ( nNotaryPay >= balance || received != balance )
        return false;
    return true;
}

UniValue getnotarypayinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getnotarypayinfo\n"
            "\nReturns infomation about ac_notaypay status (requires addressindex to be enabled).\n"
            "\nResult:\n"
            "{\n"
            "  \"height\"  (number) The current block height\n"
            "  \"balance\"  (number) The current balance of the burn address\n"
            "  \"spent\"  (bool) true if coins have been spent from the burn address\n"
            "  \"Total_NotaryPay\"  (number) Total amount paid to notaries\n"
            "  \"Estimated_Notarizations_Left\"  (number) the estimated amount of notarizations left before the balance is consumed\n"
            "  \"Estimated_Days_Left\"  (number) the estimated amount of days the current balance will last\n"
            "  \"Estimated_Height\"  (number) the estimated block height funds will run out\n"
            "}\n"
        );
    
    if ( ASSETCHAINS_NOTARY_PAY[0] == 0 )
        throw runtime_error("only works for ac_notarypay chains");
    
    UniValue result(UniValue::VOBJ); CAmount balance = 0, received = 0; int64_t TotalNotaryPay = 0, NotaryPay, notaleft = 0, daysleft = 0, notarycount; int32_t height, endheight = 0; uint8_t notarypubkeys[64][33] = {0};
    
    // pubkey 020000000000000000000000000000000
    balance = checkburnaddress(received, TotalNotaryPay, height, "REDVp3ox1pbcWYCzySadfHhk8UU3HM4k5x");
    
    notarycount = komodo_notaries(notarypubkeys, height, chainActive[height]->GetBlockTime());
    NotaryPay = komodo_notarypayamount(height, notarycount)*notarycount;
    bool spent = (received != balance);
    if ( !spent )
    {
        notaleft = ((int64_t)balance - TotalNotaryPay) / NotaryPay;
        daysleft = (((ASSETCHAINS_BLOCKTIME * 5) * notaleft) / 3600) / 24;
        endheight = (notaleft * 5) + height;
    }
    
    result.push_back(Pair("height", height));
    result.push_back(Pair("balance", ValueFromAmount(balance)));
    result.push_back(Pair("spent", spent));
    result.push_back(Pair("Total_NotaryPay", ValueFromAmount(TotalNotaryPay)));
    result.push_back(Pair("Estimated_Notarizations_Left", notaleft));
    result.push_back(Pair("Estimated_Days_Left", daysleft));
    result.push_back(Pair("Estimated_Height", endheight));
    return result;
}

UniValue getaddressbalance(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp ||params.size() > 2 || params.size() == 0)
        throw runtime_error(
            "getaddressbalance\n"
            "\nReturns the balance for an address(es) (requires addressindex to be enabled).\n"
            "\nArguments:\n"
            "{\n"
            "  \"addresses\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "}\n"
            "\nCCvout (optional) Return CCvouts instead of normal vouts\n"
            "\nResult:\n"
            "{\n"
            "  \"balance\"  (string) The current balance in satoshis\n"
            "  \"received\"  (string) The total number of satoshis received (including change)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressbalance", "'{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]}' (ccvout)")
            + HelpExampleRpc("getaddressbalance", "{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]} (ccvout)")
        );

    std::vector<std::pair<uint160, int> > addresses;

    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    CAmount balance = 0;
    CAmount received = 0;

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        if (it->second > 0) {
            received += it->second;
        }
        balance += it->second;
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("balance", balance));
    result.push_back(Pair("received", received));

    return result;

}

UniValue komodo_snapshot(int top);

UniValue getsnapshot(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t total; int32_t top = 0;

    if (params.size() > 0 && !params[0].isNull()) {
        top = atoi(params[0].get_str().c_str());
        if ( top < 0 ) 
        {
            if ( KOMODO_SNAPSHOT_INTERVAL == 0 )
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, top must be a positive integer");
            else 
                top = -1;
        }
    }

    if ( fHelp || params.size() > 1)
    {
        throw runtime_error(
                            "getsnapshot\n"
			    "\nReturns a snapshot of (address,amount) pairs at current height (requires addressindex to be enabled).\n"
			    "\nArguments:\n"
			    "  \"top\" (number, optional) Only return this many addresses, i.e. top N richlist\n"
			    "\nResult:\n"
			    "{\n"
			    "   \"addresses\": [\n"
			    "    {\n"
			    "      \"addr\": \"RMEBhzvATA8mrfVK82E5TgPzzjtaggRGN3\",\n"
			    "      \"amount\": \"100.0\"\n"
			    "    },\n"
			    "    {\n"
			    "      \"addr\": \"RqEBhzvATAJmrfVL82E57gPzzjtaggR777\",\n"
			    "      \"amount\": \"23.45\"\n"
			    "    }\n"
			    "  ],\n"
			    "  \"total\": 123.45           (numeric) Total amount in snapshot\n"
			    "  \"average\": 61.7,          (numeric) Average amount in each address \n"
			    "  \"utxos\": 14,              (number) Total number of UTXOs in snapshot\n"
			    "  \"total_addresses\": 2,     (number) Total number of addresses in snapshot,\n"
			    "  \"start_height\": 91,       (number) Block height snapshot began\n"
			    "  \"ending_height\": 91       (number) Block height snapsho finished,\n"
			    "  \"start_time\": 1531982752, (number) Unix epoch time snapshot started\n"
			    "  \"end_time\": 1531982752    (number) Unix epoch time snapshot finished\n"
			    "}\n"
			    "\nExamples:\n"
			    + HelpExampleCli("getsnapshot","")
			    + HelpExampleRpc("getsnapshot", "1000")
                            );
    }
    result = komodo_snapshot(top);
    if ( result.size() > 0 ) {
        result.push_back(Pair("end_time", (int) time(NULL)));
    } else {
	result.push_back(Pair("error", "no addressindex"));
    }
    return(result);
}

UniValue getaddresstxids(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 2 || params.size() < 1)
        throw runtime_error(
            "getaddresstxids (ccvout)\n"
            "\nReturns the txids for an address(es) (requires addressindex to be enabled).\n"
            "\nArguments:\n"
            "{\n"
            "  \"addresses\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "  \"start\" (number) The start block height\n"
            "  \"end\" (number) The end block height\n"
            "}\n"
            "\nCCvout (optional) Return CCvouts instead of normal vouts\n"
            "\nResult:\n"
            "[\n"
            "  \"transactionid\"  (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddresstxids", "'{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]}' (ccvout)")
            + HelpExampleRpc("getaddresstxids", "{\"addresses\": [\"RY5LccmGiX9bUHYGtSWQouNy1yFhc5rM87\"]} (ccvout)")
        );

    std::vector<std::pair<uint160, int> > addresses;

    if (!getAddressesFromParams(params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    int start = 0;
    int end = 0;
    if (params[0].isObject()) {
        UniValue startValue = find_value(params[0].get_obj(), "start");
        UniValue endValue = find_value(params[0].get_obj(), "end");
        if (startValue.isNum() && endValue.isNum()) {
            start = startValue.get_int();
            end = endValue.get_int();
        }
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (start > 0 && end > 0) {
            if (!GetAddressIndex((*it).first, (*it).second, addressIndex, start, end)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        } else {
            if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        }
    }

    std::set<std::pair<int, std::string> > txids;
    UniValue result(UniValue::VARR);

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        int height = it->first.blockHeight;
        std::string txid = it->first.txhash.GetHex();

        if (addresses.size() > 1) {
            txids.insert(std::make_pair(height, txid));
        } else {
            if (txids.insert(std::make_pair(height, txid)).second) {
                result.push_back(txid);
            }
        }
    }

    if (addresses.size() > 1) {
        for (std::set<std::pair<int, std::string> >::const_iterator it=txids.begin(); it!=txids.end(); it++) {
            result.push_back(it->second);
        }
    }

    return result;

}

UniValue getspentinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{

    if (fHelp || params.size() != 1 || !params[0].isObject())
        throw runtime_error(
            "getspentinfo\n"
            "\nReturns the txid and index where an output is spent.\n"
            "\nArguments:\n"
            "{\n"
            "  \"txid\" (string) The hex string of the txid\n"
            "  \"index\" (number) The start block height\n"
            "}\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\"  (string) The transaction id\n"
            "  \"index\"  (number) The spending input index\n"
            "  ,...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getspentinfo", "'{\"txid\": \"0437cd7f8525ceed2324359c2d0ba26006d92d856a9c20fa0241106ee5a597c9\", \"index\": 0}'")
            + HelpExampleRpc("getspentinfo", "{\"txid\": \"0437cd7f8525ceed2324359c2d0ba26006d92d856a9c20fa0241106ee5a597c9\", \"index\": 0}")
        );

    UniValue txidValue = find_value(params[0].get_obj(), "txid");
    UniValue indexValue = find_value(params[0].get_obj(), "index");

    if (!txidValue.isStr() || !indexValue.isNum()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid txid or index");
    }

    uint256 txid = ParseHashV(txidValue, "txid");
    int outputIndex = indexValue.get_int();

    CSpentIndexKey key(txid, outputIndex);
    CSpentIndexValue value;

    if (!GetSpentIndex(key, value)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get spent info");
    }

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("txid", value.txid.GetHex()));
    obj.push_back(Pair("index", (int)value.inputIndex));
    obj.push_back(Pair("height", value.blockHeight));

    return obj;
}

UniValue txnotarizedconfirmed(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    bool notarizedconfirmed; uint256 txid;

    if (fHelp || params.size() < 1 || params.size() > 1)
    {
        string msg = "txnotarizedconfirmed txid\n"
            "\nReturns true if transaction is notarized on chain that has dPoW or if confirmation number is greater than 60 on chain taht does not have dPoW.\n"

            "\nArguments:\n"
            "1. txid      (string, required) Transaction id.\n"

            "\nResult:\n"
            "{\n"
            "  true,  (bool) The value the check.\n"
            "}\n"
        ;
        throw runtime_error(msg);
    }
    txid = uint256S((char *)params[0].get_str().c_str());
    notarizedconfirmed=komodo_txnotarizedconfirmed(txid);
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result", notarizedconfirmed));
    return result;
}

UniValue decodeccopret(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    CTransaction tx; uint256 tokenid,txid,hashblock;
    std::vector<uint8_t> vopret,vOpretExtra; uint8_t *script,tokenevalcode;
    UniValue result(UniValue::VOBJ),array(UniValue::VARR); std::vector<CPubKey> pubkeys;

    if (fHelp || params.size() < 1 || params.size() > 1)
    {
        string msg = "decodeccopret scriptPubKey\n"
            "\nReturns eval code and function id for CC OP RETURN data.\n"           

            "\nArguments:\n"
            "1. scriptPubKey      (string, required) Hex of scriptPubKey with OP_RETURN data.\n"          

            "\nResult:\n"
            "{\n"
            "  eval_code,  (string) Eval code name.\n" 
            "  function,   (char) Function id char.\n"           
            "}\n"           
        ;
        throw runtime_error(msg);
    }
    std::vector<unsigned char> hex(ParseHex(params[0].get_str()));
    CScript scripthex(hex.begin(),hex.end());
    std::vector<std::pair<uint8_t, vscript_t>>  oprets;
    if (DecodeTokenOpRet(scripthex,tokenevalcode,tokenid,pubkeys, oprets)!=0 && tokenevalcode==EVAL_TOKENS && oprets.size()>0)
    {
        // seems we need a loop here
        vOpretExtra = oprets[0].second;  
        UniValue obj(UniValue::VOBJ);
        GetOpReturnData(scripthex,vopret);
        script = (uint8_t *)vopret.data();
        if ( vopret.size() > 1)
        {        
            char func[5];
            sprintf(func,"%c",script[1]);
            obj.push_back(Pair("eval_code", EvalToStr(script[0])));
            obj.push_back(Pair("function", func));
        }
        else
        {
            obj.push_back(Pair("error", "invalid or no CC opret data for Token OP_RETURN"));
        }
        array.push_back(obj);
        if (!E_UNMARSHAL(vOpretExtra, { ss >> vopret; })) return (0);
    }
    else GetOpReturnData(scripthex,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 1)
    {        
        char func[5]; UniValue obj(UniValue::VOBJ);
        result.push_back(Pair("result", "success"));
        sprintf(func,"%c",script[1]);        
        obj.push_back(Pair("eval_code", EvalToStr(script[0])));
        obj.push_back(Pair("function", func));
        array.push_back(obj);
        result.push_back(Pair("OpRets",array));
    }
    else
    {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "invalid or no CC opret data"));
    }
    return result;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "control",            "getinfo",                &getinfo,                true  }, /* uses wallet if enabled */
    { "util",               "validateaddress",        &validateaddress,        true  }, /* uses wallet if enabled */
    { "util",               "z_validateaddress",      &z_validateaddress,      true  }, /* uses wallet if enabled */
    { "util",               "createmultisig",         &createmultisig,         true  },
    { "util",               "verifymessage",          &verifymessage,          true  },

    /* Not shown in help */
    { "hidden",             "setmocktime",            &setmocktime,            true  },
};

void RegisterMiscRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
