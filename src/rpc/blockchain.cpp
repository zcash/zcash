// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "consensus/validation.h"
#include "experimental_features.h"
#include "key_io.h"
#include "main.h"
#include "metrics.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "streams.h"
#include "sync.h"
#include "util/system.h"

#include <stdint.h>

#include <univalue.h>

#include <optional>
#include <regex>

using namespace std;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry);
void ScriptPubKeyToJSON(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex);

double GetDifficultyINTERNAL(const CBlockIndex* blockindex, bool networkDifficulty)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (chainActive.Tip() == NULL)
            return 1.0;
        else
            blockindex = chainActive.Tip();
    }

    uint32_t bits;
    if (networkDifficulty) {
        bits = GetNextWorkRequired(blockindex, nullptr, Params().GetConsensus());
    } else {
        bits = blockindex->nBits;
    }

    uint32_t powLimit =
        UintToArith256(Params().GetConsensus().powLimit).GetCompact();
    int nShift = (bits >> 24) & 0xff;
    int nShiftAmount = (powLimit >> 24) & 0xff;

    double dDiff =
        (double)(powLimit & 0x00ffffff) /
        (double)(bits & 0x00ffffff);

    while (nShift < nShiftAmount)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > nShiftAmount)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

double GetDifficulty(const CBlockIndex* blockindex)
{
    return GetDifficultyINTERNAL(blockindex, false);
}

double GetNetworkDifficulty(const CBlockIndex* blockindex)
{
    return GetDifficultyINTERNAL(blockindex, true);
}

static UniValue ValuePoolDesc(
    const std::optional<std::string> name,
    const std::optional<CAmount> chainValue,
    const std::optional<CAmount> valueDelta)
{
    UniValue rv(UniValue::VOBJ);
    if (name.has_value()) {
        rv.pushKV("id", name.value());
    }
    rv.pushKV("monitored", (bool)chainValue);
    if (chainValue) {
        rv.pushKV("chainValue", ValueFromAmount(*chainValue));
        rv.pushKV("chainValueZat", *chainValue);
    }
    if (valueDelta) {
        rv.pushKV("valueDelta", ValueFromAmount(*valueDelta));
        rv.pushKV("valueDeltaZat", *valueDelta);
    }
    return rv;
}

UniValue blockheaderToJSON(const CBlockIndex* blockindex)
{
    AssertLockHeld(cs_main);
    UniValue result(UniValue::VOBJ);
    result.pushKV("hash", blockindex->GetBlockHash().GetHex());
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.pushKV("confirmations", confirmations);
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", blockindex->nVersion);
    result.pushKV("merkleroot", blockindex->hashMerkleRoot.GetHex());
    result.pushKV("finalsaplingroot", blockindex->hashFinalSaplingRoot.GetHex());
    result.pushKV("time", (int64_t)blockindex->nTime);
    result.pushKV("nonce", blockindex->nNonce.GetHex());
    result.pushKV("solution", HexStr(blockindex->GetBlockHeader().nSolution));
    result.pushKV("bits", strprintf("%08x", blockindex->nBits));
    result.pushKV("difficulty", GetDifficulty(blockindex));
    result.pushKV("chainwork", blockindex->nChainWork.GetHex());

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.pushKV("nextblockhash", pnext->GetBlockHash().GetHex());
    return result;
}

// insightexplorer
UniValue blockToDeltasJSON(const CBlock& block, const CBlockIndex* blockindex)
{
    UniValue result(UniValue::VOBJ);
    result.pushKV("hash", block.GetHash().GetHex());
    // Only report confirmations if the block is on the main chain
    if (!chainActive.Contains(blockindex))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block is an orphan");
    int confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.pushKV("confirmations", confirmations);
    result.pushKV("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION));
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", block.nVersion);
    result.pushKV("merkleroot", block.hashMerkleRoot.GetHex());

    KeyIO keyIO(Params());
    UniValue deltas(UniValue::VARR);
    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        const CTransaction &tx = block.vtx[i];
        const uint256 txhash = tx.GetHash();

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("txid", txhash.GetHex());
        entry.pushKV("index", (int)i);

        UniValue inputs(UniValue::VARR);
        if (!tx.IsCoinBase()) {
            for (size_t j = 0; j < tx.vin.size(); j++) {
                const CTxIn input = tx.vin[j];
                UniValue delta(UniValue::VOBJ);
                CSpentIndexValue spentInfo;
                CSpentIndexKey spentKey(input.prevout.hash, input.prevout.n);

                if (!GetSpentIndex(spentKey, spentInfo)) {
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "Spent information not available");
                }
                CTxDestination dest = DestFromAddressHash(spentInfo.addressType, spentInfo.addressHash);
                if (IsValidDestination(dest)) {
                    delta.pushKV("address", keyIO.EncodeDestination(dest));
                }
                delta.pushKV("satoshis", -1 * spentInfo.satoshis);
                delta.pushKV("index", (int)j);
                delta.pushKV("prevtxid", input.prevout.hash.GetHex());
                delta.pushKV("prevout", (int)input.prevout.n);

                inputs.push_back(delta);
            }
        }
        entry.pushKV("inputs", inputs);

        UniValue outputs(UniValue::VARR);
        for (unsigned int k = 0; k < tx.vout.size(); k++) {
            const CTxOut &out = tx.vout[k];
            UniValue delta(UniValue::VOBJ);
            const uint160 addrhash = out.scriptPubKey.AddressHash();
            CTxDestination dest;

            if (out.scriptPubKey.IsPayToScriptHash()) {
                dest = CScriptID(addrhash);
            } else if (out.scriptPubKey.IsPayToPublicKeyHash()) {
                dest = CKeyID(addrhash);
            }
            if (IsValidDestination(dest)) {
                delta.pushKV("address", keyIO.EncodeDestination(dest));
            }
            delta.pushKV("satoshis", out.nValue);
            delta.pushKV("index", (int)k);

            outputs.push_back(delta);
        }
        entry.pushKV("outputs", outputs);
        deltas.push_back(entry);
    }
    result.pushKV("deltas", deltas);
    result.pushKV("time", block.GetBlockTime());
    result.pushKV("mediantime", (int64_t)blockindex->GetMedianTimePast());
    result.pushKV("nonce", block.nNonce.GetHex());
    result.pushKV("bits", strprintf("%08x", block.nBits));
    result.pushKV("difficulty", GetDifficulty(blockindex));
    result.pushKV("chainwork", blockindex->nChainWork.GetHex());

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.pushKV("nextblockhash", pnext->GetBlockHash().GetHex());
    return result;
}

UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false)
{
    AssertLockHeld(cs_main);
    bool nu5Active = Params().GetConsensus().NetworkUpgradeActive(
        blockindex->nHeight, Consensus::UPGRADE_NU5);

    UniValue result(UniValue::VOBJ);
    result.pushKV("hash", block.GetHash().GetHex());
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.pushKV("confirmations", confirmations);
    result.pushKV("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION));
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", block.nVersion);
    result.pushKV("merkleroot", block.hashMerkleRoot.GetHex());
    result.pushKV("blockcommitments", blockindex->hashBlockCommitments.GetHex());
    result.pushKV("authdataroot", blockindex->hashAuthDataRoot.GetHex());
    result.pushKV("finalsaplingroot", blockindex->hashFinalSaplingRoot.GetHex());
    if (nu5Active) {
        auto finalOrchardRootBytes = blockindex->hashFinalOrchardRoot;
        result.pushKV("finalorchardroot", HexStr(finalOrchardRootBytes.begin(), finalOrchardRootBytes.end()));
    }
    result.pushKV("chainhistoryroot", blockindex->hashChainHistoryRoot.GetHex());
    UniValue txs(UniValue::VARR);
    for (const CTransaction&tx : block.vtx)
    {
        if(txDetails)
        {
            UniValue objTx(UniValue::VOBJ);
            TxToJSON(tx, uint256(), objTx);
            txs.push_back(objTx);
        }
        else
            txs.push_back(tx.GetHash().GetHex());
    }
    result.pushKV("tx", txs);
    result.pushKV("time", block.GetBlockTime());
    result.pushKV("nonce", block.nNonce.GetHex());
    result.pushKV("solution", HexStr(block.nSolution));
    result.pushKV("bits", strprintf("%08x", block.nBits));
    result.pushKV("difficulty", GetDifficulty(blockindex));
    result.pushKV("chainwork", blockindex->nChainWork.GetHex());
    result.pushKV("anchor", blockindex->hashFinalSproutRoot.GetHex());
    result.pushKV("chainSupply", ValuePoolDesc(std::nullopt, blockindex->nChainTotalSupply, blockindex->nChainSupplyDelta));
    UniValue valuePools(UniValue::VARR);
    valuePools.push_back(ValuePoolDesc("transparent", blockindex->nChainTransparentValue, blockindex->nTransparentValue));
    valuePools.push_back(ValuePoolDesc("sprout", blockindex->nChainSproutValue, blockindex->nSproutValue));
    valuePools.push_back(ValuePoolDesc("sapling", blockindex->nChainSaplingValue, blockindex->nSaplingValue));
    valuePools.push_back(ValuePoolDesc("orchard", blockindex->nChainOrchardValue, blockindex->nOrchardValue));
    valuePools.push_back(ValuePoolDesc("lockbox", blockindex->nChainLockboxValue, blockindex->nLockboxValue));
    result.pushKV("valuePools", valuePools);

    {
        UniValue trees(UniValue::VOBJ);

        SaplingMerkleTree saplingTree;
        if (pcoinsTip != nullptr && pcoinsTip->GetSaplingAnchorAt(blockindex->hashFinalSaplingRoot, saplingTree)) {
            UniValue sapling(UniValue::VOBJ);
            sapling.pushKV("size", (uint64_t)saplingTree.size());
            trees.pushKV("sapling", sapling);
        }

        OrchardMerkleFrontier orchardTree;
        if (pcoinsTip != nullptr && pcoinsTip->GetOrchardAnchorAt(blockindex->hashFinalOrchardRoot, orchardTree)) {
            UniValue orchard(UniValue::VOBJ);
            orchard.pushKV("size", (uint64_t)orchardTree.size());
            trees.pushKV("orchard", orchard);
        }

        result.pushKV("trees", trees);
    }

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.pushKV("nextblockhash", pnext->GetBlockHash().GetHex());
    return result;
}

UniValue getblockcount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "\nReturns the height of the most recent block in the best valid block chain (equivalently,\n"
            "the number of blocks in this chain excluding the genesis block).\n"
            "\nResult:\n"
            "n    (numeric) The height of the most recent block.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockcount", "")
            + HelpExampleRpc("getblockcount", "")
        );

    LOCK(cs_main);
    return chainActive.Height();
}

UniValue getbestblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "\nReturns the hash of the best (tip) block in the longest block chain.\n"
            "\nResult\n"
            "\"hex\"      (string) the block hash hex encoded\n"
            "\nExamples\n"
            + HelpExampleCli("getbestblockhash", "")
            + HelpExampleRpc("getbestblockhash", "")
        );

    LOCK(cs_main);
    return chainActive.Tip()->GetBlockHash().GetHex();
}

UniValue getdifficulty(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "\nReturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nResult:\n"
            "n.nnn       (numeric) the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nExamples:\n"
            + HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
        );

    LOCK(cs_main);
    return GetNetworkDifficulty();
}

UniValue mempoolToJSON(bool fVerbose = false)
{
    if (fVerbose)
    {
        LOCK(mempool.cs);
        UniValue o(UniValue::VOBJ);
        for (const CTxMemPoolEntry& e : mempool.mapTx)
        {
            const uint256& hash = e.GetTx().GetHash();
            UniValue info(UniValue::VOBJ);
            info.pushKV("size", (int)e.GetTxSize());
            info.pushKV("fee", ValueFromAmount(e.GetFee()));
            info.pushKV("modifiedfee", ValueFromAmount(e.GetModifiedFee()));
            info.pushKV("time", e.GetTime());
            info.pushKV("height", (int)e.GetHeight());
            info.pushKV("descendantcount", e.GetCountWithDescendants());
            info.pushKV("descendantsize", e.GetSizeWithDescendants());
            info.pushKV("descendantfees", e.GetModFeesWithDescendants());
            const CTransaction& tx = e.GetTx();
            set<string> setDepends;
            for (const CTxIn& txin : tx.vin)
            {
                if (mempool.exists(txin.prevout.hash))
                    setDepends.insert(txin.prevout.hash.ToString());
            }

            UniValue depends(UniValue::VARR);
            for (const string& dep : setDepends)
            {
                depends.push_back(dep);
            }

            info.pushKV("depends", depends);
            o.pushKV(hash.ToString(), info);
        }
        return o;
    }
    else
    {
        vector<uint256> vtxid;
        mempool.queryHashes(vtxid);

        UniValue a(UniValue::VARR);
        for (const uint256& hash : vtxid)
            a.push_back(hash.ToString());

        return a;
    }
}

UniValue getrawmempool(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getrawmempool ( verbose )\n"
            "\nReturns all transaction ids in memory pool as a json array of string transaction ids.\n"
            "\nArguments:\n"
            "1. verbose           (boolean, optional, default=false) true for a json object, false for array of transaction ids\n"
            "\nResult: (for verbose = false):\n"
            "[                     (json array of string)\n"
            "  \"transactionid\"     (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nResult: (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            "    \"size\" : n,             (numeric) transaction size in bytes\n"
            "    \"fee\" : n,              (numeric) transaction fee in " + CURRENCY_UNIT + "\n"
            "    \"modifiedfee\" : n,      (numeric) transaction fee with fee deltas used for mining priority\n"
            "    \"time\" : n,             (numeric) local time transaction entered pool in seconds since 1 Jan 1970 GMT\n"
            "    \"height\" : n,           (numeric) block height when transaction entered pool\n"
            "    \"descendantcount\" : n,  (numeric) number of in-mempool descendant transactions (including this one)\n"
            "    \"descendantsize\" : n,   (numeric) size of in-mempool descendants (including this one)\n"
            "    \"descendantfees\" : n,   (numeric) modified fees (see \"modifiedfee\" above) of in-mempool descendants (including this one)\n"
            "    \"depends\" : [           (array) unconfirmed transactions used as inputs for this transaction\n"
            "        \"transactionid\",    (string) parent transaction id\n"
            "       ... ]\n"
            "  }, ...\n"
            "}\n"
            "\nExamples\n"
            + HelpExampleCli("getrawmempool", "true")
            + HelpExampleRpc("getrawmempool", "true")
        );

    LOCK(cs_main);

    bool fVerbose = false;
    if (params.size() > 0)
        fVerbose = params[0].get_bool();

    return mempoolToJSON(fVerbose);
}

