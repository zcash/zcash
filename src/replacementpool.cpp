
#include "main.h"
#include "replacementpool.h"


CTxReplacementPool replacementPool;


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
    AssertLockHeld(cs_main);

    // Perform some validations.
    if (item.tx.vin.size() > 1) {
        // Replaceable transactions with multiple inputs are disabled.
        // It seems like quite alot of additional complexity.
        return RP_Invalid;
    }

    // A transaction with 0 priority is not valid.
    if (item.priority == 0) {
        return RP_Invalid;
    }

    auto it = replaceMap.find(item.tx.vin[0].prevout);

    if (it != replaceMap.end()) {
        if (it->second.priority >= item.priority) {
            return RP_HaveBetter; // (ThanksThough)
        }
    }

    // This transaction has higher priority
    replaceMap[item.tx.vin[0].prevout] = item;
    replaceMap[item.tx.vin[0].prevout].startBlock = it->second.startBlock;

    return RP_Accepted;
}


/*
 * Remove and return any spends that have matured
 */
void CTxReplacementPool::removePending(int height, std::vector<CTransaction> &txs)
{
    AssertLockHeld(cs_main);

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
