// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"

#include "memusage.h"
#include "random.h"
#include "version.h"
#include "policy/fees.h"
#include "komodo_defs.h"

#include <assert.h>

/**
 * calculate number of bytes for the bitmask, and its number of non-zero bytes
 * each bit in the bitmask represents the availability of one output, but the
 * availabilities of the first two outputs are encoded separately
 */
void CCoins::CalcMaskSize(unsigned int &nBytes, unsigned int &nNonzeroBytes) const {
    unsigned int nLastUsedByte = 0;
    for (unsigned int b = 0; 2+b*8 < vout.size(); b++) {
        bool fZero = true;
        for (unsigned int i = 0; i < 8 && 2+b*8+i < vout.size(); i++) {
            if (!vout[2+b*8+i].IsNull()) {
                fZero = false;
                continue;
            }
        }
        if (!fZero) {
            nLastUsedByte = b + 1;
            nNonzeroBytes++;
        }
    }
    nBytes += nLastUsedByte;
}

bool CCoins::Spend(uint32_t nPos) 
{
    if (nPos >= vout.size() || vout[nPos].IsNull())
        return false;
    vout[nPos].SetNull();
    Cleanup();
    return true;
}
bool CCoinsView::GetAnchorAt(const uint256 &rt, ZCIncrementalMerkleTree &tree) const { return false; }
bool CCoinsView::GetNullifier(const uint256 &nullifier) const { return false; }
bool CCoinsView::GetCoins(const uint256 &txid, CCoins &coins) const { return false; }
bool CCoinsView::HaveCoins(const uint256 &txid) const { return false; }
uint256 CCoinsView::GetBestBlock() const { return uint256(); }
uint256 CCoinsView::GetBestAnchor() const { return uint256(); };
bool CCoinsView::BatchWrite(CCoinsMap &mapCoins,
                            const uint256 &hashBlock,
                            const uint256 &hashAnchor,
                            CAnchorsMap &mapAnchors,
                            CNullifiersMap &mapNullifiers) { return false; }
bool CCoinsView::GetStats(CCoinsStats &stats) const { return false; }


CCoinsViewBacked::CCoinsViewBacked(CCoinsView *viewIn) : base(viewIn) { }

