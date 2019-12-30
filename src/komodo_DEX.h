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

#define KOMODO_DEX_LOCALHEARTBEAT 10 // eventuall set to 2

// quote: bid/ask vol, pubkey, timestamp, sig -> shorthash; cached for one hour

// shortquote: +/- price, vol, elapsed, shorthash

// subscription: order pair, recent best shortquotes

// DEX state: subscriptions[]

// incoming shortquotes[] -> send all unexpired quotes that is better than incoming[]
// quote -> update best shortquotes, send quote to any neighbor that needs it

int32_t komodo_DEXprocess(CNode *pfrom,std::vector<uint8_t> &response,uint8_t *msg,int32_t len) // incoming message
{
    int32_t i; uint32_t t;
    for (i=0; i<len; i++)
        fprintf(stderr,"%02x",msg[i]);
    if ( len >= 4 )
    {
        iguana_rwnum(0,msg,sizeof(t),&t);
        fprintf(stderr," t.%u",t);
    }
    fprintf(stderr," recv at %u from (%s)\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str());
    return(0);
}

int32_t komodo_DEXgenping(std::vector<uint8_t> &ping,uint32_t timestamp)
{
    int32_t len = 0;
    ping.resize(sizeof(uint32_t)); // send list of recently added shorthashes
    len += iguana_rwnum(1,&ping[len],sizeof(timestamp),&timestamp);
    return(len);
}

void komodo_DEXpoll(CNode *pto) // from SendMessages polling
{
    std::vector<uint8_t> ping; uint32_t timestamp = (uint32_t)time(NULL);
    if ( timestamp > pto->dexlastping+KOMODO_DEX_LOCALHEARTBEAT && komodo_DEXgenping(ping,timestamp) > 0 )
    {
        pto->PushMessage("DEX",ping);
        pto->dexlastping = timestamp;
    }
}

cJSON *komodo_DEXrpc(cJSON *jsonargs) // from cli
{
    if ( strncmp(ASSETCHAINS_SYMBOL,"DEX",3) == 0 )
    {
        // price subscription
        // setprice
    }
}

