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

#define KOMODO_DEX_LOCALHEARTBEAT 10 // eventually set to 2 seconds
#define KOMODO_DEX_RELAYDEPTH 3 // increase as network size increases
#define KOMODO_DEX_QUOTESIZE 16

// message format: <relay depth> <funcid> <timestamp> <payload>

// quote: bid/ask vol, pubkey, timestamp, sig -> shorthash; cached for one hour

// shortquote: +/- price, vol, elapsed, shorthash

// subscription: order pair, recent best shortquotes

// DEX state: subscriptions[]

// incoming shortquotes[] -> send all unexpired quotes that is better than incoming[]
// quote -> update best shortquotes, send quote to any neighbor that needs it

uint32_t komodo_DEXquotehash(bits256 &hash,uint8_t *msg,int32_t len)
{
    vcalc_sha256(0,hash.bytes,&msg[1],len-1);
    return(hash.ints[0]); // might have endian issues
}

int32_t komodo_DEXprocess(CNode *pfrom,std::vector<uint8_t> &response,uint8_t *msg,int32_t len) // incoming message
{
    int32_t i,relay=0; uint32_t t,h; uint8_t funcid; bits256 hash;
    if ( len >= 6 )
    {
        relay = msg[0];
        funcid = msg[1];
        iguana_rwnum(0,&msg[2],sizeof(t),&t);
        if ( funcid != 'P' )
        {
            h = komodo_DEXquotehash(hash,msg,len);
            fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
            fprintf(stderr," recv at %u from (%s) shorthash.%08x\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str(),h);
        }
        if ( relay > 0 && relay <= KOMODO_DEX_RELAYDEPTH )
        {
            response.resize(len);
            memcpy(&response[0],msg,len);
            response[0] = relay-1;
            return(len);
        }
    }
    return(0);
}

void komodo_DEXmsg(CNode *pfrom,std::vector<uint8_t> request) // received a packet
{
    int32_t len; std::vector<uint8_t> response; bits256 hash; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= request.size()) > 0 )
    {
        if ( komodo_DEXprocess(pfrom,response,&request[0],len) > 0 )
        {
            pfrom->PushMessage("DEX",response);
            fprintf(stderr,"RELAY %x\n",komodo_DEXquotehash(hash,&request[0],len));
        }
    }
}

int32_t komodo_DEXgenquote(std::vector<uint8_t> &quote,uint32_t timestamp,uint8_t data[KOMODO_DEX_QUOTESIZE])
{
    int32_t i,osize = 0,len = 0; bits256 hash;
    quote.resize(2 + sizeof(uint32_t) + KOMODO_DEX_QUOTESIZE); // send list of recently added shorthashes
    quote[len++] = KOMODO_DEX_RELAYDEPTH;
    quote[len++] = 'Q';
    len += iguana_rwnum(1,&quote[len],sizeof(timestamp),&timestamp);
    for (i=0; i<KOMODO_DEX_QUOTESIZE; i++)
        quote[len++] = data[i];
    fprintf(stderr,"issue order %08x!\n",komodo_DEXquotehash(hash,&quote[0],len));
    return(len);
}

int32_t komodo_DEXgenping(std::vector<uint8_t> &ping,uint32_t timestamp)
{
    ping.resize(2 + sizeof(uint32_t)); // send list of recently added shorthashes
    ping[len++] = 0;
    ping[len++] = 'P';
    len += iguana_rwnum(1,&ping[len],sizeof(timestamp),&timestamp);
    return(len);
}

void komodo_DEXpoll(CNode *pto)
{
    std::vector<uint8_t> packet; uint8_t quote[KOMODO_DEX_QUOTESIZE]; uint32_t timestamp = (uint32_t)time(NULL);
    if ( timestamp > pto->dexlastping+KOMODO_DEX_LOCALHEARTBEAT )
    {
        if ( (rand() % 100) == 0 ) // eventually via api
        {
            for (i=0; i<KOMODO_DEX_QUOTESIZE; i++)
                quote[i] = (rand() >> 11) & 0xff;
            komodo_DEXgenquote(packet,timestamp,quote);
            pto->PushMessage("DEX",packet);
        }
        komodo_DEXgenping(packet,timestamp);
        pto->PushMessage("DEX",packet);
        pto->dexlastping = timestamp;
        //fprintf(stderr," send at %u to (%s)\n",timestamp,pto->addr.ToString().c_str());
    }
}

/*cJSON *komodo_DEXrpc(cJSON *jsonargs) // from cli
{
    if ( strncmp(ASSETCHAINS_SYMBOL,"DEX",3) == 0 )
    {
        // price subscription
        // setprice
    }
}*/

