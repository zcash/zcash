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
#define KOMODO_DEX_RELAYDEPTH 2 // increase as network size increases
#define KOMODO_DEX_QUOTESIZE 16
#define KOMODO_DEX_QUOTETIME 3600   // expires after an hour

// message format: <relay depth> <funcid> <timestamp> <payload>

std::vector<uint8_t> RecentPackets[4096];
uint32_t RecentHashes[sizeof(RecentPackets)/sizeof(*RecentPackets)],RequestHashes[sizeof(RecentPackets)/sizeof(*RecentPackets)];

int32_t komodo_DEXrecentquoteadd(uint32_t recentquotes[],int32_t maxquotes,uint32_t shorthash)
{
    int32_t i;
    for (i=0; i<maxquotes; i++)
    {
        if ( recentquotes[i] == 0 )
            break;
    }
    if ( i == maxquotes )
        i = rand() % maxquotes;
    recentquotes[i] = shorthash;
    return(i);
}

int32_t komodo_DEXrecentquotefind(uint32_t recentquotes[],int32_t maxquotes,uint32_t shorthash)
{
    int32_t i;
    for (i=0; i<maxquotes; i++)
    {
        if ( shorthash == recentquotes[i] )
            return(i);
    }
    return(-1);
}

int32_t komodo_DEXrecentpackets(uint32_t now,CNode *pto,uint32_t recentquotes[],int32_t maxquotes)
{
    int32_t i,n = 0; uint8_t relay,funcid,*msg; uint32_t t;
    for (i=0; i<(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)); i++)
    {
        if ( RecentHashes[i] != 0 && RecentPackets[i].size() >= 6 )
        {
            msg = &RecentPackets[i][0];
            relay = msg[0];
            funcid = msg[1];
            iguana_rwnum(0,&msg[2],sizeof(t),&t);
            if ( now < t+KOMODO_DEX_LOCALHEARTBEAT )
            {
                if ( komodo_DEXrecentquotefind(recentquotes,maxquotes,RecentHashes[i]) < 0 )
                {
                    komodo_DEXrecentquoteadd(recentquotes,maxquotes,RecentHashes[i]);
                    pto->PushMessage("DEX",RecentPackets[i]);
                    n++;
                }
            }
        }
    }
    return(n);
}

uint32_t komodo_DEXquotehash(bits256 &hash,uint8_t *msg,int32_t len)
{
    vcalc_sha256(0,hash.bytes,&msg[1],len-1);
    return(hash.uints[0]); // might have endian issues
}

int32_t komodo_DEXprocess(uint32_t now,CNode *pfrom,std::vector<uint8_t> &response,uint8_t *msg,int32_t len) // incoming message
{
    int32_t i,ind; uint32_t t,h; uint8_t funcid,relay=0; bits256 hash;
    if ( len >= 6 )
    {
        relay = msg[0];
        funcid = msg[1];
        iguana_rwnum(0,&msg[2],sizeof(t),&t);
        if ( t > now+KOMODO_DEX_LOCALHEARTBEAT )
        {
            fprintf(stderr,"reject packet from future t.%u vs now.%u\n",t,now);
        }
        else if ( funcid != 'P' )
        {
            h = komodo_DEXquotehash(hash,msg,len);
            fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
            fprintf(stderr," recv at %u from (%s) shorthash.%08x\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str(),h);
            if ( relay <= KOMODO_DEX_RELAYDEPTH )
            {
                if ( komodo_DEXrecentquotefind(pfrom->recentquotes,(int32_t)(sizeof(pfrom->recentquotes)/sizeof(*pfrom->recentquotes)),h) < 0 )
                {
                    komodo_DEXrecentquoteadd(pfrom->recentquotes,(int32_t)(sizeof(pfrom->recentquotes)/sizeof(*pfrom->recentquotes)),h);
                }
                // change to hashtable for scaling
                if ( komodo_DEXrecentquotefind(RecentHashes,(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)),h) < 0 )
                {
                    if ( (ind= komodo_DEXrecentquoteadd(RecentHashes,(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)),h)) >= 0 )
                    {
                        if ( RecentPackets[ind].size() != len )
                            RecentPackets[ind].resize(len);
                        memcpy(&RecentPackets[ind][0],msg,len);
                        RecentPackets[ind][0] = relay - 1;
                        fprintf(stderr,"update slot.%d [%d]\n",ind,RecentPackets[ind][0]);
                    }
                    return(len);
                }
            }
        }
        else
        {
            // scan list of available hashes
            // if this node doesnt have it and hasnt already requested it, send request and update RequestHashes[]
        }
    }
    return(0);
}

