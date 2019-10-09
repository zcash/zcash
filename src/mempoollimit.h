// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef MEMPOOLLIMIT_H
#define MEMPOOLLIMIT_H

#include <map>
#include <set>
#include <vector>

#include "boost/optional.hpp"
#include "primitives/transaction.h"
#include "uint256.h"

const size_t DEFAULT_MEMPOOL_TOTAL_COST_LIMIT = 80000000;
const int64_t DEFAULT_MEMPOOL_EVICTION_MEMORY_MINUTES = 60;

const size_t RECENTLY_EVICTED_SIZE = 40000;
const uint64_t MIN_TX_WEIGHT = 4000;
const uint64_t LOW_FEE_PENALTY = 16000; 

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
        assert(capacity <= RECENTLY_EVICTED_SIZE);
    }
    RecentlyEvictedList(int64_t timeToKeep_) : RecentlyEvictedList(RECENTLY_EVICTED_SIZE, timeToKeep_) {}

    void add(const uint256& txId);
    bool contains(const uint256& txId);
};


struct TxWeight {
    int64_t cost;
    int64_t evictionWeight;

    TxWeight(int64_t cost_, int64_t evictionWeight_)
        : cost(cost_), evictionWeight(evictionWeight_) {}

    TxWeight add(const TxWeight& other) const;
    TxWeight negate() const;
};


struct WeightedTxInfo {
    uint256 txId;
    TxWeight txWeight;

    WeightedTxInfo(uint256 txId_, TxWeight txWeight_) : txId(txId_), txWeight(txWeight_) {}

    static WeightedTxInfo from(const CTransaction& tx, const CAmount& fee);
};


class WeightedTxTree
{
    const int64_t capacity;
    size_t size = 0;
    
    std::vector<WeightedTxInfo> txIdAndWeights;
    std::vector<TxWeight> childWeights;
    std::map<uint256, size_t> txIdToIndexMap;

    TxWeight getWeightAt(size_t index) const;
    void backPropagate(size_t fromIndex, const TxWeight& weightDelta);
    size_t findByEvictionWeight(size_t fromIndex, int64_t weightToFind) const;

public:
    WeightedTxTree(int64_t capacity_) : capacity(capacity_) {
        assert(capacity >= 0);
    }

    TxWeight getTotalWeight() const;

    void add(const WeightedTxInfo& weightedTxInfo);
    void remove(const uint256& txId);

    boost::optional<uint256> maybeDropRandom();
};


#endif // MEMPOOLLIMIT_H
