// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_MEMPOOL_LIMIT_H
#define ZCASH_MEMPOOL_LIMIT_H

#include <deque>
#include <map>
#include <optional>
#include <set>

#include "logging.h"
#include "random.h"
#include "primitives/transaction.h"
#include "policy/fees.h"
#include "uint256.h"
#include "util/time.h"
#include "weighted_map.h"

const size_t DEFAULT_MEMPOOL_TOTAL_COST_LIMIT = 80000000;
const int64_t DEFAULT_MEMPOOL_EVICTION_MEMORY_MINUTES = 60;

const size_t EVICTION_MEMORY_ENTRIES = 40000;
const int64_t MIN_TX_COST = 10000;
const int64_t LOW_FEE_PENALTY = 40000;


// This class keeps track of transactions which have been recently evicted from the mempool
// in order to prevent them from being re-accepted for a given amount of time.
class RecentlyEvictedList
{
    const CClock* const clock;

    const size_t capacity;

    const int64_t timeToKeep;

    // Pairs of txid and time (seconds since epoch)
    std::deque<std::pair<uint256, int64_t>> txIdsAndTimes;

    std::set<uint256> txIdSet;

    void pruneList();

public:
    RecentlyEvictedList(const CClock* clock_, size_t capacity_, int64_t timeToKeep_) :
        clock(clock_), capacity(capacity_), timeToKeep(timeToKeep_)
    {
        assert(capacity <= EVICTION_MEMORY_ENTRIES);
    }
    RecentlyEvictedList(const CClock* clock_, int64_t timeToKeep_) :
        RecentlyEvictedList(clock_, EVICTION_MEMORY_ENTRIES, timeToKeep_) {}

    void add(const uint256& txId);
    bool contains(const uint256& txId);
};


// The mempool of a node holds a set of transactions. Each transaction has a *cost*,
// which is an integer defined as:
//   max(memory usage in bytes, 4000)
//
// Each transaction also has an *eviction weight*, which is *cost* + *fee_penalty*,
// where *fee_penalty* is 16000 if the transaction pays a fee less than the
// conventional fee, otherwise 0.

// Calculate cost and eviction weight based on the memory usage and fee.
std::pair<int64_t, int64_t> MempoolCostAndEvictionWeight(const CTransaction& tx, const CAmount& fee);


class MempoolLimitTxSet
{
    WeightedMap<uint256, int64_t, int64_t, GetRandInt64> txmap;
    int64_t capacity;
    int64_t cost;

public:
    MempoolLimitTxSet(int64_t capacity_) : capacity(capacity_), cost(0) {
        assert(capacity >= 0);
    }

    int64_t getTotalWeight() const
    {
        return txmap.getTotalWeight();
    }
    bool empty() const
    {
        return txmap.empty();
    }
    void add(const uint256& txId, int64_t txCost, int64_t txWeight)
    {
        if (txmap.add(txId, txCost, txWeight)) {
            cost += txCost;
        }
    }
    void remove(const uint256& txId)
    {
        cost -= txmap.remove(txId).value_or(0);
    }

    // If the total cost limit has not been exceeded, return std::nullopt. Otherwise,
    // pick a transaction at random with probability proportional to its eviction weight;
    // remove and return that transaction's txid.
    std::optional<uint256> maybeDropRandom()
    {
        if (cost <= capacity) {
            return std::nullopt;
        }
        LogPrint("mempool", "Mempool cost limit exceeded (cost=%d, limit=%d)\n", cost, capacity);
        assert(!txmap.empty());
        auto [txId, txCost, txWeight] = txmap.takeRandom().value();
        cost -= txCost;
        LogPrint("mempool", "Evicting transaction (txid=%s, cost=%d, weight=%d)\n",
            txId.ToString(), txCost, txWeight);
        return txId;
    }
};

#endif // ZCASH_MEMPOOL_LIMIT_H
