// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "amount.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/funding.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "core_io.h"
#ifdef ENABLE_MINING
#include "crypto/equihash.h"
#endif
#include "deprecation.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "metrics.h"
#include "miner.h"
#include "net.h"
#include "pow.h"
#include "rpc/server.h"
#include "txmempool.h"
#include "util/match.h"
#include "util/system.h"
#include "validationinterface.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <stdint.h>
#include <variant>

#include <boost/assign/list_of.hpp>
#include <boost/shared_ptr.hpp>

#include <univalue.h>

using namespace std;

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or over the difficulty averaging window if 'lookup' is nonpositive.
 * If 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
int64_t GetNetworkHashPS(int lookup, int height) {
    CBlockIndex *pb = chainActive.Tip();

    if (height >= 0 && height < chainActive.Height())
        pb = chainActive[height];

    if (pb == NULL || !pb->nHeight)
        return 0;

    // If lookup is nonpositive, then use difficulty averaging window.
    if (lookup <= 0)
        lookup = Params().GetConsensus().nPowAveragingWindow;

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nHeight)
        lookup = pb->nHeight;

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

    arith_uint256 workDiff = pb->nChainWork - pb0->nChainWork;
    int64_t timeDiff = maxTime - minTime;

    return (int64_t)(workDiff.getdouble() / timeDiff);
}

UniValue getlocalsolps(const UniValue& params, bool fHelp)
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

UniValue getnetworksolps(const UniValue& params, bool fHelp)
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

UniValue getnetworkhashps(const UniValue& params, bool fHelp)
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
UniValue getgenerate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "\nReturn if the server is set to generate coins or not. The default is false.\n"
            "It is set with the command line argument -gen (or " + std::string(BITCOIN_CONF_FILENAME) + " setting gen)\n"
            "It can also be set with the setgenerate call.\n"
            "\nResult\n"
            "true|false      (boolean) If the server is set to generate coins or not\n"
            "\nExamples:\n"
            + HelpExampleCli("getgenerate", "")
            + HelpExampleRpc("getgenerate", "")
        );

    LOCK(cs_main);
    return GetBoolArg("-gen", DEFAULT_GENERATE);
}

UniValue generate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "generate numblocks\n"
            "\nMine blocks immediately (before the RPC call returns)\n"
            "\nNote: this function can only be used on the regtest network\n"
            "\nArguments:\n"
            "1. numblocks    (numeric, required) How many blocks are generated immediately.\n"
            "\nResult\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            + HelpExampleCli("generate", "11")
        );

    if (!Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "This method can only be used on regtest");

    int nHeightStart = 0;
    int nHeightEnd = 0;
    int nHeight = 0;
    int nGenerate = params[0].get_int();

    std::optional<MinerAddress> maybeMinerAddress;
    GetMainSignals().AddressForMining(maybeMinerAddress);

    // Throw an error if no address valid for mining was provided.
    if (!maybeMinerAddress.has_value()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No miner address available (mining requires a wallet or -mineraddress)");
    } else {
        // Detect and handle keypool exhaustion separately from IsValidMinerAddress().
        auto resv = std::get_if<boost::shared_ptr<CReserveScript>>(&maybeMinerAddress.value());
        if (resv && !resv->get()) {
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
        }

        // Catch any other invalid miner address issues.
        if (!std::visit(IsValidMinerAddress(), maybeMinerAddress.value())) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Miner address is invalid");
        }
    }
    auto minerAddress = maybeMinerAddress.value();

    {   // Don't keep cs_main locked
        LOCK(cs_main);
        nHeightStart = chainActive.Height();
        nHeight = nHeightStart;
        nHeightEnd = nHeightStart+nGenerate;
    }
    unsigned int nExtraNonce = 0;
    UniValue blockHashes(UniValue::VARR);
    unsigned int n = Params().GetConsensus().nEquihashN;
    unsigned int k = Params().GetConsensus().nEquihashK;
    while (nHeight < nHeightEnd)
    {
        std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(minerAddress));
        if (!pblocktemplate.get())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");
        CBlock *pblock = &pblocktemplate->block;
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblocktemplate.get(), chainActive.Tip(), nExtraNonce, Params().GetConsensus());
        }

        // Hash state
        eh_HashState eh_state = EhInitialiseState(n, k);

        // I = the block header minus nonce and solution.
        CEquihashInput I{*pblock};
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << I;

        // H(I||...
        eh_state.Update((unsigned char*)&ss[0], ss.size());

        while (true) {
            // Yes, there is a chance every nonce could fail to satisfy the -regtest
            // target -- 1 in 2^(2^256). That ain't gonna happen
            pblock->nNonce = ArithToUint256(UintToArith256(pblock->nNonce) + 1);

            // H(I||V||...
            eh_HashState curr_state(eh_state);
            curr_state.Update(pblock->nNonce.begin(), pblock->nNonce.size());

            // (x_1, x_2, ...) = A(I, V, n, k)
            std::function<bool(std::vector<unsigned char>)> validBlock =
                    [&pblock](std::vector<unsigned char> soln) {
                pblock->nSolution = soln;
                solutionTargetChecks.increment();
                return CheckProofOfWork(pblock->GetHash(), pblock->nBits, Params().GetConsensus());
            };
            bool found = EhBasicSolveUncancellable(n, k, curr_state, validBlock);
            ehSolverRuns.increment();
            if (found) {
                goto endloop;
            }
        }
