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

#ifndef H_KOMODOEVENTS_H
#define H_KOMODOEVENTS_H
#include "komodo_defs.h"

struct komodo_event *komodo_eventadd(struct komodo_state *sp,int32_t height,char *symbol,uint8_t type,uint8_t *data,uint16_t datalen)
{
    struct komodo_event *ep=0; uint16_t len = (uint16_t)(sizeof(*ep) + datalen);
    if ( sp != 0 && ASSETCHAINS_SYMBOL[0] != 0 )
    {
        portable_mutex_lock(&komodo_mutex);
        ep = (struct komodo_event *)calloc(1,len);
        ep->len = len;
        ep->height = height;
        ep->type = type;
        strcpy(ep->symbol,symbol);
        if ( datalen != 0 )
            memcpy(ep->space,data,datalen);
        sp->Komodo_events = (struct komodo_event **)realloc(sp->Komodo_events,(1 + sp->Komodo_numevents) * sizeof(*sp->Komodo_events));
        sp->Komodo_events[sp->Komodo_numevents++] = ep;
        portable_mutex_unlock(&komodo_mutex);
    }
    return(ep);
}

void komodo_eventadd_notarized(struct komodo_state *sp,char *symbol,int32_t height,char *dest,uint256 notarized_hash,uint256 notarized_desttxid,int32_t notarizedheight,uint256 MoM,int32_t MoMdepth)
{
    static uint32_t counter; int32_t verified=0; char *coin; struct komodo_event_notarized N;
    coin = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;
    if ( IS_KOMODO_NOTARY != 0 && (verified= komodo_verifynotarization(symbol,dest,height,notarizedheight,notarized_hash,notarized_desttxid)) < 0 )
    {
        if ( counter++ < 100 )
            printf("[%s] error validating notarization ht.%d notarized_height.%d, if on a pruned %s node this can be ignored\n",ASSETCHAINS_SYMBOL,height,notarizedheight,dest);
    }
    else if ( strcmp(symbol,coin) == 0 )
    {
        if ( 0 && IS_KOMODO_NOTARY != 0 && verified != 0 )
            fprintf(stderr,"validated [%s] ht.%d notarized %d\n",coin,height,notarizedheight);
        memset(&N,0,sizeof(N));
        N.blockhash = notarized_hash;
        N.desttxid = notarized_desttxid;
        N.notarizedheight = notarizedheight;
        N.MoM = MoM;
        N.MoMdepth = MoMdepth;
        strncpy(N.dest,dest,sizeof(N.dest)-1);
        komodo_eventadd(sp,height,symbol,KOMODO_EVENT_NOTARIZED,(uint8_t *)&N,sizeof(N));
        if ( sp != 0 )
            komodo_notarized_update(sp,height,notarizedheight,notarized_hash,notarized_desttxid,MoM,MoMdepth);
    }
}

void komodo_eventadd_pubkeys(struct komodo_state *sp,char *symbol,int32_t height,uint8_t num,uint8_t pubkeys[64][33])
{
    struct komodo_event_pubkeys P;
    //printf("eventadd pubkeys ht.%d\n",height);
    memset(&P,0,sizeof(P));
    P.num = num;
    memcpy(P.pubkeys,pubkeys,33 * num);
    komodo_eventadd(sp,height,symbol,KOMODO_EVENT_RATIFY,(uint8_t *)&P,(int32_t)(sizeof(P.num) + 33 * num));
    if ( sp != 0 )
        komodo_notarysinit(height,pubkeys,num);
}

void komodo_eventadd_pricefeed(struct komodo_state *sp,char *symbol,int32_t height,uint32_t *prices,uint8_t num)
{
    struct komodo_event_pricefeed F;
    if ( num == sizeof(F.prices)/sizeof(*F.prices) )
    {
        memset(&F,0,sizeof(F));
        F.num = num;
        memcpy(F.prices,prices,sizeof(*F.prices) * num);
        komodo_eventadd(sp,height,symbol,KOMODO_EVENT_PRICEFEED,(uint8_t *)&F,(int32_t)(sizeof(F.num) + sizeof(*F.prices) * num));
        if ( sp != 0 )
            komodo_pvals(height,prices,num);
    } //else fprintf(stderr,"skip pricefeed[%d]\n",num);
}

