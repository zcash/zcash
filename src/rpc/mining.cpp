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
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "core_io.h"
#ifdef ENABLE_MINING
#include "crypto/equihash.h"
#endif
#include "init.h"
#include "main.h"
#include "metrics.h"
#include "miner.h"
#include "net.h"
#include "pow.h"
#include "rpc/server.h"
#include "txmempool.h"
#include "util.h"
#include "validationinterface.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include <univalue.h>

using namespace std;

#include "komodo_defs.h"

extern int32_t ASSETCHAINS_FOUNDERS;
uint64_t komodo_commission(const CBlock *pblock,int32_t height);
int32_t komodo_blockload(CBlock& block,CBlockIndex *pindex);
arith_uint256 komodo_PoWtarget(int32_t *percPoSp,arith_uint256 target,int32_t height,int32_t goalperc,int32_t newStakerActive);
int32_t komodo_newStakerActive(int32_t height, uint32_t timestamp);

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or over the difficulty averaging window if 'lookup' is nonpositive.
 * If 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
int64_t GetNetworkHashPS(int lookup, int height) 
{
    CBlockIndex *pb = chainActive.LastTip();

    if (height >= 0 && height < chainActive.Height())
        pb = chainActive[height];

    if (pb == NULL || !pb->GetHeight())
        return 0;

    // If lookup is nonpositive, then use difficulty averaging window.
    if (lookup <= 0)
        lookup = Params().GetConsensus().nPowAveragingWindow;

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->GetHeight())
        lookup = pb->GetHeight();

    CBlockIndex *pb0 = pb;
    int64_t minTime = pb0->GetBlockTime();
    int64_t maxTime = minTime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64_t time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a divide by zero exception.
    if (minTime == maxTime)
        return 0;

    arith_uint256 workDiff = pb->chainPower.chainWork - pb0->chainPower.chainWork;
    int64_t timeDiff = maxTime - minTime;

    return (int64_t)(workDiff.getdouble() / timeDiff);
}

UniValue getlocalsolps(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp)
        throw runtime_error(
            "getlocalsolps\n"
            "\nReturns the average local solutions per second since this node was started.\n"
            "This is the same information shown on the metrics screen (if enabled).\n"
            "\nResult:\n"
            "xxx.xxxxx     (numeric) Solutions per second average\n"
            "\nExamples:\n"
            + HelpExampleCli("getlocalsolps", "")
            + HelpExampleRpc("getlocalsolps", "")
       );

    LOCK(cs_main);
    return GetLocalSolPS();
}

UniValue getnetworksolps(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getnetworksolps ( blocks height )\n"
            "\nReturns the estimated network solutions per second based on the last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies over difficulty averaging window.\n"
            "Pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\nArguments:\n"
            "1. blocks     (numeric, optional, default=120) The number of blocks, or -1 for blocks over difficulty averaging window.\n"
            "2. height     (numeric, optional, default=-1) To estimate at the time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Solutions per second estimated\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworksolps", "")
            + HelpExampleRpc("getnetworksolps", "")
       );

    LOCK(cs_main);
    return GetNetworkHashPS(params.size() > 0 ? params[0].get_int() : 120, params.size() > 1 ? params[1].get_int() : -1);
}

UniValue getnetworkhashps(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getnetworkhashps ( blocks height )\n"
            "\nDEPRECATED - left for backwards-compatibility. Use getnetworksolps instead.\n"
            "\nReturns the estimated network solutions per second based on the last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies over difficulty averaging window.\n"
            "Pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\nArguments:\n"
            "1. blocks     (numeric, optional, default=120) The number of blocks, or -1 for blocks over difficulty averaging window.\n"
            "2. height     (numeric, optional, default=-1) To estimate at the time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Solutions per second estimated\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkhashps", "")
            + HelpExampleRpc("getnetworkhashps", "")
       );

    LOCK(cs_main);
    return GetNetworkHashPS(params.size() > 0 ? params[0].get_int() : 120, params.size() > 1 ? params[1].get_int() : -1);
}

#ifdef ENABLE_MINING
extern bool VERUS_MINTBLOCKS;
UniValue getgenerate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "\nReturn if the server is set to mine and/or mint coins or not. The default is false.\n"
            "It is set with the command line argument -gen (or komodo.conf setting gen) and -mint\n"
            "It can also be set with the setgenerate call.\n"
            "\nResult\n"
            "{\n"
            "  \"staking\": true|false      (boolean) If staking is on or off (see setgenerate)\n"
            "  \"generate\": true|false     (boolean) If mining is on or off (see setgenerate)\n"
            "  \"numthreads\": n            (numeric) The processor limit for mining. (see setgenerate)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getgenerate", "")
            + HelpExampleRpc("getgenerate", "")
        );

    LOCK(cs_main);
    UniValue obj(UniValue::VOBJ);
    bool staking = VERUS_MINTBLOCKS;
    if ( ASSETCHAINS_STAKED != 0 && GetBoolArg("-gen", false) && GetBoolArg("-genproclimit", -1) == 0 )
        staking = true;
    obj.push_back(Pair("staking",          staking));
    obj.push_back(Pair("generate",         GetBoolArg("-gen", false) && GetBoolArg("-genproclimit", -1) != 0 ));
    obj.push_back(Pair("numthreads",       (int64_t)KOMODO_MININGTHREADS));
    return obj;
}

