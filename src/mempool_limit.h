// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_MEMPOOL_LIMIT_H
#define ZCASH_MEMPOOL_LIMIT_H

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "primitives/transaction.h"
#include "policy/fees.h"
#include "uint256.h"

const size_t DEFAULT_MEMPOOL_TOTAL_COST_LIMIT = 80000000;
const int64_t DEFAULT_MEMPOOL_EVICTION_MEMORY_MINUTES = 60;

const size_t EVICTION_MEMORY_ENTRIES = 40000;
const uint64_t MIN_TX_COST = 4000;
const uint64_t LOW_FEE_PENALTY = 16000;


// This class keeps track of transactions which have been recently evicted from the mempool
// in order to prevent them from being re-accepted for a given amount of time.
class RecentlyEvictedList
{
    const size_t capacity;

    const int64_t timeToKeep;

    // Pairs of txid and time (seconds since epoch)
    std::deque<std::pair<uint256, int64_t>> txIdsAndTimes;

    std::set<uint256> txIdSet;

    void pruneList();

public:
    RecentlyEvictedList(size_t capacity_, int64_t timeToKeep_) : capacity(capacity_), timeToKeep(timeToKeep_)
    {
        assert(capacity <= EVICTION_MEMORY_ENTRIES);
    }
    RecentlyEvictedList(int64_t timeToKeep_) : RecentlyEvictedList(EVICTION_MEMORY_ENTRIES, timeToKeep_) {}

    void add(const uint256& txId);
    bool contains(const uint256& txId);
};


// The mempool of a node holds a set of transactions. Each transaction has a *cost*,
// which is an integer defined as:
//   max(serialized transaction size in bytes, 4000)
// Each transaction also has an *eviction weight*, which is *cost* + *fee_penalty*,
// where *fee_penalty* is 16000 if the transaction pays a fee less than 10000 zatoshi,
// otherwise 0.
struct TxWeight {
    int64_t cost;
    int64_t evictionWeight; // *cost* + *fee_penalty*

    TxWeight(int64_t cost_, int64_t evictionWeight_)
        : cost(cost_), evictionWeight(evictionWeight_) {}

    TxWeight add(const TxWeight& other) const;
    TxWeight negate() const;
};


// This struct is a pair of txid, cost.
struct WeightedTxInfo {
    uint256 txId;
    TxWeight txWeight;

    WeightedTxInfo(uint256 txId_, TxWeight txWeight_) : txId(txId_), txWeight(txWeight_) {}

    // Factory method which calculates cost based on size in bytes and fee.
    static WeightedTxInfo from(const CTransaction& tx, const CAmount& fee);
};


// The following class is a collection of transaction ids and their costs.
// In order to be able to remove transactions randomly weighted by their cost,
// we keep track of the total cost of all transactions in this collection.
// For performance reasons, the collection is represented as a complete binary
// tree where each node knows the sum of the weights of the children. This
// allows for addition, removal, and random selection/dropping in logarithmic time.
class WeightedTxTree
{
    const int64_t capacity;
    size_t size = 0;

    // The following two vectors are the tree representation of this collection.
    // We keep track of 3 data points for each node: A transaction's txid, its cost,
    // and the sum of the weights of all children and descendant of that node.
    std::vector<WeightedTxInfo> txIdAndWeights;
    std::vector<TxWeight> childWeights;

    // The following map is to simplify removal. When removing a tx, we do so by txid.
    // This map allows looking up the transaction's index in the tree.
    std::map<uint256, size_t> txIdToIndexMap;

    // Returns the sum of a node and all of its children's TxWeights for a given index.
    TxWeight getWeightAt(size_t index) const;

    // When adding and removing a node we need to update its parent and all of its
    // ancestors to reflect its cost.
    void backPropagate(size_t fromIndex, const TxWeight& weightDelta);

    // For a given random cost + fee penalty, this method recursively finds the
    // correct transaction. This is used by WeightedTxTree::maybeDropRandom().
    size_t findByEvictionWeight(size_t fromIndex, int64_t weightToFind) const;

public:
    WeightedTxTree(int64_t capacity_) : capacity(capacity_) {
        assert(capacity >= 0);
    }

    TxWeight getTotalWeight() const;

    void add(const WeightedTxInfo& weightedTxInfo);
    void remove(const uint256& txId);

    // If the total cost limit is exceeded, pick a random number based on the total cost
    // of the collection and remove the associated transaction.
    std::optional<uint256> maybeDropRandom();
};


#endif // ZCASH_MEMPOOL_LIMIT_H
