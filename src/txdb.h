// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "coins.h"
#include "dbwrapper.h"
#include "chain.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/function.hpp>
#include "zcash/History.hpp"

class CBlockIndex;

// START insightexplorer
struct CAddressUnspentKey;
struct CAddressUnspentValue;
struct CAddressIndexKey;
struct CAddressIndexIteratorKey;
struct CAddressIndexIteratorHeightKey;
struct CSpentIndexKey;
struct CSpentIndexValue;
struct CTimestampIndexKey;
struct CTimestampIndexIteratorKey;
struct CTimestampBlockIndexKey;
struct CTimestampBlockIndexValue;

typedef std::pair<CAddressUnspentKey, CAddressUnspentValue> CAddressUnspentDbEntry;
typedef std::pair<CAddressIndexKey, CAmount> CAddressIndexDbEntry;
typedef std::pair<CSpentIndexKey, CSpentIndexValue> CSpentIndexDbEntry;
// END insightexplorer

class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 450;
//! max. -dbcache (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache in (MiB)
static const int64_t nMinDbCache = 4;

struct CDiskTxPos : public CDiskBlockPos
{
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CDiskBlockPos*)this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos &blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {
    }

    CDiskTxPos() {
        SetNull();
    }

    void SetNull() {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};

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
    bool GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleTree &tree) const;
    bool GetNullifier(const uint256 &nf, ShieldedType type) const;
    bool GetCoins(const uint256 &txid, CCoins &coins) const;
    bool HaveCoins(const uint256 &txid) const;
    uint256 GetBestBlock() const;
    uint256 GetBestAnchor(ShieldedType type) const;
    HistoryIndex GetHistoryLength(uint32_t epochId) const;
    HistoryNode GetHistoryAt(uint32_t epochId, HistoryIndex index) const;
    uint256 GetHistoryRoot(uint32_t epochId) const;
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

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CDBWrapper
{
public:
    CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
private:
    CBlockTreeDB(const CBlockTreeDB&);
    void operator=(const CBlockTreeDB&);
public:
    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool EraseBatchSync(const std::vector<const CBlockIndex*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &info);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReindexing(bool fReindexing);
    bool ReadReindexing(bool &fReindexing);
    bool ReadTxIndex(const uint256 &txid, CDiskTxPos &pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> > &vect);

    // START insightexplorer
    bool UpdateAddressUnspentIndex(const std::vector<CAddressUnspentDbEntry> &vect);
    bool ReadAddressUnspentIndex(uint160 addressHash, int type, std::vector<CAddressUnspentDbEntry> &vect);
    bool WriteAddressIndex(const std::vector<CAddressIndexDbEntry> &vect);
    bool EraseAddressIndex(const std::vector<CAddressIndexDbEntry> &vect);
    bool ReadAddressIndex(uint160 addressHash, int type, std::vector<CAddressIndexDbEntry> &addressIndex, int start = 0, int end = 0);
    bool ReadSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value);
    bool UpdateSpentIndex(const std::vector<CSpentIndexDbEntry> &vect);
    bool WriteTimestampIndex(const CTimestampIndexKey &timestampIndex);
    bool ReadTimestampIndex(unsigned int high, unsigned int low,
            const bool fActiveOnly, std::vector<std::pair<uint256, unsigned int> > &vect);
    bool WriteTimestampBlockIndex(const CTimestampBlockIndexKey &blockhashIndex,
            const CTimestampBlockIndexValue &logicalts);
    bool ReadTimestampBlockIndex(const uint256 &hash, unsigned int &logicalTS);
    // END insightexplorer

    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts(
        std::function<CBlockIndex*(const uint256&)> insertBlockIndex,
        const CChainParams& chainParams);
};