extern uint8_t NOTARY_PUBKEY33[33];

//Value generate(const Array& params, bool fHelp)
UniValue generate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "generate numblocks\n"
            "\nMine blocks immediately (before the RPC call returns)\n"
            "\nNote: this function can only be used on the regtest network\n"
            "\nArguments:\n"
            "1. numblocks    (numeric) How many blocks are generated immediately.\n"
            "\nResult\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            + HelpExampleCli("generate", "11")
        );

    if (GetArg("-mineraddress", "").empty()) {
#ifdef ENABLE_WALLET
        if (!pwalletMain) {
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Wallet disabled and -mineraddress not set");
        }
#else
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "komodod compiled without wallet and -mineraddress not set");
#endif
    }
    if (!Params().MineBlocksOnDemand())
    {
        if ( params[0].get_int() == 1 )
        {
            mapArgs["disablemining"] = "1";
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Mining Disabled");
        }
        else 
        {
            mapArgs["disablemining"] = "0";
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Mining Enabled");
        }
    }

    int nHeightStart = 0;
    int nHeightEnd = 0;
    int nHeight = 0;
    int nGenerate = params[0].get_int();
#ifdef ENABLE_WALLET
    CReserveKey reservekey(pwalletMain);
#endif

    {   // Don't keep cs_main locked
        LOCK(cs_main);
        nHeightStart = chainActive.Height();
        nHeight = nHeightStart;
        nHeightEnd = nHeightStart+nGenerate;
    }
    unsigned int nExtraNonce = 0;
    UniValue blockHashes(UniValue::VARR);
    unsigned int n = Params().EquihashN();
    unsigned int k = Params().EquihashK();
    uint64_t lastTime = 0;
    while (nHeight < nHeightEnd)
    {
        // Validation may fail if block generation is too fast
        if (GetTime() == lastTime) MilliSleep(1001);
        lastTime = GetTime();

#ifdef ENABLE_WALLET
        std::unique_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey,nHeight,KOMODO_MAXGPUCOUNT));
#else
        std::unique_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey());
#endif
        if (!pblocktemplate.get())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet keypool empty");
        CBlock *pblock = &pblocktemplate->block;
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock, chainActive.LastTip(), nExtraNonce);
        }

        // Hash state
        crypto_generichash_blake2b_state eh_state;
        EhInitialiseState(n, k, eh_state);

        // I = the block header minus nonce and solution.
        CEquihashInput I{*pblock};
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << I;

        // H(I||...
        crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());

        while (true) {
            // Yes, there is a chance every nonce could fail to satisfy the -regtest
            // target -- 1 in 2^(2^256). That ain't gonna happen
            pblock->nNonce = ArithToUint256(UintToArith256(pblock->nNonce) + 1);

            // H(I||V||...
            crypto_generichash_blake2b_state curr_state;
            curr_state = eh_state;
            crypto_generichash_blake2b_update(&curr_state,
                                              pblock->nNonce.begin(),
                                              pblock->nNonce.size());

            // (x_1, x_2, ...) = A(I, V, n, k)
            std::function<bool(std::vector<unsigned char>)> validBlock =
                    [&pblock](std::vector<unsigned char> soln)
            {
                LOCK(cs_main);
                pblock->nSolution = soln;
                solutionTargetChecks.increment();
                return CheckProofOfWork(*pblock,NOTARY_PUBKEY33,chainActive.Height(),Params().GetConsensus());
            };
            bool found = EhBasicSolveUncancellable(n, k, curr_state, validBlock);
            ehSolverRuns.increment();
            if (found) {
                goto endloop;
            }
        }
endloop:
        CValidationState state;
        if (!ProcessNewBlock(1,chainActive.LastTip()->GetHeight()+1,state, NULL, pblock, true, NULL))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());
    }
    return blockHashes;
}


