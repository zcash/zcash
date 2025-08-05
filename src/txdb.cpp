// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "txdb.h"

#include "chainparams.h"
#include "hash.h"
#include "main.h"
#include "pow.h"
#include "init.h"
#include "uint256.h"
#include "zcash/History.hpp"

#include <stdint.h>

#include <boost/thread.hpp>

#include <rust/metrics.h>

using namespace std;

// NOTE: Per issue #3277, do not use the prefix 'X' or 'x' as they were
// previously used by DB_SAPLING_ANCHOR and DB_BEST_SAPLING_ANCHOR.
static const char DB_SPROUT_ANCHOR = 'A';
static const char DB_SAPLING_ANCHOR = 'Z';
static const char DB_ORCHARD_ANCHOR = 'Y';
static const char DB_NULLIFIER = 's';
static const char DB_SAPLING_NULLIFIER = 'S';
static const char DB_ORCHARD_NULLIFIER = 'O';
static const char DB_COINS = 'c';
static const char DB_BLOCK_FILES = 'f';
static const char DB_TXINDEX = 't';
static const char DB_BLOCK_INDEX = 'b';

static const char DB_BEST_BLOCK = 'B';
static const char DB_BEST_SPROUT_ANCHOR = 'a';
static const char DB_BEST_SAPLING_ANCHOR = 'z';
static const char DB_BEST_ORCHARD_ANCHOR = 'y';
static const char DB_FLAG = 'F';
static const char DB_REINDEX_FLAG = 'R';
static const char DB_LAST_BLOCK = 'l';

static const char DB_MMR_LENGTH = 'M';
static const char DB_MMR_NODE = 'm';
static const char DB_MMR_ROOT = 'r';

static const char DB_SUBTREE_LATEST = 'e';
static const char DB_SUBTREE_DATA = 'n';

// insightexplorer
static const char DB_ADDRESSINDEX = 'd';
static const char DB_ADDRESSUNSPENTINDEX = 'u';
static const char DB_SPENTINDEX = 'p';
static const char DB_TIMESTAMPINDEX = 'T';
static const char DB_BLOCKHASHINDEX = 'h';

CCoinsViewDB::CCoinsViewDB(std::string dbName, size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / dbName, nCacheSize, fMemory, fWipe) {
}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe)
{
}

bool CCoinsViewDB::GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
    if (rt == SproutMerkleTree::empty_root()) {
        SproutMerkleTree new_tree;
        tree = new_tree;
        return true;
    }

    bool read = db.Read(make_pair(DB_SPROUT_ANCHOR, rt), tree);

    return read;
}

bool CCoinsViewDB::GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
    if (rt == SaplingMerkleTree::empty_root()) {
        SaplingMerkleTree new_tree;
        tree = new_tree;
        return true;
    }

    bool read = db.Read(make_pair(DB_SAPLING_ANCHOR, rt), tree);

    return read;
}

bool CCoinsViewDB::GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const {
    if (rt == OrchardMerkleFrontier::empty_root()) {
        OrchardMerkleFrontier new_tree;
        tree = new_tree;
        return true;
    }

    bool read = db.Read(make_pair(DB_ORCHARD_ANCHOR, rt), tree);

    return read;
}

bool CCoinsViewDB::GetNullifier(const uint256 &nf, ShieldedType type) const {
    bool spent = false;
    char dbChar;
    switch (type) {
        case SPROUT:
            dbChar = DB_NULLIFIER;
            break;
        case SAPLING:
            dbChar = DB_SAPLING_NULLIFIER;
            break;
        case ORCHARD:
            dbChar = DB_ORCHARD_NULLIFIER;
            break;
        default:
            throw runtime_error("Unknown shielded type");
    }
    return db.Read(make_pair(dbChar, nf), spent);
}

bool CCoinsViewDB::GetCoins(const uint256 &txid, CCoins &coins) const {
    return db.Read(make_pair(DB_COINS, txid), coins);
}