// insightexplorer
UniValue getblockdeltas(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        disabledMsg = experimentalDisabledHelpMsg("getblockdeltas", {"insightexplorer", "lightwalletd"});
    }
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockdeltas \"blockhash\"\n"
            "\nReturns information about the given block and its transactions.\n"
            + disabledMsg +
            "\nArguments:\n"
            "1. \"hash\"          (string, required) The block hash\n"
            "\nResult:\n"
            "{\n"
            "  \"hash\": \"hash\",              (string) block ID\n"
            "  \"confirmations\": n,          (numeric) number of confirmations\n"
            "  \"size\": n,                   (numeric) block size in bytes\n"
            "  \"height\": n,                 (numeric) block height\n"
            "  \"version\": n,                (numeric) block version (e.g. 4)\n"
            "  \"merkleroot\": \"hash\",        (hexstring) block Merkle root\n"
            "  \"deltas\": [\n"
            "    {\n"
            "      \"txid\": \"hash\",          (hexstring) transaction ID\n"
            "      \"index\": n,              (numeric) The offset of the tx in the block\n"
            "      \"inputs\": [                (array of json objects)\n"
            "        {\n"
            "          \"address\": \"taddr\",  (string) transparent address\n"
            "          \"satoshis\": n,       (numeric) negative of spend amount\n"
            "          \"index\": n,          (numeric) vin index\n"
            "          \"prevtxid\": \"hash\",  (string) source utxo tx ID\n"
            "          \"prevout\": n         (numeric) source utxo index\n"
            "        }, ...\n"
            "      ],\n"
            "      \"outputs\": [             (array of json objects)\n"
            "        {\n"
            "          \"address\": \"taddr\",  (string) transparent address\n"
            "          \"satoshis\": n,       (numeric) amount\n"
            "          \"index\": n           (numeric) vout index\n"
            "        }, ...\n"
            "      ]\n"
            "    }, ...\n"
            "  ],\n"
            "  \"time\" : n,                  (numeric) The block version\n"
            "  \"mediantime\": n,             (numeric) The most recent blocks' ave time\n"
            "  \"nonce\" : \"nonce\",           (hex string) The nonce\n"
            "  \"bits\" : \"1d00ffff\",         (hex string) The bits\n"
            "  \"difficulty\": n,             (numeric) the current difficulty\n"
            "  \"chainwork\": \"xxxx\"          (hex string) total amount of work in active chain\n"
            "  \"previousblockhash\" : \"hash\",(hex string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"     (hex string) The hash of the next block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockdeltas", "00227e566682aebd6a7a5b772c96d7a999cadaebeaf1ce96f4191a3aad58b00b")
            + HelpExampleRpc("getblockdeltas", "\"00227e566682aebd6a7a5b772c96d7a999cadaebeaf1ce96f4191a3aad58b00b\"")
        );

    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: getblockdeltas is disabled. "
            "Run './" CLI_NAME " help getblockdeltas' for instructions on how to enable this feature.");
    }

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));

    LOCK(cs_main);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];

    if (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not available (pruned data)");

    if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

    return blockToDeltasJSON(block, pblockindex);
}

// insightexplorer
UniValue getblockhashes(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        disabledMsg = experimentalDisabledHelpMsg("getblockhashes", {"insightexplorer", "lightwalletd"});
    }
    if (fHelp || params.size() < 2)
        throw runtime_error(
            "getblockhashes high low ( {\"noOrphans\": true|false, \"logicalTimes\": true|false} )\n"
            "\nReturns array of hashes of blocks within the timestamp range provided,\n"
            "\ngreater or equal to low, less than high.\n"
            + disabledMsg +
            "\nArguments:\n"
            "1. high                            (numeric, required) The newer block timestamp\n"
            "2. low                             (numeric, required) The older block timestamp\n"
            "3. options                         (string, optional) A json object\n"
            "    {\n"
            "      \"noOrphans\": true|false      (boolean) will only include blocks on the main chain\n"
            "      \"logicalTimes\": true|false   (boolean) will include logical timestamps with hashes\n"
            "    }\n"
            "\nResult:\n"
            "[\n"
            "  \"xxxx\"                   (hex string) The block hash\n"
            "]\n"
            "or\n"
            "[\n"
            "  {\n"
            "    \"blockhash\": \"xxxx\"    (hex string) The block hash\n"
            "    \"logicalts\": n         (numeric) The logical timestamp\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockhashes", "1558141697 1558141576")
            + HelpExampleRpc("getblockhashes", "1558141697, 1558141576")
            + HelpExampleCli("getblockhashes", "1558141697 1558141576 '{\"noOrphans\":false, \"logicalTimes\":true}'")
            );

    if (!(fExperimentalInsightExplorer || fExperimentalLightWalletd)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: getblockhashes is disabled. "
            "Run './" CLI_NAME " help getblockhashes' for instructions on how to enable this feature.");
    }

    unsigned int high = params[0].get_int();
    unsigned int low = params[1].get_int();
    bool fActiveOnly = false;
    bool fLogicalTS = false;

    if (params.size() > 2) {
        UniValue noOrphans = find_value(params[2].get_obj(), "noOrphans");
        if (!noOrphans.isNull())
            fActiveOnly = noOrphans.get_bool();

        UniValue returnLogical = find_value(params[2].get_obj(), "logicalTimes");
        if (!returnLogical.isNull())
            fLogicalTS = returnLogical.get_bool();
    }

    std::vector<std::pair<uint256, unsigned int> > blockHashes;
    {
        LOCK(cs_main);
        if (!GetTimestampIndex(high, low, fActiveOnly, blockHashes)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                "No information available for block hashes");
        }
    }
    UniValue result(UniValue::VARR);
    for (std::vector<std::pair<uint256, unsigned int> >::const_iterator it=blockHashes.begin();
            it!=blockHashes.end(); it++) {
        if (fLogicalTS) {
            UniValue item(UniValue::VOBJ);
            item.pushKV("blockhash", it->first.GetHex());
            item.pushKV("logicalts", (int)it->second);
            result.push_back(item);
        } else {
            result.push_back(it->first.GetHex());
        }
    }
    return result;
}

//! Sanity-check a height argument and interpret negative values.
int interpretHeightArg(int nHeight, int currentHeight)
{
    if (nHeight < 0) {
        nHeight += currentHeight + 1;
    }
    if (nHeight < 0 || nHeight > currentHeight) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
    }
    return nHeight;
}

//! Parse and sanity-check a height argument, return its integer representation.
int parseHeightArg(const std::string& strHeight, int currentHeight)
{
    // std::stoi allows (locale-dependent) whitespace and optional '+' sign,
    // whereas we want to be strict.
    regex r("(?:(-?)[1-9][0-9]*|[0-9]+)");
    if (!regex_match(strHeight, r)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block height parameter");
    }
    int nHeight;
    try {
        nHeight = std::stoi(strHeight);
    }
    catch (const std::exception &e) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block height parameter");
    }
    return interpretHeightArg(nHeight, currentHeight);
}

UniValue getblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash index\n"
            "\nReturns hash of block in best-block-chain at index provided.\n"
            "\nArguments:\n"
            "1. index         (numeric, required) The block index. If negative then -1 is the last known valid block\n"
            "\nResult:\n"
            "\"hash\"         (string) The block hash\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockhash", "1000")
            + HelpExampleRpc("getblockhash", "1000")
        );

    LOCK(cs_main);

    const CBlockIndex* pblockindex = chainActive[interpretHeightArg(params[0].get_int(), chainActive.Height())];
    return pblockindex->GetBlockHash().GetHex();
}

UniValue getblockheader(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblockheader \"hash\" ( verbose )\n"
            "\nIf verbose is false, returns a string that is serialized, hex-encoded data for blockheader 'hash'.\n"
            "If verbose is true, returns an Object with information about blockheader <hash>.\n"
            "\nArguments:\n"
            "1. \"hash\"          (string, required) The block hash\n"
            "2. verbose           (boolean, optional, default=true) true for a json object, false for the hex encoded data\n"
            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"finalsaplingroot\" : \"xxxx\", (string) The root of the Sapling commitment tree after applying this block\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        );

    LOCK(cs_main);

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlockIndex* pblockindex = mapBlockIndex[hash];

    try {
        if (!fVerbose) {
            CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
            ssBlock << pblockindex->GetBlockHeader();
            std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
            return strHex;
        } else {
            return blockheaderToJSON(pblockindex);
        }
    } catch (const runtime_error&) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to read index entry");
    }
}

