// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "txmempool.h"

#include "clientversion.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "main.h"
#include "policy/fees.h"
#include "streams.h"
#include "timedata.h"
#include "util/system.h"
#include "util/moneystr.h"
#include "validationinterface.h"
#include "version.h"

#include <optional>

using namespace std;

CTxMemPoolEntry::CTxMemPoolEntry(const CTransaction& _tx, const CAmount& _nFee,
                                 int64_t _nTime, double _dPriority,
                                 unsigned int _nHeight, bool poolHasNoInputsOf,
                                 bool _spendsCoinbase, unsigned int _sigOps, uint32_t _nBranchId):
    tx(std::make_shared<CTransaction>(_tx)), nFee(_nFee), nTime(_nTime), dPriority(_dPriority), nHeight(_nHeight),
    hadNoDependencies(poolHasNoInputsOf),
    spendsCoinbase(_spendsCoinbase), sigOpCount(_sigOps), nBranchId(_nBranchId)
{
    nTxSize = ::GetSerializeSize(_tx, SER_NETWORK, PROTOCOL_VERSION);
    nModSize = _tx.CalculateModifiedSize(nTxSize);
    nUsageSize = RecursiveDynamicUsage(*tx) + memusage::DynamicUsage(tx);

    nCountWithDescendants = 1;
    nSizeWithDescendants = nTxSize;
    nFeesWithDescendants = nFee;

    feeDelta = 0;
}

CTxMemPoolEntry::CTxMemPoolEntry(const CTxMemPoolEntry& other)
{
    *this = other;
}

double
CTxMemPoolEntry::GetPriority(unsigned int currentHeight) const
{
    CAmount nValueIn = tx->GetValueOut()+nFee;
    double deltaPriority = ((double)(currentHeight-nHeight)*nValueIn)/nModSize;
    double dResult = dPriority + deltaPriority;
    return dResult;
}

void CTxMemPoolEntry::UpdateFeeDelta(int64_t newFeeDelta)
{
    feeDelta = newFeeDelta;
}

// Update the given tx for any in-mempool descendants.
// Assumes that setMemPoolChildren is correct for the given tx and all
// descendants.
bool CTxMemPool::UpdateForDescendants(txiter updateIt, int maxDescendantsToVisit, cacheMap &cachedDescendants, const std::set<uint256> &setExclude)
{
    // Track the number of entries (outside setExclude) that we'd need to visit
    // (will bail out if it exceeds maxDescendantsToVisit)
    int nChildrenToVisit = 0;

    setEntries stageEntries, setAllDescendants;
    stageEntries = GetMemPoolChildren(updateIt);

    while (!stageEntries.empty()) {
        const txiter cit = *stageEntries.begin();
        if (cit->IsDirty()) {
            // Don't consider any more children if any descendant is dirty
            return false;
        }
        setAllDescendants.insert(cit);
        stageEntries.erase(cit);
        const setEntries &setChildren = GetMemPoolChildren(cit);
        for (const txiter childEntry : setChildren) {
            cacheMap::iterator cacheIt = cachedDescendants.find(childEntry);
            if (cacheIt != cachedDescendants.end()) {
                // We've already calculated this one, just add the entries for this set
                // but don't traverse again.
                for (const txiter cacheEntry : cacheIt->second) {
                    // update visit count only for new child transactions
                    // (outside of setExclude and stageEntries)
                    if (setAllDescendants.insert(cacheEntry).second &&
                            !setExclude.count(cacheEntry->GetTx().GetHash()) &&
                            !stageEntries.count(cacheEntry)) {
                        nChildrenToVisit++;
                    }
                }
            } else if (!setAllDescendants.count(childEntry)) {
                // Schedule for later processing and update our visit count
                if (stageEntries.insert(childEntry).second && !setExclude.count(childEntry->GetTx().GetHash())) {
                        nChildrenToVisit++;
                }
            }
            if (nChildrenToVisit > maxDescendantsToVisit) {
                return false;
            }
        }
    }
    // setAllDescendants now contains all in-mempool descendants of updateIt.
    // Update and add to cached descendant map
    int64_t modifySize = 0;
    CAmount modifyFee = 0;
    int64_t modifyCount = 0;
    for (txiter cit : setAllDescendants) {
        if (!setExclude.count(cit->GetTx().GetHash())) {
            modifySize += cit->GetTxSize();
            modifyFee += cit->GetFee();
            modifyCount++;
            cachedDescendants[updateIt].insert(cit);
        }
    }
    mapTx.modify(updateIt, update_descendant_state(modifySize, modifyFee, modifyCount));
    return true;
}

// vHashesToUpdate is the set of transaction hashes from a disconnected block
// which has been re-added to the mempool.
// for each entry, look for descendants that are outside hashesToUpdate, and
// add fee/size information for such descendants to the parent.
void CTxMemPool::UpdateTransactionsFromBlock(const std::vector<uint256> &vHashesToUpdate)
{
    LOCK(cs);
    // For each entry in vHashesToUpdate, store the set of in-mempool, but not
    // in-vHashesToUpdate transactions, so that we don't have to recalculate
    // descendants when we come across a previously seen entry.
    cacheMap mapMemPoolDescendantsToUpdate;

    // Use a set for lookups into vHashesToUpdate (these entries are already
    // accounted for in the state of their ancestors)
    std::set<uint256> setAlreadyIncluded(vHashesToUpdate.begin(), vHashesToUpdate.end());

    // Iterate in reverse, so that whenever we are looking at at a transaction
    // we are sure that all in-mempool descendants have already been processed.
    // This maximizes the benefit of the descendant cache and guarantees that
    // setMemPoolChildren will be updated, an assumption made in
    // UpdateForDescendants.
    BOOST_REVERSE_FOREACH(const uint256 &hash, vHashesToUpdate) {
        // we cache the in-mempool children to avoid duplicate updates
        setEntries setChildren;
        // calculate children from mapNextTx
        txiter it = mapTx.find(hash);
        if (it == mapTx.end()) {
            continue;
        }
        std::map<COutPoint, CInPoint>::iterator iter = mapNextTx.lower_bound(COutPoint(hash, 0));
        // First calculate the children, and update setMemPoolChildren to
        // include them, and update their setMemPoolParents to include this tx.
        for (; iter != mapNextTx.end() && iter->first.hash == hash; ++iter) {
            const uint256 &childHash = iter->second.ptx->GetHash();
            txiter childIter = mapTx.find(childHash);
            assert(childIter != mapTx.end());
            // We can skip updating entries we've encountered before or that
            // are in the block (which are already accounted for).
            if (setChildren.insert(childIter).second && !setAlreadyIncluded.count(childHash)) {
                UpdateChild(it, childIter, true);
                UpdateParent(childIter, it, true);
            }
        }
        if (!UpdateForDescendants(it, 100, mapMemPoolDescendantsToUpdate, setAlreadyIncluded)) {
            // Mark as dirty if we can't do the calculation.
            mapTx.modify(it, set_dirty());
        }
    }
}