UniValue setgenerate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate generate ( genproclimit )\n"
            "\nSet 'generate' true to turn either mining/generation or minting/staking on and false to turn both off.\n"
            "Mining is limited to 'genproclimit' processors, -1 is unlimited, setgenerate true with 0 genproclimit turns on staking\n"
            "See the getgenerate call for the current setting.\n"
            "\nArguments:\n"
            "1. generate         (boolean, required) Set to true to turn on generation, off to turn off.\n"
            "2. genproclimit     (numeric, optional) Set processor limit when generation is on. Can be -1 for unlimited, 0 to turn on staking.\n"
            "\nExamples:\n"
            "\nSet the generation on with a limit of one processor\n"
            + HelpExampleCli("setgenerate", "true 1") +
            "\nTurn minting/staking on\n"
            + HelpExampleCli("setgenerate", "true 0") +
            "\nCheck the setting\n"
            + HelpExampleCli("getgenerate", "") +
            "\nTurn off generation and minting\n"
            + HelpExampleCli("setgenerate", "false") +
            "\nUsing json rpc\n"
            + HelpExampleRpc("setgenerate", "true, 1")
        );

    if (GetArg("-mineraddress", "").empty()) {
#ifdef ENABLE_WALLET
        if (!pwalletMain) {
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Wallet disabled and -mineraddress not set");
        }
#else
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "komodod compiled without wallet and -mineraddress not set");
#endif
    }
    if (Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Use the generate method instead of setgenerate on this network");

    bool fGenerate = true;
    if (params.size() > 0)
        fGenerate = params[0].get_bool();

    int nGenProcLimit = GetArg("-genproclimit", 0);;
    if (params.size() > 1)
    {
        nGenProcLimit = params[1].get_int();
        //if (nGenProcLimit == 0)
        //    fGenerate = false;
    }
    if ( ASSETCHAINS_LWMAPOS != 0 )
    {
        if (fGenerate && !nGenProcLimit)
        {
            VERUS_MINTBLOCKS = 1;
            fGenerate = GetBoolArg("-gen", false);
            KOMODO_MININGTHREADS = nGenProcLimit;
        }
        else if (!fGenerate)
        {
            VERUS_MINTBLOCKS = 0;
            KOMODO_MININGTHREADS = 0;
        }
        else KOMODO_MININGTHREADS = (int32_t)nGenProcLimit;
    }
    else
    {
        KOMODO_MININGTHREADS = (int32_t)nGenProcLimit;
    }

    mapArgs["-gen"] = (fGenerate ? "1" : "0");
    mapArgs ["-genproclimit"] = itostr(KOMODO_MININGTHREADS);

#ifdef ENABLE_WALLET
    GenerateBitcoins(fGenerate, pwalletMain, nGenProcLimit);
#else
    GenerateBitcoins(fGenerate, nGenProcLimit);
#endif

    return NullUniValue;
}
#endif

CBlockIndex *komodo_chainactive(int32_t height);
arith_uint256 zawy_ctB(arith_uint256 bnTarget,uint32_t solvetime);

UniValue genminingCSV(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t i,z,height; uint32_t solvetime,prevtime=0; FILE *fp; char str[65],str2[65],fname[256]; uint256 hash; arith_uint256 bnTarget; CBlockIndex *pindex; bool fNegative,fOverflow; UniValue result(UniValue::VOBJ);
    if (fHelp || params.size() != 0 )
        throw runtime_error("genminingCSV\n");
    LOCK(cs_main);
    sprintf(fname,"%s_mining.csv",ASSETCHAINS_SYMBOL[0] == 0 ? "KMD" : ASSETCHAINS_SYMBOL);
    if ( (fp= fopen(fname,"wb")) != 0 )
    {
        fprintf(fp,"height,nTime,nBits,bnTarget,bnTargetB,diff,solvetime\n");
        height = komodo_nextheight();
        for (i=0; i<height; i++)
        {
            if ( (pindex= komodo_chainactive(i)) != 0 )
            {
                bnTarget.SetCompact(pindex->nBits,&fNegative,&fOverflow);
                solvetime = (prevtime==0) ? 0 : (int32_t)(pindex->nTime - prevtime);
                for (z=0; z<16; z++)
                    sprintf(&str[z<<1],"%02x",((uint8_t *)&bnTarget)[31-z]);
                str[32] = 0;
                //hash = pindex->GetBlockHash();
                memset(&hash,0,sizeof(hash));
                if ( i >= 64 && (pindex->nBits & 3) != 0 )
                    hash = ArithToUint256(zawy_ctB(bnTarget,solvetime));
                for (z=0; z<16; z++)
                    sprintf(&str2[z<<1],"%02x",((uint8_t *)&hash)[31-z]);
                str2[32] = 0; fprintf(fp,"%d,%u,%08x,%s,%s,%.1f,%d\n",i,pindex->nTime,pindex->nBits,str,str2,GetDifficulty(pindex),solvetime);
                prevtime = pindex->nTime;
            }
        }
        fclose(fp);
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("created", fname));
    }
    else
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("error", "couldnt create mining.csv"));
        result.push_back(Pair("filename", fname));
    }
    return(result);
}
                            
