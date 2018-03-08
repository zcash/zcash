// Copyright (c) 2018 The Komodo Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KOMODO_REPLACEMENTCACHE_H
#define KOMODO_REPLACEMENTCACHE_H

#include "coins.h"
#include "primitives/transaction.h"


enum CTxReplacementPoolResult { RP_NotReplace, RP_Valid, RP_Invalid };

class CTxReplacementPoolItem
{
public:
    CTransaction tx;
    int startBlock;
    uint64_t priority;
    uint32_t replacementWindow;

    CTxReplacementPoolItem() {}

    CTxReplacementPoolItem(const CTransaction &_tx, int _startBlock) {
        tx = _tx;
        startBlock = _startBlock;
        priority = 0;
        replacementWindow = 0;
    }

    int GetTargetBlock()
    {
        return startBlock + replacementWindow;
    }
};

/**
 * CTxReplacementPool stores valid-according-to-the-current-best-chain (??? do we need to do this?)
 * transactions that are valid but held for period of time during which
 * they may be replaced.
 *
 * Transactions are added when they are found to be valid but not added
 * to the mempool until a timeout.
 *
 * Replacement pool is like another mempool before the main mempool.
 */
class CTxReplacementPool
{
private:
    // A potential replacement is first stored here, not in the replaceMap.
    // This is in case some other checks fail, during AcceptToMemoryPool.
    // Later on, if all checks pass, processReplacement() is called.
    
    /* Index of spends that may be replaced */
    std::map<COutPoint, CTxReplacementPoolItem> replaceMap;
public:
    CTxReplacementPoolResult replace(CTxReplacementPoolItem &item);

    // Remove and return all transactions up to a given block height.
    void removePending(int height, std::vector<CTransaction> &txs);

    bool lookup(uint256 txHash, CTransaction &tx);
};


extern CTxReplacementPool replacementPool;

#endif // KOMODO_REPLACEMENTCACHE_H