bool CTxMemPool::CalculateMemPoolAncestors(const CTxMemPoolEntry &entry, setEntries &setAncestors, uint64_t limitAncestorCount, uint64_t limitAncestorSize, uint64_t limitDescendantCount, uint64_t limitDescendantSize, std::string &errString, bool fSearchForParents /* = true */)
{
    setEntries parentHashes;
    const CTransaction &tx = entry.GetTx();

    if (fSearchForParents) {
        // Get parents of this transaction that are in the mempool
        // GetMemPoolParents() is only valid for entries in the mempool, so we
        // iterate mapTx to find parents.
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            txiter piter = mapTx.find(tx.vin[i].prevout.hash);
            if (piter != mapTx.end()) {
                parentHashes.insert(piter);
                if (parentHashes.size() + 1 > limitAncestorCount) {
                    errString = strprintf("too many unconfirmed parents [limit: %u]", limitAncestorCount);
                    return false;
                }
            }
        }
    } else {
        // If we're not searching for parents, we require this to be an
        // entry in the mempool already.
        txiter it = mapTx.iterator_to(entry);
        parentHashes = GetMemPoolParents(it);
    }

    size_t totalSizeWithAncestors = entry.GetTxSize();

    while (!parentHashes.empty()) {
        txiter stageit = *parentHashes.begin();

        setAncestors.insert(stageit);
        parentHashes.erase(stageit);
        totalSizeWithAncestors += stageit->GetTxSize();

        if (stageit->GetSizeWithDescendants() + entry.GetTxSize() > limitDescendantSize) {
            errString = strprintf("exceeds descendant size limit for tx %s [limit: %u]", stageit->GetTx().GetHash().ToString(), limitDescendantSize);
            return false;
        } else if (stageit->GetCountWithDescendants() + 1 > limitDescendantCount) {
            errString = strprintf("too many descendants for tx %s [limit: %u]", stageit->GetTx().GetHash().ToString(), limitDescendantCount);
            return false;
        } else if (totalSizeWithAncestors > limitAncestorSize) {
            errString = strprintf("exceeds ancestor size limit [limit: %u]", limitAncestorSize);
            return false;
        }

        const setEntries & setMemPoolParents = GetMemPoolParents(stageit);
        for (const txiter &phash : setMemPoolParents) {
            // If this is a new ancestor, add it.
            if (setAncestors.count(phash) == 0) {
                parentHashes.insert(phash);
            }
            if (parentHashes.size() + setAncestors.size() + 1 > limitAncestorCount) {
                errString = strprintf("too many unconfirmed ancestors [limit: %u]", limitAncestorCount);
                return false;
            }
        }
    }

    return true;
}

void CTxMemPool::UpdateAncestorsOf(bool add, txiter it, setEntries &setAncestors)
{
    setEntries parentIters = GetMemPoolParents(it);
    // add or remove this tx as a child of each parent
    for (txiter piter : parentIters) {
        UpdateChild(piter, it, add);
    }
    const int64_t updateCount = (add ? 1 : -1);
    const int64_t updateSize = updateCount * it->GetTxSize();
    const CAmount updateFee = updateCount * it->GetFee();
    for (txiter ancestorIt : setAncestors) {
        mapTx.modify(ancestorIt, update_descendant_state(updateSize, updateFee, updateCount));
    }
}

void CTxMemPool::UpdateChildrenForRemoval(txiter it)
{
    const setEntries &setMemPoolChildren = GetMemPoolChildren(it);
    for (txiter updateIt : setMemPoolChildren) {
        UpdateParent(updateIt, it, false);
    }
}

void CTxMemPool::UpdateForRemoveFromMempool(const setEntries &entriesToRemove)
{
    // For each entry, walk back all ancestors and decrement size associated with this
    // transaction
    const uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
    for (txiter removeIt : entriesToRemove) {
        setEntries setAncestors;
        const CTxMemPoolEntry &entry = *removeIt;
        std::string dummy;
        // Since this is a tx that is already in the mempool, we can call CMPA
        // with fSearchForParents = false.  If the mempool is in a consistent
        // state, then using true or false should both be correct, though false
        // should be a bit faster.
        // However, if we happen to be in the middle of processing a reorg, then
        // the mempool can be in an inconsistent state.  In this case, the set
        // of ancestors reachable via mapLinks will be the same as the set of 
        // ancestors whose packages include this transaction, because when we
        // add a new transaction to the mempool in addUnchecked(), we assume it
        // has no children, and in the case of a reorg where that assumption is
        // false, the in-mempool children aren't linked to the in-block tx's
        // until UpdateTransactionsFromBlock() is called.
        // So if we're being called during a reorg, ie before
        // UpdateTransactionsFromBlock() has been called, then mapLinks[] will
        // differ from the set of mempool parents we'd calculate by searching,
        // and it's important that we use the mapLinks[] notion of ancestor
        // transactions as the set of things to update for removal.
        CalculateMemPoolAncestors(entry, setAncestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);
        // Note that UpdateAncestorsOf severs the child links that point to
        // removeIt in the entries for the parents of removeIt.  This is
        // fine since we don't need to use the mempool children of any entries
        // to walk back over our ancestors (but we do need the mempool
        // parents!)
        UpdateAncestorsOf(false, removeIt, setAncestors);
    }
    // After updating all the ancestor sizes, we can now sever the link between each
    // transaction being removed and any mempool children (ie, update setMemPoolParents
    // for each direct child of a transaction being removed).
    for (txiter removeIt : entriesToRemove) {
        UpdateChildrenForRemoval(removeIt);
    }
}

void CTxMemPoolEntry::SetDirty()
{
    nCountWithDescendants = 0;
    nSizeWithDescendants = nTxSize;
    nFeesWithDescendants = nFee;
}

void CTxMemPoolEntry::UpdateState(int64_t modifySize, CAmount modifyFee, int64_t modifyCount)
{
    if (!IsDirty()) {
        nSizeWithDescendants += modifySize;
        assert(int64_t(nSizeWithDescendants) > 0);
        nFeesWithDescendants += modifyFee;
        assert(nFeesWithDescendants >= 0);
        nCountWithDescendants += modifyCount;
        assert(int64_t(nCountWithDescendants) > 0);
    }
}

