// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef MEMPOOLLIMIT_H
#define MEMPOOLLIMIT_H

#include <vector>
#include <set>
#include "uint256.h"
#include "primitives/transaction.h"

#include "boost/optional.hpp"

const size_t RECENTLY_EVICTED_SIZE = 10000;
const uint64_t MIN_TX_COST = 4000;
const uint64_t LOW_FEE_PENALTY = 16000; 

struct WeightedTxInfo;

class RecentlyEvictedList
{
    const size_t maxSize;
    const int64_t timeToKeep;
    // Pairs of txid and time (seconds since epoch)
    boost::optional<std::pair<uint256, int64_t>> txIdsAndTimes[RECENTLY_EVICTED_SIZE];
    size_t txIdsAndTimesIndex;
    std::set<uint256> txIdSet;

    void pruneList();
public:
    RecentlyEvictedList(size_t maxSize_, int64_t timeToKeep_) :
        maxSize(maxSize_),
        timeToKeep(timeToKeep_),
        txIdsAndTimesIndex(0)    
    {
        assert(maxSize <= RECENTLY_EVICTED_SIZE);
        std::fill_n(txIdsAndTimes, maxSize, boost::none);
    }
    RecentlyEvictedList(int64_t timeToKeep_) : RecentlyEvictedList(RECENTLY_EVICTED_SIZE, timeToKeep_) {}

    void add(uint256 txId);
    bool contains(const uint256& txId);
};


class WeightedTransactionList
{
    const uint64_t maxTotalCost;
    std::vector<WeightedTxInfo> weightedTxInfos;
public:
    WeightedTransactionList(int64_t maxTotalCost_) : maxTotalCost(maxTotalCost_) {}

    void clear();

    int64_t getTotalCost() const;
    int64_t getTotalLowFeePenaltyCost() const;

    void add(WeightedTxInfo weightedTxInfo);
    boost::optional<WeightedTxInfo> maybeDropRandom(bool rebuildList);
};


struct WeightedTxInfo {
    uint256 txId;
    uint64_t cost;
    uint64_t lowFeePenaltyCost;

    WeightedTxInfo(uint256 txId_, uint64_t cost_, uint64_t lowFeePenaltyCost_)
        : txId(txId_), cost(cost_), lowFeePenaltyCost(lowFeePenaltyCost_) {}

    static WeightedTxInfo from(const CTransaction& tx, const CAmount& fee);
    
    void plusEquals(const WeightedTxInfo& other);
    void minusEquals(const WeightedTxInfo& other);
};

#endif // MEMPOOLLIMIT_H
