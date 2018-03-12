
#include "main.h"
#include "replacementpool.h"
#include "sync.h"


CTxReplacementPool replacementPool;
CCriticalSection cs_replacementPool;


/*
 * Validate the item
 * 
 * Compare the item with any current replacement candidate
 *
 * Ensure that the item is not passed the replacement window
 *
 * Insert the item into the map
 */
CTxReplacementPoolResult CTxReplacementPool::replace(CTxReplacementPoolItem &item)
{
    LOCK(cs_replacementPool);

    // Replaceable transactions with multiple inputs are disabled
    // until someone figures out how they would work.
    if (item.tx.vin.size() > 1) return RP_Invalid;

    // replacementWindow of 0 goes direct to mempool
    if (item.replacementWindow == 0) 
    {
        // But we also need to remove replacement candidates
        replaceMap.erase(item.tx.vin[0].prevout);
        return RP_NoReplace;
    }

    int startBlock = item.startBlock;

    auto it = replaceMap.find(item.tx.vin[0].prevout);
    if (it != replaceMap.end())
    {
        if (it->second.replacementWindow <= item.replacementWindow &&
                it->second.priority >= item.priority) {
            return RP_HaveBetter;
        }
        startBlock = it->second.startBlock;
    }

    // This transaction has higher priority
    replaceMap[item.tx.vin[0].prevout] = item;
    replaceMap[item.tx.vin[0].prevout].startBlock = startBlock;
    return RP_Accept;
}


/*
 * Remove and return any spends that have matured
 */
void CTxReplacementPool::removePending(int height, std::vector<CTransaction> &txs)
{
    LOCK(cs_replacementPool);

    for (auto it = replaceMap.begin(); it != replaceMap.end(); /**/) {
        CTxReplacementPoolItem &rep = it->second;

        if (rep.GetTargetBlock() <= height) {
            txs.push_back(rep.tx);
            replaceMap.erase(it++);
        } else {
            ++it;
        }
    }
}


/*
 * O(n) lookup of tx by hash
 */
bool CTxReplacementPool::lookup(uint256 txHash, CTransaction &tx)
{
    LOCK(cs_replacementPool);
    for (auto it = replaceMap.begin(); it != replaceMap.end(); it++) {
        if (it->second.tx.GetHash() == txHash) {
            tx = it->second.tx;
            return true;
        }
    }
    return false;
}