/*
 * TXDB serialization for ccoins (without tzeout)
 *
 * Serialized format:
 * - VARINT(nVersion)
 * - VARINT(nCode)
 * - unspentness bitvector, for vout[2] and further; least significant byte first
 * - the non-spent CTxOuts (via CTxOutCompressor)
 * - VARINT(nTzeCode)
 * - unspentness bitvector, for vtzeout[2] and further; least significant byte first
 * - the non-spent CTzeOuts (via CTzeOutCompressor)
 * - VARINT(nHeight)
 *
 * The nCode value consists of:
 * - bit 1: IsCoinBase()
 * - bit 2: vout[0] is not spent
 * - bit 4: vout[1] is not spent
 * - The higher bits encode N, the number of non-zero bytes in the following bitvector.
 *   - In case both bit 2 and bit 4 are unset, they encode N-1, as there must be at
 *     least one non-spent output).
 *
 * Example: 0104835800816115944e077fe7c803cfa57f29b36bf87c1d358bb85e
 *          <><><--------------------------------------------><---->
 *          |  \                  |                             /
 *    version   code             vout[1]                  height
 *
 *    - version = 1
 *    - code = 4 (vout[1] is not spent, and 0 non-zero bytes of bitvector follow)
 *    - unspentness bitvector: as 0 non-zero bytes follow, it has length 0
 *    - vout[1]: 835800816115944e077fe7c803cfa57f29b36bf87c1d35
 *               * 8358: compact amount representation for 60000000000 (600 BTC)
 *               * 00: special txout type pay-to-pubkey-hash
 *               * 816115944e077fe7c803cfa57f29b36bf87c1d35: address uint160
 *    - height = 203998
 *
 *
 * Example: 0109044086ef97d5790061b01caab50f1b8e9c50a5057eb43c2d9563a4eebbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa486af3b
 *          <><><--><--------------------------------------------------><----------------------------------------------><---->
 *         /  \   \                     |                                                           |                     /
 *  version  code  unspentness       vout[4]                                                     vout[16]           height
 *
 *  - version = 1
 *  - code = 9 (coinbase, neither vout[0] or vout[1] are unspent,
 *                2 (1, +1 because both bit 2 and bit 4 are unset) non-zero bitvector bytes follow)
 *  - unspentness bitvector: bits 2 (0x04) and 14 (0x4000) are set, so vout[2+2] and vout[14+2] are unspent
 *  - vout[4]: 86ef97d5790061b01caab50f1b8e9c50a5057eb43c2d9563a4ee
 *             * 86ef97d579: compact amount representation for 234925952 (2.35 BTC)
 *             * 00: special txout type pay-to-pubkey-hash
 *             * 61b01caab50f1b8e9c50a5057eb43c2d9563a4ee: address uint160
 *  - vout[16]: bbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa4
 *              * bbd123: compact amount representation for 110397 (0.001 BTC)
 *              * 00: special txout type pay-to-pubkey-hash
 *              * 8c988f1a4a4de2161e0f50aac7f17e7f9555caa4: address uint160
 *  - height = 120891
 */
