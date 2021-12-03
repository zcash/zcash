// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_COINS_H
#define BITCOIN_COINS_H

#include "compressor.h"
#include "core_memusage.h"
#include "hash.h"
#include "memusage.h"
#include "serialize.h"
#include "uint256.h"

#include <assert.h>
#include <stdint.h>

#include <boost/unordered_map.hpp>
#include "zcash/History.hpp"
#include "zcash/IncrementalMerkleTree.hpp"

enum Spentness {
    UNSPENT,
    SPENT
};

/**
 * Pruned version of CTransaction: only retains metadata and unspent transaction outputs
 */
class CCoins
{
public:

    //! whether transaction is a coinbase
    bool fCoinBase;

    //! unspent transaction outputs; spent outputs are .IsNull(); spent outputs at the end of the array are dropped
    std::vector<CTxOut> vout;

    //! unconsumed transparent extension outputs
    typedef std::pair<CTzeOut, Spentness> TzeOutCoin;
    std::vector<TzeOutCoin> vtzeout;

    //! at which height this transaction was included in the active block chain
    int nHeight;

    //! version of the CTransaction; accesses to this value should probably check for nHeight as well,
    //! as new tx version will probably only be introduced at certain heights
    int nVersion;

    void FromTx(const CTransaction &tx, int nHeightIn) {
        fCoinBase = tx.IsCoinBase();
        vout = tx.vout;
        for (uint32_t i = 0; i < tx.vtzeout.size(); i++ ) {
            vtzeout.push_back(std::make_pair(tx.vtzeout[i], UNSPENT));
        }
        nHeight = nHeightIn;
        nVersion = tx.nVersion;
        ClearUnspendable();
    }

    //! construct a CCoins from a CTransaction, at a given height
    CCoins(const CTransaction &tx, int nHeightIn) {
        FromTx(tx, nHeightIn);
    }

    void Clear() {
        fCoinBase = false;
        std::vector<CTxOut>().swap(vout);
        std::vector<TzeOutCoin>().swap(vtzeout);
        nHeight = 0;
        nVersion = 0;
    }

    //! empty constructor
    CCoins() : fCoinBase(false), vout(0), vtzeout(), nHeight(0), nVersion(0) { }

    /**
     * Remove spent outputs at the end of vout & vtzeout.
     *
     * This is principally useful in relation to the serialized form; it should
     * likely be removed from the interface in favor of the serialization code
     * handling the appropriate compactions. --kjn
     */
    void Cleanup() {
        while (vout.size() > 0 && vout.back().IsNull()) {
            vout.pop_back();
        }
        if (vout.empty()) {
            std::vector<CTxOut>().swap(vout);
        }

        while (vtzeout.size() > 0 && vtzeout.back().second == SPENT) {
            vtzeout.pop_back();
        }
        if (vtzeout.empty()) {
            std::vector<TzeOutCoin>().swap(vtzeout);
        }
    }

    void ClearUnspendable() {
        for (CTxOut &txout : vout) {
            if (txout.scriptPubKey.IsUnspendable())
                txout.SetNull();
        }
        Cleanup();

        // TZE: no analog is possible without a check that calls
        // through to the extension itself?
    }

    void swap(CCoins &to) {
        std::swap(to.fCoinBase, fCoinBase);
        to.vout.swap(vout);
        to.vtzeout.swap(vtzeout);
        std::swap(to.nHeight, nHeight);
        std::swap(to.nVersion, nVersion);
    }

    //! equality test
    friend bool operator==(const CCoins &a, const CCoins &b) {
         // Empty CCoins objects are always equal.
         if (!a.HasUnspent() && !b.HasUnspent())
             return true;

         return a.fCoinBase == b.fCoinBase &&
                a.nHeight == b.nHeight &&
                a.nVersion == b.nVersion &&
                a.vout == b.vout &&
                a.vtzeout == b.vtzeout;
    }
    friend bool operator!=(const CCoins &a, const CCoins &b) {
        return !(a == b);
    }

    bool IsCoinBase() const {
        return fCoinBase;
    }

    //! mark a txout spent
    bool Spend(uint32_t nPos);

    //! mark a tzeout spent
    bool SpendTzeOut(uint32_t nPos);

