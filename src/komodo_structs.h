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

#include "komodo_defs.h"

#include "uthash.h"
#include "utlist.h"

/*#ifdef _WIN32
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif*/

#ifndef KOMODO_STRUCTS_H
#define KOMODO_STRUCTS_H

#define GENESIS_NBITS 0x1f00ffff
#define KOMODO_MINRATIFY ((height < 90000) ? 7 : 11)
#define KOMODO_NOTARIES_HARDCODED 180000 // DONT CHANGE
#define KOMODO_MAXBLOCKS 250000 // DONT CHANGE

#define KOMODO_EVENT_RATIFY 'P'
#define KOMODO_EVENT_NOTARIZED 'N'
#define KOMODO_EVENT_KMDHEIGHT 'K'
#define KOMODO_EVENT_REWIND 'B'
#define KOMODO_EVENT_PRICEFEED 'V'
#define KOMODO_EVENT_OPRETURN 'R'
#define KOMODO_OPRETURN_DEPOSIT 'D'
#define KOMODO_OPRETURN_ISSUED 'I' // assetchain
#define KOMODO_OPRETURN_WITHDRAW 'W' // assetchain
#define KOMODO_OPRETURN_REDEEMED 'X'

#define KOMODO_KVPROTECTED 1
#define KOMODO_KVBINARY 2
#define KOMODO_KVDURATION 1440
#define KOMODO_ASSETCHAIN_MAXLEN 65

#include "bits256.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <set>

struct komodo_kv { UT_hash_handle hh; bits256 pubkey; uint8_t *key,*value; int32_t height; uint32_t flags; uint16_t keylen,valuesize; };

struct komodo_event_notarized { uint256 blockhash,desttxid,MoM; int32_t notarizedheight,MoMdepth; char dest[16]; };
struct komodo_event_pubkeys { uint8_t num; uint8_t pubkeys[64][33]; };
struct komodo_event_opreturn { uint256 txid; uint64_t value; uint16_t vout,oplen; uint8_t opret[]; };
struct komodo_event_pricefeed { uint8_t num; uint32_t prices[35]; };

struct komodo_event
{
    struct komodo_event *related;
    uint16_t len;
    int32_t height;
    uint8_t type,reorged;
    char symbol[KOMODO_ASSETCHAIN_MAXLEN];
    uint8_t space[];
};

struct pax_transaction
{
    UT_hash_handle hh;
    uint256 txid;
    uint64_t komodoshis,fiatoshis,validated;
    int32_t marked,height,otherheight,approved,didstats,ready;
    uint16_t vout;
    char symbol[KOMODO_ASSETCHAIN_MAXLEN],source[KOMODO_ASSETCHAIN_MAXLEN],coinaddr[64]; uint8_t rmd160[20],type,buf[35];
};

struct knotary_entry { UT_hash_handle hh; uint8_t pubkey[33],notaryid; };
struct knotaries_entry { int32_t height,numnotaries; struct knotary_entry *Notaries; };

struct notarized_checkpoint
{
    uint256 notarized_hash;
    uint256 notarized_desttxid;
    uint256 MoM;
    uint256 MoMoM;
    int32_t nHeight;
    int32_t notarized_height;
    int32_t MoMdepth;
    int32_t MoMoMdepth;
    int32_t MoMoMoffset;
    int32_t kmdstarti;
    int32_t kmdendi;
    bool operator==(const notarized_checkpoint& in) const
    {
        if (in.kmdendi == kmdendi
                && in.kmdstarti == kmdstarti
                && in.MoM == MoM
                && in.MoMdepth == MoMdepth
                && in.MoMoM == MoMoM
                && in.MoMoMdepth == MoMoMdepth
                && in.MoMoMoffset == MoMoMoffset
                && in.nHeight == nHeight
                && in.notarized_desttxid == notarized_desttxid
                && in.notarized_hash == notarized_hash
                && in.notarized_height == notarized_height )
            return true;
        return false;
    }
};

typedef boost::multi_index::multi_index_container<
        notarized_checkpoint,
        boost::multi_index::indexed_by<
                boost::multi_index::sequenced<>, // sorted by insertion order
                boost::multi_index::ordered_non_unique<
                        boost::multi_index::member<
                                notarized_checkpoint, int32_t, &notarized_checkpoint::nHeight
                        >
                >, // sorted by nHeight
                boost::multi_index::ordered_non_unique<
                        boost::multi_index::member<
                                notarized_checkpoint, int32_t, &notarized_checkpoint::notarized_height
                        >
                > // sorted by notarized_height
        > > notarized_checkpoint_container;
                            
struct komodo_ccdataMoM
{
    uint256 MoM;
    int32_t MoMdepth,notarized_height,height,txi;
};

struct komodo_ccdata_entry { uint256 MoM; int32_t notarized_height,kmdheight,txi; char symbol[65]; };
struct komodo_ccdatapair { int32_t notarized_height,MoMoMoffset; };

struct komodo_ccdataMoMoM
{
    uint256 MoMoM;
    int32_t kmdstarti,kmdendi,MoMoMoffset,MoMoMdepth,numpairs,len;
    struct komodo_ccdatapair *pairs;
};

struct komodo_ccdata
{
    struct komodo_ccdata *next,*prev;
    struct komodo_ccdataMoM MoMdata;
    uint32_t CCid,len;
    char symbol[65];
};

