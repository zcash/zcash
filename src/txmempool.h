// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_TXMEMPOOL_H
#define BITCOIN_TXMEMPOOL_H

#include <list>
#include <memory>

#include "amount.h"
#include "coins.h"
#include "mempool_limit.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "random.h"
#include "addressindex.h"
#include "spentindex.h"

#undef foreach
#include "boost/multi_index_container.hpp"
#include "boost/multi_index/ordered_index.hpp"
#include "boost/multi_index/hashed_index.hpp"

class CAutoFile;

inline double AllowFreeThreshold()
{
    return COIN * 144 / 250;
}

inline bool AllowFree(double dPriority)
{
    // Large (in bytes) low-priority (new, small-coin) transactions
    // need a fee.
    return dPriority > AllowFreeThreshold();
}

/** Fake height value used in CCoins to signify they are only in the memory pool (since 0.8) */
static const unsigned int MEMPOOL_HEIGHT = 0x7FFFFFFF;

/**
 * CTxMemPool stores these:
 */
class CTxMemPoolEntry
{
private:
    std::shared_ptr<const CTransaction> tx;
    CAmount nFee;              //!< Cached to avoid expensive parent-transaction lookups
    size_t nTxSize;            //!< ... and avoid recomputing tx size
    size_t nModSize;           //!< ... and modified size for priority
    size_t nUsageSize;         //!< ... and total memory usage
    CFeeRate feeRate;          //!< ... and fee per kB
    int64_t nTime;             //!< Local time when entering the mempool
    double dPriority;          //!< Priority when entering the mempool
    unsigned int nHeight;      //!< Chain height when entering the mempool
    bool hadNoDependencies;    //!< Not dependent on any other txs when it entered the mempool
    bool spendsCoinbase;       //!< keep track of transactions that spend a coinbase
    unsigned int sigOpCount;   //!< Legacy sig ops plus P2SH sig op count
    int64_t feeDelta;          //!< Used for determining the priority of the transaction for mining in a block
    uint32_t nBranchId;        //!< Branch ID this transaction is known to commit to, cached for efficiency

public:
    CTxMemPoolEntry(const CTransaction& _tx, const CAmount& _nFee,
                    int64_t _nTime, double _dPriority, unsigned int _nHeight,
                    bool poolHasNoInputsOf, bool spendsCoinbase,
                    unsigned int nSigOps, uint32_t nBranchId);
    CTxMemPoolEntry();
    CTxMemPoolEntry(const CTxMemPoolEntry& other);

    const CTransaction& GetTx() const { return *this->tx; }
    std::shared_ptr<const CTransaction> GetSharedTx() const { return this->tx; }
    double GetPriority(unsigned int currentHeight) const;
    const CAmount& GetFee() const { return nFee; }
    CFeeRate GetFeeRate() const { return feeRate; }
    size_t GetTxSize() const { return nTxSize; }
    int64_t GetTime() const { return nTime; }
    unsigned int GetHeight() const { return nHeight; }
    bool WasClearAtEntry() const { return hadNoDependencies; }
    unsigned int GetSigOpCount() const { return sigOpCount; }
    int64_t GetModifiedFee() const { return nFee + feeDelta; }
    size_t DynamicMemoryUsage() const { return nUsageSize; }

    // Updates the fee delta used for mining priority score
    void UpdateFeeDelta(int64_t feeDelta);

    bool GetSpendsCoinbase() const { return spendsCoinbase; }
    uint32_t GetValidatedBranchId() const { return nBranchId; }
};

struct update_fee_delta
{
    update_fee_delta(int64_t _feeDelta) : feeDelta(_feeDelta) { }

    void operator() (CTxMemPoolEntry &e) { e.UpdateFeeDelta(feeDelta); }

private:
    int64_t feeDelta;
};

// extracts a TxMemPoolEntry's transaction hash
struct mempoolentry_txid
{
    typedef uint256 result_type;
    result_type operator() (const CTxMemPoolEntry &entry) const
    {
        return entry.GetTx().GetHash();
    }
};

class CompareTxMemPoolEntryByFee
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        if (a.GetFeeRate() == b.GetFeeRate())
            return a.GetTime() >= b.GetTime();
        return a.GetFeeRate() < b.GetFeeRate();
    }
};

/** \class CompareTxMemPoolEntryByScore
 *
 *  Sort by score of entry ((fee+delta)/size) in descending order
 */
class CompareTxMemPoolEntryByScore
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        double f1 = (double)a.GetModifiedFee() * b.GetTxSize();
        double f2 = (double)b.GetModifiedFee() * a.GetTxSize();
        if (f1 == f2) {
            return b.GetTx().GetHash() < a.GetTx().GetHash();
        }
        return f1 > f2;
    }
};

class CBlockPolicyEstimator;

/** An inpoint - a combination of a transaction and an index n into its vin */
class CInPoint
{
public:
    const CTransaction* ptx;
    uint32_t n;

