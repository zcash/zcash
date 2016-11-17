/******************************************************************************
 * Copyright © 2014-2016 The SuperNET Developers.                             *
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

#ifndef H_KOMODOEVENTS_H
#define H_KOMODOEVENTS_H


#ifdef WIN32
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

#define KOMODO_EVENT_RATIFY 'P'
#define KOMODO_EVENT_NOTARIZED 'N'
#define KOMODO_EVENT_UTXO 'U'
#define KOMODO_EVENT_KMDHEIGHT 'K'
//#define KOMODO_EVENT_DELETE 'D'
#define KOMODO_EVENT_PRICEFEED 'V'
#define KOMODO_EVENT_OPRETURN 'R'
#define KOMODO_OPRETURN_DEPOSIT 'D'
#define KOMODO_OPRETURN_ISSUED 'I' // assetchain
#define KOMODO_OPRETURN_WITHDRAW 'W' // assetchain
#define KOMODO_OPRETURN_REDEEMED 'X'

struct komodo_event_notarized { uint256 blockhash,desttxid; int32_t notarizedheight; char dest[16]; } PACKED;
struct komodo_event_pubkeys { uint8_t num; uint8_t pubkeys[64][33]; } PACKED;
struct komodo_event_utxo { uint256 txid; uint64_t voutmask; uint8_t numvouts; } PACKED;
struct komodo_event_opreturn { uint256 txid; uint64_t value; uint16_t vout,oplen; uint8_t opret[]; } PACKED;
struct komodo_event_pricefeed { uint8_t num; uint32_t prices[35]; } PACKED;

struct komodo_event
{
    struct komodo_event *related;
    uint16_t len;
    int32_t height;
    uint8_t type,reorged;
    char symbol[16];
    uint8_t space[];
} PACKED;

struct komodo_event **Komodo_events; int32_t Komodo_numevents;

struct komodo_event *komodo_eventadd(int32_t height,char *symbol,uint8_t type,uint8_t *data,uint16_t datalen)
{
    struct komodo_event *ep; uint16_t len = (uint16_t)(sizeof(*ep) + datalen);
    ep = (struct komodo_event *)calloc(1,len);
    ep->len = len;
    ep->height = height;
    ep->type = type;
    strcpy(ep->symbol,symbol);
    if ( datalen != 0 )
        memcpy(ep->space,data,datalen);
    Komodo_events = (struct komodo_event **)realloc(Komodo_events,(1 + Komodo_numevents) * sizeof(*Komodo_events));
    Komodo_events[Komodo_numevents++] = ep;
    return(ep);
}

void komodo_eventadd_notarized(struct komodo_state *sp,char *symbol,int32_t height,char *dest,uint256 notarized_hash,uint256 notarized_desttxid,int32_t notarizedheight)
{
    struct komodo_event_notarized N;
    memset(&N,0,sizeof(N));
    N.blockhash = notarized_hash;
    N.desttxid = notarized_desttxid;
    N.notarizedheight = notarizedheight;
    strcpy(N.dest,dest);
    komodo_eventadd(height,symbol,KOMODO_EVENT_NOTARIZED,(uint8_t *)&N,sizeof(N));
    if ( sp != 0 )
        komodo_notarized_update(sp,height,notarizedheight,notarized_hash,notarized_desttxid);
}

void komodo_eventadd_pubkeys(struct komodo_state *sp,char *symbol,int32_t height,uint8_t num,uint8_t pubkeys[64][33])
{
    struct komodo_event_pubkeys P;
    memset(&P,0,sizeof(P));
    P.num = num;
    memcpy(P.pubkeys,pubkeys,33 * num);
    komodo_eventadd(height,symbol,KOMODO_EVENT_RATIFY,(uint8_t *)&P,(int32_t)(sizeof(P.num) + 33 * num));
    if ( sp != 0 )
        komodo_notarysinit(height,pubkeys,num);
}

void komodo_eventadd_utxo(struct komodo_state *sp,char *symbol,int32_t height,uint8_t notaryid,uint256 txid,uint64_t voutmask,uint8_t numvouts)
{
    struct komodo_event_utxo U;
    memset(&U,0,sizeof(U));
    U.txid = txid;
    U.voutmask = voutmask;
    U.numvouts = numvouts;
    komodo_eventadd(height,symbol,KOMODO_EVENT_UTXO,(uint8_t *)&U,sizeof(U));
    if ( sp != 0 )
        komodo_nutxoadd(height,notaryid,txid,voutmask,numvouts);
}

void komodo_eventadd_pricefeed(struct komodo_state *sp,char *symbol,int32_t height,uint32_t *prices,uint8_t num)
{
    struct komodo_event_pricefeed F;
    memset(&F,0,sizeof(F));
    F.num = num;
    memcpy(F.prices,prices,sizeof(*F.prices) * num);
    komodo_eventadd(height,symbol,KOMODO_EVENT_PRICEFEED,(uint8_t *)&F,(int32_t)(sizeof(F.num) + sizeof(*F.prices) * num));
    if ( sp != 0 )
        komodo_pvals(height,prices,num);
}

void komodo_eventadd_kmdheight(struct komodo_state *sp,char *symbol,int32_t height,int32_t kmdheight)
{
    komodo_eventadd(height,symbol,KOMODO_EVENT_KMDHEIGHT,(uint8_t *)&kmdheight,sizeof(kmdheight));
    if ( sp != 0 )
        komodo_setkmdheight(kmdheight);
}

void komodo_eventadd_opreturn(struct komodo_state *sp,char *symbol,int32_t height,uint256 txid,uint64_t value,uint16_t vout,uint8_t *buf,uint16_t opretlen)
{
    struct komodo_event_opreturn O; uint8_t opret[10000];
    memset(&O,0,sizeof(O));
    O.txid = txid;
    O.value = value;
    O.vout = vout;
    memcpy(opret,&O,sizeof(O));
    memcpy(&opret[sizeof(O)],buf,opretlen);
    O.oplen = (int32_t)(opretlen + sizeof(O));
    komodo_eventadd(height,symbol,KOMODO_EVENT_OPRETURN,opret,O.oplen);
    if ( sp != 0 )
        komodo_opreturn(height,value,buf,opretlen,txid,vout);
}

void komodo_event_undo(struct komodo_event *ep)
{
    switch ( ep->type )
    {
        case KOMODO_EVENT_RATIFY:
        case KOMODO_EVENT_NOTARIZED:
        case KOMODO_EVENT_UTXO:
        case KOMODO_EVENT_KMDHEIGHT:
        case KOMODO_EVENT_PRICEFEED:
        case KOMODO_EVENT_OPRETURN:
            break;
    }
}

void komodo_event_rewind(int32_t height)
{
    struct komodo_event *ep;
    while ( Komodo_numevents > 0 )
    {
        if ( (ep= Komodo_events[Komodo_numevents-1]) != 0 )
        {
            if ( ep->height < height )
                break;
            komodo_event_undo(ep);
            Komodo_numevents--;
        }
    }
}

/*void komodo_eventadd_deposit(int32_t actionflag,char *symbol,int32_t height,uint64_t komodoshis,char *fiat,uint64_t fiatoshis,uint8_t rmd160[20],bits256 kmdtxid,uint16_t kmdvout,uint64_t price)
{
    uint8_t opret[512]; uint16_t opretlen;
    komodo_eventadd_opreturn(symbol,height,KOMODO_OPRETURN_DEPOSIT,kmdtxid,komodoshis,kmdvout,opret,opretlen);
}

void komodo_eventadd_issued(int32_t actionflag,char *symbol,int32_t height,int32_t fiatheight,bits256 fiattxid,uint16_t fiatvout,bits256 kmdtxid,uint16_t kmdvout,uint64_t fiatoshis)
{
    uint8_t opret[512]; uint16_t opretlen;
    komodo_eventadd_opreturn(symbol,height,KOMODO_OPRETURN_ISSUED,fiattxid,fiatoshis,fiatvout,opret,opretlen);
}

void komodo_eventadd_withdraw(int32_t actionflag,char *symbol,int32_t height,uint64_t komodoshis,char *fiat,uint64_t fiatoshis,uint8_t rmd160[20],bits256 fiattxid,int32_t fiatvout,uint64_t price)
{
    uint8_t opret[512]; uint16_t opretlen;
    komodo_eventadd_opreturn(symbol,height,KOMODO_OPRETURN_WITHDRAW,fiattxid,fiatoshis,fiatvout,opret,opretlen);
}

void komodo_eventadd_redeemed(int32_t actionflag,char *symbol,int32_t height,bits256 kmdtxid,uint16_t kmdvout,int32_t fiatheight,bits256 fiattxid,uint16_t fiatvout,uint64_t komodoshis)
{
    uint8_t opret[512]; uint16_t opretlen;
    komodo_eventadd_opreturn(symbol,height,KOMODO_OPRETURN_REDEEMED,kmdtxid,komodoshis,kmdvout,opret,opretlen);
}*/

// process events
// 

#endif