CTxMemPool::CTxMemPool(const CFeeRate& _minReasonableRelayFee) :
    nTransactionsUpdated(0)
{
    _clear(); // unlocked clear

    // Sanity checks off by default for performance, because otherwise
    // accepting transactions becomes O(N^2) where N is the number
    // of transactions in the pool
    nCheckFrequency = 0;

    minerPolicyEstimator = new CBlockPolicyEstimator(_minReasonableRelayFee);
    minReasonableRelayFee = _minReasonableRelayFee;
}

CTxMemPool::~CTxMemPool()
{
    delete minerPolicyEstimator;
    delete recentlyEvicted;
    delete weightedTxTree;
}

void CTxMemPool::pruneSpent(const uint256 &hashTx, CCoins &coins)
{
    LOCK(cs);

    std::map<COutPoint, CInPoint>::iterator it = mapNextTx.lower_bound(COutPoint(hashTx, 0));

    // iterate over all COutPoints in mapNextTx whose hash equals the provided hashTx
    while (it != mapNextTx.end() && it->first.hash == hashTx) {
        coins.Spend(it->first.n); // and remove those outputs from coins
        it++;
    }
}

unsigned int CTxMemPool::GetTransactionsUpdated() const
{
    LOCK(cs);
    return nTransactionsUpdated;
}

void CTxMemPool::AddTransactionsUpdated(unsigned int n)
{
    LOCK(cs);
    nTransactionsUpdated += n;
}

bool CTxMemPool::addUnchecked(const uint256& hash, const CTxMemPoolEntry &entry, setEntries &setAncestors, bool fCurrentEstimate)
{
    // Add to memory pool without checking anything.
    // Used by main.cpp AcceptToMemoryPool(), which DOES do
    // all the appropriate checks.
    LOCK(cs);
    weightedTxTree->add(WeightedTxInfo::from(entry.GetTx(), entry.GetFee()));
    indexed_transaction_set::iterator newit = mapTx.insert(entry).first;
    mapLinks.insert(make_pair(newit, TxLinks()));

    // Update cachedInnerUsage to include contained transaction's usage.
    // (When we update the entry for in-mempool parents, memory usage will be
    // further updated.)
    cachedInnerUsage += entry.DynamicMemoryUsage();

    const CTransaction& tx = newit->GetTx();
    mapRecentlyAddedTx[tx.GetHash()] = &tx;
    nRecentlyAddedSequence += 1;
    std::set<uint256> setParentTransactions;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        mapNextTx[tx.vin[i].prevout] = CInPoint(&tx, i);
        setParentTransactions.insert(tx.vin[i].prevout.hash);
    }
    // Don't bother worrying about child transactions of this one.
    // Normal case of a new transaction arriving is that there can't be any
    // children, because such children would be orphans.
    // An exception to that is if a transaction enters that used to be in a block.
    // In that case, our disconnect block logic will call UpdateTransactionsFromBlock
    // to clean up the mess we're leaving here.

    // Update ancestors with information about this tx
    for (const uint256 &phash : setParentTransactions) {
        txiter pit = mapTx.find(phash);
        if (pit != mapTx.end()) {
            UpdateParent(newit, pit, true);
        }
    }
    UpdateAncestorsOf(true, newit, setAncestors);

    for (const JSDescription &joinsplit : tx.vJoinSplit) {
        for (const uint256 &nf : joinsplit.nullifiers) {
            mapSproutNullifiers[nf] = &tx;
        }
    }
    for (const SpendDescription &spendDescription : tx.vShieldedSpend) {
        mapSaplingNullifiers[spendDescription.nullifier] = &tx;
    }
    for (const uint256 &orchardNullifier : tx.GetOrchardBundle().GetNullifiers()) {
        mapOrchardNullifiers[orchardNullifier] = &tx;
    }

    // Update transaction's score for any feeDelta created by PrioritiseTransaction
    std::map<uint256, std::pair<double, CAmount> >::const_iterator pos = mapDeltas.find(hash);
    if (pos != mapDeltas.end()) {
        const std::pair<double, CAmount> &deltas = pos->second;
        if (deltas.second) {
            mapTx.modify(newit, update_fee_delta(deltas.second));
        }
    }

    nTransactionsUpdated++;
    totalTxSize += entry.GetTxSize();
    minerPolicyEstimator->processTransaction(entry, fCurrentEstimate);

    return true;
}

// START insightexplorer
void CTxMemPool::addAddressIndex(const CTxMemPoolEntry &entry, const CCoinsViewCache &view)
{
    LOCK(cs);
    const CTransaction& tx = entry.GetTx();
    std::vector<CMempoolAddressDeltaKey> inserted;

    uint256 txhash = tx.GetHash();
    for (unsigned int j = 0; j < tx.vin.size(); j++) {
        const CTxIn input = tx.vin[j];
        const CTxOut &prevout = view.GetOutputFor(input);
        CScript::ScriptType type = prevout.scriptPubKey.GetType();
        if (type == CScript::UNKNOWN)
            continue;
        CMempoolAddressDeltaKey key(type, prevout.scriptPubKey.AddressHash(), txhash, j, 1);
        CMempoolAddressDelta delta(entry.GetTime(), prevout.nValue * -1, input.prevout.hash, input.prevout.n);
        mapAddress.insert(make_pair(key, delta));
        inserted.push_back(key);
    }

    for (unsigned int j = 0; j < tx.vout.size(); j++) {
        const CTxOut &out = tx.vout[j];
        CScript::ScriptType type = out.scriptPubKey.GetType();
        if (type == CScript::UNKNOWN)
            continue;
        CMempoolAddressDeltaKey key(type, out.scriptPubKey.AddressHash(), txhash, j, 0);
        mapAddress.insert(make_pair(key, CMempoolAddressDelta(entry.GetTime(), out.nValue)));
        inserted.push_back(key);
    }

    mapAddressInserted.insert(make_pair(txhash, inserted));
}

void CTxMemPool::getAddressIndex(
    const std::vector<std::pair<uint160, int>>& addresses,
    std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta>>& results)
{
    LOCK(cs);
    for (const auto& it : addresses) {
        auto ait = mapAddress.lower_bound(CMempoolAddressDeltaKey(it.second, it.first));
        while (ait != mapAddress.end() && (*ait).first.addressBytes == it.first && (*ait).first.type == it.second) {
            results.push_back(*ait);
            ait++;
        }
    }
}

