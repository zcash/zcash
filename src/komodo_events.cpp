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
#include "komodo_events.h"
#include "komodo_extern_globals.h"
#include "komodo_bitcoind.h" // komodo_verifynotarization
#include "komodo_notary.h" // komodo_notarized_update
#include "komodo_pax.h" // komodo_pvals
#include "komodo_gateway.h" // komodo_opreturn

/*****
 * Add a notarized event to the collection
 * @param sp the state to add to
 * @param symbol
 * @param height
 * @param ntz the event
 */
void komodo_eventadd_notarized( komodo_state *sp, char *symbol, int32_t height, std::shared_ptr<komodo::event_notarized> ntz)
{
    char *coin = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;

    if ( IS_KOMODO_NOTARY 
            && komodo_verifynotarization(symbol,ntz->dest,height,ntz->notarizedheight,ntz->blockhash, ntz->desttxid) < 0 )
    {
        static uint32_t counter;
        if ( counter++ < 100 )
            printf("[%s] error validating notarization ht.%d notarized_height.%d, if on a pruned %s node this can be ignored\n",
                    ASSETCHAINS_SYMBOL,height,ntz->notarizedheight, ntz->dest);
    }
    else if ( strcmp(symbol,coin) == 0 )
    {
        if ( sp != nullptr )
        {
            sp->add_event(symbol, height, ntz);
            komodo_notarized_update(sp,height, ntz->notarizedheight, ntz->blockhash, ntz->desttxid, ntz->MoM, ntz->MoMdepth);
        }
    }
}

/*****
 * Add a pubkeys event to the collection
 * @param sp where to add
 * @param symbol
 * @param height
 * @param pk the event
 */
void komodo_eventadd_pubkeys(komodo_state *sp, char *symbol, int32_t height, std::shared_ptr<komodo::event_pubkeys> pk)
{
    if (sp != nullptr)
    {
        sp->add_event(symbol, height, pk);
        komodo_notarysinit(height, pk->pubkeys, pk->num);
    }
}

/********
 * Add a pricefeed event to the collection
 * @param sp where to add
 * @param symbol
 * @param height
 * @param pf the event
 */
void komodo_eventadd_pricefeed( komodo_state *sp, char *symbol, int32_t height, std::shared_ptr<komodo::event_pricefeed> pf)
{
    if (sp != nullptr)
    {
        sp->add_event(symbol, height, pf);
        komodo_pvals(height,pf->prices, pf->num);
    }
}

/*****
 * Add an opreturn event to the collection
 * @param sp where to add
 * @param symbol
 * @param height
 * @param opret the event
 */
void komodo_eventadd_opreturn( komodo_state *sp, char *symbol, int32_t height, std::shared_ptr<komodo::event_opreturn> opret)
{
    if ( sp != nullptr )
    {
        sp->add_event(symbol, height, opret);
        komodo_opreturn(height, opret->value, opret->opret.data(), opret->opret.size(), opret->txid, opret->vout, symbol);
    }
}

/*****
 * @brief Undo an event
 * @note seems to only work for KMD height events
 * @param sp the state object
 * @param ep the event to undo
 */
void komodo_event_undo(struct komodo_state *sp,struct komodo_event *ep)
{
    switch ( ep->type )
    {
        case KOMODO_EVENT_RATIFY: 
            printf("rewind of ratify, needs to be coded.%d\n",ep->height); 
            break;
        case KOMODO_EVENT_NOTARIZED: 
            break;
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
    komodo_event *ep = nullptr;
    if ( sp != nullptr )
    {
        if ( ASSETCHAINS_SYMBOL[0] == 0 && height <= KOMODO_LASTMINED && prevKOMODO_LASTMINED != 0 )
        {
            printf("undo KOMODO_LASTMINED %d <- %d\n",KOMODO_LASTMINED,prevKOMODO_LASTMINED);
            KOMODO_LASTMINED = prevKOMODO_LASTMINED;
            prevKOMODO_LASTMINED = 0;
        }
        while ( sp->Komodo_events != 0 && sp->Komodo_numevents > 0 )
        {
            if ( (ep= sp->Komodo_events[sp->Komodo_numevents-1]) != nullptr )
            {
                if ( ep->height < height )
                    break;
                komodo_event_undo(sp,ep);
                sp->Komodo_numevents--;
            }
        }
    }
}

void komodo_setkmdheight(struct komodo_state *sp,int32_t kmdheight,uint32_t timestamp)
{
    if ( sp != nullptr )
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

/******
 * @brief handle a height change event (forward or rewind)
 * @param sp
 * @param symbol
 * @param height
 * @param kmdht the event
 */
void komodo_eventadd_kmdheight(struct komodo_state *sp,char *symbol,int32_t height, std::shared_ptr<komodo::event_kmdheight> kmdht)
{
    if (sp != nullptr)
    {
        if ( kmdht->kheight > 0 ) // height is advancing
        {

            sp->add_event(symbol, height, kmdht);
            komodo_setkmdheight(sp, kmdht->kheight, kmdht->timestamp);
        }
        else // rewinding
        {
            std::shared_ptr<komodo::event_rewind> e = std::make_shared<komodo::event_rewind>(height);
            sp->add_event(symbol, height, e);
            komodo_event_rewind(sp,symbol,height);
        }
    }
}
