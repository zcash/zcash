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
#define KOMODO_DEX_RELAYDEPTH 3 // increase as <avepeers> root of network size increases
#define KOMODO_DEX_QUOTESIZE 1024
#define KOMODO_DEX_QUOTETIME 3600   // expires after an hour, quote needs to be resubmitted after KOMODO_DEX_QUOTETIME

/*
 message format: <relay depth> <funcid> <timestamp> <payload>
 
 <payload> is the datablob for a 'Q' quote or <uint16_t> + n * <uint32_t> for a 'P' ping of recent shorthashes

 To achieve very fast performance a hybrid push/poll/pull gossip protocol is used. All new quotes are broadcast KOMODO_DEX_RELAYDEPTH levels deep, this gets the quote to large percentage of nodes assuming <ave peers> ^ KOMODO_DEX_RELAYDEPTH power is approx totalnodes/2. Nodes in the broacast "cone" will receive a new quote in less than a second in most cases.
 
 For efficiency it is better to aim for one third to half of the nodes during the push phase. The higher the KOMODO_DEX_RELAYDEPTH the faster a quote will be broadcast, but the more redundant packets will be sent. If each node would be able to have a full network connectivity map, each node could locally simulate packet propagation and select a subset of peers with the maximum propagation and minimum overlap. However, such an optimization requires up to date and complete network topography and the current method has the advantage of being much simpler and reasonably efficient.
 
 Additionally, all nodes will be broadcasting to their immediate peers, the most recent quotes not known to be known by the destination, which will allow the receiving peer to find any quotes they are missing and request it directly. At the cost of the local broadcasting, all the nodes will be able to directly request any quote they didnt get during the push phase.
 
 For sparsely connected nodes, as the pull process propagates a new quote, they will eventually also see the new quote. Worst case would be the last node in a singly connected chain of peers. Assuming most all nodes will have 3 or more peers, then most all nodes will get a quote broadcast in a few multiples of KOMODO_DEX_LOCALHEARTBEAT
 
 initial implemention uses linear searches for simplicity. changing to a hashtable/doubly-linked list will allow it to scale up to RAM limitations. it is possible using bloom filters for tracking shorthashes/peer will be adequate. As it is, if there are more than 255 new quotes to a peer per KOMODO_DEX_LOCALHEARTBEAT, efficiency will be degraded. The other limit is 4096 quotes in an hour, but the local hashtable will change that to a RAM limited ceiling. i think a combined list of 255 recent quotes and a periodically cleared bloom filter will allow a lot high number of quotes per peer, with minimal efficiency loss. When these two improvements are made internally, i think the total number of orders that can be handled will be quite large. actual testing on p2p networks will be needed to determine the specific performance limits.

 
 to support a newly bootstrapping node, a bootstrap query funcid is needed that will send all known shorthashes. the node can split up the queries for orders evenly among its peers to reduce the total time it will take to get caught up to current state.
 
 todo:
    add 'G' get shorthash funcid
    add 'B' bootstrap query funcid
    add nSPV remote api for bootstrap state and specific quotes and submitting new quote
    add layer that works at orderbook level
    optimize local orders and peer based tracking of shorthashes
    if RequestHashes[i] doesnt arrive within timelimit, clear it. more efficient than bruteforce clearing
 */

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
            if ( relay >= 0 && relay <= KOMODO_DEX_RELAYDEPTH && now < t+KOMODO_DEX_LOCALHEARTBEAT )
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