void CTxMemPool::removeAddressIndex(const uint256& txhash)
{
    LOCK(cs);
    auto it = mapAddressInserted.find(txhash);

    if (it != mapAddressInserted.end()) {
        std::vector<CMempoolAddressDeltaKey> keys = it->second;
        for (const auto& mit : keys) {
            mapAddress.erase(mit);
        }
        mapAddressInserted.erase(it);
    }
}

void CTxMemPool::addSpentIndex(const CTxMemPoolEntry &entry, const CCoinsViewCache &view)
{
    LOCK(cs);
    const CTransaction& tx = entry.GetTx();
    uint256 txhash = tx.GetHash();
    std::vector<CSpentIndexKey> inserted;

    for (unsigned int j = 0; j < tx.vin.size(); j++) {
        const CTxIn input = tx.vin[j];
        const CTxOut &prevout = view.GetOutputFor(input);
        CSpentIndexKey key = CSpentIndexKey(input.prevout.hash, input.prevout.n);
        CSpentIndexValue value = CSpentIndexValue(txhash, j, -1, prevout.nValue,
            prevout.scriptPubKey.GetType(),
            prevout.scriptPubKey.AddressHash());
        mapSpent.insert(make_pair(key, value));
        inserted.push_back(key);
    }
    mapSpentInserted.insert(make_pair(txhash, inserted));
}

bool CTxMemPool::getSpentIndex(const CSpentIndexKey &key, CSpentIndexValue &value)
{
    LOCK(cs);
    std::map<CSpentIndexKey, CSpentIndexValue, CSpentIndexKeyCompare>::iterator it = mapSpent.find(key);
    if (it != mapSpent.end()) {
        value = it->second;
        return true;
    }
    return false;
}

void CTxMemPool::removeSpentIndex(const uint256 txhash)
{
    LOCK(cs);
    auto it = mapSpentInserted.find(txhash);

    if (it != mapSpentInserted.end()) {
        std::vector<CSpentIndexKey> keys = (*it).second;
        for (std::vector<CSpentIndexKey>::iterator mit = keys.begin(); mit != keys.end(); mit++) {
            mapSpent.erase(*mit);
        }
        mapSpentInserted.erase(it);
    }
}
// END insightexplorer

void CTxMemPool::removeUnchecked(txiter it)
{
    const uint256 hash = it->GetTx().GetHash();
    mapRecentlyAddedTx.erase(hash);
    for (const CTxIn& txin : it->GetTx().vin)
        mapNextTx.erase(txin.prevout);
    for (const JSDescription& joinsplit : it->GetTx().vJoinSplit) {
        for (const uint256& nf : joinsplit.nullifiers) {
            mapSproutNullifiers.erase(nf);
        }
    }
    for (const SpendDescription &spendDescription : it->GetTx().vShieldedSpend) {
        mapSaplingNullifiers.erase(spendDescription.nullifier);
    }
    for (const uint256 &orchardNullifier : it->GetTx().GetOrchardBundle().GetNullifiers()) {
        mapOrchardNullifiers.erase(orchardNullifier);
    }

    totalTxSize -= it->GetTxSize();
    cachedInnerUsage -= it->DynamicMemoryUsage();
    cachedInnerUsage -= memusage::DynamicUsage(mapLinks[it].parents) + memusage::DynamicUsage(mapLinks[it].children);
    mapLinks.erase(it);
    mapTx.erase(it);
    nTransactionsUpdated++;
    minerPolicyEstimator->removeTx(hash);

    // insightexplorer
    if (fAddressIndex)
        removeAddressIndex(hash);
    if (fSpentIndex)
        removeSpentIndex(hash);
}

// Calculates descendants of entry that are not already in setDescendants, and adds to
// setDescendants. Assumes entryit is already a tx in the mempool and setMemPoolChildren
// is correct for tx and all descendants.
// Also assumes that if an entry is in setDescendants already, then all
// in-mempool descendants of it are already in setDescendants as well, so that we
// can save time by not iterating over those entries.
void CTxMemPool::CalculateDescendants(txiter entryit, setEntries &setDescendants)
{
    setEntries stage;
    if (setDescendants.count(entryit) == 0) {
        stage.insert(entryit);
    }
    // Traverse down the children of entry, only adding children that are not
    // accounted for in setDescendants already (because those children have either
    // already been walked, or will be walked in this iteration).
    while (!stage.empty()) {
        txiter it = *stage.begin();
        setDescendants.insert(it);
        stage.erase(it);

        const setEntries &setChildren = GetMemPoolChildren(it);
        for (const txiter &childiter : setChildren) {
            if (!setDescendants.count(childiter)) {
                stage.insert(childiter);
            }
        }
    }
}

void CTxMemPool::remove(const CTransaction &origTx, std::list<CTransaction>& removed, bool fRecursive)
{
    // Remove transaction from memory pool
    {
        LOCK(cs);
        setEntries txToRemove;
        txiter origit = mapTx.find(origTx.GetHash());
        if (origit != mapTx.end()) {
            txToRemove.insert(origit);
        } else if (fRecursive) {
            // If recursively removing but origTx isn't in the mempool
            // be sure to remove any children that are in the pool. This can
            // happen during chain re-orgs if origTx isn't re-accepted into
            // the mempool for any reason.
            for (unsigned int i = 0; i < origTx.vout.size(); i++) {
                std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(COutPoint(origTx.GetHash(), i));
                if (it == mapNextTx.end())
                    continue;
                txiter nextit = mapTx.find(it->second.ptx->GetHash());
                assert(nextit != mapTx.end());
                txToRemove.insert(nextit);
            }
        }
        setEntries setAllRemoves;
        if (fRecursive) {
            for (txiter it : txToRemove) {
                CalculateDescendants(it, setAllRemoves);
            }
        } else {
            setAllRemoves.swap(txToRemove);
        }
        for (txiter it : setAllRemoves) {
            removed.push_back(it->GetTx());
        }
        RemoveStaged(setAllRemoves);
        for (CTransaction tx : removed) {
            weightedTxTree->remove(tx.GetHash());
        }
    }
}