UniValue getblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock \"hash|height\" ( verbosity )\n"
            "\nIf verbosity is 0, returns a string that is serialized, hex-encoded data for the block.\n"
            "If verbosity is 1, returns an Object with information about the block.\n"
            "If verbosity is 2, returns an Object with information about the block and information about each transaction. \n"
            "\nArguments:\n"
            "1. \"hash|height\"          (string, required) The block hash or height. Height can be negative where -1 is the last known valid block\n"
            "2. verbosity              (numeric, optional, default=1) 0 for hex encoded data, 1 for a json object, and 2 for json object with transaction data\n"
            "\nResult (for verbosity = 0):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for the block.\n"
            "\nResult (for verbosity = 1):\n"
            "{\n"
            "  \"hash\" : \"hash\",       (string) the block hash (same as provided hash)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"size\" : n,            (numeric) The block size\n"
            "  \"height\" : n,          (numeric) The block height or index (same as provided height)\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"finalsaplingroot\" : \"xxxx\", (string) The root of the Sapling commitment tree after applying this block\n"
            "  \"finalorchardroot\" : \"xxxx\", (string, optional) The root of the Orchard commitment tree after\n"
            "                               applying this block. Omitted for blocks prior to NU5 activation. This\n"
            "                               will be the null hash if this block has never been connected to a\n"
            "                               main chain.\n"
            "                               NB: The serialized representation of this field returned by this method\n"
            "                                   was byte-flipped relative to its representation in the `getrawtransaction`\n"
            "                                   output in prior releases up to and including v5.2.0. This has now been\n"
            "                                   rectified.\n"
            "  \"tx\" : [               (array of string) The transaction ids\n"
            "     \"transactionid\"     (string) The transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\",   (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"chainSupply\": {          (object) information about the total supply\n"
            "      \"monitored\": xx,           (boolean) true if the total supply is being monitored\n"
            "      \"chainValue\": xxxxxx,      (numeric, optional) total chain supply after this block, in " + CURRENCY_UNIT + "\n"
            "      \"chainValueZat\": xxxxxx,   (numeric, optional) total chain supply after this block, in " + MINOR_CURRENCY_UNIT + "\n"
            "      \"valueDelta\": xxxxxx,      (numeric, optional) change to the chain supply produced by this block, in " + CURRENCY_UNIT + "\n"
            "      \"valueDeltaZat\": xxxxxx,   (numeric, optional) change to the chain supply produced by this block, in " + MINOR_CURRENCY_UNIT + "\n"
            "  },\n"
            "  \"valuePools\": [            (array) information about each value pool\n"
            "      {\n"
            "          \"id\": \"xxxx\",            (string) name of the pool\n"
            "          \"monitored\": xx,           (boolean) true if the pool is being monitored\n"
            "          \"chainValue\": xxxxxx,      (numeric, optional) total amount in the pool, in " + CURRENCY_UNIT + "\n"
            "          \"chainValueZat\": xxxxxx,   (numeric, optional) total amount in the pool, in " + MINOR_CURRENCY_UNIT + "\n"
            "          \"valueDelta\": xxxxxx,      (numeric, optional) change to the amount in the pool produced by this block, in " + CURRENCY_UNIT + "\n"
            "          \"valueDeltaZat\": xxxxxx,   (numeric, optional) change to the amount in the pool produced by this block, in " + MINOR_CURRENCY_UNIT + "\n"
            "      }, ...\n"
            "  ],\n"
            "  \"trees\": {                 (object) information about the note commitment trees\n"
            "      \"sapling\": {             (object, optional)\n"
            "          \"size\": n,             (numeric) the total number of Sapling note commitments as of the end of this block\n"
            "      },\n"
            "      \"orchard\": {             (object, optional)\n"
            "          \"size\": n,             (numeric) the total number of Orchard note commitments as of the end of this block\n"
            "      },\n"
            "  },\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbosity = 2):\n"
            "{\n"
            "  ...,                     Same output as verbosity = 1.\n"
            "  \"tx\" : [               (array of Objects) The transactions in the format of the getrawtransaction RPC. Different from verbosity = 1 \"tx\" result.\n"
            "         ,...\n"
            "  ],\n"
            "  ,...                     Same output as verbosity = 1.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblock", "\"00000000febc373a1da2bd9f887b105ad79ddc26ac26c2b28652d64e5207c5b5\"")
            + HelpExampleRpc("getblock", "\"00000000febc373a1da2bd9f887b105ad79ddc26ac26c2b28652d64e5207c5b5\"")
            + HelpExampleCli("getblock", "12800")
            + HelpExampleRpc("getblock", "12800")
        );

    LOCK(cs_main);

    std::string strHash = params[0].get_str();

    // If height is supplied, find the hash
    if (strHash.size() < (2 * sizeof(uint256))) {
        strHash = chainActive[parseHeightArg(strHash, chainActive.Height())]->GetBlockHash().GetHex();
    }

    uint256 hash(uint256S(strHash));

    int verbosity = 1;
    if (params.size() > 1) {
        if(params[1].isNum()) {
            verbosity = params[1].get_int();
        } else {
            verbosity = params[1].get_bool() ? 1 : 0;
        }
    }

    if (verbosity < 0 || verbosity > 2) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Verbosity must be in range from 0 to 2");
    }

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];

    if (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not available (pruned data)");

    if(!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

    if (verbosity == 0)
    {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockToJSON(block, pblockindex, verbosity >= 2);
}

UniValue gettxoutsetinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gettxoutsetinfo\n"
            "\nReturns statistics about the unspent transaction output set.\n"
            "Note this call may take some time.\n"
            "\nResult:\n"
            "{\n"
            "  \"height\":n,     (numeric) The current block height (index)\n"
            "  \"bestblock\": \"hex\",   (string) the best block hash hex\n"
            "  \"transactions\": n,      (numeric) The number of transactions\n"
            "  \"txouts\": n,            (numeric) The number of output transactions\n"
            "  \"bytes_serialized\": n,  (numeric) The serialized size\n"
            "  \"hash_serialized\": \"hash\",   (string) The serialized hash\n"
            "  \"total_amount\": x.xxx          (numeric) The total amount\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettxoutsetinfo", "")
            + HelpExampleRpc("gettxoutsetinfo", "")
        );

    UniValue ret(UniValue::VOBJ);

    CCoinsStats stats;
    FlushStateToDisk();
    if (pcoinsTip->GetStats(stats)) {
        ret.pushKV("height", (int64_t)stats.nHeight);
        ret.pushKV("bestblock", stats.hashBlock.GetHex());
        ret.pushKV("transactions", (int64_t)stats.nTransactions);
        ret.pushKV("txouts", (int64_t)stats.nTransactionOutputs);
        ret.pushKV("bytes_serialized", (int64_t)stats.nSerializedSize);
        ret.pushKV("hash_serialized", stats.hashSerialized.GetHex());
        ret.pushKV("total_amount", ValueFromAmount(stats.nTotalAmount));
    }
    return ret;
}