endloop:
        CValidationState state;
        if (!ProcessNewBlock(state, Params(), NULL, pblock, true, NULL))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());

        //mark miner address as important because it was used at least for one coinbase output
        std::visit(KeepMinerAddress(), minerAddress);
    }
    return blockHashes;
}

UniValue setgenerate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate generate ( genproclimit )\n"
            "\nSet 'generate' true or false to turn generation on or off.\n"
            "Generation is limited to 'genproclimit' processors, -1 is unlimited.\n"
            "See the getgenerate call for the current setting.\n"
            "\nArguments:\n"
            "1. generate         (boolean, required) Set to true to turn on generation, off to turn off.\n"
            "2. genproclimit     (numeric, optional) Set the processor limit for when generation is on. Can be -1 for unlimited.\n"
            "\nExamples:\n"
            "\nSet the generation on with a limit of one processor\n"
            + HelpExampleCli("setgenerate", "true 1") +
            "\nCheck the setting\n"
            + HelpExampleCli("getgenerate", "") +
            "\nTurn off generation\n"
            + HelpExampleCli("setgenerate", "false") +
            "\nUsing json rpc\n"
            + HelpExampleRpc("setgenerate", "true, 1")
        );

    if (Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Use the generate method instead of setgenerate on this network");

    bool fGenerate = true;
    if (params.size() > 0)
        fGenerate = params[0].get_bool();

    int nGenProcLimit = GetArg("-genproclimit", DEFAULT_GENERATE_THREADS);
    if (params.size() > 1)
    {
        nGenProcLimit = params[1].get_int();
        if (nGenProcLimit == 0)
            fGenerate = false;
    }

    mapArgs["-gen"] = (fGenerate ? "1" : "0");
    mapArgs ["-genproclimit"] = itostr(nGenProcLimit);
    GenerateBitcoins(fGenerate, nGenProcLimit, Params());

    return NullUniValue;
}
#endif

UniValue getmininginfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\nReturns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) The current block\n"
            "  \"currentblocksize\": nnn,   (numeric, optional) The block size of the last assembled block (only present if a block was ever assembled)\n"
            "  \"currentblocktx\": nnn,     (numeric, optional) The number of block non-coinbase transactions of the last assembled block (only present if a block was ever assembled)\n"
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
    obj.pushKV("blocks",           (int)chainActive.Height());
    if (last_block_size.has_value()) obj.pushKV("currentblocksize", last_block_size.value());
    if (last_block_num_txs.has_value()) obj.pushKV("currentblocktx", last_block_num_txs.value());
    obj.pushKV("difficulty",       (double)GetNetworkDifficulty());
    auto warnings = GetWarnings("statusbar");
    obj.pushKV("errors",           warnings.first);
    obj.pushKV("errorstimestamp",  warnings.second);
    obj.pushKV("genproclimit",     (int)GetArg("-genproclimit", DEFAULT_GENERATE_THREADS));
    obj.pushKV("localsolps"  ,     getlocalsolps(params, false));
    obj.pushKV("networksolps",     getnetworksolps(params, false));
    obj.pushKV("networkhashps",    getnetworksolps(params, false));
    obj.pushKV("pooledtx",         (uint64_t)mempool.size());
    obj.pushKV("testnet",          Params().TestnetToBeDeprecatedFieldRPC());
    obj.pushKV("chain",            Params().NetworkIDString());
