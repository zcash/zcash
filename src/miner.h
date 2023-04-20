// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
// Copyright (c) 2017-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include "primitives/block.h"
#include "txmempool.h"
#include "weighted_map.h"

#include <stdint.h>
#include <memory>
#include <variant>

#include <boost/shared_ptr.hpp>

class CBlockIndex;
class CChainParams;
class CScript;

namespace Consensus { struct Params; };

static const bool DEFAULT_GENERATE = false;
static const int DEFAULT_GENERATE_THREADS = 1;

static const bool DEFAULT_PRINTPRIORITY = false;

typedef std::variant<
    libzcash::OrchardRawAddress,
    libzcash::SaplingPaymentAddress,
    boost::shared_ptr<CReserveScript>> MinerAddress;

class ExtractMinerAddress
{
    const Consensus::Params& consensus;
    int height;

public:
    ExtractMinerAddress(const Consensus::Params& consensus, int height) :
        consensus(consensus), height(height) {}

    std::optional<MinerAddress> operator()(const CKeyID &keyID) const;
    std::optional<MinerAddress> operator()(const CScriptID &addr) const;
    std::optional<MinerAddress> operator()(const libzcash::SproutPaymentAddress &addr) const;
    std::optional<MinerAddress> operator()(const libzcash::SaplingPaymentAddress &addr) const;
    std::optional<MinerAddress> operator()(const libzcash::UnifiedAddress &addr) const;
};

class KeepMinerAddress
{
public:
    KeepMinerAddress() {}

    void operator()(const libzcash::OrchardRawAddress &addr) const {}
    void operator()(const libzcash::SaplingPaymentAddress &pa) const {}
    void operator()(const boost::shared_ptr<CReserveScript> &coinbaseScript) const {
        coinbaseScript->KeepScript();
    }
};

bool IsShieldedMinerAddress(const MinerAddress& minerAddr);

class IsValidMinerAddress
{
public:
    IsValidMinerAddress() {}

    bool operator()(const libzcash::OrchardRawAddress &addr) const {
        return true;
    }
    bool operator()(const libzcash::SaplingPaymentAddress &pa) const {
        return true;
    }
    bool operator()(const boost::shared_ptr<CReserveScript> &coinbaseScript) const {
        // Return false if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        return coinbaseScript.get() && !coinbaseScript->reserveScript.empty();
    }
};

struct CBlockTemplate
{
    CBlock block;
    // Cached whenever we update `block`, so we can update hashBlockCommitments
    // when we change the coinbase transaction.
    uint256 hashChainHistoryRoot;
    // Cached whenever we update `block`, so we can return it from `getblocktemplate`
    // (enabling the caller to update `hashBlockCommitments` when they change
    // `hashPrevBlock`).
    uint256 hashAuthDataRoot;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;
};

CMutableTransaction CreateCoinbaseTransaction(const CChainParams& chainparams, CAmount nFees, const MinerAddress& minerAddress, int nHeight);

/** Generate a new block, without valid proof-of-work */
class BlockAssembler
{
private:
    // The constructed block template
    std::unique_ptr<CBlockTemplate> pblocktemplate;
    // A convenience pointer that always refers to the CBlock in pblocktemplate
    CBlock* pblock;

    // Configuration parameters for the block size and unpaid action limit
    unsigned int nBlockMaxSize;
    size_t nBlockUnpaidActionLimit;

    // Information on the current status of the block
    uint64_t nBlockSize;
    uint64_t nBlockTx;
    unsigned int nBlockSigOps;
    CAmount nFees;
    CTxMemPool::setEntries inBlock;

    // Information on the current chain state after this block
    CAmount sproutValue;
    CAmount saplingValue;
    CAmount orchardValue;
    bool monitoring_pool_balances;

    // Chain context for the block
    int nHeight;
    int64_t nLockTimeCutoff;
    const CChainParams& chainparams;

    // Variables used for addScoreTxs and addPriorityTxs
    int lastFewTxs;
    bool blockFinished;

public:
    BlockAssembler(const CChainParams& chainparams);
    /** Construct a new block template with coinbase to minerAddress */
    CBlockTemplate* CreateNewBlock(
        const MinerAddress& minerAddress,
        const std::optional<CMutableTransaction>& next_coinbase_mtx = std::nullopt);

private:
    void constructZIP317BlockTemplate();
    void addTransactions(
        CTxMemPool::weightedCandidates& candidates,
        CTxMemPool::queueEntries& waiting,
        CTxMemPool::queueEntries& cleared);

    // utility functions
    /** Clear the block's state and prepare for assembling a new block */
    void resetBlock(const MinerAddress& minerAddress);
    /** Add a tx to the block */
    void AddToBlock(CTxMemPool::txiter iter);

    // Methods for how to add transactions to a block.
    /** Add transactions based on modified feerate */
    void addScoreTxs();

    // helper function for addScoreTxs and addPriorityTxs
    /** Test if tx will still "fit" in the block */
    bool TestForBlock(CTxMemPool::txiter iter);
    /** Test if tx still has unconfirmed parents not yet in block */
    bool isStillDependent(CTxMemPool::txiter iter);
};

#ifdef ENABLE_MINING
/** Get -mineraddress */
void GetMinerAddress(std::optional<MinerAddress> &minerAddress);
/** Modify the extranonce in a block */
void IncrementExtraNonce(
    CBlockTemplate* pblocktemplate,
    const CBlockIndex* pindexPrev,
    unsigned int& nExtraNonce,
    const Consensus::Params& consensusParams);
/** Run the miner threads */
void GenerateBitcoins(bool fGenerate, int nThreads, const CChainParams& chainparams);
#endif

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev);

#endif // BITCOIN_MINER_H