bool CCoinsViewBacked::GetAnchorAt(const uint256 &rt, ZCIncrementalMerkleTree &tree) const { return base->GetAnchorAt(rt, tree); }
bool CCoinsViewBacked::GetNullifier(const uint256 &nullifier) const { return base->GetNullifier(nullifier); }
bool CCoinsViewBacked::GetCoins(const uint256 &txid, CCoins &coins) const { return base->GetCoins(txid, coins); }
bool CCoinsViewBacked::HaveCoins(const uint256 &txid) const { return base->HaveCoins(txid); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
uint256 CCoinsViewBacked::GetBestAnchor() const { return base->GetBestAnchor(); }
void CCoinsViewBacked::SetBackend(CCoinsView &viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap &mapCoins,
                                  const uint256 &hashBlock,
                                  const uint256 &hashAnchor,
                                  CAnchorsMap &mapAnchors,
                                  CNullifiersMap &mapNullifiers) { return base->BatchWrite(mapCoins, hashBlock, hashAnchor, mapAnchors, mapNullifiers); }
bool CCoinsViewBacked::GetStats(CCoinsStats &stats) const { return base->GetStats(stats); }

CCoinsKeyHasher::CCoinsKeyHasher() : salt(GetRandHash()) {}

CCoinsViewCache::CCoinsViewCache(CCoinsView *baseIn) : CCoinsViewBacked(baseIn), hasModifier(false), cachedCoinsUsage(0) { }

CCoinsViewCache::~CCoinsViewCache()
{
    assert(!hasModifier);
}

size_t CCoinsViewCache::DynamicMemoryUsage() const {
    return memusage::DynamicUsage(cacheCoins) +
           memusage::DynamicUsage(cacheAnchors) +
           memusage::DynamicUsage(cacheNullifiers) +
           cachedCoinsUsage;
}

CCoinsMap::const_iterator CCoinsViewCache::FetchCoins(const uint256 &txid) const {
    CCoinsMap::iterator it = cacheCoins.find(txid);
    if (it != cacheCoins.end())
        return it;
    CCoins tmp;
    if (!base->GetCoins(txid, tmp))
        return cacheCoins.end();
    CCoinsMap::iterator ret = cacheCoins.insert(std::make_pair(txid, CCoinsCacheEntry())).first;
    tmp.swap(ret->second.coins);
    if (ret->second.coins.IsPruned()) {
        // The parent only has an empty entry for this txid; we can consider our
        // version as fresh.
        ret->second.flags = CCoinsCacheEntry::FRESH;
    }
    cachedCoinsUsage += ret->second.coins.DynamicMemoryUsage();
    return ret;
}


bool CCoinsViewCache::GetAnchorAt(const uint256 &rt, ZCIncrementalMerkleTree &tree) const {
    CAnchorsMap::const_iterator it = cacheAnchors.find(rt);
    if (it != cacheAnchors.end()) {
        if (it->second.entered) {
            tree = it->second.tree;
            return true;
        } else {
            return false;
        }
    }

    if (!base->GetAnchorAt(rt, tree)) {
        return false;
    }

    CAnchorsMap::iterator ret = cacheAnchors.insert(std::make_pair(rt, CAnchorsCacheEntry())).first;
    ret->second.entered = true;
    ret->second.tree = tree;
    cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();

    return true;
}

bool CCoinsViewCache::GetNullifier(const uint256 &nullifier) const {
    CNullifiersMap::iterator it = cacheNullifiers.find(nullifier);
    if (it != cacheNullifiers.end())
        return it->second.entered;

    CNullifiersCacheEntry entry;
    bool tmp = base->GetNullifier(nullifier);
    entry.entered = tmp;

    cacheNullifiers.insert(std::make_pair(nullifier, entry));

    return tmp;
}

void CCoinsViewCache::PushAnchor(const ZCIncrementalMerkleTree &tree) {
    uint256 newrt = tree.root();

    auto currentRoot = GetBestAnchor();

    // We don't want to overwrite an anchor we already have.
    // This occurs when a block doesn't modify mapAnchors at all,
    // because there are no joinsplits. We could get around this a
    // different way (make all blocks modify mapAnchors somehow)
    // but this is simpler to reason about.
    if (currentRoot != newrt) {
        auto insertRet = cacheAnchors.insert(std::make_pair(newrt, CAnchorsCacheEntry()));
        CAnchorsMap::iterator ret = insertRet.first;

        ret->second.entered = true;
        ret->second.tree = tree;
        ret->second.flags = CAnchorsCacheEntry::DIRTY;

        if (insertRet.second) {
            // An insert took place
            cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();
        }

        hashAnchor = newrt;
    }
}

void CCoinsViewCache::PopAnchor(const uint256 &newrt) {
    auto currentRoot = GetBestAnchor();

    // Blocks might not change the commitment tree, in which
    // case restoring the "old" anchor during a reorg must
    // have no effect.
    if (currentRoot != newrt) {
        // Bring the current best anchor into our local cache
        // so that its tree exists in memory.
        {
            ZCIncrementalMerkleTree tree;
            assert(GetAnchorAt(currentRoot, tree));
        }

        // Mark the anchor as unentered, removing it from view
        cacheAnchors[currentRoot].entered = false;

        // Mark the cache entry as dirty so it's propagated
        cacheAnchors[currentRoot].flags = CAnchorsCacheEntry::DIRTY;

        // Mark the new root as the best anchor
        hashAnchor = newrt;
    }
}

void CCoinsViewCache::SetNullifier(const uint256 &nullifier, bool spent) {
    std::pair<CNullifiersMap::iterator, bool> ret = cacheNullifiers.insert(std::make_pair(nullifier, CNullifiersCacheEntry()));
    ret.first->second.entered = spent;
    ret.first->second.flags |= CNullifiersCacheEntry::DIRTY;
}

bool CCoinsViewCache::GetCoins(const uint256 &txid, CCoins &coins) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    if (it != cacheCoins.end()) {
        coins = it->second.coins;
        return true;
    }
    return false;
}

