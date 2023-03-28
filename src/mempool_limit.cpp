// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "mempool_limit.h"

#include "core_memusage.h"
#include "serialize.h"
#include "timedata.h"
#include "util/time.h"
#include "version.h"

void RecentlyEvictedList::pruneList()
{
    if (txIdSet.empty()) {
        return;
    }
    int64_t now = clock->GetTime();
    while (txIdsAndTimes.size() > 0 && now - txIdsAndTimes.front().second > timeToKeep) {
        txIdSet.erase(txIdsAndTimes.front().first);
        txIdsAndTimes.pop_front();
    }
}

void RecentlyEvictedList::add(const uint256& txId)
{
    pruneList();
    if (txIdsAndTimes.size() == capacity) {
        txIdSet.erase(txIdsAndTimes.front().first);
        txIdsAndTimes.pop_front();
    }
    txIdsAndTimes.push_back(std::make_pair(txId, clock->GetTime()));
    txIdSet.insert(txId);
}

bool RecentlyEvictedList::contains(const uint256& txId)
{
    pruneList();
    return txIdSet.count(txId) > 0;
}

std::pair<int64_t, int64_t> MempoolCostAndEvictionWeight(const CTransaction& tx, const CAmount& fee)
{
    size_t memUsage = RecursiveDynamicUsage(tx);
    int64_t cost = std::max((int64_t) memUsage, (int64_t) MIN_TX_COST);
    int64_t evictionWeight = cost;
    if (fee < tx.GetConventionalFee()) {
        evictionWeight += LOW_FEE_PENALTY;
    }
    return std::make_pair(cost, evictionWeight);
}