void CTxMemPool::removeForReorg(const CCoinsViewCache *pcoins, unsigned int nMemPoolHeight, int flags)
{
    // Remove transactions spending a coinbase which are now immature and no-longer-final transactions
    LOCK(cs);
    list<CTransaction> transactionsToRemove;
    for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++) {
        const CTransaction& tx = it->GetTx();
        if (!CheckFinalTx(tx, flags)) {
            transactionsToRemove.push_back(tx);
        } else if (it->GetSpendsCoinbase()) {
            for (const CTxIn& txin : tx.vin) {
                indexed_transaction_set::const_iterator it2 = mapTx.find(txin.prevout.hash);
                if (it2 != mapTx.end())
                    continue;
                const CCoins *coins = pcoins->AccessCoins(txin.prevout.hash);
		if (nCheckFrequency != 0) assert(coins);
                if (!coins || (coins->IsCoinBase() && ((signed long)nMemPoolHeight) - coins->nHeight < COINBASE_MATURITY)) {
                    transactionsToRemove.push_back(tx);
                    break;
                }
            }
        }
    }
    for (const CTransaction& tx : transactionsToRemove) {
        list<CTransaction> removed;
        remove(tx, removed, true);
    }
}


void CTxMemPool::removeWithAnchor(const uint256 &invalidRoot, ShieldedType type)
{
    // If a block is disconnected from the tip, and the root changed,
    // we must invalidate transactions from the mempool which spend
    // from that root -- almost as though they were spending coinbases
    // which are no longer valid to spend due to coinbase maturity.
    LOCK(cs);
    list<CTransaction> transactionsToRemove;

    for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++) {
        const CTransaction& tx = it->GetTx();
        switch (type) {
            case SPROUT:
                for (const JSDescription& joinsplit : tx.vJoinSplit) {
                    if (joinsplit.anchor == invalidRoot) {
                        transactionsToRemove.push_back(tx);
                        break;
                    }
                }
            break;
            case SAPLING:
                for (const SpendDescription& spendDescription : tx.vShieldedSpend) {
                    if (spendDescription.anchor == invalidRoot) {
                        transactionsToRemove.push_back(tx);
                        break;
                    }
                }
            break;
            case ORCHARD:
            {
                auto anchor = tx.GetOrchardBundle().GetAnchor();
                if (anchor == invalidRoot) {
                    transactionsToRemove.push_back(tx);
                }
                break;
            }
            default:
                throw runtime_error("Unknown shielded type");
            break;
        }
    }

    for (const CTransaction& tx : transactionsToRemove) {
        list<CTransaction> removed;
        remove(tx, removed, true);
    }
}

void CTxMemPool::removeConflicts(const CTransaction &tx, std::list<CTransaction>& removed)
{
    // Remove transactions which depend on inputs of tx, recursively
    list<CTransaction> result;
    LOCK(cs);
    for (const CTxIn &txin : tx.vin) {
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction &txConflict = *it->second.ptx;
            if (txConflict != tx)
            {
                remove(txConflict, removed, true);
            }
        }
    }

    for (const JSDescription &joinsplit : tx.vJoinSplit) {
        for (const uint256 &nf : joinsplit.nullifiers) {
            std::map<uint256, const CTransaction*>::iterator it = mapSproutNullifiers.find(nf);
            if (it != mapSproutNullifiers.end()) {
                const CTransaction &txConflict = *it->second;
                if (txConflict != tx) {
                    remove(txConflict, removed, true);
                }
            }
        }
    }
    for (const SpendDescription &spendDescription : tx.vShieldedSpend) {
        std::map<uint256, const CTransaction*>::iterator it = mapSaplingNullifiers.find(spendDescription.nullifier);
        if (it != mapSaplingNullifiers.end()) {
            const CTransaction &txConflict = *it->second;
            if (txConflict != tx) {
                remove(txConflict, removed, true);
            }
        }
    }
    for (const uint256 &orchardNullifier : tx.GetOrchardBundle().GetNullifiers()) {
        std::map<uint256, const CTransaction*>::iterator it = mapOrchardNullifiers.find(orchardNullifier);
        if (it != mapOrchardNullifiers.end()) {
            const CTransaction &txConflict = *it->second;
            if (txConflict != tx) {
                remove(txConflict, removed, true);
            }
        }
    }
}

std::vector<uint256> CTxMemPool::removeExpired(unsigned int nBlockHeight)
{
    // Remove expired txs from the mempool
    LOCK(cs);
    list<CTransaction> transactionsToRemove;
    for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++)
    {
        const CTransaction& tx = it->GetTx();
        if (IsExpiredTx(tx, nBlockHeight)) {
            transactionsToRemove.push_back(tx);
        }
    }
    std::vector<uint256> ids;
    for (const CTransaction& tx : transactionsToRemove) {
        list<CTransaction> removed;
        remove(tx, removed, true);
        ids.push_back(tx.GetHash());
        LogPrint("mempool", "Removing expired txid: %s\n", tx.GetHash().ToString());
    }
    return ids;
}

/**
 * Called when a block is connected. Removes from mempool and updates the miner fee estimator.
 */
void CTxMemPool::removeForBlock(const std::vector<CTransaction>& vtx, unsigned int nBlockHeight,
                                std::list<CTransaction>& conflicts, bool fCurrentEstimate)
{
    LOCK(cs);
    std::vector<CTxMemPoolEntry> entries;
    for (const CTransaction& tx : vtx)
    {
        uint256 hash = tx.GetHash();

        indexed_transaction_set::iterator i = mapTx.find(hash);
        if (i != mapTx.end())
            entries.push_back(*i);
    }
    for (const CTransaction& tx : vtx)
    {
        std::list<CTransaction> dummy;
        remove(tx, dummy, false);
        removeConflicts(tx, conflicts);
        ClearPrioritisation(tx.GetHash());
    }
    // After the txs in the new block have been removed from the mempool, update policy estimates
    minerPolicyEstimator->processBlock(nBlockHeight, entries, fCurrentEstimate);
}

/**
 * Called whenever the tip changes. Removes transactions which don't commit to
 * the given branch ID from the mempool.
 */
void CTxMemPool::removeWithoutBranchId(uint32_t nMemPoolBranchId)
{
    LOCK(cs);
    std::list<CTransaction> transactionsToRemove;

    for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++) {
        const CTransaction& tx = it->GetTx();
        if (it->GetValidatedBranchId() != nMemPoolBranchId) {
            transactionsToRemove.push_back(tx);
        }
    }

    for (const CTransaction& tx : transactionsToRemove) {
        std::list<CTransaction> removed;
        remove(tx, removed, true);
    }
}

void CTxMemPool::_clear()
{
    mapLinks.clear();
    mapTx.clear();
    mapNextTx.clear();
    totalTxSize = 0;
    cachedInnerUsage = 0;
    ++nTransactionsUpdated;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    _clear();
}