    //! check whether a particular output is still available
    bool IsAvailable(unsigned int nPos) const {
        return nPos < vout.size() && !vout[nPos].IsNull();
    }

    bool IsTzeAvailable(unsigned int nPos) const {
        return nPos < vtzeout.size() && vtzeout[nPos].second == UNSPENT;
    }

    //! check whether the entire CCoins is spent
    //! note that only HasUnspent() CCoins can be serialized
    bool HasUnspent() const {
        for (const CTxOut& txout : vout) {
            if (!txout.IsNull()) {
                return true;
            }
        }

        for (const TzeOutCoin& tzeout : vtzeout) {
            if (tzeout.second == UNSPENT) {
                return true;
            }
        }

        return false;
    }

    size_t DynamicMemoryUsage() const {
        size_t ret = memusage::DynamicUsage(vout);
        for (const CTxOut &out : vout) {
            ret += RecursiveDynamicUsage(out.scriptPubKey);
        }

        for (const TzeOutCoin& tzeout : vtzeout) {
            ret += RecursiveDynamicUsage(tzeout.first);
        }

        return ret;
    }
};

class SaltedTxidHasher
{
private:
    /** Salt */
    const uint64_t k0, k1;

public:
    SaltedTxidHasher();

    /**
     * This *must* return size_t. With Boost 1.46 on 32-bit systems the
     * unordered_map will behave unpredictably if the custom hasher returns a
     * uint64_t, resulting in failures when syncing the chain (#4634).
     */
    size_t operator()(const uint256& txid) const {
        return SipHashUint256(k0, k1, txid);
    }
};

struct CCoinsCacheEntry
{
    CCoins coins; // The actual cached data.
    unsigned char flags;

    enum Flags {
        DIRTY = (1 << 0), // This cache entry is potentially different from the version in the parent view.
        FRESH = (1 << 1), // The parent view does not have this entry (or it is pruned).
    };

    CCoinsCacheEntry() : coins(), flags(0) {}
};

struct CAnchorsSproutCacheEntry
{
    bool entered; // This will be false if the anchor is removed from the cache
    SproutMerkleTree tree; // The tree itself
    unsigned char flags;

    enum Flags {
        DIRTY = (1 << 0), // This cache entry is potentially different from the version in the parent view.
    };

    CAnchorsSproutCacheEntry() : entered(false), flags(0) {}
};

struct CAnchorsSaplingCacheEntry
{
    bool entered; // This will be false if the anchor is removed from the cache
    SaplingMerkleTree tree; // The tree itself
    unsigned char flags;

    enum Flags {
        DIRTY = (1 << 0), // This cache entry is potentially different from the version in the parent view.
    };

    CAnchorsSaplingCacheEntry() : entered(false), flags(0) {}
};

struct CAnchorsOrchardCacheEntry
{
    bool entered; // This will be false if the anchor is removed from the cache
    OrchardMerkleFrontier tree; // The tree itself
    unsigned char flags;

    enum Flags {
        DIRTY = (1 << 0), // This cache entry is potentially different from the version in the parent view.
    };

    CAnchorsOrchardCacheEntry() : entered(false), flags(0) {}
};

struct CNullifiersCacheEntry
{
    bool entered; // If the nullifier is spent or not
    unsigned char flags;

    enum Flags {
        DIRTY = (1 << 0), // This cache entry is potentially different from the version in the parent view.
    };

    CNullifiersCacheEntry() : entered(false), flags(0) {}
};

/// These identify the value pool, and as such, Canopy (for example)
/// isn't here, since value created during the Canopy network upgrade
/// is part of the Sapling pool.
enum ShieldedType
{
    SPROUT,
    SAPLING,
    ORCHARD,
};

typedef boost::unordered_map<uint256, CCoinsCacheEntry, SaltedTxidHasher> CCoinsMap;
typedef boost::unordered_map<uint256, CAnchorsSproutCacheEntry, SaltedTxidHasher> CAnchorsSproutMap;
typedef boost::unordered_map<uint256, CAnchorsSaplingCacheEntry, SaltedTxidHasher> CAnchorsSaplingMap;
typedef boost::unordered_map<uint256, CAnchorsOrchardCacheEntry, SaltedTxidHasher> CAnchorsOrchardMap;
typedef boost::unordered_map<uint256, CNullifiersCacheEntry, SaltedTxidHasher> CNullifiersMap;
typedef boost::unordered_map<uint32_t, HistoryCache> CHistoryCacheMap;