CCoinsModifier CCoinsViewCache::ModifyCoins(const uint256 &txid) {
    assert(!hasModifier);
    std::pair<CCoinsMap::iterator, bool> ret = cacheCoins.insert(std::make_pair(txid, CCoinsCacheEntry()));
    size_t cachedCoinUsage = 0;
    if (ret.second) {
        if (!base->GetCoins(txid, ret.first->second.coins)) {
            // The parent view does not have this entry; mark it as fresh.
            ret.first->second.coins.Clear();
            ret.first->second.flags = CCoinsCacheEntry::FRESH;
        } else if (ret.first->second.coins.IsPruned()) {
            // The parent view only has a pruned entry for this; mark it as fresh.
            ret.first->second.flags = CCoinsCacheEntry::FRESH;
        }
    } else {
        cachedCoinUsage = ret.first->second.coins.DynamicMemoryUsage();
    }
    // Assume that whenever ModifyCoins is called, the entry will be modified.
    ret.first->second.flags |= CCoinsCacheEntry::DIRTY;
    return CCoinsModifier(*this, ret.first, cachedCoinUsage);
}

const CCoins* CCoinsViewCache::AccessCoins(const uint256 &txid) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    if (it == cacheCoins.end()) {
        return NULL;
    } else {
        return &it->second.coins;
    }
}

bool CCoinsViewCache::HaveCoins(const uint256 &txid) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    // We're using vtx.empty() instead of IsPruned here for performance reasons,
    // as we only care about the case where a transaction was replaced entirely
    // in a reorganization (which wipes vout entirely, as opposed to spending
    // which just cleans individual outputs).
    return (it != cacheCoins.end() && !it->second.coins.vout.empty());
}

uint256 CCoinsViewCache::GetBestBlock() const {
    if (hashBlock.IsNull())
    {
        if (base)
        {
            hashBlock = base->GetBestBlock();
        }
        else
        {
            hashBlock = uint256();
        }
    }
    return hashBlock;
}


uint256 CCoinsViewCache::GetBestAnchor() const {
    if (hashAnchor.IsNull())
        hashAnchor = base->GetBestAnchor();
    return hashAnchor;
}

void CCoinsViewCache::SetBestBlock(const uint256 &hashBlockIn) {
    hashBlock = hashBlockIn;
}

bool CCoinsViewCache::BatchWrite(CCoinsMap &mapCoins,
                                 const uint256 &hashBlockIn,
                                 const uint256 &hashAnchorIn,
                                 CAnchorsMap &mapAnchors,
                                 CNullifiersMap &mapNullifiers) {
    assert(!hasModifier);
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) { // Ignore non-dirty entries (optimization).
            CCoinsMap::iterator itUs = cacheCoins.find(it->first);
            if (itUs == cacheCoins.end()) {
                if (!it->second.coins.IsPruned()) {
                    // The parent cache does not have an entry, while the child
                    // cache does have (a non-pruned) one. Move the data up, and
                    // mark it as fresh (if the grandparent did have it, we
                    // would have pulled it in at first GetCoins).
                    assert(it->second.flags & CCoinsCacheEntry::FRESH);
                    CCoinsCacheEntry& entry = cacheCoins[it->first];
                    entry.coins.swap(it->second.coins);
                    cachedCoinsUsage += entry.coins.DynamicMemoryUsage();
                    entry.flags = CCoinsCacheEntry::DIRTY | CCoinsCacheEntry::FRESH;
                }
            } else {
                if ((itUs->second.flags & CCoinsCacheEntry::FRESH) && it->second.coins.IsPruned()) {
                    // The grandparent does not have an entry, and the child is
                    // modified and being pruned. This means we can just delete
                    // it from the parent.
                    cachedCoinsUsage -= itUs->second.coins.DynamicMemoryUsage();
                    cacheCoins.erase(itUs);
                } else {
                    // A normal modification.
                    cachedCoinsUsage -= itUs->second.coins.DynamicMemoryUsage();
                    itUs->second.coins.swap(it->second.coins);
                    cachedCoinsUsage += itUs->second.coins.DynamicMemoryUsage();
                    itUs->second.flags |= CCoinsCacheEntry::DIRTY;
                }
            }
        }
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }

    for (CAnchorsMap::iterator child_it = mapAnchors.begin(); child_it != mapAnchors.end();)
    {
        if (child_it->second.flags & CAnchorsCacheEntry::DIRTY) {
            CAnchorsMap::iterator parent_it = cacheAnchors.find(child_it->first);

            if (parent_it == cacheAnchors.end()) {
                CAnchorsCacheEntry& entry = cacheAnchors[child_it->first];
                entry.entered = child_it->second.entered;
                entry.tree = child_it->second.tree;
                entry.flags = CAnchorsCacheEntry::DIRTY;

                cachedCoinsUsage += entry.tree.DynamicMemoryUsage();
            } else {
                if (parent_it->second.entered != child_it->second.entered) {
                    // The parent may have removed the entry.
                    parent_it->second.entered = child_it->second.entered;
                    parent_it->second.flags |= CAnchorsCacheEntry::DIRTY;
                }
            }
        }

        CAnchorsMap::iterator itOld = child_it++;
        mapAnchors.erase(itOld);
    }

    for (CNullifiersMap::iterator child_it = mapNullifiers.begin(); child_it != mapNullifiers.end();)
    {
        if (child_it->second.flags & CNullifiersCacheEntry::DIRTY) { // Ignore non-dirty entries (optimization).
            CNullifiersMap::iterator parent_it = cacheNullifiers.find(child_it->first);

            if (parent_it == cacheNullifiers.end()) {
                CNullifiersCacheEntry& entry = cacheNullifiers[child_it->first];
                entry.entered = child_it->second.entered;
                entry.flags = CNullifiersCacheEntry::DIRTY;
            } else {
                if (parent_it->second.entered != child_it->second.entered) {
                    parent_it->second.entered = child_it->second.entered;
                    parent_it->second.flags |= CNullifiersCacheEntry::DIRTY;
                }
            }
        }
        CNullifiersMap::iterator itOld = child_it++;
        mapNullifiers.erase(itOld);
    }

    hashAnchor = hashAnchorIn;
    hashBlock = hashBlockIn;
    return true;
}