void komodo_eventadd_opreturn(struct komodo_state *sp,char *symbol,int32_t height,uint256 txid,uint64_t value,uint16_t vout,uint8_t *buf,uint16_t opretlen)
{
    struct komodo_event_opreturn O; uint8_t *opret;
    if ( ASSETCHAINS_SYMBOL[0] != 0 )
    {
        opret = (uint8_t *)calloc(1,sizeof(O) + opretlen + 16);
        O.txid = txid;
        O.value = value;
        O.vout = vout;
        memcpy(opret,&O,sizeof(O));
        memcpy(&opret[sizeof(O)],buf,opretlen);
        O.oplen = (int32_t)(opretlen + sizeof(O));
        komodo_eventadd(sp,height,symbol,KOMODO_EVENT_OPRETURN,opret,O.oplen);
        free(opret);
        if ( sp != 0 )
            komodo_opreturn(height,value,buf,opretlen,txid,vout,symbol);
    }
}

void komodo_event_undo(struct komodo_state *sp,struct komodo_event *ep)
{
    switch ( ep->type )
    {
        case KOMODO_EVENT_RATIFY: printf("rewind of ratify, needs to be coded.%d\n",ep->height); break;
        case KOMODO_EVENT_NOTARIZED: break;
        case KOMODO_EVENT_KMDHEIGHT:
            if ( ep->height <= sp->SAVEDHEIGHT )
                sp->SAVEDHEIGHT = ep->height;
            break;
        case KOMODO_EVENT_PRICEFEED:
            // backtrack prices;
            break;
        case KOMODO_EVENT_OPRETURN:
            // backtrack opreturns
            break;
    }
}

void komodo_event_rewind(struct komodo_state *sp,char *symbol,int32_t height)
{
    struct komodo_event *ep;
    if ( sp != 0 )
    {
        if ( ASSETCHAINS_SYMBOL[0] == 0 && height <= KOMODO_LASTMINED && prevKOMODO_LASTMINED != 0 )
        {
            printf("undo KOMODO_LASTMINED %d <- %d\n",KOMODO_LASTMINED,prevKOMODO_LASTMINED);
            KOMODO_LASTMINED = prevKOMODO_LASTMINED;
            prevKOMODO_LASTMINED = 0;
        }
        while ( sp->Komodo_events != 0 && sp->Komodo_numevents > 0 )
        {
            if ( (ep= sp->Komodo_events[sp->Komodo_numevents-1]) != 0 )
            {
                if ( ep->height < height )
                    break;
                //printf("[%s] undo %s event.%c ht.%d for rewind.%d\n",ASSETCHAINS_SYMBOL,symbol,ep->type,ep->height,height);
                komodo_event_undo(sp,ep);
                sp->Komodo_numevents--;
            }
        }
    }
}

void komodo_setkmdheight(struct komodo_state *sp,int32_t kmdheight,uint32_t timestamp)
{
    if ( sp != 0 )
    {
        if ( kmdheight > sp->SAVEDHEIGHT )
        {
            sp->SAVEDHEIGHT = kmdheight;
            sp->SAVEDTIMESTAMP = timestamp;
        }
        if ( kmdheight > sp->CURRENT_HEIGHT )
            sp->CURRENT_HEIGHT = kmdheight;
    }
}

void komodo_eventadd_kmdheight(struct komodo_state *sp,char *symbol,int32_t height,int32_t kmdheight,uint32_t timestamp)
{
    uint32_t buf[2];
    if ( kmdheight > 0 )
    {
        buf[0] = (uint32_t)kmdheight;
        buf[1] = timestamp;
        komodo_eventadd(sp,height,symbol,KOMODO_EVENT_KMDHEIGHT,(uint8_t *)buf,sizeof(buf));
        if ( sp != 0 )
            komodo_setkmdheight(sp,kmdheight,timestamp);
    }
    else
    {
        //fprintf(stderr,"REWIND kmdheight.%d\n",kmdheight);
        kmdheight = -kmdheight;
        komodo_eventadd(sp,height,symbol,KOMODO_EVENT_REWIND,(uint8_t *)&height,sizeof(height));
        if ( sp != 0 )
            komodo_event_rewind(sp,symbol,height);
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
