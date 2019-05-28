// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#ifdef ENABLE_MINING
#include "pow/tromp/equi_miner.h"
#endif

#include "amount.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#ifdef ENABLE_MINING
#include "crypto/equihash.h"
#endif
#include "hash.h"
#include "key_io.h"
#include "main.h"
#include "metrics.h"
#include "net.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "random.h"
#include "timedata.h"
#include "ui_interface.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"

#include "sodium.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#ifdef ENABLE_MINING
#include <functional>
#endif
#include <mutex>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }
};

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    // Updating time can change work required on testnet:
    if (consensusParams.nPowAllowMinDifficultyBlocksAfterHeight != boost::none) {
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);
    }
}

CBlockTemplate* CreateNewBlock(const CChainParams& chainparams, const CScript& scriptPubKeyIn)
{
    // Create new block
    std::unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience

    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Add dummy coinbase tx as first transaction
    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);
        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        uint32_t consensusBranchId = CurrentEpochBranchId(nHeight, chainparams.GetConsensus());
        pblock->nTime = GetAdjustedTime();
        const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();
        CCoinsViewCache view(pcoinsTip);

        SaplingMerkleTree sapling_tree;
        assert(view.GetSaplingAnchorAt(view.GetBestAnchor(SAPLING), sapling_tree));

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi)
        {
            const CTransaction& tx = mi->GetTx();

            int64_t nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                                    ? nMedianTimePast
                                    : pblock->GetBlockTime();

            if (tx.IsCoinBase() || !IsFinalTx(tx, nHeight, nLockTimeCutoff) || IsExpiredTx(tx, nHeight))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                if (!view.HaveCoins(txin.prevout.hash))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx.find(txin.prevout.hash)->GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }
                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                assert(coins);

                CAmount nValueIn = coins->vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = nHeight - coins->nHeight;

                dPriority += (double)nValueIn * nConf;
            }
            nTotalIn += tx.GetShieldedValueIn();

            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn-tx.GetValueOut(), nTxSize);

            if (porphan)
            {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            }
            else
                vecPriority.push_back(TxPriority(dPriority, feeRate, &(mi->GetTx())));
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        // We want to track the value pool, but if the miner gets
        // invoked on an old block before the hardcoded fallback
        // is active we don't want to trip up any assertions. So,
        // we only adhere to the turnstile (as a miner) if we
        // actually have all of the information necessary to do
        // so.
        CAmount sproutValue = 0;
        CAmount saplingValue = 0;
        bool monitoring_pool_balances = true;
        if (chainparams.ZIP209Enabled()) {
            if (pindexPrev->nChainSproutValue) {
                sproutValue = *pindexPrev->nChainSproutValue;
            } else {
                monitoring_pool_balances = false;
            }
            if (pindexPrev->nChainSaplingValue) {
                saplingValue = *pindexPrev->nChainSaplingValue;
            } else {
                monitoring_pool_balances = false;
            }
        }

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            if (!view.HaveInputs(tx))
                continue;

            CAmount nTxFees = view.GetValueIn(tx)-tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            PrecomputedTransactionData txdata(tx);
            if (!ContextualCheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true, txdata, chainparams.GetConsensus(), consensusBranchId))
                continue;

            if (chainparams.ZIP209Enabled() && monitoring_pool_balances) {
                // Does this transaction lead to a turnstile violation?

                CAmount sproutValueDummy = sproutValue;
                CAmount saplingValueDummy = saplingValue;

                saplingValueDummy += -tx.valueBalance;

                for (auto js : tx.vjoinsplit) {
                    sproutValueDummy += js.vpub_old;
                    sproutValueDummy -= js.vpub_new;
                }

                if (sproutValueDummy < 0) {
                    LogPrintf("CreateNewBlock(): tx %s appears to violate Sprout turnstile\n", tx.GetHash().ToString());
                    continue;
                }
                if (saplingValueDummy < 0) {
                    LogPrintf("CreateNewBlock(): tx %s appears to violate Sapling turnstile\n", tx.GetHash().ToString());
                    continue;
                }

                sproutValue = sproutValueDummy;
                saplingValue = saplingValueDummy;
            }

            UpdateCoins(tx, view, nHeight);

            BOOST_FOREACH(const OutputDescription &outDescription, tx.vShieldedOutput) {
                sapling_tree.append(outDescription.cm);
            }

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fPrintPriority)
            {
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority, feeRate.ToString(), tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->feeRate, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);

        // Create coinbase tx
        CMutableTransaction txNew = CreateNewContextualCMutableTransaction(chainparams.GetConsensus(), nHeight);
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vout.resize(1);
        txNew.vout[0].scriptPubKey = scriptPubKeyIn;
        txNew.vout[0].nValue = GetBlockSubsidy(nHeight, chainparams.GetConsensus());
        // Set to 0 so expiry height does not apply to coinbase txs
        txNew.nExpiryHeight = 0;

        if ((nHeight > 0) && (nHeight <= chainparams.GetConsensus().GetLastFoundersRewardBlockHeight())) {
            // Founders reward is 20% of the block subsidy
            auto vFoundersReward = txNew.vout[0].nValue / 5;
            // Take some reward away from us
            txNew.vout[0].nValue -= vFoundersReward;

            // And give it to the founders
            txNew.vout.push_back(CTxOut(vFoundersReward, chainparams.GetFoundersRewardScriptAtHeight(nHeight)));
        }

        // Add fees
        txNew.vout[0].nValue += nFees;
        txNew.vin[0].scriptSig = CScript() << nHeight << OP_0;

        pblock->vtx[0] = txNew;
        pblocktemplate->vTxFees[0] = -nFees;

        // Randomise nonce
        arith_uint256 nonce = UintToArith256(GetRandHash());
        // Clear the top and bottom 16 bits (for local use as thread flags and counters)
        nonce <<= 32;
        nonce >>= 16;
        pblock->nNonce = ArithToUint256(nonce);

        // Fill in header
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        pblock->hashFinalSaplingRoot   = sapling_tree.root();
        UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
        pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
        pblock->nSolution.clear();
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        CValidationState state;
        if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false))
            throw std::runtime_error("CreateNewBlock(): TestBlockValidity failed");
    }

    return pblocktemplate.release();
}