bool CCoinsViewCache::Flush() {
    bool fOk = base->BatchWrite(cacheCoins, hashBlock, hashAnchor, cacheAnchors, cacheNullifiers);
    cacheCoins.clear();
    cacheAnchors.clear();
    cacheNullifiers.clear();
    cachedCoinsUsage = 0;
    return fOk;
}

unsigned int CCoinsViewCache::GetCacheSize() const {
    return cacheCoins.size();
}

const CTxOut &CCoinsViewCache::GetOutputFor(const CTxIn& input) const
{
    const CCoins* coins = AccessCoins(input.prevout.hash);
    assert(coins && coins->IsAvailable(input.prevout.n));
    return coins->vout[input.prevout.n];
}

//uint64_t komodo_interest(int32_t txheight,uint64_t nValue,uint32_t nLockTime,uint32_t tiptime);
uint64_t komodo_accrued_interest(int32_t *txheightp,uint32_t *locktimep,uint256 hash,int32_t n,int32_t checkheight,uint64_t checkvalue,int32_t tipheight);
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];

const CScript &CCoinsViewCache::GetSpendFor(const CCoins *coins, const CTxIn& input)
{
    assert(coins);
    if (coins->nHeight < 6400 && !strcmp(ASSETCHAINS_SYMBOL, "VRSC"))
    {
        std::string hc = input.prevout.hash.ToString();
        if (LaunchMap().lmap.count(hc))
        {
            CTransactionExceptionData &txData = LaunchMap().lmap[hc];
            if ((txData.voutMask & (((uint64_t)1) << (uint64_t)input.prevout.n)) != 0)
            {
                return txData.scriptPubKey;
            }
        }
    }
    return coins->vout[input.prevout.n].scriptPubKey;
}

const CScript &CCoinsViewCache::GetSpendFor(const CTxIn& input) const
{
    const CCoins* coins = AccessCoins(input.prevout.hash);
    return GetSpendFor(coins, input);
}