UniValue gettxout(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "gettxout \"txid\" n ( includemempool )\n"
            "\nReturns details about an unspent transaction output.\n"
            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id\n"
            "2. n              (numeric, required) vout value\n"
            "3. includemempool  (boolean, optional) Whether to include the mempool\n"
            "\nResult:\n"
            "{\n"
            "  \"bestblock\" : \"hash\",    (string) the block hash\n"
            "  \"confirmations\" : n,       (numeric) The number of confirmations\n"
            "  \"value\" : x.xxx,           (numeric) The transaction value in " + CURRENCY_UNIT + "\n"
            "  \"scriptPubKey\" : {         (json object)\n"
            "     \"asm\" : \"code\",       (string) \n"
            "     \"hex\" : \"hex\",        (string) \n"
            "     \"reqSigs\" : n,          (numeric) Number of required signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [          (array of string) array of " PACKAGE_NAME " addresses\n"
            "        \"zcashaddress\"        (string) " PACKAGE_NAME " address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"version\" : n,              (numeric) The version\n"
            "  \"coinbase\" : true|false     (boolean) Coinbase or not\n"
            "}\n"

            "\nExamples:\n"
            "\nGet unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nView the details\n"
            + HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("gettxout", "\"txid\", 1")
        );

    LOCK(cs_main);

    UniValue ret(UniValue::VOBJ);

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));
    int n = params[1].get_int();
    bool fMempool = true;
    if (params.size() > 2)
        fMempool = params[2].get_bool();

    CCoins coins;
    if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(pcoinsTip, mempool);
        if (!view.GetCoins(hash, coins))
            return NullUniValue;
        mempool.pruneSpent(hash, coins); // TODO: this should be done by the CCoinsViewMemPool
    } else {
        if (!pcoinsTip->GetCoins(hash, coins))
            return NullUniValue;
    }
    if (n<0 || (unsigned int)n>=coins.vout.size() || coins.vout[n].IsNull())
        return NullUniValue;

    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    CBlockIndex *pindex = it->second;
    ret.pushKV("bestblock", pindex->GetBlockHash().GetHex());
    if ((unsigned int)coins.nHeight == MEMPOOL_HEIGHT)
        ret.pushKV("confirmations", 0);
    else
        ret.pushKV("confirmations", pindex->nHeight - coins.nHeight + 1);
    ret.pushKV("value", ValueFromAmount(coins.vout[n].nValue));
    UniValue o(UniValue::VOBJ);
    ScriptPubKeyToJSON(coins.vout[n].scriptPubKey, o, true);
    ret.pushKV("scriptPubKey", o);
    ret.pushKV("version", coins.nVersion);
    ret.pushKV("coinbase", coins.fCoinBase);

    return ret;
}

UniValue verifychain(const UniValue& params, bool fHelp)
{
    int nCheckLevel = GetArg("-checklevel", DEFAULT_CHECKLEVEL);
    int nCheckDepth = GetArg("-checkblocks", DEFAULT_CHECKBLOCKS);
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "verifychain ( checklevel numblocks )\n"
            "\nVerifies blockchain database.\n"
            "\nArguments:\n"
            "1. checklevel   (numeric, optional, 0-4, default=" + strprintf("%d", nCheckLevel) + ") How thorough the block verification is.\n"
            "2. numblocks    (numeric, optional, default=" + strprintf("%d", nCheckDepth) + ", 0=all) The number of blocks to check.\n"
            "\nResult:\n"
            "true|false       (boolean) Verified or not\n"
            "\nExamples:\n"
            + HelpExampleCli("verifychain", "")
            + HelpExampleRpc("verifychain", "")
        );

    LOCK(cs_main);

    if (params.size() > 0)
        nCheckLevel = params[0].get_int();
    if (params.size() > 1)
        nCheckDepth = params[1].get_int();

    return CVerifyDB().VerifyDB(Params(), pcoinsTip, nCheckLevel, nCheckDepth);
}

/** Implementation of IsSuperMajority with better feedback */
static UniValue SoftForkMajorityDesc(int minVersion, CBlockIndex* pindex, int nRequired, const Consensus::Params& consensusParams)
{
    int nFound = 0;
    CBlockIndex* pstart = pindex;
    for (int i = 0; i < consensusParams.nMajorityWindow && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }

    UniValue rv(UniValue::VOBJ);
    rv.pushKV("status", nFound >= nRequired);
    rv.pushKV("found", nFound);
    rv.pushKV("required", nRequired);
    rv.pushKV("window", consensusParams.nMajorityWindow);
    return rv;
}

static UniValue SoftForkDesc(const std::string &name, int version, CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    UniValue rv(UniValue::VOBJ);
    rv.pushKV("id", name);
    rv.pushKV("version", version);
    rv.pushKV("enforce", SoftForkMajorityDesc(version, pindex, consensusParams.nMajorityEnforceBlockUpgrade, consensusParams));
    rv.pushKV("reject", SoftForkMajorityDesc(version, pindex, consensusParams.nMajorityRejectBlockOutdated, consensusParams));
    return rv;
}

static UniValue NetworkUpgradeDesc(const Consensus::Params& consensusParams, Consensus::UpgradeIndex idx, int height)
{
    UniValue rv(UniValue::VOBJ);
    auto upgrade = NetworkUpgradeInfo[idx];
    rv.pushKV("name", upgrade.strName);
    rv.pushKV("activationheight", consensusParams.vUpgrades[idx].nActivationHeight);
    switch (NetworkUpgradeState(height, consensusParams, idx)) {
        case UPGRADE_DISABLED: rv.pushKV("status", "disabled"); break;
        case UPGRADE_PENDING: rv.pushKV("status", "pending"); break;
        case UPGRADE_ACTIVE: rv.pushKV("status", "active"); break;
    }
    rv.pushKV("info", upgrade.strInfo);
    return rv;
}

void NetworkUpgradeDescPushBack(
    UniValue& networkUpgrades,
    const Consensus::Params& consensusParams,
    Consensus::UpgradeIndex idx,
    int height)
{
    // Network upgrades with an activation height of NO_ACTIVATION_HEIGHT are
    // hidden. This is used when network upgrade implementations are merged
    // without specifying the activation height.
    if (consensusParams.vUpgrades[idx].nActivationHeight != Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT) {
        networkUpgrades.pushKV(
            HexInt(NetworkUpgradeInfo[idx].nBranchId),
            NetworkUpgradeDesc(consensusParams, idx, height));
    }
}


