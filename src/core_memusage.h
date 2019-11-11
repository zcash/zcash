// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_CORE_MEMUSAGE_H
#define BITCOIN_CORE_MEMUSAGE_H

#include "primitives/transaction.h"
#include "primitives/block.h"
#include "memusage.h"

static inline size_t RecursiveDynamicUsage(const CScript& script) {
    return memusage::DynamicUsage(*static_cast<const CScriptBase*>(&script));
}

static inline size_t RecursiveDynamicUsage(const COutPoint& out) {
    return 0;
}

static inline size_t RecursiveDynamicUsage(const CTxIn& in) {
    return RecursiveDynamicUsage(in.scriptSig) + RecursiveDynamicUsage(in.prevout);
}

static inline size_t RecursiveDynamicUsage(const CTxOut& out) {
    return RecursiveDynamicUsage(out.scriptPubKey);
}

// These constants are defined in the protocol § 7.1:
// https://zips.z.cash/protocol/protocol.pdf#txnencoding
#define OUTPUTDESCRIPTION_SIZE 948
#define SPENDDESCRIPTION_SIZE 384
static inline size_t JOINSPLIT_SIZE(int transactionVersion) {
    return transactionVersion >= SAPLING_TX_VERSION ? 1698 : 1802;
}

static inline size_t RecursiveDynamicUsage(const CTransaction& tx) {
    size_t mem = memusage::DynamicUsage(tx.vin) + memusage::DynamicUsage(tx.vout);
    for (std::vector<CTxIn>::const_iterator it = tx.vin.begin(); it != tx.vin.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    for (std::vector<CTxOut>::const_iterator it = tx.vout.begin(); it != tx.vout.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    mem += tx.vJoinSplit.size() * JOINSPLIT_SIZE(tx.nVersion);
    mem += tx.vShieldedOutput.size() * OUTPUTDESCRIPTION_SIZE;
    mem += tx.vShieldedSpend.size() * SPENDDESCRIPTION_SIZE;
    return mem;
}

static inline size_t RecursiveDynamicUsage(const CMutableTransaction& tx) {
    size_t mem = memusage::DynamicUsage(tx.vin) + memusage::DynamicUsage(tx.vout);
    for (std::vector<CTxIn>::const_iterator it = tx.vin.begin(); it != tx.vin.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    for (std::vector<CTxOut>::const_iterator it = tx.vout.begin(); it != tx.vout.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    mem += tx.vJoinSplit.size() * JOINSPLIT_SIZE(tx.nVersion);
    mem += tx.vShieldedOutput.size() * OUTPUTDESCRIPTION_SIZE;
    mem += tx.vShieldedSpend.size() * SPENDDESCRIPTION_SIZE;
    return mem;
}

static inline size_t RecursiveDynamicUsage(const CBlock& block) {
    size_t mem = memusage::DynamicUsage(block.vtx) + memusage::DynamicUsage(block.vMerkleTree);
    for (std::vector<CTransaction>::const_iterator it = block.vtx.begin(); it != block.vtx.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    return mem;
}

static inline size_t RecursiveDynamicUsage(const CBlockLocator& locator) {
    return memusage::DynamicUsage(locator.vHave);
}

#endif // BITCOIN_CORE_MEMUSAGE_H