struct CCoinsStats
{
    int nHeight;
    uint256 hashBlock;
    uint64_t nTransactions;
    uint64_t nTransactionOutputs;
    uint64_t nSerializedSize;
    uint256 hashSerialized;
    CAmount nTotalAmount;

    CCoinsStats() : nHeight(0), nTransactions(0), nTransactionOutputs(0), nSerializedSize(0), nTotalAmount(0) {}
};


/** Abstract view on the open txout dataset. */
class CCoinsView
{
public:
    //! Retrieve the tree (Sprout) at a particular anchored root in the chain
    virtual bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const;

    //! Retrieve the tree (Sapling) at a particular anchored root in the chain
    virtual bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const;

    //! Retrieve the tree (Orchard) at a particular anchored root in the chain
    virtual bool GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const;

    //! Determine whether a nullifier is spent or not
    virtual bool GetNullifier(const uint256 &nullifier, ShieldedType type) const;

    //! Retrieve the CCoins (unspent transaction outputs) for a given txid
    virtual bool GetCoins(const uint256 &txid, CCoins &coins) const;

    //! Just check whether we have data for a given txid.
    //! This may (but cannot always) return true for fully spent transactions
    virtual bool HaveCoins(const uint256 &txid) const;

    //! Retrieve the block hash whose state this CCoinsView currently represents
    virtual uint256 GetBestBlock() const;

    //! Get the current "tip" or the latest anchored tree root in the chain
    virtual uint256 GetBestAnchor(ShieldedType type) const;

    //! Get the current chain history length (which should be roughly chain height x2)
    virtual HistoryIndex GetHistoryLength(uint32_t epochId) const;

    //! Get history node at specified index
    virtual HistoryNode GetHistoryAt(uint32_t epochId, HistoryIndex index) const;

    //! Get current history root
    virtual uint256 GetHistoryRoot(uint32_t epochId) const;

    //! Do a bulk modification (multiple CCoins changes + BestBlock change).
    //! The passed mapCoins can be modified.
    virtual bool BatchWrite(CCoinsMap &mapCoins,
                            const uint256 &hashBlock,
                            const uint256 &hashSproutAnchor,
                            const uint256 &hashSaplingAnchor,
                            const uint256 &hashOrchardAnchor,
                            CAnchorsSproutMap &mapSproutAnchors,
                            CAnchorsSaplingMap &mapSaplingAnchors,
                            CAnchorsOrchardMap &mapOrchardAnchors,
                            CNullifiersMap &mapSproutNullifiers,
                            CNullifiersMap &mapSaplingNullifiers,
                            CNullifiersMap &mapOrchardNullifiers,
                            CHistoryCacheMap &historyCacheMap);

    //! Calculate statistics about the unspent transaction output set
    virtual bool GetStats(CCoinsStats &stats) const;

    //! As we use CCoinsViews polymorphically, have a virtual destructor
    virtual ~CCoinsView() {}
};


/** CCoinsView backed by another CCoinsView */
class CCoinsViewBacked : public CCoinsView
{
protected:
    CCoinsView *base;

public:
    CCoinsViewBacked(CCoinsView *viewIn);
    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const;
    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const;
    bool GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const;
    bool GetNullifier(const uint256 &nullifier, ShieldedType type) const;
    bool GetCoins(const uint256 &txid, CCoins &coins) const;
    bool HaveCoins(const uint256 &txid) const;
    uint256 GetBestBlock() const;
    uint256 GetBestAnchor(ShieldedType type) const;
    HistoryIndex GetHistoryLength(uint32_t epochId) const;
    HistoryNode GetHistoryAt(uint32_t epochId, HistoryIndex index) const;
    uint256 GetHistoryRoot(uint32_t epochId) const;
    void SetBackend(CCoinsView &viewIn);
    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    const uint256 &hashOrchardAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CAnchorsOrchardMap &mapOrchardAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap &mapSaplingNullifiers,
                    CNullifiersMap &mapOrchardNullifiers,
                    CHistoryCacheMap &historyCacheMap);
    bool GetStats(CCoinsStats &stats) const;
};