UniValue getmininginfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\nReturns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) The current block\n"
            "  \"currentblocksize\": nnn,   (numeric) The last block size\n"
            "  \"currentblocktx\": nnn,     (numeric) The last block transaction\n"
            "  \"difficulty\": xxx.xxxxx    (numeric) The current difficulty\n"
            "  \"errors\": \"...\"          (string) Current errors\n"
            "  \"generate\": true|false     (boolean) If the generation is on or off (see getgenerate or setgenerate calls)\n"
            "  \"genproclimit\": n          (numeric) The processor limit for generation. -1 if no generation. (see getgenerate or setgenerate calls)\n"
            "  \"localsolps\": xxx.xxxxx    (numeric) The average local solution rate in Sol/s since this node was started\n"
            "  \"networksolps\": x          (numeric) The estimated network solution rate in Sol/s\n"
            "  \"pooledtx\": n              (numeric) The size of the mem pool\n"
            "  \"testnet\": true|false      (boolean) If using testnet or not\n"
            "  \"chain\": \"xxxx\",         (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmininginfo", "")
            + HelpExampleRpc("getmininginfo", "")
        );


    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("blocks",           (int)chainActive.Height()));
    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx",   (uint64_t)nLastBlockTx));
    obj.push_back(Pair("difficulty",       (double)GetNetworkDifficulty()));
    obj.push_back(Pair("errors",           GetWarnings("statusbar")));
    obj.push_back(Pair("genproclimit",     (int)GetArg("-genproclimit", -1)));
    if (ASSETCHAINS_ALGO == ASSETCHAINS_EQUIHASH)
    {
        obj.push_back(Pair("localsolps"  , getlocalsolps(params, false, mypk)));
        obj.push_back(Pair("networksolps", getnetworksolps(params, false, mypk)));
    }
    else
    {
        obj.push_back(Pair("localhashps"  , GetBoolArg("-gen", false) ? getlocalsolps(params, false, mypk) : (double)0.0));
    }
    obj.push_back(Pair("networkhashps",    getnetworksolps(params, false, mypk)));
    obj.push_back(Pair("pooledtx",         (uint64_t)mempool.size()));
    obj.push_back(Pair("testnet",          Params().TestnetToBeDeprecatedFieldRPC()));
    obj.push_back(Pair("chain",            Params().NetworkIDString()));
#ifdef ENABLE_MINING
    bool staking = VERUS_MINTBLOCKS;
    if ( ASSETCHAINS_STAKED != 0 && GetBoolArg("-gen", false) && GetBoolArg("-genproclimit", -1) == 0 )
        staking = true;
    obj.push_back(Pair("staking",          staking));
    obj.push_back(Pair("generate",         GetBoolArg("-gen", false) && GetBoolArg("-genproclimit", -1) != 0 ));
    obj.push_back(Pair("numthreads",       (int64_t)KOMODO_MININGTHREADS));
#endif
    return obj;
}


// NOTE: Unlike wallet RPC (which use BTC values), mining RPCs follow GBT (BIP 22) in using satoshi amounts
UniValue prioritisetransaction(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "prioritisetransaction <txid> <priority delta> <fee delta>\n"
            "Accepts the transaction into mined blocks at a higher (or lower) priority\n"
            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id.\n"
            "2. priority delta (numeric, required) The priority to add or subtract.\n"
            "                  The transaction selection algorithm considers the tx as it would have a higher priority.\n"
            "                  (priority of a transaction is calculated: coinage * value_in_satoshis / txsize) \n"
            "3. fee delta      (numeric, required) The fee value (in satoshis) to add (or subtract, if negative).\n"
            "                  The fee is not actually paid, only the algorithm for selecting transactions into a block\n"
            "                  considers the transaction as it would have paid a higher (or lower) fee.\n"
            "\nResult\n"
            "true              (boolean) Returns true\n"
            "\nExamples:\n"
            + HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000")
            + HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000")
        );

    LOCK(cs_main);

    uint256 hash = ParseHashStr(params[0].get_str(), "txid");
    CAmount nAmount = params[2].get_int64();

    mempool.PrioritiseTransaction(hash, params[0].get_str(), params[1].get_real(), nAmount);
    return true;
}


// NOTE: Assumes a conclusive result; if result is inconclusive, it must be handled by caller
static UniValue BIP22ValidationResult(const CValidationState& state)
{
    if (state.IsValid())
        return NullUniValue;

    std::string strRejectReason = state.GetRejectReason();
    if (state.IsError())
        throw JSONRPCError(RPC_VERIFY_ERROR, strRejectReason);
    if (state.IsInvalid())
    {
        if (strRejectReason.empty())
            return "rejected";
        return strRejectReason;
    }
    // Should be impossible
    return "valid?";
}

