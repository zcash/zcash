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
    uint256 notarized_hash,notarized_desttxid,MoM,MoMoM;
    int32_t nHeight,notarized_height,MoMdepth,MoMoMdepth,MoMoMoffset,kmdstarti,kmdendi;
};

struct notarized_checkpoint_height_compare
{
    bool operator()(const notarized_checkpoint& a, const notarized_checkpoint& b)
    {
        return a.nHeight < b.nHeight;
    }
};

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

struct komodo_state
{
    uint256 NOTARIZED_HASH; // the latest notarized hash
    uint256 NOTARIZED_DESTTXID; // the latest notarized dest txid
    uint256 MoM;
    int32_t SAVEDHEIGHT;
    int32_t CURRENT_HEIGHT;
    int32_t NOTARIZED_HEIGHT; // the height of the latest notarization
    int32_t MoMdepth; // the MOM depth of the latest notarization
    uint32_t SAVEDTIMESTAMP;
    uint64_t deposited;
    uint64_t issued;
    uint64_t withdrawn;
    uint64_t approved;
    uint64_t redeemed;
    uint64_t shorted;
    std::multiset<notarized_checkpoint, notarized_checkpoint_height_compare> NPOINTS; // collection of notarizations
    struct komodo_event **Komodo_events; int32_t Komodo_numevents;
    uint32_t RTbufs[64][3]; uint64_t RTmask;
};

#endif /* KOMODO_STRUCTS_H */