UniValue getblockchaininfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockchaininfo\n"
            "Returns an object containing various state info regarding block chain processing.\n"
            "\nNote that when the chain tip is at the last block before a network upgrade activation,\n"
            "consensus.chaintip != consensus.nextblock.\n"
            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",        (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "  \"blocks\": xxxxxx,         (numeric) the current number of blocks processed in the server\n"
            "  \"initial_block_download_complete\": xx, (boolean) true if the initial download of the blockchain is complete\n"
            "  \"headers\": xxxxxx,        (numeric) the current number of headers we have validated\n"
            "  \"bestblockhash\": \"...\", (string) the hash of the currently best block\n"
            "  \"difficulty\": xxxxxx,     (numeric) the current difficulty\n"
            "  \"verificationprogress\": xxxx, (numeric) estimate of verification progress [0..1]\n"
            "  \"estimatedheight\": xxxx,  (numeric) if syncing, the estimated height of the chain, else the current best height\n"
            "  \"chainwork\": \"xxxx\"     (string) total amount of work in active chain, in hexadecimal\n"
            "  \"size_on_disk\": xxxxxx,       (numeric) the estimated size of the block and undo files on disk\n"
            "  \"commitments\": xxxxxx,    (numeric) the current number of note commitments in the commitment tree\n"
            "  \"chainSupply\": {          (object) information about the total supply\n"
            "      \"monitored\": xx,           (boolean) true if the total supply is being monitored\n"
            "      \"chainValue\": xxxxxx,      (numeric, optional) total chain supply after this block, in " + CURRENCY_UNIT + "\n"
            "      \"chainValueZat\": xxxxxx,   (numeric, optional) total chain supply after this block, in " + MINOR_CURRENCY_UNIT + "\n"
            "  }\n"
            "  \"valuePools\": [            (array) information about each value pool\n"
            "      {\n"
            "          \"id\": \"xxxx\",            (string) name of the pool\n"
            "          \"monitored\": xx,           (boolean) true if the pool is being monitored\n"
            "          \"chainValue\": xxxxxx,      (numeric, optional) total amount in the pool, in " + CURRENCY_UNIT + "\n"
            "          \"chainValueZat\": xxxxxx,   (numeric, optional) total amount in the pool, in " + MINOR_CURRENCY_UNIT + "\n"
            "      }, ...\n"
            "  ]\n"
            "  \"softforks\": [            (array) status of softforks in progress\n"
            "     {\n"
            "        \"id\": \"xxxx\",        (string) name of softfork\n"
            "        \"version\": xx,         (numeric) block version\n"
            "        \"enforce\": {           (object) progress toward enforcing the softfork rules for new-version blocks\n"
            "           \"status\": xx,       (boolean) true if threshold reached\n"
            "           \"found\": xx,        (numeric) number of blocks with the new version found\n"
            "           \"required\": xx,     (numeric) number of blocks required to trigger\n"
            "           \"window\": xx,       (numeric) maximum size of examined window of recent blocks\n"
            "        },\n"
            "        \"reject\": { ... }      (object) progress toward rejecting pre-softfork blocks (same fields as \"enforce\")\n"
            "     }, ...\n"
            "  ],\n"
            "  \"upgrades\": {                (object) status of network upgrades\n"
            "     \"xxxx\" : {                (string) branch ID of the upgrade\n"
            "        \"name\": \"xxxx\",        (string) name of upgrade\n"
            "        \"activationheight\": xxxxxx,  (numeric) block height of activation\n"
            "        \"status\": \"xxxx\",      (string) status of upgrade\n"
            "        \"info\": \"xxxx\",        (string) additional information about upgrade\n"
            "     }, ...\n"
            "  },\n"
            "  \"consensus\": {               (object) branch IDs of the current and upcoming consensus rules\n"
            "     \"chaintip\": \"xxxxxxxx\",   (string) branch ID used to validate the current chain tip\n"
            "     \"nextblock\": \"xxxxxxxx\"   (string) branch ID that the next block will be validated under\n"
            "  }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockchaininfo", "")
            + HelpExampleRpc("getblockchaininfo", "")
        );

    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("chain",                 Params().NetworkIDString());
    obj.pushKV("blocks",                (int)chainActive.Height());
    obj.pushKV("initial_block_download_complete", !IsInitialBlockDownload(Params().GetConsensus()));
    obj.pushKV("headers",               pindexBestHeader ? pindexBestHeader->nHeight : -1);
    obj.pushKV("bestblockhash",         chainActive.Tip()->GetBlockHash().GetHex());
    obj.pushKV("difficulty",            (double)GetNetworkDifficulty());
    obj.pushKV("verificationprogress",  Checkpoints::GuessVerificationProgress(Params().Checkpoints(), chainActive.Tip()));
    obj.pushKV("chainwork",             chainActive.Tip()->nChainWork.GetHex());
    obj.pushKV("pruned",                fPruneMode);
    obj.pushKV("size_on_disk",          CalculateCurrentUsage());

    if (IsInitialBlockDownload(Params().GetConsensus()))
        obj.pushKV("estimatedheight",       EstimateNetHeight(Params().GetConsensus(), (int)chainActive.Height(), chainActive.Tip()->GetMedianTimePast()));
    else
        obj.pushKV("estimatedheight",       (int)chainActive.Height());

    SproutMerkleTree tree;
    pcoinsTip->GetSproutAnchorAt(pcoinsTip->GetBestAnchor(SPROUT), tree);
    obj.pushKV("commitments",           static_cast<uint64_t>(tree.size()));

    CBlockIndex* tip = chainActive.Tip();
    obj.pushKV("chainSupply", ValuePoolDesc(std::nullopt, tip->nChainTotalSupply, std::nullopt));
    UniValue valuePools(UniValue::VARR);
    valuePools.push_back(ValuePoolDesc("transparent", tip->nChainTransparentValue, std::nullopt));
    valuePools.push_back(ValuePoolDesc("sprout", tip->nChainSproutValue, std::nullopt));
    valuePools.push_back(ValuePoolDesc("sapling", tip->nChainSaplingValue, std::nullopt));
    valuePools.push_back(ValuePoolDesc("orchard", tip->nChainOrchardValue, std::nullopt));
    valuePools.push_back(ValuePoolDesc("lockbox", tip->nChainLockboxValue, std::nullopt));
    obj.pushKV("valuePools",            valuePools);

    const CChainParams& chainparams = Params();
    const Consensus::Params& consensusParams = chainparams.GetConsensus();

    UniValue softforks(UniValue::VARR);
    softforks.push_back(SoftForkDesc("bip34", 2, tip, consensusParams));
    softforks.push_back(SoftForkDesc("bip66", 3, tip, consensusParams));
    softforks.push_back(SoftForkDesc("bip65", 4, tip, consensusParams));
    obj.pushKV("softforks",             softforks);

    UniValue upgrades(UniValue::VOBJ);
    for (int i = Consensus::UPGRADE_OVERWINTER; i < Consensus::MAX_NETWORK_UPGRADES; i++) {
        NetworkUpgradeDescPushBack(upgrades, consensusParams, Consensus::UpgradeIndex(i), tip->nHeight);
    }
    obj.pushKV("upgrades", upgrades);

    UniValue consensus(UniValue::VOBJ);
    consensus.pushKV("chaintip", HexInt(CurrentEpochBranchId(tip->nHeight, consensusParams)));
    consensus.pushKV("nextblock", HexInt(CurrentEpochBranchId(tip->nHeight + 1, consensusParams)));
    obj.pushKV("consensus", consensus);

    if (fPruneMode)
    {
        CBlockIndex *block = chainActive.Tip();
        while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA))
            block = block->pprev;

        obj.pushKV("pruneheight",        block->nHeight);
    }

    if (Params().NetworkIDString() == "regtest") {
        obj.pushKV("fullyNotified", ChainIsFullyNotified(chainparams));
    }

    return obj;
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight
{
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->nHeight != b->nHeight)
          return (a->nHeight > b->nHeight);

        return a < b;
    }
};

