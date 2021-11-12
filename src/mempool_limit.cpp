// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "mempool_limit.h"

#include "core_memusage.h"
#include "logging.h"
#include "random.h"
#include "serialize.h"
#include "timedata.h"
#include "utiltime.h"
#include "version.h"

const TxWeight ZERO_WEIGHT = TxWeight(0, 0);

void RecentlyEvictedList::pruneList()
{
    if (txIdSet.empty()) {
        return;
    }
    int64_t now = GetTime();
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
    txIdsAndTimes.push_back(std::make_pair(txId, GetTime()));
    txIdSet.insert(txId);
}

bool RecentlyEvictedList::contains(const uint256& txId)
{
    pruneList();
    return txIdSet.count(txId) > 0;
}


TxWeight WeightedTxTree::getWeightAt(size_t index) const
{
    return index < size ? txIdAndWeights[index].txWeight.add(childWeights[index]) : ZERO_WEIGHT;
}

void WeightedTxTree::backPropagate(size_t fromIndex, const TxWeight& weightDelta)
{
    while (fromIndex > 0) {
        fromIndex = (fromIndex - 1) / 2;
        childWeights[fromIndex] = childWeights[fromIndex].add(weightDelta);
    }
}

size_t WeightedTxTree::findByEvictionWeight(size_t fromIndex, int64_t weightToFind) const
{
    int leftWeight = getWeightAt(fromIndex * 2 + 1).evictionWeight;
    int rightWeight = getWeightAt(fromIndex).evictionWeight - getWeightAt(fromIndex * 2 + 2).evictionWeight;
    // On Left
    if (weightToFind < leftWeight) {
        return findByEvictionWeight(fromIndex * 2 + 1, weightToFind);
    }
    // Found
    if (weightToFind < rightWeight) {
        return fromIndex;
    }
    // On Right
    return findByEvictionWeight(fromIndex * 2 + 2, weightToFind - rightWeight);
}

TxWeight WeightedTxTree::getTotalWeight() const
{
    return getWeightAt(0);
}


void WeightedTxTree::add(const WeightedTxInfo& weightedTxInfo)
{
    if (txIdToIndexMap.count(weightedTxInfo.txId) > 0) {
        // This should not happen, but should be prevented nonetheless
        return;
    }
    txIdAndWeights.push_back(weightedTxInfo);
    childWeights.push_back(ZERO_WEIGHT);
    txIdToIndexMap[weightedTxInfo.txId] = size;
    backPropagate(size, weightedTxInfo.txWeight);
    size += 1;
}

void WeightedTxTree::remove(const uint256& txId)
{
    if (txIdToIndexMap.find(txId) == txIdToIndexMap.end()) {
        // Remove may be called multiple times for a given tx, so this is necessary
        return;
    }

    size_t removeIndex = txIdToIndexMap[txId];

    // We reduce the size at the start of this method to avoid saying size - 1
    // when referring to the last element of the array below
    size -= 1;

    TxWeight lastChildWeight = txIdAndWeights[size].txWeight;
    backPropagate(size, lastChildWeight.negate());

    if (removeIndex < size) {
        TxWeight weightDelta = lastChildWeight.add(txIdAndWeights[removeIndex].txWeight.negate());
        txIdAndWeights[removeIndex] = txIdAndWeights[size];
        txIdToIndexMap[txIdAndWeights[removeIndex].txId] = removeIndex;
        backPropagate(removeIndex, weightDelta);
    }

    txIdToIndexMap.erase(txId);
    txIdAndWeights.pop_back();
    childWeights.pop_back();
}

std::optional<uint256> WeightedTxTree::maybeDropRandom()
{
    TxWeight totalTxWeight = getTotalWeight();
    if (totalTxWeight.cost <= capacity) {
        return std::nullopt;
    }
    LogPrint("mempool", "Mempool cost limit exceeded (cost=%d, limit=%d)\n", totalTxWeight.cost, capacity);
    int randomWeight = GetRand(totalTxWeight.evictionWeight);
    WeightedTxInfo drop = txIdAndWeights[findByEvictionWeight(0, randomWeight)];
    LogPrint("mempool", "Evicting transaction (txid=%s, cost=%d, evictionWeight=%d)\n",
        drop.txId.ToString(), drop.txWeight.cost, drop.txWeight.evictionWeight);
    remove(drop.txId);
    return drop.txId;
}


TxWeight TxWeight::add(const TxWeight& other) const
{
    return TxWeight(cost + other.cost, evictionWeight + other.evictionWeight);
}

TxWeight TxWeight::negate() const
{
    return TxWeight(-cost, -evictionWeight);
}


WeightedTxInfo WeightedTxInfo::from(const CTransaction& tx, const CAmount& fee)
{
    size_t memUsage = RecursiveDynamicUsage(tx);
    int64_t cost = std::max((int64_t) memUsage, (int64_t) MIN_TX_COST);
    int64_t evictionWeight = cost;
    if (fee < DEFAULT_FEE) {
        evictionWeight += LOW_FEE_PENALTY;
    }
    return WeightedTxInfo(tx.GetHash(), TxWeight(cost, evictionWeight));
}