void komodo_DEXmsg(CNode *pfrom,std::vector<uint8_t> request) // received a packet
{
    int32_t len; std::vector<uint8_t> response; bits256 hash; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= request.size()) > 0 )
    {
        if ( komodo_DEXprocess(timestamp,pfrom,response,&request[0],len) > 0 )
        {
            pfrom->PushMessage("DEX",response);
        }
    }
}

int32_t komodo_DEXgenquote(std::vector<uint8_t> &quote,uint32_t timestamp,uint8_t data[KOMODO_DEX_QUOTESIZE])
{
    int32_t i,len = 0; bits256 hash;
    quote.resize(2 + sizeof(uint32_t) + KOMODO_DEX_QUOTESIZE); // send list of recently added shorthashes
    quote[len++] = KOMODO_DEX_RELAYDEPTH;
    quote[len++] = 'Q';
    len += iguana_rwnum(1,&quote[len],sizeof(timestamp),&timestamp);
    for (i=0; i<KOMODO_DEX_QUOTESIZE; i++)
        quote[len++] = data[i];
    fprintf(stderr,"issue order %08x!\n",komodo_DEXquotehash(hash,&quote[0],len));
    return(len);
}

int32_t komodo_DEXgenping(std::vector<uint8_t> &ping,uint32_t timestamp,uint32_t *recentquotes,int32_t maxquotes)
{
    int32_t len = 0;
    ping.resize(2 + sizeof(uint32_t));
    // send list of recently added (2*KOMODO_DEX_LOCALHEARTBEAT seconds) that other node doesnt already have
    ping[len++] = 0;
    ping[len++] = 'P';
    len += iguana_rwnum(1,&ping[len],sizeof(timestamp),&timestamp);
    return(len);
}

void komodo_DEXpoll(CNode *pto)
{
    std::vector<uint8_t> packet; uint8_t quote[KOMODO_DEX_QUOTESIZE]; uint32_t i,timestamp;
    timestamp = (uint32_t)time(NULL);
komodo_DEXrecentpackets(timestamp,pto,pto->recentquotes,(int32_t)(sizeof(pto->recentquotes)/sizeof(*pto->recentquotes)));
    if ( timestamp > pto->dexlastping+KOMODO_DEX_LOCALHEARTBEAT )
    {
        if ( (rand() % 500) == 0 ) // eventually via api
        {
            for (i=0; i<KOMODO_DEX_QUOTESIZE; i++)
                quote[i] = (rand() >> 11) & 0xff;
            komodo_DEXgenquote(packet,timestamp,quote);
            pto->PushMessage("DEX",packet);
        }
        komodo_DEXgenping(packet,timestamp,pto->recentquotes,(int32_t)(sizeof(pto->recentquotes)/sizeof(*pto->recentquotes)));
        pto->PushMessage("DEX",packet);
        pto->dexlastping = timestamp;
        //fprintf(stderr," send at %u to (%s)\n",timestamp,pto->addr.ToString().c_str());
    }
}

// make nSPV api that allows to verify which shorthash is on a node

/*cJSON *komodo_DEXrpc(cJSON *jsonargs) // from cli
{
    if ( strncmp(ASSETCHAINS_SYMBOL,"DEX",3) == 0 )
    {
        // price subscription
        // setprice
    }
}*/