    CInPoint() { SetNull(); }
    CInPoint(const CTransaction* ptxIn, uint32_t nIn) { ptx = ptxIn; n = nIn; }
    void SetNull() { ptx = NULL; n = (uint32_t) -1; }
    bool IsNull() const { return (ptx == NULL && n == (uint32_t) -1); }
    size_t DynamicMemoryUsage() const { return 0; }
};

/**
 * Information about a mempool transaction.
 */
struct TxMempoolInfo
{
    /** The transaction itself */
    std::shared_ptr<const CTransaction> tx;

    /** Time the transaction entered the mempool. */
    int64_t nTime;

    /** Feerate of the transaction. */
    CFeeRate feeRate;
};

/**
 * CTxMemPool stores valid-according-to-the-current-best-chain
 * transactions that may be included in the next block.
 *
 * Transactions are added when they are seen on the network
 * (or created by the local node), but not all transactions seen
 * are added to the pool: if a new transaction double-spends
 * an input of a transaction in the pool, it is dropped,
 * as are non-standard transactions.
 *
 * CTxMemPool::mapTx, and CTxMemPoolEntry bookkeeping:
 *
 * mapTx is a boost::multi_index that sorts the mempool on 3 criteria:
 * - transaction hash
 * - feerate
 * - mining score (feerate modified by any fee deltas from PrioritiseTransaction)
 *
 */
class CTxMemPool
{
private:
    uint32_t nCheckFrequency; //!< Value n means that n times in 2^32 we check.
    unsigned int nTransactionsUpdated;
    CBlockPolicyEstimator* minerPolicyEstimator;

    uint64_t totalTxSize = 0;  //!< sum of all mempool tx' byte sizes
    uint64_t cachedInnerUsage; //!< sum of dynamic memory usage of all the map elements (NOT the maps themselves)

    std::map<uint256, const CTransaction*> mapRecentlyAddedTx;
    uint64_t nRecentlyAddedSequence = 0;
    uint64_t nNotifiedSequence = 0;

    std::map<uint256, const CTransaction*> mapSproutNullifiers;
    std::map<uint256, const CTransaction*> mapSaplingNullifiers;
    std::map<uint256, const CTransaction*> mapOrchardNullifiers;
    RecentlyEvictedList* recentlyEvicted = new RecentlyEvictedList(DEFAULT_MEMPOOL_EVICTION_MEMORY_MINUTES * 60);
    WeightedTxTree* weightedTxTree = new WeightedTxTree(DEFAULT_MEMPOOL_TOTAL_COST_LIMIT);

    void checkNullifiers(ShieldedType type) const;

    CFeeRate minReasonableRelayFee;

public:
    typedef boost::multi_index_container<
        CTxMemPoolEntry,
        boost::multi_index::indexed_by<
            // sorted by txid
            boost::multi_index::hashed_unique<mempoolentry_txid, SaltedTxidHasher>,
            // sorted by fee rate
            boost::multi_index::ordered_non_unique<
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByFee
            >,
            // sorted by score (for mining prioritization)
            boost::multi_index::ordered_unique<
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByScore
            >
        >
    > indexed_transaction_set;

    mutable CCriticalSection cs;
    indexed_transaction_set mapTx;
    typedef indexed_transaction_set::nth_index<0>::type::iterator txiter;

private:
    // insightexplorer
    std::map<CMempoolAddressDeltaKey, CMempoolAddressDelta, CMempoolAddressDeltaKeyCompare> mapAddress;
    std::map<uint256, std::vector<CMempoolAddressDeltaKey> > mapAddressInserted;
    std::map<CSpentIndexKey, CSpentIndexValue, CSpentIndexKeyCompare> mapSpent;
    std::map<uint256, std::vector<CSpentIndexKey>> mapSpentInserted;

    std::vector<indexed_transaction_set::const_iterator> GetSortedDepthAndScore() const;

public:
    std::map<COutPoint, CInPoint> mapNextTx;
    std::map<uint256, std::pair<double, CAmount> > mapDeltas;

    /** Create a new CTxMemPool.
     *  minReasonableRelayFee should be a feerate which is, roughly, somewhere
     *  around what it "costs" to relay a transaction around the network and
     *  below which we would reasonably say a transaction has 0-effective-fee.
     */
    CTxMemPool(const CFeeRate& _minReasonableRelayFee);
    ~CTxMemPool();

    /**
     * If sanity-checking is turned on, check makes sure the pool is
     * consistent (does not contain two transactions that spend the same inputs,
     * all inputs are in the mapNextTx array). If sanity-checking is turned off,
     * check does nothing.
     */
    void check(const CCoinsViewCache *pcoins) const;
    void setSanityCheck(double dFrequency = 1.0) { nCheckFrequency = static_cast<uint32_t>(dFrequency * 4294967295.0); }

    bool addUnchecked(const uint256& hash, const CTxMemPoolEntry &entry, bool fCurrentEstimate = true);