UniValue getchaintips(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getchaintips\n"
            "Return information about all known tips in the block tree,"
            " including the main chain as well as orphaned branches.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,         (numeric) height of the chain tip\n"
            "    \"hash\": \"xxxx\",         (string) block hash of the tip\n"
            "    \"branchlen\": 0          (numeric) zero for main chain\n"
            "    \"status\": \"active\"      (string) \"active\" for the main chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1          (numeric) length of branch connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"        (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"
            "Possible values for status:\n"
            "1.  \"invalid\"               This branch contains at least one invalid block\n"
            "2.  \"headers-only\"          Not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"         All blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"            This branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                This is the tip of the active main chain, which is certainly valid\n"
            "\nExamples:\n"
            + HelpExampleCli("getchaintips", "")
            + HelpExampleRpc("getchaintips", "")
        );

    LOCK(cs_main);

    /* Build up a list of chain tips.  We start with the list of all
       known blocks, and successively remove blocks that appear as pprev
       of another block.  */
    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    for (const std::pair<const uint256, CBlockIndex*>& item : mapBlockIndex)
        setTips.insert(item.second);
    for (const std::pair<const uint256, CBlockIndex*>& item : mapBlockIndex)
    {
        const CBlockIndex* pprev = item.second->pprev;
        if (pprev)
            setTips.erase(pprev);
    }

    // Always report the currently active tip.
    setTips.insert(chainActive.Tip());

    /* Construct the output array.  */
    UniValue res(UniValue::VARR);
    for (const CBlockIndex* block : setTips)
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("height", block->nHeight);
        obj.pushKV("hash", block->phashBlock->GetHex());

        const int branchLen = block->nHeight - chainActive.FindFork(block)->nHeight;
        obj.pushKV("branchlen", branchLen);

        string status;
        if (chainActive.Contains(block)) {
            // This block is part of the currently active chain.
            status = "active";
        } else if (block->nStatus & BLOCK_FAILED_MASK) {
            // This block or one of its ancestors is invalid.
            status = "invalid";
        } else if (block->nChainTx == 0) {
            // This block cannot be connected because full block data for it or one of its parents is missing.
            status = "headers-only";
        } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
            // This block is fully validated, but no longer part of the active chain. It was probably the active block once, but was reorganized.
            status = "valid-fork";
        } else if (block->IsValid(BLOCK_VALID_TREE)) {
            // The headers for this block are valid, but it has not been validated. It was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // No clue.
            status = "unknown";
        }
        obj.pushKV("status", status);

        res.push_back(obj);
    }

    return res;
}

UniValue z_gettreestate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_gettreestate \"hash|height\"\n"
            "Return information about the given block's tree state.\n"
            "\nArguments:\n"
            "1. \"hash|height\"          (string, required) The block hash or height. Height can be negative where -1 is the last known valid block\n"
            "\nResult:\n"
            "{\n"
            "  \"hash\": \"hash\",         (string) hex block hash\n"
            "  \"height\": n,              (numeric) block height\n"
            "  \"time\": n,                (numeric) block time: UTC seconds since the Unix 1970-01-01 epoch\n"
            "  \"sprout\": {\n"
            "    \"skipHash\": \"hash\",   (string) hash of most recent block with more information\n"
            "    \"commitments\": {\n"
            "      \"finalRoot\": \"hex\", (string)\n"
            "      \"finalState\": \"hex\" (string)\n"
            "    }\n"
            "  },\n"
            "  \"sapling\": {\n"
            "    \"skipHash\": \"hash\",   (string) hash of most recent block with more information\n"
            "    \"commitments\": {\n"
            "      \"finalRoot\": \"hex\", (string)\n"
            "                          NB: The serialized representation of this field returned by this method\n"
            "                              was byte-flipped relative to its representation in the `getrawtransaction`\n"
            "                              output in prior releases up to and including v5.2.0. This has now been\n"
            "                              rectified.\n"
            "      \"finalState\": \"hex\" (string)\n"
            "    }\n"
            "  },\n"
            "  \"orchard\": {\n"
            "    \"skipHash\": \"hash\",   (string) hash of most recent block with more information\n"
            "    \"commitments\": {\n"
            "      \"finalRoot\": \"hex\", (string)\n"
            "      \"finalState\": \"hex\" (string)\n"
            "    }\n"
            "  }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_gettreestate", "\"00000000febc373a1da2bd9f887b105ad79ddc26ac26c2b28652d64e5207c5b5\"")
            + HelpExampleRpc("z_gettreestate", "\"00000000febc373a1da2bd9f887b105ad79ddc26ac26c2b28652d64e5207c5b5\"")
            + HelpExampleCli("z_gettreestate", "12800")
            + HelpExampleRpc("z_gettreestate", "12800")
        );

    LOCK(cs_main);

    std::string strHash = params[0].get_str();

    // If height is supplied, find the hash
    if (strHash.size() < (2 * sizeof(uint256))) {
        strHash = chainActive[parseHeightArg(strHash, chainActive.Height())]->GetBlockHash().GetHex();
    }
    uint256 hash(uint256S(strHash));

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
    const CBlockIndex* const pindex = mapBlockIndex[hash];
    if (!chainActive.Contains(pindex)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Requested block is not part of the main chain");
    }

    UniValue res(UniValue::VOBJ);
    res.pushKV("hash", pindex->GetBlockHash().GetHex());
    res.pushKV("height", pindex->nHeight);
    res.pushKV("time", int64_t(pindex->nTime));

    // sprout
    {
        UniValue sprout_result(UniValue::VOBJ);
        UniValue sprout_commitments(UniValue::VOBJ);
        sprout_commitments.pushKV("finalRoot", pindex->hashFinalSproutRoot.GetHex());
        SproutMerkleTree tree;
        if (pcoinsTip->GetSproutAnchorAt(pindex->hashFinalSproutRoot, tree)) {
            CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
            s << tree;
            sprout_commitments.pushKV("finalState", HexStr(s.begin(), s.end()));
        } else {
            // Set skipHash to the most recent block that has a finalState.
            const CBlockIndex* pindex_skip = pindex->pprev;
            while (pindex_skip && !pcoinsTip->GetSproutAnchorAt(pindex_skip->hashFinalSproutRoot, tree)) {
                pindex_skip = pindex_skip->pprev;
            }
            if (pindex_skip) {
                sprout_result.pushKV("skipHash", pindex_skip->GetBlockHash().GetHex());
            }
        }
        sprout_result.pushKV("commitments", sprout_commitments);
        res.pushKV("sprout", sprout_result);
    }

    // sapling
    auto sapling_activation_height = Params().GetConsensus().GetActivationHeight(Consensus::UPGRADE_SAPLING);
    if (sapling_activation_height.has_value()) {
        UniValue sapling_result(UniValue::VOBJ);
        UniValue sapling_commitments(UniValue::VOBJ);
        sapling_commitments.pushKV("finalRoot", pindex->hashFinalSaplingRoot.GetHex());
        bool need_skiphash = false;
        SaplingMerkleTree tree;
        if (pcoinsTip->GetSaplingAnchorAt(pindex->hashFinalSaplingRoot, tree)) {
            CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
            s << tree;
            sapling_commitments.pushKV("finalState", HexStr(s.begin(), s.end()));
        } else {
            // Set skipHash to the most recent block that has a finalState.
            const CBlockIndex* pindex_skip = pindex->pprev;
            auto saplingActive = [&](const CBlockIndex* pindex_cur) -> bool {
                return pindex_cur && pindex_cur->nHeight >= sapling_activation_height.value();
            };
            while (saplingActive(pindex_skip) && !pcoinsTip->GetSaplingAnchorAt(pindex_skip->hashFinalSaplingRoot, tree)) {
                pindex_skip = pindex_skip->pprev;
            }
            if (saplingActive(pindex_skip)) {
                sapling_result.pushKV("skipHash", pindex_skip->GetBlockHash().GetHex());
            }
        }
        sapling_result.pushKV("commitments", sapling_commitments);
        res.pushKV("sapling", sapling_result);
    }

    // orchard
    auto nu5_activation_height = Params().GetConsensus().GetActivationHeight(Consensus::UPGRADE_NU5);
    if (nu5_activation_height.has_value()) {
        UniValue orchard_result(UniValue::VOBJ);
        UniValue orchard_commitments(UniValue::VOBJ);
        auto finalOrchardRootBytes = pindex->hashFinalOrchardRoot;
        orchard_commitments.pushKV("finalRoot", HexStr(finalOrchardRootBytes.begin(), finalOrchardRootBytes.end()));
        bool need_skiphash = false;
        OrchardMerkleFrontier tree;
        if (pcoinsTip->GetOrchardAnchorAt(pindex->hashFinalOrchardRoot, tree)) {
            CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
            s << OrchardMerkleFrontierLegacySer(tree);
            orchard_commitments.pushKV("finalState", HexStr(s.begin(), s.end()));
        } else {
            // Set skipHash to the most recent block that has a finalState.
            const CBlockIndex* pindex_skip = pindex->pprev;
            auto orchardActive = [&](const CBlockIndex* pindex_cur) -> bool {
                return pindex_cur && pindex_cur->nHeight >= nu5_activation_height.value();
            };
            while (orchardActive(pindex_skip) && !pcoinsTip->GetOrchardAnchorAt(pindex_skip->hashFinalOrchardRoot, tree)) {
                pindex_skip = pindex_skip->pprev;
            }
            if (orchardActive(pindex_skip)) {
                orchard_result.pushKV("skipHash", pindex_skip->GetBlockHash().GetHex());
            }
        }
        orchard_result.pushKV("commitments", orchard_commitments);
        res.pushKV("orchard", orchard_result);
    }

    return res;
}