UniValue getblocktemplate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getblocktemplate ( \"jsonrequestobject\" )\n"
            "\nIf the request parameters include a 'mode' key, that is used to explicitly select between the default 'template' request or a 'proposal'.\n"
            "It returns data needed to construct a block to work on.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments:\n"
            "1. \"jsonrequestobject\"       (string, optional) A json object in the following spec\n"
            "     {\n"
            "       \"mode\":\"template\"    (string, optional) This must be set to \"template\" or omitted\n"
            "       \"capabilities\":[       (array, optional) A list of strings\n"
            "           \"support\"           (string) client side supported feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', 'serverlist', 'workid'\n"
            "           ,...\n"
            "         ]\n"
            "     }\n"
            "\n"

            "\nResult:\n"
            "{\n"
            "  \"version\" : n,                     (numeric) The block version\n"
            "  \"previousblockhash\" : \"xxxx\",    (string) The hash of current highest block\n"
            "  \"finalsaplingroothash\" : \"xxxx\", (string) The hash of the final sapling root\n"
            "  \"transactions\" : [                 (array) contents of non-coinbase transactions that should be included in the next block\n"
            "      {\n"
            "         \"data\" : \"xxxx\",          (string) transaction data encoded in hexadecimal (byte-for-byte)\n"
            "         \"hash\" : \"xxxx\",          (string) hash/id encoded in little-endian hexadecimal\n"
            "         \"depends\" : [              (array) array of numbers \n"
            "             n                        (numeric) transactions before this one (by 1-based index in 'transactions' list) that must be present in the final block if this one is\n"
            "             ,...\n"
            "         ],\n"
            "         \"fee\": n,                   (numeric) difference in value between transaction inputs and outputs (in Satoshis); for coinbase transactions, this is a negative Number of the total collected block fees (ie, not including the block subsidy); if key is not present, fee is unknown and clients MUST NOT assume there isn't one\n"
            "         \"sigops\" : n,               (numeric) total number of SigOps, as counted for purposes of block limits; if key is not present, sigop count is unknown and clients MUST NOT assume there aren't any\n"
            "         \"required\" : true|false     (boolean) if provided and true, this transaction must be in the final block\n"
            "      }\n"
            "      ,...\n"
            "  ],\n"
//            "  \"coinbaseaux\" : {                  (json object) data that should be included in the coinbase's scriptSig content\n"
//            "      \"flags\" : \"flags\"            (string) \n"
//            "  },\n"
//            "  \"coinbasevalue\" : n,               (numeric) maximum allowable input to coinbase transaction, including the generation award and transaction fees (in Satoshis)\n"
            "  \"coinbasetxn\" : { ... },           (json object) information for coinbase transaction\n"
            "  \"target\" : \"xxxx\",               (string) The hash target\n"
            "  \"mintime\" : xxx,                   (numeric) The minimum timestamp appropriate for next block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mutable\" : [                      (array of string) list of ways the block template may be changed \n"
            "     \"value\"                         (string) A way the block template may be changed, e.g. 'time', 'transactions', 'prevblock'\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\",   (string) A range of valid nonces\n"
            "  \"sigoplimit\" : n,                 (numeric) limit of sigops in blocks\n"
            "  \"sizelimit\" : n,                  (numeric) limit of block size\n"
            "  \"curtime\" : ttt,                  (numeric) current timestamp in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"bits\" : \"xxx\",                 (string) compressed target of next block\n"
            "  \"height\" : n                      (numeric) The height of the next block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("getblocktemplate", "")
            + HelpExampleRpc("getblocktemplate", "")
         );

    LOCK(cs_main);

    // Wallet or miner address is required because we support coinbasetxn
    if (GetArg("-mineraddress", "").empty()) {
#ifdef ENABLE_WALLET
        if (!pwalletMain) {
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Wallet disabled and -mineraddress not set");
        }
#else
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "komodod compiled without wallet and -mineraddress not set");
#endif
    }
    
    if ( GetArg("disablemining",false) )
        throw JSONRPCError(RPC_TYPE_ERROR, "Mining is Disabled");

    UniValue lpval = NullUniValue;
    // TODO: Re-enable coinbasevalue once a specification has been written
    bool coinbasetxn = true;
    std::string strMode;
    if (params.size() > 0)
    {
        const UniValue& oparam = params[0].get_obj();
        const UniValue& modeval = find_value(oparam, "mode");
        if (modeval.isStr())
            strMode = modeval.get_str();
        else if (modeval.isNull())
        {
            strMode = "template";
        }
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
        lpval = find_value(oparam, "longpollid");

        if (strMode == "disablecb")
            coinbasetxn = false;

        if (strMode == "proposal")
        {
            const UniValue& dataval = find_value(oparam, "data");
            if (!dataval.isStr())
                throw JSONRPCError(RPC_TYPE_ERROR, "Missing data String key for proposal");

            CBlock block;
            if (!DecodeHexBlk(block, dataval.get_str()))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

            uint256 hash = block.GetHash();
            BlockMap::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end()) {
                CBlockIndex *pindex = mi->second;
                if (pindex)
                {
                    if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                        return "duplicate";
                    if (pindex->nStatus & BLOCK_FAILED_MASK)
                        return "duplicate-invalid";
                }
                return "duplicate-inconclusive";
            }

            CBlockIndex* const pindexPrev = chainActive.LastTip();
            // TestBlockValidity only supports blocks built on the current Tip
            if (block.hashPrevBlock != pindexPrev->GetBlockHash())
                return "inconclusive-not-best-prevblk";
            CValidationState state;
            TestBlockValidity(state, block, pindexPrev, false, true);
            return BIP22ValidationResult(state);
        }
    }
    else
    {
        strMode = "template";
    }

    bool fvNodesEmpty;
    {
        LOCK(cs_vNodes);
        fvNodesEmpty = vNodes.empty();
    }
    if (Params().MiningRequiresPeers() && (IsNotInSync() || fvNodesEmpty))
    {
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Cannot get a block template while no peers are connected or chain not in sync!");
    }

    //if (IsInitialBlockDownload())
     //   throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Zcash is downloading blocks...");

    static unsigned int nTransactionsUpdatedLast;

    if (!lpval.isNull())
    {
        // Wait to respond until either the best block changes, OR a minute has passed and there are more transactions
        uint256 hashWatchedChain;
        boost::system_time checktxtime;
        unsigned int nTransactionsUpdatedLastLP;

        if (lpval.isStr())
        {
            // Format: <hashBestChain><nTransactionsUpdatedLast>
            std::string lpstr = lpval.get_str();

            hashWatchedChain.SetHex(lpstr.substr(0, 64));
            nTransactionsUpdatedLastLP = atoi64(lpstr.substr(64));
        }
        else
        {
            // NOTE: Spec does not specify behaviour for non-string longpollid, but this makes testing easier
            hashWatchedChain = chainActive.LastTip()->GetBlockHash();
            nTransactionsUpdatedLastLP = nTransactionsUpdatedLast;
        }

        // Release the wallet and main lock while waiting
        LEAVE_CRITICAL_SECTION(cs_main);
        {
            checktxtime = boost::get_system_time() + boost::posix_time::minutes(1);

            boost::unique_lock<boost::mutex> lock(csBestBlock);
            while (chainActive.LastTip()->GetBlockHash() == hashWatchedChain && IsRPCRunning())
            {
                if (!cvBlockChange.timed_wait(lock, checktxtime))
                {
                    // Timeout: Check transactions for update
                    if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLastLP)
                        break;
                    checktxtime += boost::posix_time::seconds(10);
                }
            }
        }
        ENTER_CRITICAL_SECTION(cs_main);

        if (!IsRPCRunning())
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Shutting down");
        // TODO: Maybe recheck connections/IBD and (if something wrong) send an expires-immediately template to stop miners?
    }

    // Update block
    static CBlockIndex* pindexPrev;
    static int64_t nStart;
    static CBlockTemplate* pblocktemplate;
    if (pindexPrev != chainActive.LastTip() ||
        (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 5))
    {
        // Clear pindexPrev so future calls make a new block, despite any failures from here on
        pindexPrev = NULL;

        // Store the pindexBest used before CreateNewBlockWithKey, to avoid races
        nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
        CBlockIndex* pindexPrevNew = chainActive.LastTip();
        nStart = GetTime();

        // Create new block
        if(pblocktemplate)
        {
            delete pblocktemplate;
            pblocktemplate = NULL;
        }
#ifdef ENABLE_WALLET
        CReserveKey reservekey(pwalletMain);
        LEAVE_CRITICAL_SECTION(cs_main);
        pblocktemplate = CreateNewBlockWithKey(reservekey,pindexPrevNew->GetHeight()+1,KOMODO_MAXGPUCOUNT,false);
#else
        pblocktemplate = CreateNewBlockWithKey();
#endif
        ENTER_CRITICAL_SECTION(cs_main);
        if (!pblocktemplate)
            throw std::runtime_error("CreateNewBlock(): create block failed");
            //throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory or no available utxo for staking");

        // Need to update only after we know CreateNewBlockWithKey succeeded
        pindexPrev = pindexPrevNew;
    }
    CBlock* pblock = &pblocktemplate->block; // pointer for convenience

    // Update nTime
    UpdateTime(pblock, Params().GetConsensus(), pindexPrev);
    pblock->nNonce = uint256();

    UniValue aCaps(UniValue::VARR); aCaps.push_back("proposal");

    UniValue txCoinbase = NullUniValue;
    UniValue transactions(UniValue::VARR);
    map<uint256, int64_t> setTxIndex;
    int i = 0;
    BOOST_FOREACH (const CTransaction& tx, pblock->vtx) {
        uint256 txHash = tx.GetHash();
        setTxIndex[txHash] = i++;

        //if (tx.IsCoinBase() && !coinbasetxn)
        //    continue;

        UniValue entry(UniValue::VOBJ);

        entry.push_back(Pair("data", EncodeHexTx(tx)));

        entry.push_back(Pair("hash", txHash.GetHex()));

        UniValue deps(UniValue::VARR);
        BOOST_FOREACH (const CTxIn &in, tx.vin)
        {
            if (setTxIndex.count(in.prevout.hash))
                deps.push_back(setTxIndex[in.prevout.hash]);
        }
        entry.push_back(Pair("depends", deps));

        int index_in_template = i - 1;
        entry.push_back(Pair("fee", pblocktemplate->vTxFees[index_in_template]));
        entry.push_back(Pair("sigops", pblocktemplate->vTxSigOps[index_in_template]));

        if (tx.IsCoinBase() && coinbasetxn == true ) {
            // Show founders' reward if it is required
            if (ASSETCHAINS_FOUNDERS && pblock->vtx[0].vout.size() > 1) {
                // Correct this if GetBlockTemplate changes the order
                entry.push_back(Pair("foundersreward", (int64_t)tx.vout[1].nValue));
            }
            CAmount nReward = GetBlockSubsidy(chainActive.LastTip()->GetHeight()+1, Params().GetConsensus());
            entry.push_back(Pair("coinbasevalue", nReward));
            entry.push_back(Pair("required", true));
            txCoinbase = entry;
        } else
            transactions.push_back(entry);
    }

    UniValue aux(UniValue::VOBJ);
    aux.push_back(Pair("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end())));

    arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);

    static UniValue aMutable(UniValue::VARR);
    if (aMutable.empty())
    {
        aMutable.push_back("time");
        aMutable.push_back("transactions");
        aMutable.push_back("prevblock");
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("capabilities", aCaps));
    result.push_back(Pair("version", pblock->nVersion));
    result.push_back(Pair("previousblockhash", pblock->hashPrevBlock.GetHex()));
    result.push_back(Pair("finalsaplingroothash", pblock->hashFinalSaplingRoot.GetHex()));
    result.push_back(Pair("transactions", transactions));
    if (coinbasetxn) {
        assert(txCoinbase.isObject());
        result.push_back(Pair("coinbasetxn", txCoinbase));
    } // else {
      //  result.push_back(Pair("coinbaseaux", aux));
      //  result.push_back(Pair("coinbasevalue", (int64_t)pblock->vtx[0].vout[0].nValue));
    //}
    result.push_back(Pair("longpollid", chainActive.LastTip()->GetBlockHash().GetHex() + i64tostr(nTransactionsUpdatedLast)));
    if ( ASSETCHAINS_STAKED != 0 )
    {
        arith_uint256 POWtarget; int32_t PoSperc;
        POWtarget = komodo_PoWtarget(&PoSperc,hashTarget,(int32_t)(pindexPrev->GetHeight()+1),ASSETCHAINS_STAKED,komodo_newStakerActive(chainActive.Height()+1, pblock->nTime));
        result.push_back(Pair("target", POWtarget.GetHex()));
        result.push_back(Pair("PoSperc", (int64_t)PoSperc));
        result.push_back(Pair("ac_staked", (int64_t)ASSETCHAINS_STAKED));
        result.push_back(Pair("origtarget", hashTarget.GetHex()));
    }
    /*else if ( ASSETCHAINS_ADAPTIVEPOW > 0 )
        result.push_back(Pair("target",komodo_adaptivepow_target((int32_t)(pindexPrev->GetHeight()+1),hashTarget,pblock->nTime).GetHex()));*/
    else
        result.push_back(Pair("target", hashTarget.GetHex()));
    result.push_back(Pair("mintime", (int64_t)pindexPrev->GetMedianTimePast()+1));
    result.push_back(Pair("mutable", aMutable));
    result.push_back(Pair("noncerange", "00000000ffffffff"));
    result.push_back(Pair("sigoplimit", (int64_t)MAX_BLOCK_SIGOPS));
    result.push_back(Pair("sizelimit", (int64_t)MAX_BLOCK_SIZE(chainActive.LastTip()->GetHeight()+1)));
    result.push_back(Pair("curtime", pblock->GetBlockTime()));
    result.push_back(Pair("bits", strprintf("%08x", pblock->nBits)));
    result.push_back(Pair("height", (int64_t)(pindexPrev->GetHeight()+1)));

    //fprintf(stderr,"return complete template\n");
    return result;
}


