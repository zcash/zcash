
#include <map>
#include <string>
#include <iterator>

#include "main.h"
#include "coins.h"
#include "replacementpool.h"


CTxReplacementPool replacementPool;


/*
 * Add a transaction to the pool, with a priority.
 * Return true if valid, false if not valid. */
bool ValidateReplacementPoolItem(CTxReplacementPoolItem item)
{
    // Perform some validations.
    if (item.tx.vin.size() > 1) {
        // Replaceable transactions with multiple inputs are disabled for now. It's not yet clear
        // what edge cases may arise. It is speculated that it will "just work", since if
        // replaceable transactions spend multiple outputs using the replacement protocol,
        // they will never conflict in the replaceMap data structure. But for now, to be prudent, disable.
        return false;
    }

    // A transaction with 0 priority is not valid.
    if (item.priority == 0) {
        return false;
    }

    return true;
}


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
    if (!ValidateReplacementPoolItem(item)) {
        return RP_Invalid;
    }

    auto it = replaceMap.find(item.tx.vin[0].prevout);

    if (it != replaceMap.end()) {
        if (it->second.priority >= item.priority) {
            // Already have a transaction with equal or greater priority; this is not valid
            return RP_Invalid;
        }
        // copy the previous starting block over
        item.startBlock = it->second.startBlock;
    }

    // This transaction has higher priority
    replaceMap[item.tx.vin[0].prevout] = item;

    return RP_Valid;
}


void CTxReplacementPool::removePending(int height, std::vector<CTransaction> &txs)
{
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
    for (auto it = replaceMap.begin(); it != replaceMap.end(); it++) {
        if (it->second.tx.GetHash() == txHash) {
            tx = it->second.tx;
            return true;
        }
    }
    return false;
}