//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

#ifdef ENABLE_MINING

class MinerAddressScript : public CReserveScript
{
    // CReserveScript requires implementing this function, so that if an
    // internal (not-visible) wallet address is used, the wallet can mark it as
    // important when a block is mined (so it then appears to the user).
    // If -mineraddress is set, the user already knows about and is managing the
    // address, so we don't need to do anything here.
    void KeepScript() {}
};

void GetScriptForMinerAddress(boost::shared_ptr<CReserveScript> &script)
{
    CTxDestination addr = DecodeDestination(GetArg("-mineraddress", ""));
    if (!IsValidDestination(addr)) {
        return;
    }

    boost::shared_ptr<MinerAddressScript> mAddr(new MinerAddressScript());
    CKeyID keyID = boost::get<CKeyID>(addr);

    script = mAddr;
    script->reserveScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}

static bool ProcessBlockFound(const CBlock* pblock, const CChainParams& chainparams)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("ZcashMiner: generated block is stale");
    }

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, chainparams, NULL, pblock, true, NULL))
        return error("ZcashMiner: ProcessNewBlock, block not accepted");

    TrackMinedBlock(pblock->GetHash());

    return true;
}

void static BitcoinMiner(const CChainParams& chainparams)
{
    LogPrintf("ZcashMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("zcash-miner");

    // Each thread has its own counter
    unsigned int nExtraNonce = 0;

    boost::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);

    unsigned int n = chainparams.GetConsensus().nEquihashN;
    unsigned int k = chainparams.GetConsensus().nEquihashK;

    std::string solver = GetArg("-equihashsolver", "default");
    assert(solver == "tromp" || solver == "default");
    LogPrint("pow", "Using Equihash solver \"%s\" with n = %u, k = %u\n", solver, n, k);

    std::mutex m_cs;
    bool cancelSolver = false;
    boost::signals2::connection c = uiInterface.NotifyBlockTip.connect(
        [&m_cs, &cancelSolver](const uint256& hashNewTip) mutable {
            std::lock_guard<std::mutex> lock{m_cs};
            cancelSolver = true;
        }
    );
    miningTimer.start();

    try {
        //throw an error if no script was provided
        if (!coinbaseScript->reserveScript.size())
            throw std::runtime_error("No coinbase script available (mining requires a wallet or -mineraddress)");

        while (true) {
            if (chainparams.MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                miningTimer.stop();
                do {
                    bool fvNodesEmpty;
                    {
                        LOCK(cs_vNodes);
                        fvNodesEmpty = vNodes.empty();
                    }
                    if (!fvNodesEmpty && !IsInitialBlockDownload(chainparams))
                        break;
                    MilliSleep(1000);
                } while (true);
                miningTimer.start();
            }

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            unique_ptr<CBlockTemplate> pblocktemplate(CreateNewBlock(chainparams, coinbaseScript->reserveScript));
            if (!pblocktemplate.get())
            {
                if (GetArg("-mineraddress", "").empty()) {
                    LogPrintf("Error in ZcashMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                } else {
                    // Should never reach here, because -mineraddress validity is checked in init.cpp
                    LogPrintf("Error in ZcashMiner: Invalid -mineraddress\n");
                }
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("Running ZcashMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            //
            // Search
            //
            int64_t nStart = GetTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);

            while (true) {
                // Hash state
                crypto_generichash_blake2b_state state;
                EhInitialiseState(n, k, state);

                // I = the block header minus nonce and solution.
                CEquihashInput I{*pblock};
                CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                ss << I;

                // H(I||...
                crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

                // H(I||V||...
                crypto_generichash_blake2b_state curr_state;
                curr_state = state;
                crypto_generichash_blake2b_update(&curr_state,
                                                  pblock->nNonce.begin(),
                                                  pblock->nNonce.size());

                // (x_1, x_2, ...) = A(I, V, n, k)
                LogPrint("pow", "Running Equihash solver \"%s\" with nNonce = %s\n",
                         solver, pblock->nNonce.ToString());

                std::function<bool(std::vector<unsigned char>)> validBlock =
                        [&pblock, &hashTarget, &chainparams, &m_cs, &cancelSolver, &coinbaseScript]
                        (std::vector<unsigned char> soln) {
                    // Write the solution to the hash and compute the result.
                    LogPrint("pow", "- Checking solution against target\n");
                    pblock->nSolution = soln;
                    solutionTargetChecks.increment();

                    if (UintToArith256(pblock->GetHash()) > hashTarget) {
                        return false;
                    }

                    // Found a solution
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    LogPrintf("ZcashMiner:\n");
                    LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", pblock->GetHash().GetHex(), hashTarget.GetHex());
                    if (ProcessBlockFound(pblock, chainparams)) {
                        // Ignore chain updates caused by us
                        std::lock_guard<std::mutex> lock{m_cs};
                        cancelSolver = false;
                    }
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);
                    coinbaseScript->KeepScript();

                    // In regression test mode, stop mining after a block is found.
                    if (chainparams.MineBlocksOnDemand()) {
                        // Increment here because throwing skips the call below
                        ehSolverRuns.increment();
                        throw boost::thread_interrupted();
                    }

                    return true;
                };
                std::function<bool(EhSolverCancelCheck)> cancelled = [&m_cs, &cancelSolver](EhSolverCancelCheck pos) {
                    std::lock_guard<std::mutex> lock{m_cs};
                    return cancelSolver;
                };

                // TODO: factor this out into a function with the same API for each solver.
                if (solver == "tromp") {
                    // Create solver and initialize it.
                    equi eq(1);
                    eq.setstate(&curr_state);

                    // Initialization done, start algo driver.
                    eq.digit0(0);
                    eq.xfull = eq.bfull = eq.hfull = 0;
                    eq.showbsizes(0);
                    for (u32 r = 1; r < WK; r++) {
                        (r&1) ? eq.digitodd(r, 0) : eq.digiteven(r, 0);
                        eq.xfull = eq.bfull = eq.hfull = 0;
                        eq.showbsizes(r);
                    }
                    eq.digitK(0);
                    ehSolverRuns.increment();

                    // Convert solution indices to byte array (decompress) and pass it to validBlock method.
                    for (size_t s = 0; s < eq.nsols; s++) {
                        LogPrint("pow", "Checking solution %d\n", s+1);
                        std::vector<eh_index> index_vector(PROOFSIZE);
                        for (size_t i = 0; i < PROOFSIZE; i++) {
                            index_vector[i] = eq.sols[s][i];
                        }
                        std::vector<unsigned char> sol_char = GetMinimalFromIndices(index_vector, DIGITBITS);

                        if (validBlock(sol_char)) {
                            // If we find a POW solution, do not try other solutions
                            // because they become invalid as we created a new block in blockchain.
                            break;
                        }
                    }
                } else {
                    try {
                        // If we find a valid block, we rebuild
                        bool found = EhOptimisedSolve(n, k, curr_state, validBlock, cancelled);
                        ehSolverRuns.increment();
                        if (found) {
                            break;
                        }
                    } catch (EhSolverCancelledException&) {
                        LogPrint("pow", "Equihash solver cancelled\n");
                        std::lock_guard<std::mutex> lock{m_cs};
                        cancelSolver = false;
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if (vNodes.empty() && chainparams.MiningRequiresPeers())
                    break;
                if ((UintToArith256(pblock->nNonce) & 0xffff) == 0xffff)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nNonce and nTime
                pblock->nNonce = ArithToUint256(UintToArith256(pblock->nNonce) + 1);
                UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
                if (chainparams.GetConsensus().nPowAllowMinDifficultyBlocksAfterHeight != boost::none)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        miningTimer.stop();
        c.disconnect();
        LogPrintf("ZcashMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        miningTimer.stop();
        c.disconnect();
        LogPrintf("ZcashMiner runtime error: %s\n", e.what());
        return;
    }
    miningTimer.stop();
    c.disconnect();
}

void GenerateBitcoins(bool fGenerate, int nThreads, const CChainParams& chainparams)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0)
        nThreads = GetNumCores();

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        minerThreads->join_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++) {
        minerThreads->create_thread(boost::bind(&BitcoinMiner, boost::cref(chainparams)));
    }
}

#endif // ENABLE_MINING
