// Copyright (c) 2018 The Komodo Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KOMODO_REPLACEMENTCACHE_H
#define KOMODO_REPLACEMENTCACHE_H

#include "primitives/transaction.h"


// My kingdom for a proper sum type...
enum CTxReplacementPoolResult {
    RP_Accept,
    RP_HaveBetter,
    RP_InvalidZeroPriority,
    RP_InvalidStructure,
    RP_NoReplace
};


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

    int GetTargetBlock() { return startBlock + replacementWindow; }
};

/**
 * CTxReplacementPool stores transactions that are valid but held for
 * period of time during which they may be replaced.
 *
 * Transactions are added when they are found to be valid but not added
 * to the mempool until a timeout.
 *
 * Replacement pool is like another mempool before the main mempool.
 *
 * Transactions in the replacement pool are indexed by the output
 * that they are spending. Once a replaceable transaction tries to
 * spend an output, a countdown of blocks begins at the current block
 * plus a window that is set by "userland" code. If another, better
 * transaction replaces the spend that's already pending, the countdown
 * start block remains the same.
 */
class CTxReplacementPool
{
private:
    /* Index of spends that may be replaced */
    std::map<COutPoint, CTxReplacementPoolItem> replaceMap;
public:
    /* Try to replace a transaction in the index */
    CTxReplacementPoolResult replace(CTxReplacementPoolItem &item);

    /* Remove and return all transactions up to a given block height */
    void removePending(int height, std::vector<CTransaction> &txs);

    /* Find a transaction in the index by it's hash. */
    bool lookup(uint256 txHash, CTransaction &tx);
};


/* Global instance */
extern CTxReplacementPool replacementPool;

#endif // KOMODO_REPLACEMENTCACHE_H