class submitblock_StateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    CValidationState state;

    submitblock_StateCatcher(const uint256 &hashIn) : hash(hashIn), found(false), state() {};

protected:
    virtual void BlockChecked(const CBlock& block, const CValidationState& stateIn) {
        if (block.GetHash() != hash)
            return;
        found = true;
        state = stateIn;
    };
};

UniValue submitblock(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "submitblock \"hexdata\" ( \"jsonparametersobject\" )\n"
            "\nAttempts to submit new block to network.\n"
            "The 'jsonparametersobject' parameter is currently ignored.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments\n"
            "1. \"hexdata\"    (string, required) the hex-encoded block data to submit\n"
            "2. \"jsonparametersobject\"     (string, optional) object of optional parameters\n"
            "    {\n"
            "      \"workid\" : \"id\"    (string, optional) if the server provided a workid, it MUST be included with submissions\n"
            "    }\n"
            "\nResult:\n"
            "\"duplicate\" - node already has valid copy of block\n"
            "\"duplicate-invalid\" - node already has block, but it is invalid\n"
            "\"duplicate-inconclusive\" - node already has block but has not validated it\n"
            "\"inconclusive\" - node has not validated the block, it may not be on the node's current best chain\n"
            "\"rejected\" - block was rejected as invalid\n"
            "For more information on submitblock parameters and results, see: https://github.com/bitcoin/bips/blob/master/bip-0022.mediawiki#block-submission\n"
            "\nExamples:\n"
            + HelpExampleCli("submitblock", "\"mydata\"")
            + HelpExampleRpc("submitblock", "\"mydata\"")
        );

    CBlock block;
    //LogPrintStr("Hex block submission: " + params[0].get_str());
    if (!DecodeHexBlk(block, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

    uint256 hash = block.GetHash();
    bool fBlockPresent = false;
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndex *pindex = mi->second;
            if (pindex)
            {
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                    return "duplicate";
                if (pindex->nStatus & BLOCK_FAILED_MASK)
                    return "duplicate-invalid";
                // Otherwise, we might only have the header - process the block before returning
                fBlockPresent = true;
            }
        }
    }

    CValidationState state;
    submitblock_StateCatcher sc(block.GetHash());
    RegisterValidationInterface(&sc);
    //printf("submitblock, height=%d, coinbase sequence: %d, scriptSig: %s\n", chainActive.LastTip()->GetHeight()+1, block.vtx[0].vin[0].nSequence, block.vtx[0].vin[0].scriptSig.ToString().c_str());
    bool fAccepted = ProcessNewBlock(1,chainActive.LastTip()->GetHeight()+1,state, NULL, &block, true, NULL);
    UnregisterValidationInterface(&sc);
    if (fBlockPresent)
    {
        if (fAccepted && !sc.found)
            return "duplicate-inconclusive";
        return "duplicate";
    }
    if (fAccepted)
    {
        if (!sc.found)
            return "inconclusive";
        state = sc.state;
    }
    return BIP22ValidationResult(state);
}