class CCoinsSer
{
private:
    /**
     * Calculate number of bytes for the bitmask, and its number of non-zero bytes
     * each bit in the bitmask represents the availability of one output, but the
     * availabilities of the first two outputs are encoded separately.
     */
    template<typename Elem, typename Pred>
    static void CalcMaskSize(const std::vector<Elem>& v, const Pred isSpent, unsigned int &nBytes, unsigned int &nNonzeroBytes) {
        unsigned int nLastUsedByte = 0;
        for (unsigned int b = 0; 2+b*8 < v.size(); b++) {
            // for each vout/vtzeout entry beyond the
            bool fZero = true;
            for (unsigned int i = 0; i < 8 && 2+b*8+i < v.size(); i++) {
                if (!isSpent(v[2+b*8+i])) {
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

    template<typename Elem, typename Pred>
    static unsigned int SerCode(const std::vector<Elem>& v, const Pred isSpent, bool fCoinBase, unsigned int nMaskCode) {
        bool fFirst = v.size() > 0 && !isSpent(v[0]);
        bool fSecond = v.size() > 1 && !isSpent(v[1]);
        assert(fFirst || fSecond || nMaskCode);

        return 8*(nMaskCode - (fFirst || fSecond ? 0 : 1)) +
               (fCoinBase ? 1 : 0) +
               (fFirst ? 2 : 0) +
               (fSecond ? 4 : 0);
    }

    template<typename Stream, typename Elem, typename Pred>
    static void WriteMask(Stream& s, const std::vector<Elem>& v, const Pred isSpent, const int nMaskSize) {
        for (unsigned int b = 0; b<nMaskSize; b++) {
            // Serialize a series of bytes, each of which indicates the
            // spent-ness of 8 elements of the vout/vtzeout vector. The
            // first two elements of the vector are treated specially and ignored.
            unsigned char chAvail = 0;
            for (unsigned int i = 0; i < 8 && 2+b*8+i < v.size(); i++)
                if (!isSpent(v[2+b*8+i]))
                    chAvail |= (1 << i);
            ::Serialize(s, chAvail);
        }
    }

    template<typename Stream>
    static std::vector<bool> ReadMask(Stream& s, const unsigned int nCode) {
        std::vector<bool> vAvail(2, false);
        vAvail[0] = (nCode & 2) != 0;
        vAvail[1] = (nCode & 4) != 0;
        unsigned int nMaskCode = (nCode / 8) + ((nCode & 6) != 0 ? 0 : 1);

        // spentness bitmask
        while (nMaskCode > 0) {
            unsigned char chAvail = 0;
            ::Unserialize(s, chAvail);
            for (unsigned int p = 0; p < 8; p++) {
                bool f = (chAvail & (1 << p)) != 0;
                vAvail.push_back(f);
            }
            if (chAvail != 0)
                nMaskCode--;
        }

        return vAvail;
    }

public:
    CCoins& coins;

    CCoinsSer(CCoins& coinsIn): coins(coinsIn) { }

    template<typename Stream>
    void Serialize(Stream &s) const {
        auto txoutSpent = [](const CTxOut& elem) { return elem.IsNull(); };

        unsigned int nMaskSize = 0, nMaskCode = 0;
        CalcMaskSize(coins.vout, txoutSpent, nMaskSize, nMaskCode);
        unsigned int nCode = SerCode(coins.vout, txoutSpent, coins.fCoinBase, nMaskSize);

        // version
        ::Serialize(s, VARINT(coins.nVersion));
        // header code
        ::Serialize(s, VARINT(nCode));
        // spentness bitmask
        WriteMask(s, coins.vout, txoutSpent, nMaskSize);
        // txouts themself
        for (unsigned int i = 0; i < coins.vout.size(); i++) {
            // only unspent txos are written out
            if (!coins.vout[i].IsNull())
                ::Serialize(s, CTxOutCompressor(REF(coins.vout[i])));
        }
        // tzeout
        if (coins.nVersion >= ZFUTURE_TX_VERSION) {
            auto tzeoutSpent = [](const CCoins::TzeOutCoin& elem) { return elem.second == SPENT; };

            unsigned int nTzeMaskSize = 0, nTzeMaskCode = 0;
            CalcMaskSize(coins.vtzeout, tzeoutSpent, nTzeMaskSize, nTzeMaskCode);
            unsigned int nTzeCode = SerCode(coins.vtzeout, tzeoutSpent, coins.fCoinBase, nTzeMaskSize);

            // write header code, mask, then the actual unspent coins
            ::Serialize(s, VARINT(nTzeCode));
            WriteMask(s, coins.vtzeout, tzeoutSpent, nTzeMaskSize);
            for (unsigned int i = 0; i < coins.vtzeout.size(); i++) {
                if (coins.vtzeout[i].second == UNSPENT)
                    ::Serialize(s, CTzeOutCompressor(REF(coins.vtzeout[i].first)));
            }
        }
        // coinbase height
        ::Serialize(s, VARINT(coins.nHeight));
    }

    template<typename Stream>
    void Unserialize(Stream &s) {
        // version
        ::Unserialize(s, VARINT(coins.nVersion));

        // header code
        unsigned int nCode = 0;
        ::Unserialize(s, VARINT(nCode));
        coins.fCoinBase = nCode & 1;

        auto vAvail = ReadMask(s, nCode);
        // txouts themself
        coins.vout.assign(vAvail.size(), CTxOut());
        for (unsigned int i = 0; i < vAvail.size(); i++) {
            if (vAvail[i])
                ::Unserialize(s, REF(CTxOutCompressor(coins.vout[i])));
        }
        // tzeout
        if (coins.nVersion >= ZFUTURE_TX_VERSION) {
            unsigned int nTzeCode = 0;
            ::Unserialize(s, VARINT(nTzeCode));

            auto vTzeAvail = ReadMask(s, nTzeCode);

            coins.vtzeout.assign(vAvail.size(), std::make_pair(CTzeOut(), SPENT));
            for (unsigned int i = 0; i < vAvail.size(); i++) {
                if (vAvail[i]) {
                    ::Unserialize(s, REF(CTzeOutCompressor(coins.vtzeout[i].first)));
                    coins.vtzeout[i].second = UNSPENT;
                }
            }
        }
        // coinbase height
        ::Unserialize(s, VARINT(coins.nHeight));
        coins.Cleanup();
    }
};

#endif // BITCOIN_TXDB_H