void CTxMemPool::check(const CCoinsViewCache *pcoins) const
{
    if (nCheckFrequency == 0)
        return;

    if (GetRand(std::numeric_limits<uint32_t>::max()) >= nCheckFrequency)
        return;

    LogPrint("mempool", "Checking mempool with %u transactions and %u inputs\n", (unsigned int)mapTx.size(), (unsigned int)mapNextTx.size());

    uint64_t checkTotal = 0;
    uint64_t innerUsage = 0;

    CCoinsViewCache mempoolDuplicate(const_cast<CCoinsViewCache*>(pcoins));
    const int64_t nSpendHeight = GetSpendHeight(mempoolDuplicate);

    LOCK(cs);
    std::list<const CTxMemPoolEntry*> waitingOnDependants;
    for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++) {
        unsigned int i = 0;
        checkTotal += it->GetTxSize();
        innerUsage += it->DynamicMemoryUsage();
        const CTransaction& tx = it->GetTx();
        txlinksMap::const_iterator linksiter = mapLinks.find(it);
        assert(linksiter != mapLinks.end());
        const TxLinks &links = linksiter->second;
        innerUsage += memusage::DynamicUsage(links.parents) + memusage::DynamicUsage(links.children);
        bool fDependsWait = false;
        setEntries setParentCheck;
        for (const CTxIn &txin : tx.vin) {
            // Check that every mempool transaction's inputs refer to available coins, or other mempool tx's.
            indexed_transaction_set::const_iterator it2 = mapTx.find(txin.prevout.hash);
            if (it2 != mapTx.end()) {
                const CTransaction& tx2 = it2->GetTx();
                assert(tx2.vout.size() > txin.prevout.n && !tx2.vout[txin.prevout.n].IsNull());
                fDependsWait = true;
                setParentCheck.insert(it2);
            } else {
                const CCoins* coins = pcoins->AccessCoins(txin.prevout.hash);
                assert(coins && coins->IsAvailable(txin.prevout.n));
            }
            // Check whether its inputs are marked in mapNextTx.
            std::map<COutPoint, CInPoint>::const_iterator it3 = mapNextTx.find(txin.prevout);
            assert(it3 != mapNextTx.end());
            assert(it3->second.ptx == &tx);
            assert(it3->second.n == i);
            i++;
        }
        assert(setParentCheck == GetMemPoolParents(it));
        // Check children against mapNextTx
        CTxMemPool::setEntries setChildrenCheck;
        std::map<COutPoint, CInPoint>::const_iterator iter = mapNextTx.lower_bound(COutPoint(it->GetTx().GetHash(), 0));
        int64_t childSizes = 0;
        CAmount childFees = 0;
        for (; iter != mapNextTx.end() && iter->first.hash == it->GetTx().GetHash(); ++iter) {
            txiter childit = mapTx.find(iter->second.ptx->GetHash());
            assert(childit != mapTx.end()); // mapNextTx points to in-mempool transactions
            if (setChildrenCheck.insert(childit).second) {
                childSizes += childit->GetTxSize();
                childFees += childit->GetFee();
            }
        }
        assert(setChildrenCheck == GetMemPoolChildren(it));
        // Also check to make sure size/fees is greater than sum with immediate children.
        // just a sanity check, not definitive that this calc is correct...
        // also check that the size is less than the size of the entire mempool.
        if (!it->IsDirty()) {
            assert(it->GetSizeWithDescendants() >= childSizes + it->GetTxSize());
            assert(it->GetFeesWithDescendants() >= childFees + it->GetFee());
        } else {
            assert(it->GetSizeWithDescendants() == it->GetTxSize());
            assert(it->GetFeesWithDescendants() == it->GetFee());
        }
        assert(it->GetFeesWithDescendants() >= 0);

        if (fDependsWait)
            waitingOnDependants.push_back(&(*it));
        else {
            CValidationState state;
            bool fCheckResult = tx.IsCoinBase() ||
                Consensus::CheckTxInputs(tx, state, mempoolDuplicate, nSpendHeight, Params().GetConsensus());
            fCheckResult &= Consensus::CheckTxShieldedInputs(tx, state, mempoolDuplicate, 0);
            assert(fCheckResult);
            UpdateCoins(tx, mempoolDuplicate, 1000000);
        }
    }
    unsigned int stepsSinceLastRemove = 0;
    while (!waitingOnDependants.empty()) {
        const CTxMemPoolEntry* entry = waitingOnDependants.front();
        waitingOnDependants.pop_front();
        CValidationState state;
        if (!mempoolDuplicate.HaveInputs(entry->GetTx())) {
            waitingOnDependants.push_back(entry);
            stepsSinceLastRemove++;
            assert(stepsSinceLastRemove < waitingOnDependants.size());
        } else {
            bool fCheckResult = entry->GetTx().IsCoinBase() ||
                Consensus::CheckTxInputs(entry->GetTx(), state, mempoolDuplicate, nSpendHeight, Params().GetConsensus());
            fCheckResult &= Consensus::CheckTxShieldedInputs(entry->GetTx(), state, mempoolDuplicate, 0);
            assert(fCheckResult);
            UpdateCoins(entry->GetTx(), mempoolDuplicate, 1000000);
            stepsSinceLastRemove = 0;
        }
    }
    for (std::map<COutPoint, CInPoint>::const_iterator it = mapNextTx.begin(); it != mapNextTx.end(); it++) {
        uint256 hash = it->second.ptx->GetHash();
        indexed_transaction_set::const_iterator it2 = mapTx.find(hash);
        const CTransaction& tx = it2->GetTx();
        assert(it2 != mapTx.end());
        assert(&tx == it->second.ptx);
        assert(tx.vin.size() > it->second.n);
        assert(it->first == it->second.ptx->vin[it->second.n].prevout);
    }

    checkNullifiers(SPROUT);
    checkNullifiers(SAPLING);
    checkNullifiers(ORCHARD);

    assert(totalTxSize == checkTotal);
    assert(innerUsage == cachedInnerUsage);
}

void CTxMemPool::checkNullifiers(ShieldedType type) const
{
    const std::map<uint256, const CTransaction*>* mapToUse;
    switch (type) {
        case SPROUT:
            mapToUse = &mapSproutNullifiers;
            break;
        case SAPLING:
            mapToUse = &mapSaplingNullifiers;
            break;
        case ORCHARD:
            mapToUse = &mapOrchardNullifiers;
            break;
        default:
            throw runtime_error("Unknown nullifier type");
    }
    for (const auto& entry : *mapToUse) {
        uint256 hash = entry.second->GetHash();
        CTxMemPool::indexed_transaction_set::const_iterator findTx = mapTx.find(hash);
        const CTransaction& tx = findTx->GetTx();
        assert(findTx != mapTx.end());
        assert(&tx == entry.second);
    }
}