UniValue estimatefee(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatefee nblocks\n"
            "\nEstimates the approximate fee per kilobyte\n"
            "needed for a transaction to begin confirmation\n"
            "within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric)\n"
            "\nResult:\n"
            "n :    (numeric) estimated fee-per-kilobyte\n"
            "\n"
            "-1.0 is returned if not enough transactions and\n"
            "blocks have been observed to make an estimate.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatefee", "6")
            );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));

    int nBlocks = params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    CFeeRate feeRate = mempool.estimateFee(nBlocks);
    if (feeRate == CFeeRate(0))
        return -1.0;

    return ValueFromAmount(feeRate.GetFeePerK());
}

UniValue estimatepriority(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatepriority nblocks\n"
            "\nEstimates the approximate priority\n"
            "a zero-fee transaction needs to begin confirmation\n"
            "within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric)\n"
            "\nResult:\n"
            "n :    (numeric) estimated priority\n"
            "\n"
            "-1.0 is returned if not enough transactions and\n"
            "blocks have been observed to make an estimate.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatepriority", "6")
            );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));

    int nBlocks = params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    return mempool.estimatePriority(nBlocks);
}

UniValue getblocksubsidy(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getblocksubsidy height\n"
            "\nReturns block subsidy reward, taking into account the mining slow start and the founders reward, of block at index provided.\n"
            "\nArguments:\n"
            "1. height         (numeric, optional) The block height.  If not provided, defaults to the current height of the chain.\n"
            "\nResult:\n"
            "{\n"
            "  \"miner\" : x.xxx           (numeric) The mining reward amount in KMD.\n"
            "  \"ac_pubkey\" : x.xxx       (numeric) The mining reward amount in KMD.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblocksubsidy", "1000")
            + HelpExampleRpc("getblocksubsidy", "1000")
        );

    LOCK(cs_main);
    int nHeight = (params.size()==1) ? params[0].get_int() : chainActive.Height();
    if (nHeight < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
    
    CAmount nFoundersReward = 0;
    CAmount nReward = GetBlockSubsidy(nHeight, Params().GetConsensus());
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("miner", ValueFromAmount(nReward)));
    
    if ( strlen(ASSETCHAINS_OVERRIDE_PUBKEY.c_str()) == 66 || ASSETCHAINS_SCRIPTPUB.size() > 1 )
    {
        if ( ASSETCHAINS_FOUNDERS == 0 && ASSETCHAINS_COMMISSION != 0 )
        {
            // ac comission chains need the block to exist to calulate the reward.
            if ( nHeight <= chainActive.Height() )
            {
                CBlockIndex* pblockIndex = chainActive[nHeight];
                CBlock block;
                if ( komodo_blockload(block, pblockIndex) == 0 )
                    nFoundersReward = komodo_commission(&block, nHeight);
            }
        }
        else if ( ASSETCHAINS_FOUNDERS != 0 )
        {
            // Assetchains founders chains have a fixed reward so can be calculated at any given height.
            nFoundersReward = komodo_commission(0, nHeight);
        }
        result.push_back(Pair("ac_pubkey", ValueFromAmount(nFoundersReward)));
    }
    return result;
}


static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "mining",             "getlocalsolps",          &getlocalsolps,          true  },
    { "mining",             "getnetworksolps",        &getnetworksolps,        true  },
    { "mining",             "getnetworkhashps",       &getnetworkhashps,       true  },
    { "mining",             "getmininginfo",          &getmininginfo,          true  },
    { "mining",             "prioritisetransaction",  &prioritisetransaction,  true  },
    { "mining",             "getblocktemplate",       &getblocktemplate,       true  },
    { "mining",             "submitblock",            &submitblock,            true  },
    { "mining",             "getblocksubsidy",        &getblocksubsidy,        true  },

#ifdef ENABLE_MINING
    { "generating",         "getgenerate",            &getgenerate,            true  },
    { "generating",         "setgenerate",            &setgenerate,            true  },
    { "generating",         "generate",               &generate,               true  },
#endif

    { "util",               "estimatefee",            &estimatefee,            true  },
    { "util",               "estimatepriority",       &estimatepriority,       true  },
};

void RegisterMiningRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
