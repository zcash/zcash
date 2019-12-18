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

// included from komodo_nSPV_superlite.h

// quote: bid/ask vol, pubkey, timestamp, sig -> shorthash; cached for one hour

// shortquote: +/- price, vol, elapsed, shorthash

// subscription: order pair, recent best shortquotes

// DEX state: subscriptions[]

// incoming shortquotes[] -> send all unexpired quotes that is better than incoming[]
// quote -> update best shortquotes, send quote to any neighbor that needs it

int32_t komodo_DEXprocess(CNode *pfrom,std::vector<uint8_t> &response,uint8_t *msg,int32_t len) // incoming message
{
    // update DEX state and set response and return nonzero if there is a response
    
    // price subscription -> return list of peers with that subscription along with bestprice
    // price quote heartbeat with best known external pricequote/timestamp
    // if any of our peers has a price subscription, mark external subscription
    return(0);
}

void komodo_DEXpoll(CNode *pto) // from SendMessages polling
{
    // determine what message to send to the other node
    // for each ptr-> price subscription or external subscription, check to see if we have a better or fresher price and send if we do
}

cJSON *komodo_DEXrpc(cJSON *jsonargs) // from cli
{
    if ( strncmp(ASSETCHAINS_SYMBOL,"DEX",3) == 0 )
    {
        // price subscription
        // setprice
    }
}