bool CTxMemPool::CompareDepthAndScore(const uint256& hasha, const uint256& hashb)
{
    LOCK(cs);
    indexed_transaction_set::const_iterator i = mapTx.find(hasha);
    if (i == mapTx.end()) return false;
    indexed_transaction_set::const_iterator j = mapTx.find(hashb);
    if (j == mapTx.end()) return true;
    // We don't actually compare by depth here because we haven't backported
    // https://github.com/bitcoin/bitcoin/pull/6654
    //
    // But the method name is left as-is because renaming it is not worth the
    // merge conflicts.
    return CompareTxMemPoolEntryByScore()(*i, *j);
}

namespace {
class DepthAndScoreComparator
{
public:
    bool operator()(const CTxMemPool::indexed_transaction_set::const_iterator& a, const CTxMemPool::indexed_transaction_set::const_iterator& b)
    {
        // Same logic applies here as to CTxMemPool::CompareDepthAndScore().
        // As of https://github.com/bitcoin/bitcoin/pull/8126 this comparator
        // duplicates that function (as it doesn't need to hold a dependency
        // on the mempool).
        return CompareTxMemPoolEntryByScore()(*a, *b);
    }
};
}

std::vector<CTxMemPool::indexed_transaction_set::const_iterator> CTxMemPool::GetSortedDepthAndScore() const
{
    std::vector<indexed_transaction_set::const_iterator> iters;
    AssertLockHeld(cs);

    iters.reserve(mapTx.size());

    for (indexed_transaction_set::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi) {
        iters.push_back(mi);
    }
    std::sort(iters.begin(), iters.end(), DepthAndScoreComparator());
    return iters;
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    LOCK(cs);
    auto iters = GetSortedDepthAndScore();

    vtxid.clear();
    vtxid.reserve(mapTx.size());

    for (auto it : iters) {
        vtxid.push_back(it->GetTx().GetHash());
    }
}

std::vector<TxMempoolInfo> CTxMemPool::infoAll() const
{
    LOCK(cs);
    auto iters = GetSortedDepthAndScore();

    std::vector<TxMempoolInfo> ret;
    ret.reserve(mapTx.size());
    for (auto it : iters) {
        ret.push_back(TxMempoolInfo{it->GetSharedTx(), it->GetTime(), CFeeRate(it->GetFee(), it->GetTxSize())});
    }

    return ret;
}

std::shared_ptr<const CTransaction> CTxMemPool::get(const uint256& hash) const
{
    LOCK(cs);
    indexed_transaction_set::const_iterator i = mapTx.find(hash);
    if (i == mapTx.end())
        return nullptr;
    return i->GetSharedTx();
}

TxMempoolInfo CTxMemPool::info(const uint256& hash) const
{
    LOCK(cs);
    indexed_transaction_set::const_iterator i = mapTx.find(hash);
    if (i == mapTx.end())
        return TxMempoolInfo();
    return TxMempoolInfo{i->GetSharedTx(), i->GetTime(), CFeeRate(i->GetFee(), i->GetTxSize())};
}

CFeeRate CTxMemPool::estimateFee(int nBlocks) const
{
    LOCK(cs);
    return minerPolicyEstimator->estimateFee(nBlocks);
}
double CTxMemPool::estimatePriority(int nBlocks) const
{
    LOCK(cs);
    return minerPolicyEstimator->estimatePriority(nBlocks);
}

bool
CTxMemPool::WriteFeeEstimates(CAutoFile& fileout) const
{
    try {
        LOCK(cs);
        fileout << 109900; // version required to read: 0.10.99 or later
        fileout << CLIENT_VERSION; // version that wrote the file
        minerPolicyEstimator->Write(fileout);
    }
    catch (const std::exception&) {
        LogPrintf("CTxMemPool::WriteFeeEstimates(): unable to write policy estimator data (non-fatal)\n");
        return false;
    }
    return true;
}

bool
CTxMemPool::ReadFeeEstimates(CAutoFile& filein)
{
    try {
        int nVersionRequired, nVersionThatWrote;
        filein >> nVersionRequired >> nVersionThatWrote;
        if (nVersionRequired > CLIENT_VERSION)
            return error("CTxMemPool::ReadFeeEstimates(): up-version (%d) fee estimate file", nVersionRequired);

        LOCK(cs);
        minerPolicyEstimator->Read(filein);
    }
    catch (const std::exception&) {
        LogPrintf("CTxMemPool::ReadFeeEstimates(): unable to read policy estimator data (non-fatal)\n");
        return false;
    }
    return true;
}

void CTxMemPool::PrioritiseTransaction(const uint256 hash, const std::string strHash, double dPriorityDelta, const CAmount& nFeeDelta)
{
    {
        LOCK(cs);
        std::pair<double, CAmount> &deltas = mapDeltas[hash];
        deltas.first += dPriorityDelta;
        deltas.second += nFeeDelta;
        txiter it = mapTx.find(hash);
        if (it != mapTx.end()) {
            mapTx.modify(it, update_fee_delta(deltas.second));
        }
    }
    LogPrintf("PrioritiseTransaction: %s priority += %f, fee += %d\n", strHash, dPriorityDelta, FormatMoney(nFeeDelta));
}

void CTxMemPool::ApplyDeltas(const uint256 hash, double &dPriorityDelta, CAmount &nFeeDelta) const
{
    LOCK(cs);
    std::map<uint256, std::pair<double, CAmount> >::const_iterator pos = mapDeltas.find(hash);
    if (pos == mapDeltas.end())
        return;
    const std::pair<double, CAmount> &deltas = pos->second;
    dPriorityDelta += deltas.first;
    nFeeDelta += deltas.second;
}

void CTxMemPool::ClearPrioritisation(const uint256 hash)
{
    LOCK(cs);
    mapDeltas.erase(hash);
}

bool CTxMemPool::HasNoInputsOf(const CTransaction &tx) const
{
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        if (exists(tx.vin[i].prevout.hash))
            return false;
    return true;
}

bool CTxMemPool::nullifierExists(const uint256& nullifier, ShieldedType type) const
{
    switch (type) {
        case SPROUT:
            return mapSproutNullifiers.count(nullifier);
        case SAPLING:
            return mapSaplingNullifiers.count(nullifier);
        case ORCHARD:
            return mapOrchardNullifiers.count(nullifier);
        default:
            throw runtime_error("Unknown nullifier type");
    }
}

