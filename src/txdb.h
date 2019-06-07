// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "coins.h"
#include "dbwrapper.h"

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <univalue.h>

class CBlockFileInfo;
class CBlockIndex;
struct CDiskTxPos;
struct CAddressUnspentKey;
struct CAddressUnspentValue;
struct CAddressIndexKey;
struct CAddressIndexIteratorKey;
struct CAddressIndexIteratorHeightKey;
struct CTimestampIndexKey;
struct CTimestampIndexIteratorKey;
struct CTimestampBlockIndexKey;
struct CTimestampBlockIndexValue;
struct CSpentIndexKey;
struct CSpentIndexValue;
class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 450;
//! max. -dbcache (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache in (MiB)
static const int64_t nMinDbCache = 4;

/** CCoinsView backed by the coin database (chainstate/) */
class CCoinsViewDB : public CCoinsView
{
protected:
    CDBWrapper db;
    CCoinsViewDB(std::string dbName, size_t nCacheSize, bool fMemory = false, bool fWipe = false);
public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const;
    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const;
    bool GetNullifier(const uint256 &nf, ShieldedType type) const;
    bool GetCoins(const uint256 &txid, CCoins &coins) const;
    bool HaveCoins(const uint256 &txid) const;
    uint256 GetBestBlock() const;
    uint256 GetBestAnchor(ShieldedType type) const;
    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap &mapSaplingNullifiers);
    bool GetStats(CCoinsStats &stats) const;
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CDBWrapper
{
public:
    CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false, bool compression = true, int maxOpenFiles = 1000);
private:
    CBlockTreeDB(const CBlockTreeDB&);
    void operator=(const CBlockTreeDB&);
public:
    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool EraseBatchSync(const std::vector<const CBlockIndex*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &fileinfo);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReindexing(bool fReindex);
    bool ReadReindexing(bool &fReindex);
    bool ReadTxIndex(const uint256 &txid, CDiskTxPos &pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> > &list);
    bool ReadSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value);
    bool UpdateSpentIndex(const std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> >&vect);
    bool UpdateAddressUnspentIndex(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect);
    bool ReadAddressUnspentIndex(uint160 addressHash, int type,
                                 std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &vect);
    bool WriteAddressIndex(const std::vector<std::pair<CAddressIndexKey, CAmount> > &vect);
    bool EraseAddressIndex(const std::vector<std::pair<CAddressIndexKey, CAmount> > &vect);
    bool ReadAddressIndex(uint160 addressHash, int type,
                          std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,
                          int start = 0, int end = 0);
    bool WriteTimestampIndex(const CTimestampIndexKey &timestampIndex);
    bool ReadTimestampIndex(const unsigned int &high, const unsigned int &low, const bool fActiveOnly, std::vector<std::pair<uint256, unsigned int> > &vect);
    bool WriteTimestampBlockIndex(const CTimestampBlockIndexKey &blockhashIndex, const CTimestampBlockIndexValue &logicalts);
    bool ReadTimestampBlockIndex(const uint256 &hash, unsigned int &logicalTS);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts();
    bool blockOnchainActive(const uint256 &hash);
    UniValue Snapshot(int top);
    bool Snapshot2(std::map <std::string, CAmount> &addressAmounts, UniValue *ret);
};

#endif // BITCOIN_TXDB_H
