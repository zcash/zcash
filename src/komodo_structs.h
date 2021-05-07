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
#pragma once
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

#ifndef _BITS256
#define _BITS256
    union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
    typedef union _bits256 bits256;
#endif    

union _bits320 { uint8_t bytes[40]; uint16_t ushorts[20]; uint32_t uints[10]; uint64_t ulongs[5]; uint64_t txid; };
typedef union _bits320 bits320;

struct komodo_kv { UT_hash_handle hh; bits256 pubkey; uint8_t *key,*value; int32_t height; uint32_t flags; uint16_t keylen,valuesize; };

enum komodo_event_type
{
    KOMODO_EVENT_PUBKEYS
};

class parse_error : public std::logic_error
{
public:
    parse_error(const std::string& in) : std::logic_error(in) {}
};


struct komodo_event_notarized 
{ 
    uint256 blockhash;
    uint256 desttxid;
    uint256 MoM; 
    int32_t notarizedheight;
    int32_t MoMdepth; 
    char dest[16];
};

class event
{
public:
    event(komodo_event_type t, int32_t height) : type(t), height(height) {}
    komodo_event_type type;
    int32_t height;
};

struct komodo_event_pubkeys : public event
{
    /***
     * Default ctor
     */
    komodo_event_pubkeys() : event(KOMODO_EVENT_PUBKEYS, 0), num(0) {}
    /***
     * ctor from data stream
     * @param data the data stream
     * @param pos the starting position (will advance)
     * @param data_len full length of data
     */
    komodo_event_pubkeys(uint8_t* data, long &pos, long data_len, int32_t height);
    uint8_t num; 
    uint8_t pubkeys[64][33]; 
};

std::ostream& operator<<(std::ostream& os, const komodo_event_pubkeys& in);

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
    uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID,MoM;
    int32_t SAVEDHEIGHT,CURRENT_HEIGHT,NOTARIZED_HEIGHT,MoMdepth;
    uint32_t SAVEDTIMESTAMP;
    uint64_t deposited,issued,withdrawn,approved,redeemed,shorted;
    struct notarized_checkpoint *NPOINTS; int32_t NUM_NPOINTS,last_NPOINTSi;
    struct komodo_event **Komodo_events; int32_t Komodo_numevents;
    std::list<std::shared_ptr<event>> events;
    uint32_t RTbufs[64][3]; uint64_t RTmask;
    bool add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<event> in);

};

#endif /* KOMODO_STRUCTS_H */
