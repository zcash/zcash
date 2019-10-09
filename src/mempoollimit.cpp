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
const TxWeight ZERO_WEIGHT = TxWeight(0, 0);

void RecentlyEvictedList::pruneList()
{
    if (txIdSet.empty()) {
        return;
    }
    int64_t now = GetAdjustedTime();
    size_t startIndex = (txIdsAndTimesIndex + capacity - txIdSet.size()) % capacity;
    boost::optional<std::pair<uint256, int64_t>> txIdAndTime;
    while ((txIdAndTime = txIdsAndTimes[startIndex]).is_initialized() && (now - txIdAndTime.get().second) > timeToKeep) {
        txIdsAndTimes[startIndex] = boost::none;
        txIdSet.erase(txIdAndTime.get().first);
        startIndex = (startIndex + 1) % capacity;
    }
}

void RecentlyEvictedList::add(const uint256& txId)
{
    pruneList();
    if (txIdsAndTimes[txIdsAndTimesIndex].is_initialized()) {
        auto txIdAndTime = txIdsAndTimes[txIdsAndTimesIndex];
        txIdSet.erase(txIdAndTime.get().first);
    }
    txIdsAndTimes[txIdsAndTimesIndex] = std::make_pair(txId, GetAdjustedTime());
    txIdSet.insert(txId);
    txIdsAndTimesIndex = (txIdsAndTimesIndex + 1) % capacity;
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

size_t WeightedTxTree::findByWeight(size_t fromIndex, uint64_t weightToFind) const
{
    int leftWeight = getWeightAt(fromIndex * 2 + 1).lowFeePenaltyWeight;
    int rightWeight = getWeightAt(fromIndex).lowFeePenaltyWeight - getWeightAt(fromIndex * 2 + 2).lowFeePenaltyWeight;
    // On Left
    if (weightToFind < leftWeight) {
        return findByWeight(fromIndex * 2 + 1, weightToFind);
    }
    // Found
    if (weightToFind < rightWeight) {
        return fromIndex;
    }
    // On Right
    return findByWeight(fromIndex * 2 + 2, weightToFind - rightWeight);
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
    // when refering to the last element of the array below
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

boost::optional<uint256> WeightedTxTree::maybeDropRandom()
{
    uint64_t totalPenaltyWeight = getTotalWeight().lowFeePenaltyWeight;
    if (totalPenaltyWeight <= capacity) {
        return boost::none;
    }
    LogPrint("mempool", "Mempool cost limit exceeded (cost=%d, limit=%d)\n", totalPenaltyWeight, capacity);
    int randomWeight = GetRand(totalPenaltyWeight);
    WeightedTxInfo drop = txIdAndWeights[findByWeight(0, randomWeight)];
    LogPrint("mempool", "Evicting transaction (txid=%s, cost=%d, penaltyCost=%d)\n",
        drop.txId.ToString(), drop.txWeight.weight, drop.txWeight.lowFeePenaltyWeight);
    remove(drop.txId);
    return drop.txId;
}


TxWeight TxWeight::add(const TxWeight& other) const
{
    return TxWeight(weight + other.weight, lowFeePenaltyWeight + other.lowFeePenaltyWeight);
}

TxWeight TxWeight::negate() const
{
    return TxWeight(-weight, -lowFeePenaltyWeight);
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
    uint64_t cost = std::max(memUsage, MIN_TX_WEIGHT);
    uint64_t lowFeePenaltyCost = cost;
    if (fee < DEFAULT_FEE) {
        lowFeePenaltyCost += LOW_FEE_PENALTY;
    }
    return WeightedTxInfo(tx.GetHash(), TxWeight(cost, lowFeePenaltyCost));
}