std::pair<std::vector<CTransaction>, uint64_t> CTxMemPool::DrainRecentlyAdded()
{
    uint64_t recentlyAddedSequence;
    std::vector<CTransaction> txs;
    {
        LOCK(cs);
        recentlyAddedSequence = nRecentlyAddedSequence;
        for (const auto& kv : mapRecentlyAddedTx) {
            txs.push_back(*(kv.second));
        }
        mapRecentlyAddedTx.clear();
    }

    return std::make_pair(txs, recentlyAddedSequence);
}

void CTxMemPool::SetNotifiedSequence(uint64_t recentlyAddedSequence) {
    assert(Params().NetworkIDString() == "regtest");
    LOCK(cs);
    nNotifiedSequence = recentlyAddedSequence;
}

bool CTxMemPool::IsFullyNotified() {
    assert(Params().NetworkIDString() == "regtest");
    LOCK(cs);
    return nRecentlyAddedSequence == nNotifiedSequence;
}

CCoinsViewMemPool::CCoinsViewMemPool(CCoinsView *baseIn, CTxMemPool &mempoolIn) : CCoinsViewBacked(baseIn), mempool(mempoolIn) { }

bool CCoinsViewMemPool::GetNullifier(const uint256 &nf, ShieldedType type) const
{
    return mempool.nullifierExists(nf, type) || base->GetNullifier(nf, type);
}

bool CCoinsViewMemPool::GetCoins(const uint256 &txid, CCoins &coins) const {
    // If an entry in the mempool exists, always return that one, as it's guaranteed to never
    // conflict with the underlying cache, and it cannot have pruned entries (as it contains full)
    // transactions. First checking the underlying cache risks returning a pruned entry instead.
    shared_ptr<const CTransaction> ptx = mempool.get(txid);
    if (ptx) {
        coins = CCoins(*ptx, MEMPOOL_HEIGHT);
        return true;
    }
    return (base->GetCoins(txid, coins) && !coins.IsPruned());
}

bool CCoinsViewMemPool::HaveCoins(const uint256 &txid) const {
    return mempool.exists(txid) || base->HaveCoins(txid);
}

size_t CTxMemPool::DynamicMemoryUsage() const {
    LOCK(cs);

    size_t total = 0;

    // Estimate the overhead of mapTx to be 9 pointers + an allocation, as no exact formula for
    // boost::multi_index_contained is implemented.
    total += memusage::MallocUsage(sizeof(CTxMemPoolEntry) + 9 * sizeof(void*)) * mapTx.size();

    // Three metadata maps inherited from Bitcoin Core
    total += memusage::DynamicUsage(mapNextTx) + memusage::DynamicUsage(mapDeltas) + memusage::DynamicUsage(mapLinks);

    // Saves iterating over the full map
    total += cachedInnerUsage;

    // Wallet notification
    total += memusage::DynamicUsage(mapRecentlyAddedTx);

    // Nullifier set tracking
    total += memusage::DynamicUsage(mapSproutNullifiers) +
             memusage::DynamicUsage(mapSaplingNullifiers) +
             memusage::DynamicUsage(mapOrchardNullifiers);

    // DoS mitigation
    total += memusage::DynamicUsage(recentlyEvicted) + memusage::DynamicUsage(weightedTxTree);

    // Insight-related structures
    size_t insight = 0;
    insight += memusage::DynamicUsage(mapAddress);
    insight += memusage::DynamicUsage(mapAddressInserted);
    insight += memusage::DynamicUsage(mapSpent);
    insight += memusage::DynamicUsage(mapSpentInserted);
    total += insight;

    return total;
}

void CTxMemPool::SetMempoolCostLimit(int64_t totalCostLimit, int64_t evictionMemorySeconds) {
    LOCK(cs);
    LogPrint("mempool", "Setting mempool cost limit: (limit=%d, time=%d)\n", totalCostLimit, evictionMemorySeconds);
    delete recentlyEvicted;
    delete weightedTxTree;
    recentlyEvicted = new RecentlyEvictedList(GetNodeClock(), evictionMemorySeconds);
    weightedTxTree = new WeightedTxTree(totalCostLimit);
}

bool CTxMemPool::IsRecentlyEvicted(const uint256& txId) {
    LOCK(cs);
    return recentlyEvicted->contains(txId);
}

void CTxMemPool::EnsureSizeLimit() {
    AssertLockHeld(cs);
    std::optional<uint256> maybeDropTxId;
    while ((maybeDropTxId = weightedTxTree->maybeDropRandom()).has_value()) {
        uint256 txId = maybeDropTxId.value();
        recentlyEvicted->add(txId);
        std::list<CTransaction> removed;
        remove(mapTx.find(txId)->GetTx(), removed, true);
    }
}

void CTxMemPool::RemoveStaged(setEntries &stage) {
    AssertLockHeld(cs);
    UpdateForRemoveFromMempool(stage);
    for (const txiter& it : stage) {
        removeUnchecked(it);
    }
}

bool CTxMemPool::addUnchecked(const uint256&hash, const CTxMemPoolEntry &entry, bool fCurrentEstimate)
{
    LOCK(cs);
    setEntries setAncestors;
    uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
    std::string dummy;
    CalculateMemPoolAncestors(entry, setAncestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy);
    return addUnchecked(hash, entry, setAncestors, fCurrentEstimate);
}

void CTxMemPool::UpdateChild(txiter entry, txiter child, bool add)
{
    setEntries s;
    if (add && mapLinks[entry].children.insert(child).second) {
        cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
    } else if (!add && mapLinks[entry].children.erase(child)) {
        cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
    }
}

void CTxMemPool::UpdateParent(txiter entry, txiter parent, bool add)
{
    setEntries s;
    if (add && mapLinks[entry].parents.insert(parent).second) {
        cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
    } else if (!add && mapLinks[entry].parents.erase(parent)) {
        cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
    }
}

const CTxMemPool::setEntries & CTxMemPool::GetMemPoolParents(txiter entry) const
{
    assert (entry != mapTx.end());
    txlinksMap::const_iterator it = mapLinks.find(entry);
    assert(it != mapLinks.end());
    return it->second.parents;
}

const CTxMemPool::setEntries & CTxMemPool::GetMemPoolChildren(txiter entry) const
{
    assert (entry != mapTx.end());
    txlinksMap::const_iterator it = mapLinks.find(entry);
    assert(it != mapLinks.end());
    return it->second.children;
}