    // START insightexplorer
    void addAddressIndex(const CTxMemPoolEntry &entry, const CCoinsViewCache &view);
    void getAddressIndex(const std::vector<std::pair<uint160, int>>& addresses,
                         std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta>>& results);
    void removeAddressIndex(const uint256& txhash);

    void addSpentIndex(const CTxMemPoolEntry &entry, const CCoinsViewCache &view);
    bool getSpentIndex(const CSpentIndexKey &key, CSpentIndexValue &value);
    void removeSpentIndex(const uint256 txhash);
    // END insightexplorer

    void remove(const CTransaction &tx, std::list<CTransaction>& removed, bool fRecursive = false);
    void removeWithAnchor(const uint256 &invalidRoot, ShieldedType type);
    void removeForReorg(const CCoinsViewCache *pcoins, unsigned int nMemPoolHeight, int flags);
    void removeConflicts(const CTransaction &tx, std::list<CTransaction>& removed);
    std::vector<uint256> removeExpired(unsigned int nBlockHeight);
    void removeForBlock(const std::vector<CTransaction>& vtx, unsigned int nBlockHeight,
                        std::list<CTransaction>& conflicts, bool fCurrentEstimate = true);
    void removeWithoutBranchId(uint32_t nMemPoolBranchId);
    void clear();
    void _clear(); // unlocked
    bool CompareDepthAndScore(const uint256& hasha, const uint256& hashb);
    void queryHashes(std::vector<uint256>& vtxid);
    void pruneSpent(const uint256& hash, CCoins &coins);
    unsigned int GetTransactionsUpdated() const;
    void AddTransactionsUpdated(unsigned int n);
    /**
     * Check that none of this transactions inputs are in the mempool, and thus
     * the tx is not dependent on other mempool transactions to be included in a block.
     */
    bool HasNoInputsOf(const CTransaction& tx) const;

    /** Affect CreateNewBlock prioritisation of transactions */
    void PrioritiseTransaction(const uint256 hash, const std::string strHash, double dPriorityDelta, const CAmount& nFeeDelta);
    void ApplyDeltas(const uint256 hash, double &dPriorityDelta, CAmount &nFeeDelta) const;
    void ClearPrioritisation(const uint256 hash);

    bool nullifierExists(const uint256& nullifier, ShieldedType type) const;

    std::pair<std::vector<CTransaction>, uint64_t> DrainRecentlyAdded();
    void SetNotifiedSequence(uint64_t recentlyAddedSequence);
    bool IsFullyNotified();

    unsigned long size()
    {
        LOCK(cs);
        return mapTx.size();
    }

    uint64_t GetTotalTxSize()
    {
        LOCK(cs);
        return totalTxSize;
    }

    bool exists(uint256 hash) const
    {
        LOCK(cs);
        return (mapTx.count(hash) != 0);
    }

    std::shared_ptr<const CTransaction> get(const uint256& hash) const;
    TxMempoolInfo info(const uint256& hash) const;
    std::vector<TxMempoolInfo> infoAll() const;

    /** Estimate fee rate needed to get into the next nBlocks */
    CFeeRate estimateFee(int nBlocks) const;

    /** Estimate priority needed to get into the next nBlocks */
    double estimatePriority(int nBlocks) const;

    /** Write/Read estimates to disk */
    bool WriteFeeEstimates(CAutoFile& fileout) const;
    bool ReadFeeEstimates(CAutoFile& filein);

    size_t DynamicMemoryUsage() const;

    /** Return nCheckFrequency */
    uint32_t GetCheckFrequency() const {
        return nCheckFrequency;
    }

    void SetMempoolCostLimit(int64_t totalCostLimit, int64_t evictionMemorySeconds);
    // Returns true if a transaction has been recently evicted
    bool IsRecentlyEvicted(const uint256& txId);
    // If the mempool size limit is exceeded, this evicts transactions from the mempool until it is below capacity
    void EnsureSizeLimit();
};

/**
 * CCoinsView that brings transactions from a memorypool into view.
 * It does not check for spendings by memory pool transactions.
 */
class CCoinsViewMemPool : public CCoinsViewBacked
{
protected:
    CTxMemPool &mempool;

public:
    CCoinsViewMemPool(CCoinsView *baseIn, CTxMemPool &mempoolIn);
    bool GetNullifier(const uint256 &txid, ShieldedType type) const;
    bool GetCoins(const uint256 &txid, CCoins &coins) const;
    bool HaveCoins(const uint256 &txid) const;
};

// We want to sort transactions by coin age priority
typedef std::pair<double, CTxMemPool::txiter> TxCoinAgePriority;

struct TxCoinAgePriorityCompare
{
    bool operator()(const TxCoinAgePriority& a, const TxCoinAgePriority& b)
    {
        if (a.first == b.first)
            return CompareTxMemPoolEntryByScore()(*(b.second), *(a.second)); //Reverse order to make sort less than
        return a.first < b.first;
    }
};

#endif // BITCOIN_TXMEMPOOL_H