CAmount CCoinsViewCache::GetValueIn(int32_t nHeight,int64_t *interestp,const CTransaction& tx,uint32_t tiptime) const
{
    if ( interestp != 0 )
        *interestp = 0;
    if ( tx.IsCoinBase() != 0 )
        return 0;
    CAmount value,nResult = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        value = GetOutputFor(tx.vin[i]).nValue;
        nResult += value;
#ifdef KOMODO_ENABLE_INTEREST
        if ( ASSETCHAINS_SYMBOL[0] == 0 && nHeight >= 60000 )
        {
            if ( value >= 10*COIN )
            {
                int64_t interest; int32_t txheight; uint32_t locktime;
                interest = komodo_accrued_interest(&txheight,&locktime,tx.vin[i].prevout.hash,tx.vin[i].prevout.n,0,value,(int32_t)nHeight);
                //printf("nResult %.8f += val %.8f interest %.8f ht.%d lock.%u tip.%u\n",(double)nResult/COIN,(double)value/COIN,(double)interest/COIN,txheight,locktime,tiptime);
                //fprintf(stderr,"nResult %.8f += val %.8f interest %.8f ht.%d lock.%u tip.%u\n",(double)nResult/COIN,(double)value/COIN,(double)interest/COIN,txheight,locktime,tiptime);
                nResult += interest;
                (*interestp) += interest;
            }
        }
#endif
    }
    nResult += tx.GetJoinSplitValueIn();

    return nResult;
}

bool CCoinsViewCache::HaveJoinSplitRequirements(const CTransaction& tx) const
{
    boost::unordered_map<uint256, ZCIncrementalMerkleTree, CCoinsKeyHasher> intermediates;

    BOOST_FOREACH(const JSDescription &joinsplit, tx.vjoinsplit)
    {
        BOOST_FOREACH(const uint256& nullifier, joinsplit.nullifiers)
        {
            if (GetNullifier(nullifier)) {
                // If the nullifier is set, this transaction
                // double-spends!
                return false;
            }
        }

        ZCIncrementalMerkleTree tree;
        auto it = intermediates.find(joinsplit.anchor);
        if (it != intermediates.end()) {
            tree = it->second;
        } else if (!GetAnchorAt(joinsplit.anchor, tree)) {
            return false;
        }

        BOOST_FOREACH(const uint256& commitment, joinsplit.commitments)
        {
            tree.append(commitment);
        }

        intermediates.insert(std::make_pair(tree.root(), tree));
    }

    return true;
}

bool CCoinsViewCache::HaveInputs(const CTransaction& tx) const
{
    if (!tx.IsCoinBase()) {
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            const COutPoint &prevout = tx.vin[i].prevout;
            const CCoins* coins = AccessCoins(prevout.hash);
            if (!coins || !coins->IsAvailable(prevout.n)) {
                fprintf(stderr,"HaveInputs missing input %s/v%d\n",prevout.hash.ToString().c_str(),prevout.n);
                return false;
            }
        }
    }
    return true;
}

double CCoinsViewCache::GetPriority(const CTransaction &tx, int nHeight) const
{
    if (tx.IsCoinBase())
        return 0.0;
    // Joinsplits do not reveal any information about the value or age of a note, so we
    // cannot apply the priority algorithm used for transparent utxos.  Instead, we just
    // use the maximum priority whenever a transaction contains any JoinSplits.
    // (Note that coinbase transactions cannot contain JoinSplits.)
    // FIXME: this logic is partially duplicated between here and CreateNewBlock in miner.cpp.
    
    if (tx.vjoinsplit.size() > 0) {
            return MAX_PRIORITY;
        }

    double dResult = 0.0;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        const CCoins* coins = AccessCoins(txin.prevout.hash);
        assert(coins);
        if (!coins->IsAvailable(txin.prevout.n)) continue;
        if (coins->nHeight < nHeight) {
            dResult += coins->vout[txin.prevout.n].nValue * (nHeight-coins->nHeight);
        }
    }

    return tx.ComputePriority(dResult);
}

CCoinsModifier::CCoinsModifier(CCoinsViewCache& cache_, CCoinsMap::iterator it_, size_t usage) : cache(cache_), it(it_), cachedCoinUsage(usage) {
    assert(!cache.hasModifier);
    cache.hasModifier = true;
}

CCoinsModifier::~CCoinsModifier()
{
    assert(cache.hasModifier);
    cache.hasModifier = false;
    it->second.coins.Cleanup();
    cache.cachedCoinsUsage -= cachedCoinUsage; // Subtract the old usage
    if ((it->second.flags & CCoinsCacheEntry::FRESH) && it->second.coins.IsPruned()) {
        cache.cacheCoins.erase(it);
    } else {
        // If the coin still exists after the modification, add the new usage
        cache.cachedCoinsUsage += it->second.coins.DynamicMemoryUsage();
    }
}