class komodo_state
{
public:
    int32_t SAVEDHEIGHT;
    int32_t CURRENT_HEIGHT;
    uint32_t SAVEDTIMESTAMP;
    uint64_t deposited;
    uint64_t issued;
    uint64_t withdrawn;
    uint64_t approved;
    uint64_t redeemed;
    uint64_t shorted;
    struct komodo_event **Komodo_events; int32_t Komodo_numevents;
    uint32_t RTbufs[64][3]; uint64_t RTmask;

protected:
    notarized_checkpoint_container NPOINTS; // collection of notarizations
    notarized_checkpoint last;

public:
    const uint256 &LastNotarizedHash() const { return last.notarized_hash; }
    void SetLastNotarizedHash(const uint256 &in) { last.notarized_hash = in; }
    const uint256 &LastNotarizedDestTxId() const { return last.notarized_desttxid; }
    void SetLastNotarizedDestTxId(const uint256 &in) { last.notarized_desttxid = in; }
    const uint256 &LastNotarizedMoM() const { return last.MoM; }
    void SetLastNotarizedMoM(const uint256 &in) { last.MoM = in; }
    const int32_t &LastNotarizedHeight() const { return last.notarized_height; }
    void SetLastNotarizedHeight(const int32_t in) { last.notarized_height = in; }
    const int32_t &LastNotarizedMoMDepth() const { return last.MoMdepth; }
    void SetLastNotarizedMoMDepth(const int32_t in) { last.MoMdepth =in; }

    /*****
     * @brief add a checkpoint to the collection and update member values
     * @param in the new values
     */
    void AddCheckpoint(const notarized_checkpoint &in)
    {
        NPOINTS.push_back(in);
        last = in;
    }

    uint64_t NumCheckpoints() { return NPOINTS.size(); }

    /****
     * Get the notarization data below a particular height
     * @param[in] nHeight the height desired
     * @param[out] notarized_hashp the hash of the notarized block
     * @param[out] notarized_desttxidp the desttxid
     * @returns the notarized height
     */
    int32_t NotarizedData(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp)
    {
        // get the nearest height without going over
        auto &idx = NPOINTS.get<1>();
        auto itr = idx.upper_bound(nHeight);
        if (itr != idx.begin())
            --itr;
        if ( itr != idx.end() && (*itr).nHeight < nHeight )
        {
            *notarized_hashp = itr->notarized_hash;
            *notarized_desttxidp = itr->notarized_desttxid;
            return itr->notarized_height;
        }
        memset(notarized_hashp,0,sizeof(*notarized_hashp));
        memset(notarized_desttxidp,0,sizeof(*notarized_desttxidp));
        return 0;
    }

    /******
     * @brief Get the last notarization information
     * @param[out] prevMoMheightp the MoM height
     * @param[out] hashp the notarized hash
     * @param[out] txidp the DESTTXID
     * @returns the notarized height
     */
    int32_t NotarizedHeight(int32_t *prevMoMheightp,uint256 *hashp,uint256 *txidp)
    {
        CBlockIndex *pindex;
        if ( (pindex= komodo_blockindex(last.notarized_hash)) == 0 || pindex->GetHeight() < 0 )
        {
            // found orphaned notarization, adjust the values in the komodo_state object
            last.notarized_hash.SetNull();
            last.notarized_desttxid.SetNull();
            last.notarized_height = 0;
        }
        else
        {
            *hashp = last.notarized_hash;
            *txidp = last.notarized_desttxid;
            *prevMoMheightp = PrevMoMHeight();
        }
        return last.notarized_height;
    }

    /****
     * Search for the last (chronological) MoM notarized height
     * @returns the last notarized height that has a MoM
     */
    int32_t PrevMoMHeight()
    {
        static uint256 zero;
        // shortcut
        if (last.MoM != zero)
        {
            return last.notarized_height;
        }
        if (NPOINTS.size() > 0)
        {
            auto &idx = NPOINTS.get<0>();
            for( auto r_itr = idx.rbegin(); r_itr != idx.rend(); ++r_itr)
            {
                if (r_itr->MoM != zero)
                    return r_itr->notarized_height;
            }
        }
        return 0;
    }

    /******
     * @brief Search the notarized checkpoints for a particular height
     * @note Finding a mach does include other criteria other than height
     *      such that the checkpoint includes the desired height
     * @param height the notarized_height desired
     * @returns the checkpoint or sp->NPOINTS.rend()
     */
    const notarized_checkpoint *CheckpointAtHeight(int32_t height)
    {
        // find the nearest notarization_height
        if(NPOINTS.size() > 0)
        {
            auto &idx = NPOINTS.get<2>(); // search by notarized_height
            auto itr = idx.upper_bound(height);
            // work backwards, get the first one that meets our criteria
            while (true)
            {
                if ( itr->MoMdepth != 0 
                        && height > itr->notarized_height-(itr->MoMdepth&0xffff) 
                        && height <= itr->notarized_height )
                {
                    return &(*itr);
                }
                if (itr == idx.begin())
                    break;
                --itr;
            }
        } // we have some elements in the collection
        return nullptr;
    }
};

#endif /* KOMODO_STRUCTS_H */