int32_t komodo_DEXrecentquotes(uint32_t now,std::vector<uint8_t> &ping,int32_t offset,uint32_t recentquotes[],int32_t maxquotes)
{
    int32_t i; uint16_t n = 0; uint8_t relay,funcid,*msg; uint32_t t; uint32_t recents[4096];
    for (i=0; i<(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)); i++)
    {
        if ( RecentHashes[i] != 0 && RecentPackets[i].size() >= 6 )
        {
            msg = &RecentPackets[i][0];
            relay = msg[0];
            funcid = msg[1];
            iguana_rwnum(0,&msg[2],sizeof(t),&t);
            if ( now < t+10*KOMODO_DEX_LOCALHEARTBEAT )
            {
                if ( komodo_DEXrecentquotefind(recentquotes,maxquotes,RecentHashes[i]) < 0 )
                {
                    komodo_DEXrecentquoteadd(recentquotes,maxquotes,RecentHashes[i]);
                    recents[n++] = RecentHashes[i];
                    if ( n >= (int32_t)(sizeof(recents)/sizeof(*recents)) )
                        break;
                }
            }
        }
    }
    ping.resize(offset + sizeof(n) + n*sizeof(uint32_t));
    offset += iguana_rwnum(1,&ping[offset],sizeof(n),&n);
    for (i=0; i<n; i++)
        offset += iguana_rwnum(1,&ping[offset],sizeof(recents[i]),&recents[i]);
    return(offset);
}

uint32_t komodo_DEXquotehash(bits256 &hash,uint8_t *msg,int32_t len)
{
    vcalc_sha256(0,hash.bytes,&msg[1],len-1);
    return(hash.uints[0]); // might have endian issues
}

int32_t komodo_DEXprocess(uint32_t now,CNode *pfrom,std::vector<uint8_t> &response,uint8_t *msg,int32_t len)
{
    int32_t i,ind,offset,flag; uint16_t n; uint32_t t,h; uint8_t funcid,relay=0; bits256 hash;
    if ( len >= 6 )
    {
        relay = msg[0];
        funcid = msg[1];
        iguana_rwnum(0,&msg[2],sizeof(t),&t);
        if ( t > now+KOMODO_DEX_LOCALHEARTBEAT )
        {
            fprintf(stderr,"reject packet from future t.%u vs now.%u\n",t,now);
        }
        else if ( funcid == 'Q' )
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
                        //fprintf(stderr,"update slot.%d [%d]\n",ind,RecentPackets[ind][0]);
                    }
                }
            }
        }
        else if ( funcid == 'P' )
        {
            if ( len >= 8 )
            {
                offset = 6;
                offset += iguana_rwnum(1,&msg[offset],sizeof(n),&n);
                if ( offset+n*sizeof(uint32_t) == len )
                {
                    for (flag=i=0; i<n; i++)
                    {
                        offset += iguana_rwnum(1,&msg[offset],sizeof(h),&h);
                        if ( komodo_DEXrecentquotefind(RecentHashes,(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)),h) < 0 && komodo_DEXrecentquotefind(RequestHashes,(int32_t)(sizeof(RequestHashes)/sizeof(*RequestHashes)),h) < 0 )
                        {
                            komodo_DEXrecentquoteadd(RequestHashes,(int32_t)(sizeof(RequestHashes)/sizeof(*RequestHashes)),h);
                            fprintf(stderr,"%08x ",h);
                            flag++;
                        }
                    }
                    if ( flag != 0 )
                    {
                        fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
                        fprintf(stderr," recv at %u from (%s) PULL these\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str());
                    }
                } else fprintf(stderr,"ping packetsize error %d != %d\n",len,offset+2*4);
            }
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
    len = komodo_DEXrecentquotes(timestamp,ping,len,recentquotes,maxquotes);
    return(len);
}

void komodo_DEXpoll(CNode *pto)
{
    std::vector<uint8_t> packet; uint8_t quote[KOMODO_DEX_QUOTESIZE]; uint32_t i,timestamp;
    timestamp = (uint32_t)time(NULL);
komodo_DEXrecentpackets(timestamp,pto,pto->recentquotes,(int32_t)(sizeof(pto->recentquotes)/sizeof(*pto->recentquotes)));
    if ( timestamp > pto->dexlastping+KOMODO_DEX_LOCALHEARTBEAT )
    {
        if ( (rand() % 3000) == 0 ) // eventually via api
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