UniValue z_getsubtreesbyindex(const UniValue& params, bool fHelp)
{
    std::string disabledMsg = "";
    if (!fExperimentalLightWalletd) {
        disabledMsg = experimentalDisabledHelpMsg("z_getsubtreesbyindex", {"lightwalletd"});
    }
    if (fHelp || params.size() < 2 || params.size() > 3) {
        auto strHeight = strprintf("%d", libzcash::TRACKED_SUBTREE_HEIGHT);
        throw runtime_error(
            "z_getsubtreesbyindex \"pool\" start_index ( limit )\n"
            "Returns roots of subtrees of the given pool's note commitment tree. Each value returned\n"
            "in the `subtrees` field is the Merkle root of a subtree containing 2^"+strHeight+" leaves.\n"
            + disabledMsg +
            "\nArguments:\n"
            "1. \"pool\"        (string, required) The pool from which subtrees should be returned. Either \"sapling\" or \"orchard\".\n"
            "2. start_index   (numeric, required) The index of the first 2^"+strHeight+"-leaf subtree to return.\n"
            "2. limit         (numeric, optional) The maximum number of subtree values to return.\n"
            "\nResult:\n"
            "{\n"
            "  \"pool\" : \"sapling|orchard\", (string) The shielded pool to which the subtrees belong\n"
            "  \"start_index\": n,      (numeric) The index of the first subtree\n"
            "  \"subtrees\": [          (array) A sequential list of complete subtrees\n"
            "    {\n"
            "      \"root\": \"hash\",    (string) Merkle root of the 2^"+strHeight+"-leaf subtree\n"
            "      \"end_height\": n,   (numeric) height of the block containing the note that completed this subtree\n"
            "    }, ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getsubtreesbyindex", "\"sapling\", 0")
            + HelpExampleRpc("z_getsubtreesbyindex", "\"orchard\", 3, 7")
        );
    }

    if (!fExperimentalLightWalletd) {
        throw JSONRPCError(RPC_MISC_ERROR, "Error: z_getsubtreesbyindex is disabled. "
            "Run './" CLI_NAME " help z_getsubtreesbyindex' for instructions on how to enable this feature.");
    }

    auto strPool = params[0].get_str();
    ShieldedType pool;
    if (strPool == "sapling") {
        pool = SAPLING;
    } else if (strPool == "orchard") {
        pool = ORCHARD;
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Requested pool must be \"sapling\" or \"orchard\"");
    }

    libzcash::SubtreeIndex startIndex = params[1].get_int();
    std::optional<uint64_t> limit = std::nullopt;
    if (params.size() > 2) {
        limit = params[2].get_int();
    }

    LOCK(cs_main);

    UniValue subtrees(UniValue::VARR);
    uint64_t count = 0;
    for (libzcash::SubtreeIndex index = startIndex; ; index++) {
        if (limit.has_value() && count >= limit.value()) {
            break;
        }

        auto subtreeData = pcoinsTip->GetSubtreeData(pool, index);
        if (!subtreeData.has_value()) {
            break;
        }

        UniValue subtree(UniValue::VOBJ);
        subtree.pushKV("root", HexStr(subtreeData->root));
        subtree.pushKV("end_height", subtreeData->nHeight);
        subtrees.push_back(subtree);
        count++;
    }

    UniValue res(UniValue::VOBJ);
    res.pushKV("pool", strPool);
    res.pushKV("start_index", startIndex);
    res.pushKV("subtrees", subtrees);

    return res;
}

UniValue mempoolInfoToJSON()
{
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("size", (int64_t) mempool.size());
    ret.pushKV("bytes", (int64_t) mempool.GetTotalTxSize());
    ret.pushKV("usage", (int64_t) mempool.DynamicMemoryUsage());

    if (Params().NetworkIDString() == "regtest") {
        ret.pushKV("fullyNotified", mempool.IsFullyNotified());
    }

    return ret;
}

UniValue getmempoolinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmempoolinfo\n"
            "\nReturns details on the active state of the TX memory pool.\n"
            "\nResult:\n"
            "{\n"
            "  \"size\": xxxxx                (numeric) Current tx count\n"
            "  \"bytes\": xxxxx               (numeric) Sum of all tx sizes\n"
            "  \"usage\": xxxxx               (numeric) Total memory usage for the mempool\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolinfo", "")
            + HelpExampleRpc("getmempoolinfo", "")
        );

    return mempoolInfoToJSON();
}

UniValue invalidateblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "invalidateblock \"hash\"\n"
            "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n"
            "\nArguments:\n"
            "1. hash   (string, required) the hash of the block to mark as invalid\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("invalidateblock", "\"blockhash\"")
            + HelpExampleRpc("invalidateblock", "\"blockhash\"")
        );

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        InvalidateBlock(state, Params(), pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state, Params());
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}

UniValue reconsiderblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "reconsiderblock \"hash\"\n"
            "\nRemoves invalidity status of a block and its descendants, reconsider them for activation.\n"
            "This can be used to undo the effects of invalidateblock.\n"
            "\nArguments:\n"
            "1. hash   (string, required) the hash of the block to reconsider\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("reconsiderblock", "\"blockhash\"")
            + HelpExampleRpc("reconsiderblock", "\"blockhash\"")
        );

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        ReconsiderBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state, Params());
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "blockchain",         "getblockchaininfo",      &getblockchaininfo,      true  },
    { "blockchain",         "getbestblockhash",       &getbestblockhash,       true  },
    { "blockchain",         "getblockcount",          &getblockcount,          true  },
    { "blockchain",         "getblock",               &getblock,               true  },
    { "blockchain",         "getblockhash",           &getblockhash,           true  },
    { "blockchain",         "getblockheader",         &getblockheader,         true  },
    { "blockchain",         "getchaintips",           &getchaintips,           true  },
    { "blockchain",         "z_gettreestate",         &z_gettreestate,         true  },
    { "blockchain",         "z_getsubtreesbyindex",   &z_getsubtreesbyindex,   true  },
    { "blockchain",         "getdifficulty",          &getdifficulty,          true  },
    { "blockchain",         "getmempoolinfo",         &getmempoolinfo,         true  },
    { "blockchain",         "getrawmempool",          &getrawmempool,          true  },
    { "blockchain",         "gettxout",               &gettxout,               true  },
    { "blockchain",         "gettxoutsetinfo",        &gettxoutsetinfo,        true  },
    { "blockchain",         "verifychain",            &verifychain,            true  },

    // insightexplorer
    { "blockchain",         "getblockdeltas",         &getblockdeltas,         false },
    { "blockchain",         "getblockhashes",         &getblockhashes,         true  },

    /* Not shown in help */
    { "hidden",             "invalidateblock",        &invalidateblock,        true  },
    { "hidden",             "reconsiderblock",        &reconsiderblock,        true  },
};

void RegisterBlockchainRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