class CCoinsViewCache;

/**
 * A reference to a mutable cache entry. Encapsulating it allows us to run
 *  cleanup code after the modification is finished, and keeping track of
 *  concurrent modifications.
 */
class CCoinsModifier
{
private:
    CCoinsViewCache& cache;
    CCoinsMap::iterator it;
    size_t cachedCoinUsage; // Cached memory usage of the CCoins object before modification
    CCoinsModifier(CCoinsViewCache& cache_, CCoinsMap::iterator it_, size_t usage);

public:
    CCoins* operator->() { return &it->second.coins; }
    CCoins& operator*() { return it->second.coins; }
    ~CCoinsModifier();
    friend class CCoinsViewCache;
};

/** The set of shielded requirements that might be unsatisfied. */
enum class UnsatisfiedShieldedReq {
    SproutDuplicateNullifier,
    SproutUnknownAnchor,
    SaplingDuplicateNullifier,
    SaplingUnknownAnchor,
    OrchardDuplicateNullifier,
    OrchardUnknownAnchor,
};

/** CCoinsView that adds a memory cache for transactions to another CCoinsView */
class CCoinsViewCache : public CCoinsViewBacked
{
protected:
    /* Whether this cache has an active modifier. */
    bool hasModifier;


    /**
     * Make mutable so that we can "fill the cache" even from Get-methods
     * declared as "const".
     */
    mutable uint256 hashBlock;
    mutable CCoinsMap cacheCoins;
    mutable uint256 hashSproutAnchor;
    mutable uint256 hashSaplingAnchor;
    mutable uint256 hashOrchardAnchor;
    mutable CAnchorsSproutMap cacheSproutAnchors;
    mutable CAnchorsSaplingMap cacheSaplingAnchors;
    mutable CAnchorsOrchardMap cacheOrchardAnchors;
    mutable CNullifiersMap cacheSproutNullifiers;
    mutable CNullifiersMap cacheSaplingNullifiers;
    mutable CNullifiersMap cacheOrchardNullifiers;
    mutable CHistoryCacheMap historyCacheMap;

    /* Cached dynamic memory usage for the inner CCoins objects. */
    mutable size_t cachedCoinsUsage;

public:
    CCoinsViewCache(CCoinsView *baseIn);
    ~CCoinsViewCache();