#ifdef ENABLE_MINING
    obj.pushKV("generate",         getgenerate(params, false));
#endif
    return obj;
}


// NOTE: Unlike wallet RPCs (which use ZEC values), mining RPCs follow GBT (BIP 22) in using zatoshi amounts.
UniValue prioritisetransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw std::runtime_error(
            "prioritisetransaction <txid> <priority delta> <fee delta>\n"
            "Accepts the transaction into mined blocks at a higher (or lower) priority\n"
            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id.\n"
            "2. priority_delta (numeric, optional) Fee-independent priority adjustment. Not supported, so must be zero or null.\n"
            "3. fee_delta      (numeric, required) The fee value (in " + MINOR_CURRENCY_UNIT + ") to add (or subtract, if negative).\n"
            "                  The modified fee is not actually paid, only the algorithm for selecting transactions into a block\n"
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

    if (!(params[1].isNull() || params[1].get_real() == 0)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Priority is not supported, and adjustment thereof must be zero.");
    }

    mempool.PrioritiseTransaction(hash, params[0].get_str(), nAmount);
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

UniValue getblocktemplate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getblocktemplate ( \"jsonrequestobject\" )\n"
            "\nIf the request parameters include a 'mode' key, that is used to explicitly select between the default 'template' request or a 'proposal'.\n"
            "It returns data needed to construct a block to work on.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nTo obtain information about founder's reward or funding stream\n"
            "amounts, use 'getblocksubsidy HEIGHT' passing in the height returned\n"
            "by this API.\n"

            "\nThe roots returned in 'defaultroots' are only valid if the block template is\n"
            "used unmodified. If any part of the block template marked as 'mutable' in the\n"
            "output is mutated, these roots may need to be recomputed. For more information\n"
            "on the derivation process, see ZIP 244.\n"

            "\nArguments:\n"
            "1. \"jsonrequestobject\"       (string, optional) A json object in the following spec\n"
            "     {\n"
            "       \"mode\":\"template,\"    (string, optional) This must be set to \"template\" or omitted\n"
            "       \"capabilities\":[      (array, optional) A list of strings\n"
            "           \"support\"         (string) client side supported feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', 'serverlist', 'workid'\n"
            "           ,...\n"
            "         ],\n"
            "       \"longpollid\":\"id\"     (string, optional) id to wait for\n"
            "     }\n"
            "\n"

            "\nResult:\n"
            "{\n"
            "  \"version\" : n,                     (numeric) The block version\n"
            "  \"previousblockhash\" : \"xxxx\",      (string) The hash of current highest block\n"
            "  \"blockcommitmentshash\" : \"xxxx\",   (string) (DEPRECATED) The hash of the block commitments field in the block header\n"
            "  \"lightclientroothash\" : \"xxxx\",    (string) (DEPRECATED) The hash of the light client root field in the block header\n"
            "  \"finalsaplingroothash\" : \"xxxx\",   (string) (DEPRECATED) The hash of the light client root field in the block header\n"
            "  \"defaultroots\" : {                 (json object) root hashes that need to be recomputed if the transaction set is modified\n"
            "     \"merkleroot\" : \"xxxx\"           (string) The hash of the transactions in the block header\n"
            "     \"chainhistoryroot\" : \"xxxx\"     (string) The hash of the chain history\n"
            "     \"authdataroot\" : \"xxxx\"         (string) (From NU5) The hash of the authorizing data merkel tree\n"
            "     \"blockcommitmentshash\" : \"xxxx\" (string) (From NU5) The hash of the block commitments field in the block header\n"
            "  }\n"
            "  \"transactions\" : [                 (array) contents of non-coinbase transactions that should be included in the next block\n"
            "      {\n"
            "         \"data\" : \"xxxx\",            (string) transaction data encoded in hexadecimal (byte-for-byte)\n"
            "         \"hash\" : \"xxxx\",            (string) hash/id encoded in little-endian hexadecimal\n"
            "         \"depends\" : [               (array) array of numbers \n"
            "             n                       (numeric) transactions before this one (by 1-based index in 'transactions' list) that must be present in the final block if this one is\n"
            "             ,...\n"
            "         ],\n"
            "         \"fee\": n,                   (numeric) difference in value between transaction inputs and outputs (in " + MINOR_CURRENCY_UNIT + "); for coinbase transactions, this is a negative Number of the total collected block fees (i.e., not including the block subsidy); if key is not present, fee is unknown and clients MUST NOT assume there isn't one\n"
            "         \"sigops\" : n,               (numeric) total number of SigOps, as counted for purposes of block limits; if key is not present, sigop count is unknown and clients MUST NOT assume there aren't any\n"
            "         \"required\" : true|false     (boolean) if provided and true, this transaction must be in the final block\n"
            "      }\n"
            "      ,...\n"
            "  ],\n"
//            "  \"coinbaseaux\" : {                  (json object) data that should be included in the coinbase's scriptSig content\n"
//            "      \"flags\" : \"flags\"            (string) \n"
//            "  },\n"
//            "  \"coinbasevalue\" : n,               (numeric) maximum allowable input to coinbase transaction, including the generation award and transaction fees (in " + MINOR_CURRENCY_UNIT + ")\n"
            "  \"coinbasetxn\" : { ... },           (json object) information for coinbase transaction\n"
            "  \"target\" : \"xxxx\",                 (string) The hash target\n"
            "  \"longpollid\" : \"str\",              (string) an id to include with a request to longpoll on an update to this template\n"
            "  \"mintime\" : xxx,                   (numeric) The minimum timestamp appropriate for next block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mutable\" : [                      (array of string) list of ways the block template may be changed \n"
            "     \"value\"                         (string) A way the block template may be changed, e.g. 'time', 'transactions', 'prevblock'\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\", (string) A range of valid nonces\n"
            "  \"sigoplimit\" : n,                  (numeric) limit of sigops in blocks\n"
            "  \"sizelimit\" : n,                   (numeric) limit of block size\n"
            "  \"curtime\" : ttt,                   (numeric) current timestamp in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"bits\" : \"xxx\",                    (string) compressed target of next block\n"
            "  \"height\" : n                       (numeric) The height of the next block\n"
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
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, DAEMON_NAME " compiled without wallet and -mineraddress not set");
#endif
    }

    std::string strMode = "template";
    UniValue lpval = NullUniValue;
    // TODO: Re-enable coinbasevalue once a specification has been written
    bool coinbasetxn = true;
    if (params.size() > 0)
    {
        const UniValue& oparam = params[0].get_obj();
        const UniValue& modeval = find_value(oparam, "mode");
        if (modeval.isStr())
            strMode = modeval.get_str();
        else if (modeval.isNull())
        {
            /* Do nothing */
        }
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
        lpval = find_value(oparam, "longpollid");

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
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                    return "duplicate";
                if (pindex->nStatus & BLOCK_FAILED_MASK)
                    return "duplicate-invalid";
                return "duplicate-inconclusive";
            }

            CBlockIndex* const pindexPrev = chainActive.Tip();
            // TestBlockValidity only supports blocks built on the current Tip
            if (block.hashPrevBlock != pindexPrev->GetBlockHash())
                return "inconclusive-not-best-prevblk";

            CValidationState state;
            TestBlockValidity(state, Params(), block, pindexPrev, false);
            return BIP22ValidationResult(state);
        }
    }

    if (strMode != "template")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");

    if (Params().NetworkIDString() != "regtest" && vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, DAEMON_NAME " is not connected!");

    if (IsInitialBlockDownload(Params().GetConsensus()))
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, DAEMON_NAME " is downloading blocks...");

    std::optional<MinerAddress> maybeMinerAddress;
    GetMainSignals().AddressForMining(maybeMinerAddress);

    // Throw an error if no address valid for mining was provided.
    if (!(maybeMinerAddress.has_value() && std::visit(IsValidMinerAddress(), maybeMinerAddress.value()))) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No miner address available (getblocktemplate requires a wallet or -mineraddress)");
    }
    auto minerAddress = maybeMinerAddress.value();

    static unsigned int nTransactionsUpdatedLast;
    static std::optional<CMutableTransaction> cached_next_cb_mtx;
    static int cached_next_cb_height;

    // Use the cached shielded coinbase only if the height hasn't changed.
    const int nHeight = chainActive.Tip()->nHeight;
    if (cached_next_cb_height != nHeight + 2) {
        cached_next_cb_mtx = nullopt;
    }

    std::optional<CMutableTransaction> next_cb_mtx(cached_next_cb_mtx);

    if (!lpval.isNull())
    {
        // Wait to respond until either the best block changes, OR some time passes and there are more transactions
        uint256 hashWatchedChain;
        std::chrono::steady_clock::time_point checktxtime;
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
            hashWatchedChain = chainActive.Tip()->GetBlockHash();
            nTransactionsUpdatedLastLP = nTransactionsUpdatedLast;
        }

        // Release the main lock while waiting
        // Don't call chainActive->Tip() without holding cs_main
        LEAVE_CRITICAL_SECTION(cs_main);
        {
            checktxtime = std::chrono::steady_clock::now() + std::chrono::seconds(10);

            WAIT_LOCK(g_best_block_mutex, lock);
            while (g_best_block == hashWatchedChain && IsRPCRunning())
            {
                // Before waiting, generate the coinbase for the block following the next
                // block (since this is cpu-intensive), so that when next block arrives,
                // we can quickly respond with a template for following block.
                // Note that the time to create the coinbase tx here does not add to,
                // but instead is included in, the 10 second delay, since we're waiting
                // until an absolute time is reached.
                if (!cached_next_cb_mtx && IsShieldedMinerAddress(minerAddress)) {
                    cached_next_cb_height = nHeight + 2;
                    cached_next_cb_mtx = CreateCoinbaseTransaction(
                        Params(), CAmount{0}, minerAddress, cached_next_cb_height);
                    next_cb_mtx = cached_next_cb_mtx;
                }
                bool timedout = g_best_block_cv.wait_until(lock, checktxtime) == std::cv_status::timeout;

                // Optimization: even if timed out, a new block may have arrived
                // while waiting for cs_main; if so, don't discard next_cb_mtx.
                if (g_best_block != hashWatchedChain) break;

                // Timeout: Check transactions for update
                if (timedout && mempool.GetTransactionsUpdated() != nTransactionsUpdatedLastLP) {
                    // Create a non-empty block.
                    next_cb_mtx = nullopt;
                    break;
                }
                checktxtime += std::chrono::seconds(10);
            }
            if (g_best_block_height != nHeight + 1) {
                // Unexpected height (reorg or >1 blocks arrived while waiting) invalidates coinbase tx.
                next_cb_mtx = nullopt;
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
    if (!lpval.isNull() || pindexPrev != chainActive.Tip() ||
        (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 5))
    {
        // Clear pindexPrev so future calls make a new block, despite any failures from here on
        pindexPrev = nullptr;

        nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();

        // If we're going to use the precomputed coinbase (empty block) and there are
        // transactions waiting in the mempool, make sure that on the next call to this
        // RPC, we consider the transaction count to have changed so we return a new
        // template (that includes these transactions) so they don't get stuck.
        if (next_cb_mtx && mempool.size() > 0) nTransactionsUpdatedLast = 0;

        CBlockIndex* pindexPrevNew = chainActive.Tip();
        nStart = GetTime();

        // Create new block
        if(pblocktemplate)
        {
            delete pblocktemplate;
            pblocktemplate = nullptr;
        }

        // Throw an error if no address valid for mining was provided.
        if (!std::visit(IsValidMinerAddress(), minerAddress)) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "No miner address available (mining requires a wallet or -mineraddress)");
        }

        pblocktemplate = BlockAssembler(Params()).CreateNewBlock(minerAddress, next_cb_mtx);
        if (!pblocktemplate)
            throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");

        // Mark script as important because it was used at least for one coinbase output
        std::visit(KeepMinerAddress(), minerAddress);

        // Need to update only after we know CreateNewBlock succeeded
        pindexPrev = pindexPrevNew;
    }
    CBlock* pblock = &pblocktemplate->block; // pointer for convenience

    const Consensus::Params& consensus = Params().GetConsensus();

    // Update nTime
    UpdateTime(pblock, consensus, pindexPrev);
    pblock->nNonce = uint256();

    UniValue aCaps(UniValue::VARR); aCaps.push_back("proposal");

    UniValue txCoinbase = NullUniValue;
    UniValue transactions(UniValue::VARR);
    map<uint256, int64_t> setTxIndex;
    int i = 0;
    for (const CTransaction& tx : pblock->vtx) {
        uint256 txHash = tx.GetHash();
        setTxIndex[txHash] = i++;

        if (tx.IsCoinBase() && !coinbasetxn)
            continue;

        UniValue entry(UniValue::VOBJ);

        entry.pushKV("data", EncodeHexTx(tx));

        entry.pushKV("hash", txHash.GetHex());
        entry.pushKV("authdigest", tx.GetAuthDigest().GetHex());

        UniValue deps(UniValue::VARR);
        for (const CTxIn &in : tx.vin)
        {
            if (setTxIndex.count(in.prevout.hash))
                deps.push_back(setTxIndex[in.prevout.hash]);
        }
        entry.pushKV("depends", deps);

        int index_in_template = i - 1;
        entry.pushKV("fee", pblocktemplate->vTxFees[index_in_template]);
        entry.pushKV("sigops", pblocktemplate->vTxSigOps[index_in_template]);

        if (tx.IsCoinBase()) {
            // Show founders' reward if it is required
            auto nextHeight = pindexPrev->nHeight+1;
            bool canopyActive = consensus.NetworkUpgradeActive(nextHeight, Consensus::UPGRADE_CANOPY);
            if (!canopyActive && nextHeight > 0 && nextHeight <= consensus.GetLastFoundersRewardBlockHeight(nextHeight)) {
                CAmount nBlockSubsidy = consensus.GetBlockSubsidy(nextHeight);
                entry.pushKV("foundersreward", nBlockSubsidy / 5);
            }
            entry.pushKV("required", true);
            txCoinbase = entry;
        } else {
            transactions.push_back(entry);
        }
    }

    UniValue aux(UniValue::VOBJ);
    aux.pushKV("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end()));

    arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);

    static UniValue aMutable(UniValue::VARR);
    if (aMutable.empty())
    {
        aMutable.push_back("time");
        aMutable.push_back("transactions");
        aMutable.push_back("prevblock");
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("capabilities", aCaps);
    result.pushKV("version", pblock->nVersion);
    result.pushKV("previousblockhash", pblock->hashPrevBlock.GetHex());
    // The following 3 are deprecated; remove in a future release.
    if (fEnableGbtOldHashes) {
        result.pushKV("blockcommitmentshash", pblock->hashBlockCommitments.GetHex());
        result.pushKV("lightclientroothash", pblock->hashBlockCommitments.GetHex());
        result.pushKV("finalsaplingroothash", pblock->hashBlockCommitments.GetHex());
    }
    {
        // These are items in the result object that are valid only if the
        // block template returned by this RPC is used unmodified. Otherwise,
        // these values must be recomputed.
        UniValue defaults(UniValue::VOBJ);
        defaults.pushKV("merkleroot", BlockMerkleRoot(*pblock).GetHex());
        defaults.pushKV("chainhistoryroot", pblocktemplate->hashChainHistoryRoot.GetHex());
        if (consensus.NetworkUpgradeActive(pindexPrev->nHeight+1, Consensus::UPGRADE_NU5)) {
            defaults.pushKV("authdataroot", pblocktemplate->hashAuthDataRoot.GetHex());
            defaults.pushKV("blockcommitmentshash", pblock->hashBlockCommitments.GetHex());
        }
        result.pushKV("defaultroots", defaults);
    }
    result.pushKV("transactions", transactions);
    if (coinbasetxn) {
        assert(txCoinbase.isObject());
        result.pushKV("coinbasetxn", txCoinbase);
    } else {
        result.pushKV("coinbaseaux", aux);
        result.pushKV("coinbasevalue", (int64_t)pblock->vtx[0].vout[0].nValue);
    }
    result.pushKV("longpollid", chainActive.Tip()->GetBlockHash().GetHex() + i64tostr(nTransactionsUpdatedLast));
    result.pushKV("target", hashTarget.GetHex());
    result.pushKV("mintime", (int64_t)pindexPrev->GetMedianTimePast()+1);
    result.pushKV("mutable", aMutable);
    result.pushKV("noncerange", "00000000ffffffff");
    result.pushKV("sigoplimit", (int64_t)MAX_BLOCK_SIGOPS);
    result.pushKV("sizelimit", (int64_t)MAX_BLOCK_SIZE);
    result.pushKV("curtime", pblock->GetBlockTime());
    result.pushKV("bits", strprintf("%08x", pblock->nBits));
    result.pushKV("height", (int64_t)(pindexPrev->nHeight+1));

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

UniValue submitblock(const UniValue& params, bool fHelp)
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
    if (!DecodeHexBlk(block, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

    uint256 hash = block.GetHash();
    bool fBlockPresent = false;
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndex *pindex = mi->second;
            if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                return "duplicate";
            if (pindex->nStatus & BLOCK_FAILED_MASK)
                return "duplicate-invalid";
            // Otherwise, we might only have the header - process the block before returning
            fBlockPresent = true;
        }
    }

    CValidationState state;
    submitblock_StateCatcher sc(block.GetHash());
    RegisterValidationInterface(&sc);
    bool fAccepted = ProcessNewBlock(state, Params(), NULL, &block, true, NULL);
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

UniValue getblocksubsidy(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getblocksubsidy height\n"
            "\nReturns block subsidy reward, taking into account the mining slow start and the founders reward, of block at index provided.\n"
            "\nArguments:\n"
            "1. height         (numeric, optional) The block height.  If not provided, defaults to the current height of the chain.\n"
            "\nResult:\n"
            "{\n"
            "  \"miner\" : x.xxx,              (numeric) The mining reward amount in " + CURRENCY_UNIT + ".\n"
            "  \"founders\" : x.xxx,           (numeric) The founders' reward amount in " + CURRENCY_UNIT + ".\n"
            "  \"fundingstreamstotal\" : x.xxx,(numeric) The total value of direct funding streams in " + CURRENCY_UNIT + ".\n"
            "  \"lockboxtotal\" : x.xxx,       (numeric) The total value sent to development funding lockboxes in " + CURRENCY_UNIT + ".\n"
            "  \"totalblocksubsidy\" : x.xxx,  (numeric) The total value of the block subsidy in " + CURRENCY_UNIT + ".\n"
            "  \"fundingstreams\" : [          (array) An array of funding stream descriptions (present only when funding streams are active).\n"
            "    {\n"
            "      \"recipient\" : \"...\",        (string) A description of the funding stream recipient.\n"
            "      \"specification\" : \"url\",    (string) A URL for the specification of this funding stream.\n"
            "      \"value\" : x.xxx             (numeric) The funding stream amount in " + CURRENCY_UNIT + ".\n"
            "      \"valueZat\" : xxxx           (numeric) The funding stream amount in " + MINOR_CURRENCY_UNIT + ".\n"
            "      \"address\" :                 (string) The transparent or Sapling address of the funding stream recipient.\n"
            "    }, ...\n"
            "  ],\n"
            "  \"lockboxstreams\" : [          (array) An array of development fund lockbox stream descriptions (present only when lockbox streams are active).\n"
            "    {\n"
            "      \"recipient\" : \"...\",        (string) A description of the lockbox.\n"
            "      \"specification\" : \"url\",    (string) A URL for the specification of this lockbox.\n"
            "      \"value\" : x.xxx             (numeric) The amount locked in " + CURRENCY_UNIT + ".\n"
            "      \"valueZat\" : xxxx           (numeric) The amount locked in " + MINOR_CURRENCY_UNIT + ".\n"
            "    }, ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblocksubsidy", "1000")
            + HelpExampleRpc("getblocksubsidy", "1000")
        );

    LOCK(cs_main);
    int nHeight = (params.size()==1) ? params[0].get_int() : chainActive.Height();
    if (nHeight < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");

    const Consensus::Params& consensus = Params().GetConsensus();
    CAmount nBlockSubsidy = consensus.GetBlockSubsidy(nHeight);
    CAmount nFoundersReward = 0;
    CAmount nFundingStreamsTotal = 0;
    CAmount nLockboxTotal = 0;
    bool canopyActive = consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_CANOPY);

    UniValue result(UniValue::VOBJ);
    if (canopyActive) {
        KeyIO keyIO(Params());
        UniValue fundingstreams(UniValue::VARR);
        UniValue lockboxstreams(UniValue::VARR);
        auto fsinfos = consensus.GetActiveFundingStreams(nHeight);
        for (int idx = 0; idx < fsinfos.size(); idx++) {
            const auto& fsinfo = fsinfos[idx].first;
            CAmount nStreamAmount = fsinfo.Value(nBlockSubsidy);

            UniValue fsobj(UniValue::VOBJ);
            fsobj.pushKV("recipient", fsinfo.recipient);
            fsobj.pushKV("specification", fsinfo.specification);
            fsobj.pushKV("value", ValueFromAmount(nStreamAmount));
            fsobj.pushKV("valueZat", nStreamAmount);

            auto fs = fsinfos[idx].second;
            auto recipient = fs.Recipient(consensus, nHeight);

            examine(recipient, match {
                [&](const CScript& scriptPubKey) {
                    // For transparent funding stream addresses
                    UniValue pubkey(UniValue::VOBJ);
                    ScriptPubKeyToUniv(scriptPubKey, pubkey, true);
                    auto addressStr = find_value(pubkey, "addresses").get_array()[0].get_str();
                    fsobj.pushKV("address", addressStr);
                    fundingstreams.push_back(fsobj);
                    nFundingStreamsTotal += nStreamAmount;
                },
                [&](const libzcash::SaplingPaymentAddress& pa) {
                    // For shielded funding stream addresses
                    auto addressStr = keyIO.EncodePaymentAddress(pa);
                    fsobj.pushKV("address", addressStr);
                    fundingstreams.push_back(fsobj);
                    nFundingStreamsTotal += nStreamAmount;
                },
                [&](const Consensus::Lockbox& lockbox) {
                    // No address is provided for lockbox streams
                    lockboxstreams.push_back(fsobj);
                    nLockboxTotal += nStreamAmount;
                }
            });

        }
        if (fundingstreams.size() > 0) {
            result.pushKV("fundingstreams", fundingstreams);
        }
        if (lockboxstreams.size() > 0) {
            result.pushKV("lockboxstreams", lockboxstreams);
        }
    } else if (nHeight > 0 && nHeight <= consensus.GetLastFoundersRewardBlockHeight(nHeight)) {
        nFoundersReward = nBlockSubsidy/5;
    }
    CAmount nMinerReward = nBlockSubsidy - nFoundersReward - nFundingStreamsTotal - nLockboxTotal;
    result.pushKV("miner", ValueFromAmount(nMinerReward));
    result.pushKV("founders", ValueFromAmount(nFoundersReward));
    result.pushKV("fundingstreamstotal", ValueFromAmount(nFundingStreamsTotal));
    result.pushKV("lockboxtotal", ValueFromAmount(nLockboxTotal));
    result.pushKV("totalblocksubsidy", ValueFromAmount(nBlockSubsidy));
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
};

void RegisterMiningRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
