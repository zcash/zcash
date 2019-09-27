// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php 

#include "core_memusage.h"
#include "mempoollimit.h"
#include "random.h"
#include "serialize.h"
#include "timedata.h"
#include "version.h"

const CAmount DEFAULT_FEE = 10000;

void RecentlyEvictedList::pruneList()
{
    if (txIdSet.empty()) {
        return;
    }
    int64_t now = GetAdjustedTime();
    size_t startIndex = (txIdsAndTimesIndex + maxSize - txIdSet.size()) % maxSize;
    boost::optional<std::pair<uint256, int64_t>> txIdAndTime;
    while ((txIdAndTime = txIdsAndTimes[startIndex]).is_initialized() && (now - txIdAndTime.get().second) > timeToKeep) {
        txIdsAndTimes[startIndex] = boost::none;
        txIdSet.erase(txIdAndTime.get().first);
        startIndex = (startIndex + 1) % maxSize;
    }
}
void RecentlyEvictedList::add(uint256 txId)
{
    pruneList();
    if (txIdsAndTimes[txIdsAndTimesIndex].is_initialized()) {
        auto txIdAndTime = txIdsAndTimes[txIdsAndTimesIndex];
        txIdSet.erase(txIdAndTime.get().first);
    }
    txIdsAndTimes[txIdsAndTimesIndex] = std::make_pair(txId, GetAdjustedTime());
    txIdSet.insert(txId);
    txIdsAndTimesIndex = (txIdsAndTimesIndex + 1) % maxSize;
}

bool RecentlyEvictedList::contains(const uint256& txId)
{
    pruneList();
    return txIdSet.count(txId) > 0;
}


void WeightedTransactionList::clear() {
    weightedTxInfos.clear();
}

int64_t WeightedTransactionList::getTotalCost() const
{
    return weightedTxInfos.empty() ? 0 : weightedTxInfos.back().cost;
}

int64_t WeightedTransactionList::getTotalLowFeePenaltyCost() const
{
    return weightedTxInfos.empty() ? 0 : weightedTxInfos.back().lowFeePenaltyCost;
}

void WeightedTransactionList::add(WeightedTxInfo weightedTxInfo)
{
    if (weightedTxInfos.empty()) {
        weightedTxInfos.push_back(weightedTxInfo);
        return;
    }
    weightedTxInfo.plusEquals(weightedTxInfos.back());
    weightedTxInfos.push_back(weightedTxInfo);
    for (int i =0; i < weightedTxInfos.size(); ++i) {
        WeightedTxInfo info = weightedTxInfos[i];
    }
}
 
boost::optional<WeightedTxInfo> WeightedTransactionList::maybeDropRandom(bool rebuildList)
{
    int64_t totalCost = getTotalCost();
    if (totalCost <= maxTotalCost) {
        return boost::none;
    }
    LogPrint("mempool", "Mempool cost limit exceeded (cost=%d, limit=%d)\n", totalCost, maxTotalCost);
    int randomWeight = GetRand(getTotalLowFeePenaltyCost());
    int i = 0;
    while (randomWeight > weightedTxInfos[i].lowFeePenaltyCost) {
        ++i;
    }
    WeightedTxInfo drop = weightedTxInfos[i];
    if (i > 0) {
        drop.minusEquals(weightedTxInfos[i - 1]);
    }
    if (rebuildList) {
        while (++i < weightedTxInfos.size()) {
            WeightedTxInfo nextTx = weightedTxInfos[i];
            nextTx.minusEquals(drop);
            weightedTxInfos[i - 1] = nextTx;
        }
        weightedTxInfos.pop_back();
    }
    LogPrint("mempool", "Evicting transaction (txid=%s, cost=%d, penaltyCost=%d)\n", drop.txId.ToString(), drop.cost, drop.lowFeePenaltyCost);
    return drop;
}

// These are also defined in rpcwallet.cpp
#define JOINSPLIT_SIZE GetSerializeSize(JSDescription(), SER_NETWORK, PROTOCOL_VERSION)
#define OUTPUTDESCRIPTION_SIZE GetSerializeSize(OutputDescription(), SER_NETWORK, PROTOCOL_VERSION)
#define SPENDDESCRIPTION_SIZE GetSerializeSize(SpendDescription(), SER_NETWORK, PROTOCOL_VERSION)

WeightedTxInfo WeightedTxInfo::from(const CTransaction& tx, const CAmount& fee)
{
    size_t memUsage = RecursiveDynamicUsage(tx);
    memUsage += tx.vJoinSplit.size() * JOINSPLIT_SIZE;
    memUsage += tx.vShieldedOutput.size() * OUTPUTDESCRIPTION_SIZE;
    memUsage += tx.vShieldedSpend.size() * SPENDDESCRIPTION_SIZE;
    int64_t cost = std::max(memUsage, MIN_TX_COST);
    int64_t lowFeePenaltyCost = cost;
    if (fee < DEFAULT_FEE) {
        lowFeePenaltyCost += LOW_FEE_PENALTY;
    }
    return WeightedTxInfo(tx.GetHash(), cost, lowFeePenaltyCost);
}

void WeightedTxInfo::plusEquals(const WeightedTxInfo& other)
{
    cost += other.cost;
    lowFeePenaltyCost += other.lowFeePenaltyCost;
}

void WeightedTxInfo::minusEquals(const WeightedTxInfo& other)
{
    cost -= other.cost;
    lowFeePenaltyCost -= other.lowFeePenaltyCost;
}