    // Standard CCoinsView methods
    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const;
    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const;
    bool GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const;
    bool GetNullifier(const uint256 &nullifier, ShieldedType type) const;
    bool GetCoins(const uint256 &txid, CCoins &coins) const;
    bool HaveCoins(const uint256 &txid) const;
    uint256 GetBestBlock() const;
    uint256 GetBestAnchor(ShieldedType type) const;
    HistoryIndex GetHistoryLength(uint32_t epochId) const;
    HistoryNode GetHistoryAt(uint32_t epochId, HistoryIndex index) const;
    uint256 GetHistoryRoot(uint32_t epochId) const;
    void SetBestBlock(const uint256 &hashBlock);
    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    const uint256 &hashOrchardAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CAnchorsOrchardMap &mapOrchardAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap &mapSaplingNullifiers,
                    CNullifiersMap &mapOrchardNullifiers,
                    CHistoryCacheMap &historyCacheMap);

    // Adds the tree to mapSproutAnchors, mapSaplingAnchors, or mapOrchardAnchors
    // based on the type of tree and sets the current commitment root to this root.
    template<typename Tree> void PushAnchor(const Tree &tree);

    // Removes the current commitment root from mapAnchors and sets
    // the new current root.
    void PopAnchor(const uint256 &rt, ShieldedType type);

    // Marks nullifiers for a given transaction as spent or not.
    void SetNullifiers(const CTransaction& tx, bool spent);

    // Push MMR node history at the end of the history tree
    void PushHistoryNode(uint32_t epochId, const HistoryNode node);

    // Pop MMR node history from the end of the history tree
    void PopHistoryNode(uint32_t epochId);

    /**
     * Return a pointer to CCoins in the cache, or NULL if not found. This is
     * more efficient than GetCoins. Modifications to other cache entries are
     * allowed while accessing the returned pointer.
     */
    const CCoins* AccessCoins(const uint256 &txid) const;

    /**
     * Return a modifiable reference to a CCoins. If no entry with the given
     * txid exists, a new one is created. Simultaneous modifications are not
     * allowed.
     */
    CCoinsModifier ModifyCoins(const uint256 &txid);

    /**
     * Return a modifiable reference to a CCoins. Assumes that no entry with the given
     * txid exists and creates a new one. This saves a database access in the case where
     * the coins were to be wiped out by FromTx anyway. We rely on Zcash-derived block chains
     * having no duplicate transactions, since BIP 30 and (except for the genesis block)
     * BIP 34 have been enforced since launch. See the Zcash protocol specification, section
     * "Bitcoin Improvement Proposals". Simultaneous modifications are not allowed.
     */
    CCoinsModifier ModifyNewCoins(const uint256 &txid);

    /**
     * Push the modifications applied to this cache to its base.
     * Failure to call this method before destruction will cause the changes to be forgotten.
     * If false is returned, the state of this cache (and its backing view) will be undefined.
     */
    bool Flush();

    //! Calculate the size of the cache (in number of transactions)
    unsigned int GetCacheSize() const;

    //! Calculate the size of the cache (in bytes)
    size_t DynamicMemoryUsage() const;

    /**
     * Amount of bitcoins coming in to a transaction
     * Note that lightweight clients may not know anything besides the hash of previous transactions,
     * so may not be able to calculate this.
     *
     * @param[in] tx	transaction for which we are checking input total
     * @return	Sum of value of all inputs (scriptSigs), (positive valueBalance or zero) and JoinSplit vpub_new
     */
    CAmount GetValueIn(const CTransaction& tx) const;

    //! Check whether all prevouts of the transaction are present in the UTXO set represented by this view
    bool HaveInputs(const CTransaction& tx) const;

    //! Check whether all joinsplit and sapling spend requirements (anchors/nullifiers) are satisfied
    std::optional<UnsatisfiedShieldedReq> HaveShieldedRequirements(const CTransaction& tx) const;

    //! Return priority of tx at height nHeight
    double GetPriority(const CTransaction &tx, int nHeight) const;

    const CTxOut &GetOutputFor(const CTxIn& input) const;

    const CTzeOut &GetTzeOutFor(const CTzeIn& input) const;

    friend class CCoinsModifier;

private:
    CCoinsMap::iterator FetchCoins(const uint256 &txid);
    CCoinsMap::const_iterator FetchCoins(const uint256 &txid) const;

    /**
     * By making the copy constructor private, we prevent accidentally using it
     * when one intends to create a cache on top of a base cache.
     */
    CCoinsViewCache(const CCoinsViewCache &);

    //! Generalized interface for popping anchors
    template<typename Tree, typename Cache, typename CacheEntry>
    void AbstractPopAnchor(
        const uint256 &newrt,
        ShieldedType type,
        Cache &cacheAnchors,
        uint256 &hash
    );

    //! Generalized interface for pushing anchors
    template<typename Tree, typename Cache, typename CacheIterator, typename CacheEntry>
    void AbstractPushAnchor(
        const Tree &tree,
        ShieldedType type,
        Cache &cacheAnchors,
        uint256 &hash
    );

    //! Interface for bringing an anchor into the cache.
    template<typename Tree>
    void BringBestAnchorIntoCache(
        const uint256 &currentRoot,
        Tree &tree
    );

    //! Preload history tree for further update.
    //!
    //! If extra = true, extra nodes for deletion are also preloaded.
    //! This will allow to delete tail entries from preloaded tree without
    //! any further database lookups.
    //!
    //! Returns number of peaks, not total number of loaded nodes.
    uint32_t PreloadHistoryTree(uint32_t epochId, bool extra, std::vector<HistoryEntry> &entries, std::vector<uint32_t> &entry_indices);

    //! Selects history cache for specified epoch.
    HistoryCache& SelectHistoryCache(uint32_t epochId) const;
};

#endif // BITCOIN_COINS_H