bool CCoinsViewDB::HaveCoins(const uint256 &txid) const {
    return db.Exists(make_pair(DB_COINS, txid));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

uint256 CCoinsViewDB::GetBestAnchor(ShieldedType type) const {
    uint256 hashBestAnchor;

    switch (type) {
        case SPROUT:
            if (!db.Read(DB_BEST_SPROUT_ANCHOR, hashBestAnchor))
                return SproutMerkleTree::empty_root();
            break;
        case SAPLING:
            if (!db.Read(DB_BEST_SAPLING_ANCHOR, hashBestAnchor))
                return SaplingMerkleTree::empty_root();
            break;
        case ORCHARD:
            if (!db.Read(DB_BEST_ORCHARD_ANCHOR, hashBestAnchor))
                return OrchardMerkleFrontier::empty_root();
            break;
        default:
            throw runtime_error("Unknown shielded type");
    }

    return hashBestAnchor;
}

HistoryIndex CCoinsViewDB::GetHistoryLength(uint32_t epochId) const {
    HistoryIndex historyLength;
    if (!db.Read(make_pair(DB_MMR_LENGTH, epochId), historyLength)) {
        // Starting new history
        historyLength = 0;
    }

    return historyLength;
}

HistoryNode CCoinsViewDB::GetHistoryAt(uint32_t epochId, HistoryIndex index) const {
    HistoryNode mmrNode = {};

    if (index >= GetHistoryLength(epochId)) {
        throw runtime_error("History data inconsistent - reindex?");
    }

    if (libzcash::IsV1HistoryTree(epochId)) {
        // History nodes serialized by `zcashd` versions that were unaware of NU5, used
        // the previous shorter maximum serialized length. Because we stored this as an
        // array, we can't just read the current (longer) maximum serialized length, as
        // it will result in an exception for those older nodes.
        //
        // Instead, we always read an array of the older length. This works as expected
        // for V1 nodes serialized by older clients, while for V1 nodes serialized by
        // NU5-aware clients this is guaranteed to ignore only trailing zero bytes.
        std::array<unsigned char, NODE_V1_SERIALIZED_LENGTH> tmpMmrNode;
        if (!db.Read(make_pair(DB_MMR_NODE, make_pair(epochId, index)), tmpMmrNode)) {
            throw runtime_error("History data inconsistent (expected node not found) - reindex?");
        }
        std::copy(std::begin(tmpMmrNode), std::end(tmpMmrNode), mmrNode.begin());
    } else {
        if (!db.Read(make_pair(DB_MMR_NODE, make_pair(epochId, index)), mmrNode)) {
            throw runtime_error("History data inconsistent (expected node not found) - reindex?");
        }
    }

    return mmrNode;
}

uint256 CCoinsViewDB::GetHistoryRoot(uint32_t epochId) const {
    uint256 root;
    if (!db.Read(make_pair(DB_MMR_ROOT, epochId), root))
    {
        root = uint256();
    }
    return root;
}


std::optional<libzcash::LatestSubtree> CCoinsViewDB::GetLatestSubtree(ShieldedType type) const {
    libzcash::LatestSubtree latestSubtree;
    if (!db.Read(make_pair(DB_SUBTREE_LATEST, (uint8_t) type), latestSubtree)) {
        return std::nullopt;
    }

    return latestSubtree;
}

std::optional<libzcash::SubtreeData> CCoinsViewDB::GetSubtreeData(
        ShieldedType type, libzcash::SubtreeIndex index) const
{
    libzcash::SubtreeData subtreeData;
    if (!db.Read(make_pair(DB_SUBTREE_DATA, make_pair((uint8_t) type, index)), subtreeData)) {
        return std::nullopt;
    }
    return subtreeData;
}

void BatchWriteNullifiers(CDBBatch& batch, CNullifiersMap& mapToUse, const char& dbChar)
{
    for (CNullifiersMap::iterator it = mapToUse.begin(); it != mapToUse.end();) {
        if (it->second.flags & CNullifiersCacheEntry::DIRTY) {
            if (!it->second.entered)
                batch.Erase(make_pair(dbChar, it->first));
            else
                batch.Write(make_pair(dbChar, it->first), true);
            // TODO: changed++? ... See comment in CCoinsViewDB::BatchWrite. If this is needed we could return an int
        }
        it = mapToUse.erase(it);
    }
}

template<typename Map, typename MapIterator, typename MapEntry, typename Tree>
void BatchWriteAnchors(CDBBatch& batch, Map& mapToUse, const char& dbChar)
{
    for (MapIterator it = mapToUse.begin(); it != mapToUse.end();) {
        if (it->second.flags & MapEntry::DIRTY) {
            if (!it->second.entered)
                batch.Erase(make_pair(dbChar, it->first));
            else {
                if (it->first != Tree::empty_root()) {
                    batch.Write(make_pair(dbChar, it->first), it->second.tree);
                }
            }
            // TODO: changed++?
        }
        it = mapToUse.erase(it);
    }
}

void BatchWriteHistory(CDBBatch& batch, CHistoryCacheMap& historyCacheMap) {
    for (auto nextHistoryCache = historyCacheMap.begin(); nextHistoryCache != historyCacheMap.end(); nextHistoryCache++) {
        auto historyCache = nextHistoryCache->second;
        auto epochId = nextHistoryCache->first;

        // delete old entries since updateDepth
        for (int i = historyCache.updateDepth + 1; i <= historyCache.length; i++) {
            batch.Erase(make_pair(DB_MMR_NODE, make_pair(epochId, i)));
        }

        // replace/append new/updated entries
        for (auto it = historyCache.appends.begin(); it != historyCache.appends.end(); it++) {
            batch.Write(make_pair(DB_MMR_NODE, make_pair(epochId, it->first)), it->second);
        }

        // write new length
        batch.Write(make_pair(DB_MMR_LENGTH, epochId), historyCache.length);

        // write current root
        batch.Write(make_pair(DB_MMR_ROOT, epochId), historyCache.root);
    }
}

void WriteSubtrees(
    CDBBatch& batch,
    ShieldedType type,
    std::optional<libzcash::LatestSubtree> oldLatestSubtree,
    std::optional<libzcash::LatestSubtree> newLatestSubtree,
    const std::vector<libzcash::SubtreeData> &newSubtrees
)
{
    // The number of subtrees we'll need to remove from the database
    libzcash::SubtreeIndex pops;

    if (!oldLatestSubtree.has_value()) {
        // Nothing to remove
        pops = 0;
        assert(!newLatestSubtree.has_value());
    } else {
        if (!newLatestSubtree.has_value()) {
            // Every subtree must be removed
            pops = oldLatestSubtree.value().index + 1;
        } else {
            // Only remove what's necessary to get us to the
            // correct index.
            assert(newLatestSubtree.value().index <= oldLatestSubtree.value().index);
            pops = oldLatestSubtree.value().index - newLatestSubtree.value().index;
        }
    }

    for (libzcash::SubtreeIndex i = 0; i < pops; i++) {
        batch.Erase(make_pair(DB_SUBTREE_DATA, make_pair((uint8_t) type, oldLatestSubtree.value().index - i)));
    }

    if (!newSubtrees.empty()) {
        libzcash::SubtreeIndex cursor_index;
        if (newLatestSubtree.has_value()) {
            cursor_index = newLatestSubtree.value().index + 1;
        } else {
            cursor_index = 0;
        }

        for (const libzcash::SubtreeData& subtreeData : newSubtrees) {
            batch.Write(make_pair(DB_SUBTREE_DATA, make_pair((uint8_t) type, cursor_index)), subtreeData);
            cursor_index += 1;
        }

        libzcash::SubtreeData latestSubtreeData = newSubtrees.back();

        // Note: the latest index will be cursor_index - 1 after the final iteration of the loop above.
        libzcash::LatestSubtree latestSubtree(cursor_index - 1, latestSubtreeData.root, latestSubtreeData.nHeight);
        batch.Write(make_pair(DB_SUBTREE_LATEST, (uint8_t) type), latestSubtree);
    } else {
        // There are no new subtrees from the cache, so newLatestSubtree is the (possibly new) best index.
        if (newLatestSubtree.has_value()) {
            batch.Write(make_pair(DB_SUBTREE_LATEST, (uint8_t) type), newLatestSubtree.value());
        } else {
            // Delete the entry if it's std::nullopt
            batch.Erase(make_pair(DB_SUBTREE_LATEST, (uint8_t) type));
        }
    }
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins,
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
                              CHistoryCacheMap &historyCacheMap,
                              SubtreeCache &cacheSaplingSubtrees,
                              SubtreeCache &cacheOrchardSubtrees) {
    auto latestSaplingSubtree = GetLatestSubtree(SAPLING);
    auto latestOrchardSubtree = GetLatestSubtree(ORCHARD);

    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            if (it->second.coins.IsPruned())
                batch.Erase(make_pair(DB_COINS, it->first));
            else
                batch.Write(make_pair(DB_COINS, it->first), it->second.coins);
            changed++;
        }
        count++;
        it = mapCoins.erase(it);
    }

    ::BatchWriteAnchors<CAnchorsSproutMap, CAnchorsSproutMap::iterator, CAnchorsSproutCacheEntry, SproutMerkleTree>(batch, mapSproutAnchors, DB_SPROUT_ANCHOR);
    ::BatchWriteAnchors<CAnchorsSaplingMap, CAnchorsSaplingMap::iterator, CAnchorsSaplingCacheEntry, SaplingMerkleTree>(batch, mapSaplingAnchors, DB_SAPLING_ANCHOR);
    ::BatchWriteAnchors<CAnchorsOrchardMap, CAnchorsOrchardMap::iterator, CAnchorsOrchardCacheEntry, OrchardMerkleFrontier>(batch, mapOrchardAnchors, DB_ORCHARD_ANCHOR);

    ::BatchWriteNullifiers(batch, mapSproutNullifiers, DB_NULLIFIER);
    ::BatchWriteNullifiers(batch, mapSaplingNullifiers, DB_SAPLING_NULLIFIER);
    ::BatchWriteNullifiers(batch, mapOrchardNullifiers, DB_ORCHARD_NULLIFIER);

    ::BatchWriteHistory(batch, historyCacheMap);

    assert(cacheSaplingSubtrees.initialized);
    assert(cacheOrchardSubtrees.initialized);

    WriteSubtrees(batch, SAPLING, latestSaplingSubtree, cacheSaplingSubtrees.parentLatestSubtree, cacheSaplingSubtrees.newSubtrees);
    WriteSubtrees(batch, ORCHARD, latestOrchardSubtree, cacheOrchardSubtrees.parentLatestSubtree, cacheOrchardSubtrees.newSubtrees);

    if (!hashBlock.IsNull())
        batch.Write(DB_BEST_BLOCK, hashBlock);
    if (!hashSproutAnchor.IsNull())
        batch.Write(DB_BEST_SPROUT_ANCHOR, hashSproutAnchor);
    if (!hashSaplingAnchor.IsNull())
        batch.Write(DB_BEST_SAPLING_ANCHOR, hashSaplingAnchor);
    if (!hashOrchardAnchor.IsNull())
        batch.Write(DB_BEST_ORCHARD_ANCHOR, hashOrchardAnchor);

    LogPrint("coindb", "Committing %u changed transactions (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return db.WriteBatch(batch);
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) const {
    return Read(make_pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write(DB_REINDEX_FLAG, '1');
    else
        return Erase(DB_REINDEX_FLAG);
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) const {
    fReindexing = Exists(DB_REINDEX_FLAG);
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) const {
    return Read(DB_LAST_BLOCK, nFile);
}

bool CCoinsViewDB::GetStats(CCoinsStats &stats) const {
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CDBWrapper*>(&db)->NewIterator());
    pcursor->Seek(DB_COINS);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    stats.hashBlock = GetBestBlock();
    ss << stats.hashBlock;
    CAmount nTotalAmount = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        CCoins coins;
        if (pcursor->GetKey(key) && key.first == DB_COINS) {
            if (pcursor->GetValue(coins)) {
                stats.nTransactions++;
                for (unsigned int i=0; i<coins.vout.size(); i++) {
                    const CTxOut &out = coins.vout[i];
                    if (!out.IsNull()) {
                        stats.nTransactionOutputs++;
                        ss << VARINT(i+1);
                        ss << out;
                        nTotalAmount += out.nValue;
                    }
                }
                stats.nSerializedSize += 32 + pcursor->GetValueSize();
                ss << VARINT(0);
            } else {
                return error("CCoinsViewDB::GetStats() : unable to read value");
            }
        } else {
            break;
        }
        pcursor->Next();
    }
    {
        LOCK(cs_main);
        stats.nHeight = mapBlockIndex.find(stats.hashBlock)->second->nHeight;
    }
    stats.hashSerialized = ss.GetHash();
    stats.nTotalAmount = nTotalAmount;
    return true;
}

bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<CBlockIndex*>& blockinfo) {
    MetricsIncrementCounter("zcashd.debug.blocktree.write_batch");
    CDBBatch batch(*this);
    for (const auto& it : fileInfo) {
        batch.Write(make_pair(DB_BLOCK_FILES, it.first), *it.second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (const auto& it : blockinfo) {
        std::pair<char, uint256> key = make_pair(DB_BLOCK_INDEX, it->GetBlockHash());
        try {
            CDiskBlockIndex dbindex {it, [this, &key]() {
                MetricsIncrementCounter("zcashd.debug.blocktree.write_batch_read_dbindex");
                // It can happen that the index entry is written, then the Equihash solution is cleared from memory,
                // then the index entry is rewritten. In that case we must read the solution from the old entry.
                CDiskBlockIndex dbindex_old;
                if (!Read(key, dbindex_old)) {
                    LogPrintf("%s: Failed to read index entry", __func__);
                    throw runtime_error("Failed to read index entry");
                }
                return dbindex_old.GetSolution();
            }};
            batch.Write(key, dbindex);
        } catch (const runtime_error&) {
            return false;
        }
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::EraseBatchSync(const std::vector<const CBlockIndex*>& blockinfo) {
    CDBBatch batch(*this);
    for (std::vector<const CBlockIndex*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
        batch.Erase(make_pair(DB_BLOCK_INDEX, (*it)->GetBlockHash()));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::ReadDiskBlockIndex(const uint256 &blockhash, CDiskBlockIndex &dbindex) const {
    return Read(make_pair(DB_BLOCK_INDEX, blockhash), dbindex);
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) const {
    return Read(make_pair(DB_TXINDEX, txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(make_pair(DB_TXINDEX, it->first), it->second);
    return WriteBatch(batch);
}

// START insightexplorer
// https://github.com/bitpay/bitcoin/commit/017f548ea6d89423ef568117447e61dd5707ec42#diff-81e4f16a1b5d5b7ca25351a63d07cb80R183
bool CBlockTreeDB::UpdateAddressUnspentIndex(const std::vector<CAddressUnspentDbEntry> &vect)
{
    CDBBatch batch(*this);
    for (std::vector<CAddressUnspentDbEntry>::const_iterator it=vect.begin(); it!=vect.end(); it++) {
        if (it->second.IsNull()) {
            batch.Erase(make_pair(DB_ADDRESSUNSPENTINDEX, it->first));
        } else {
            batch.Write(make_pair(DB_ADDRESSUNSPENTINDEX, it->first), it->second);
        }
    }
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadAddressUnspentIndex(uint160 addressHash, int type, std::vector<CAddressUnspentDbEntry> &unspentOutputs)
{
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_ADDRESSUNSPENTINDEX, CAddressIndexIteratorKey(type, addressHash)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressUnspentKey> key;
        if (!(pcursor->GetKey(key) && key.first == DB_ADDRESSUNSPENTINDEX && key.second.hashBytes == addressHash))
            break;
        CAddressUnspentValue nValue;
        if (!pcursor->GetValue(nValue))
            return error("failed to get address unspent value");
        unspentOutputs.push_back(make_pair(key.second, nValue));
        pcursor->Next();
    }
    return true;
}

bool CBlockTreeDB::WriteAddressIndex(const std::vector<CAddressIndexDbEntry> &vect) {
    CDBBatch batch(*this);
    for (std::vector<CAddressIndexDbEntry>::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(make_pair(DB_ADDRESSINDEX, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::EraseAddressIndex(const std::vector<CAddressIndexDbEntry> &vect) {
    CDBBatch batch(*this);
    for (std::vector<CAddressIndexDbEntry>::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Erase(make_pair(DB_ADDRESSINDEX, it->first));
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadAddressIndex(
        uint160 addressHash, int type,
        std::vector<CAddressIndexDbEntry> &addressIndex,
        int start, int end)
{
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    if (start > 0 && end > 0) {
        pcursor->Seek(make_pair(DB_ADDRESSINDEX, CAddressIndexIteratorHeightKey(type, addressHash, start)));
    } else {
        pcursor->Seek(make_pair(DB_ADDRESSINDEX, CAddressIndexIteratorKey(type, addressHash)));
    }

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressIndexKey> key;
        if (!(pcursor->GetKey(key) && key.first == DB_ADDRESSINDEX && key.second.hashBytes == addressHash))
            break;
        if (end > 0 && key.second.blockHeight > end)
            break;
        CAmount nValue;
        if (!pcursor->GetValue(nValue))
            return error("failed to get address index value");
        addressIndex.push_back(make_pair(key.second, nValue));
        pcursor->Next();
    }
    return true;
}

bool CBlockTreeDB::ReadSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value) const {
    return Read(make_pair(DB_SPENTINDEX, key), value);
}

bool CBlockTreeDB::UpdateSpentIndex(const std::vector<CSpentIndexDbEntry> &vect) {
    CDBBatch batch(*this);
    for (std::vector<CSpentIndexDbEntry>::const_iterator it=vect.begin(); it!=vect.end(); it++) {
        if (it->second.IsNull()) {
            batch.Erase(make_pair(DB_SPENTINDEX, it->first));
        } else {
            batch.Write(make_pair(DB_SPENTINDEX, it->first), it->second);
        }
    }
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteTimestampIndex(const CTimestampIndexKey &timestampIndex) {
    CDBBatch batch(*this);
    batch.Write(make_pair(DB_TIMESTAMPINDEX, timestampIndex), 0);
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadTimestampIndex(unsigned int high, unsigned int low,
    const bool fActiveOnly, std::vector<std::pair<uint256, unsigned int> > &hashes)
{
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_TIMESTAMPINDEX, CTimestampIndexIteratorKey(low)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, CTimestampIndexKey> key;
        if (!(pcursor->GetKey(key) && key.first == DB_TIMESTAMPINDEX && key.second.timestamp < high)) {
            break;
        }
        if (fActiveOnly) {
            CBlockIndex* pblockindex = mapBlockIndex[key.second.blockHash];
            if (chainActive.Contains(pblockindex)) {
                hashes.push_back(std::make_pair(key.second.blockHash, key.second.timestamp));
            }
        } else {
            hashes.push_back(std::make_pair(key.second.blockHash, key.second.timestamp));
        }
        pcursor->Next();
    }
    return true;
}

bool CBlockTreeDB::WriteTimestampBlockIndex(const CTimestampBlockIndexKey &blockhashIndex,
    const CTimestampBlockIndexValue &logicalts)
{
    CDBBatch batch(*this);
    batch.Write(make_pair(DB_BLOCKHASHINDEX, blockhashIndex), logicalts);
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadTimestampBlockIndex(const uint256 &hash, unsigned int &ltimestamp) const
{
    CTimestampBlockIndexValue(lts);
    if (!Read(std::make_pair(DB_BLOCKHASHINDEX, hash), lts))
        return false;

    ltimestamp = lts.ltimestamp;
    return true;
}
// END insightexplorer

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair(DB_FLAG, name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) const {
    char ch;
    if (!Read(std::make_pair(DB_FLAG, name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts(
    std::function<CBlockIndex*(const uint256&)> insertBlockIndex,
    const CChainParams& chainParams)
{
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_BLOCK_INDEX, uint256()));

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        if (ShutdownRequested()) break;
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_INDEX) {
            CDiskBlockIndex diskindex;
            if (pcursor->GetValue(diskindex)) {
                // Construct block index object
                CBlockIndex* pindexNew = insertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev          = insertBlockIndex(diskindex.hashPrev);
                pindexNew->nHeight        = diskindex.nHeight;
                pindexNew->nFile          = diskindex.nFile;
                pindexNew->nDataPos       = diskindex.nDataPos;
                pindexNew->nUndoPos       = diskindex.nUndoPos;
                pindexNew->hashSproutAnchor     = diskindex.hashSproutAnchor;
                pindexNew->nVersion       = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->hashBlockCommitments  = diskindex.hashBlockCommitments;
                pindexNew->nTime          = diskindex.nTime;
                pindexNew->nBits          = diskindex.nBits;
                pindexNew->nNonce         = diskindex.nNonce;
                // the Equihash solution will be loaded lazily from the dbindex entry
                pindexNew->nStatus        = diskindex.nStatus;
                pindexNew->nCachedBranchId = diskindex.nCachedBranchId;
                pindexNew->nTx            = diskindex.nTx;
                pindexNew->nChainSupplyDelta = diskindex.nChainSupplyDelta;
                pindexNew->nTransparentValue = diskindex.nTransparentValue;
                pindexNew->nLockboxValue = diskindex.nLockboxValue;
                pindexNew->nSproutValue   = diskindex.nSproutValue;
                pindexNew->nSaplingValue  = diskindex.nSaplingValue;
                pindexNew->nOrchardValue  = diskindex.nOrchardValue;
                pindexNew->hashFinalSaplingRoot = diskindex.hashFinalSaplingRoot;
                pindexNew->hashFinalOrchardRoot = diskindex.hashFinalOrchardRoot;
                pindexNew->hashChainHistoryRoot = diskindex.hashChainHistoryRoot;
                pindexNew->hashAuthDataRoot = diskindex.hashAuthDataRoot;

                // Check the block hash against the required difficulty as encoded in the
                // nBits field. The probability of this succeeding randomly is low enough
                // that it is a useful check to detect logic or disk storage errors.
                if (!CheckProofOfWork(pindexNew->GetBlockHash(), pindexNew->nBits, Params().GetConsensus()))
                    return error("LoadBlockIndex(): CheckProofOfWork failed: %s", pindexNew->ToString());

                // ZIP 221 consistency checks
                // These checks should only be performed for block index entries marked
                // as consensus-valid (at the time they were written).
                //
                if (pindexNew->IsValid(BLOCK_VALID_CONSENSUS)) {
                    // We assume block index entries on disk that are not at least
                    // CHAIN_HISTORY_ROOT_VERSION were created by nodes that were
                    // not Heartwood aware. Such a node would not see Heartwood block
                    // headers as valid, and so this must *either* be an index entry
                    // for a block header on a non-Heartwood chain, or be marked as
                    // consensus-invalid.
                    //
                    // It can also happen that the block index entry was written
                    // by this node when it was Heartwood-aware (so its version
                    // will be >= CHAIN_HISTORY_ROOT_VERSION), but received from
                    // a non-upgraded peer. However that case the entry will be
                    // marked as consensus-invalid.
                    //
                    if (diskindex.nClientVersion >= NU5_DATA_VERSION &&
                        chainParams.GetConsensus().NetworkUpgradeActive(pindexNew->nHeight, Consensus::UPGRADE_NU5)) {
                        // From NU5 onwards we don't enforce a consistency check, because
                        // after ZIP 244, hashBlockCommitments will not match any stored
                        // commitment.
                    } else if (diskindex.nClientVersion >= CHAIN_HISTORY_ROOT_VERSION &&
                        chainParams.GetConsensus().NetworkUpgradeActive(pindexNew->nHeight, Consensus::UPGRADE_HEARTWOOD)) {
                        if (pindexNew->hashBlockCommitments != pindexNew->hashChainHistoryRoot) {
                            return error(
                                "LoadBlockIndex(): block index inconsistency detected (post-Heartwood; hashBlockCommitments %s != hashChainHistoryRoot %s): %s",
                                pindexNew->hashBlockCommitments.ToString(), pindexNew->hashChainHistoryRoot.ToString(), pindexNew->ToString());
                        }
                    } else {
                        if (pindexNew->hashBlockCommitments != pindexNew->hashFinalSaplingRoot) {
                            return error(
                                "LoadBlockIndex(): block index inconsistency detected (pre-Heartwood; hashBlockCommitments %s != hashFinalSaplingRoot %s): %s",
                                pindexNew->hashBlockCommitments.ToString(), pindexNew->hashFinalSaplingRoot.ToString(), pindexNew->ToString());
                        }
                    }
                }

                pcursor->Next();
            } else {
                return error("LoadBlockIndex() : failed to read value");
            }
        } else {
            break;
        }
    }

    return true;
}
