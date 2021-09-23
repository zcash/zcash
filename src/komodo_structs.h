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
#include <memory>
#include <list>
#include <cstdint>

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

// structs prior to refactor
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

namespace komodo {

enum komodo_event_type
{
    EVENT_PUBKEYS,
    EVENT_NOTARIZED,
    EVENT_U,
    EVENT_KMDHEIGHT,
    EVENT_OPRETURN,
    EVENT_PRICEFEED,
    EVENT_REWIND
};

/***
 * Thrown by event constructors when it finds a problem with the input data
 */
class parse_error : public std::logic_error
{
public:
    parse_error(const std::string& in) : std::logic_error(in) {}
};

/***
 * base class for events
 */
class event
{
public:
    event(komodo_event_type t, int32_t height) : type(t), height(height) {}
    virtual ~event() = default;
    komodo_event_type type;
    int32_t height;
};
std::ostream& operator<<(std::ostream& os, const event& in);

struct event_rewind : public event
{
    event_rewind() : event(komodo_event_type::EVENT_REWIND, 0) {}
    event_rewind(int32_t ht) : event(EVENT_REWIND, ht) {}
    event_rewind(uint8_t* data, long &pos, long data_len, int32_t height);
};
std::ostream& operator<<(std::ostream& os, const event_rewind& in);

struct event_notarized : public event
{
    event_notarized() : event(komodo_event_type::EVENT_NOTARIZED, 0), notarizedheight(0), MoMdepth(0) {
        memset(this->dest, 0, sizeof(this->dest));
    }
    event_notarized(int32_t ht, const char* _dest) : event(EVENT_NOTARIZED, ht), notarizedheight(0), MoMdepth(0) {
        memcpy(this->dest, _dest, sizeof(this->dest)-1);
    }
    event_notarized(uint8_t* data, long &pos, long data_len, int32_t height, const char* _dest, bool includeMoM = false);
    event_notarized(FILE* fp, int32_t ht, const char* _dest, bool includeMoM = false);
    uint256 blockhash;
    uint256 desttxid;
    uint256 MoM; 
    int32_t notarizedheight;
    int32_t MoMdepth; 
    char dest[16];
};
std::ostream& operator<<(std::ostream& os, const event_notarized& in);

struct event_pubkeys : public event
{
    /***
     * Default ctor
     */
    event_pubkeys() : event(EVENT_PUBKEYS, 0), num(0)
    {
        memset(pubkeys, 0, 64 * 33);
    }
    event_pubkeys(int32_t ht) : event(EVENT_PUBKEYS, ht), num(0) 
    {
        memset(pubkeys, 0, 64 * 33);
    }
    /***
     * ctor from data stream
     * @param data the data stream
     * @param pos the starting position (will advance)
     * @param data_len full length of data
     */
    event_pubkeys(uint8_t* data, long &pos, long data_len, int32_t height);
    event_pubkeys(FILE* fp, int32_t height);
    uint8_t num = 0; 
    uint8_t pubkeys[64][33]; 
};
std::ostream& operator<<(std::ostream& os, const event_pubkeys& in);

struct event_u : public event
{
    event_u() : event(EVENT_U, 0) 
    {
        memset(mask, 0, 8);
        memset(hash, 0, 32);
    }
    event_u(int32_t ht) : event(EVENT_U, ht)
    {
        memset(mask, 0, 8);
        memset(hash, 0, 32);
    }
    event_u(uint8_t *data, long &pos, long data_len, int32_t height);
    event_u(FILE* fp, int32_t height);
    uint8_t n = 0;
    uint8_t nid = 0;
    uint8_t mask[8];
    uint8_t hash[32];
};
std::ostream& operator<<(std::ostream& os, const event_u& in);

struct event_kmdheight : public event
{
    event_kmdheight() : event(EVENT_KMDHEIGHT, 0) {}
    event_kmdheight(int32_t ht) : event(EVENT_KMDHEIGHT, ht) {}
    event_kmdheight(uint8_t *data, long &pos, long data_len, int32_t height, bool includeTimestamp = false);
    event_kmdheight(FILE* fp, int32_t height, bool includeTimestamp = false);
    int32_t kheight = 0;
    uint32_t timestamp = 0;
};
std::ostream& operator<<(std::ostream& os, const event_kmdheight& in);

struct event_opreturn : public event 
{ 
    event_opreturn() : event(EVENT_OPRETURN, 0) 
    {
        txid.SetNull();
    }
    event_opreturn(int32_t ht) : event(EVENT_OPRETURN, ht)
    {
        txid.SetNull();
    }
    event_opreturn(uint8_t *data, long &pos, long data_len, int32_t height);
    event_opreturn(FILE* fp, int32_t height);
    uint256 txid; 
    uint16_t vout = 0;
    uint64_t value = 0; 
    std::vector<uint8_t> opret;
};
std::ostream& operator<<(std::ostream& os, const event_opreturn& in);

struct event_pricefeed : public event
{
    event_pricefeed() : event(EVENT_PRICEFEED, 0), num(0) 
    {
        memset(prices, 0, 35);
    }
    event_pricefeed(int32_t ht) : event(EVENT_PRICEFEED, ht)
    {
        memset(prices, 0, 35);
    }
    event_pricefeed(uint8_t *data, long &pos, long data_len, int32_t height);
    event_pricefeed(FILE* fp, int32_t height); 
    uint8_t num = 0; 
    uint32_t prices[35]; 
};
std::ostream& operator<<(std::ostream& os, const event_pricefeed& in);

} // namespace komodo

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
    struct notarized_checkpoint *NPOINTS; 
    int32_t NUM_NPOINTS,last_NPOINTSi;
    std::list<std::shared_ptr<komodo::event>> events;
    uint32_t RTbufs[64][3]; uint64_t RTmask;
    bool add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<komodo::event> in);
};

#endif /* KOMODO_STRUCTS_H */
