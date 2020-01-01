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

#define KOMODO_DEX_LOCALHEARTBEAT 1
#define KOMODO_DEX_RELAYDEPTH 3 // increase as <avepeers> root of network size increases
#define KOMODO_DEX_QUOTESIZE 1024
#define KOMODO_DEX_TXPOWMASK 0x1    // should be 0x1ffff
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
    optimize local quotes, openhashtables per time period
    variable length quote
    add 'B' bootstrap query funcid ("blocks" of shorthashes?)
    add nSPV remote api for bootstrap state and specific quotes and submitting new quote
    optimize peer based tracking of shorthashes with periodically reset bloom filter?
    if RequestHashes[i] doesnt arrive within timelimit, clear it. more efficient than bruteforce clearing
    broadcast api should check existing shorthash and error if collision
    use mutex
 */

std::vector<uint8_t> RecentPackets[KOMODO_DEX_QUOTETIME * 16];
uint32_t RecentHashes[sizeof(RecentPackets)/sizeof(*RecentPackets)],RequestHashes[sizeof(RecentPackets)/sizeof(*RecentPackets)];
uint32_t Got_Recent_Quote,DEX_totalsent,DEX_totalrecv,DEX_totaladd,DEX_duplicate;

uint32_t komodo_DEXquotehash(bits256 &hash,uint8_t *msg,int32_t len)
{
    vcalc_sha256(0,hash.bytes,&msg[1],len-1);
    return(hash.uints[0]);
}

uint32_t komodo_DEXtotal(int32_t &total)
{
    uint32_t i,totalhash = 0;
    total = 0;
    for (i=0; i<(sizeof(RecentHashes)/sizeof(*RecentHashes)); i++)
    {
        if ( RecentHashes[i] != 0 )
        {
            total++;
            totalhash ^= RecentHashes[i];
        }
    }
    return(totalhash);
}

int32_t komodo_DEXpurge(uint32_t cutoff) // find the openhashtables and clear/archive them
{
    int32_t i,n; uint8_t relay,funcid,*msg; uint32_t t;
    for (i=0; i<(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)); i++)
    {
        if ( RecentHashes[i] != 0 && RecentPackets[i].size() >= 6 )
        {
            msg = &RecentPackets[i][0];
            relay = msg[0];
            funcid = msg[1];
            iguana_rwnum(0,&msg[2],sizeof(t),&t);
            if ( t < cutoff )
            {
                RecentHashes[i] = 0;
                RecentPackets[i].resize(0);
                n++;
                //fprintf(stderr,"purge.%d t.%u vs cutoff.%u\n",i,t,cutoff);
            }
        }
    }
    return(n);
}

int32_t komodo_DEXrecentquoteadd(uint32_t timestamp,uint32_t recentquotes[],int32_t maxquotes,uint32_t shorthash)
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

int32_t komodo_DEXrecentquotefind(uint32_t timestamp,uint32_t recentquotes[],int32_t maxquotes,uint32_t shorthash)
{
    int32_t i;
    for (i=0; i<maxquotes; i++)
    {
        if ( shorthash == recentquotes[i] )
            return(i);
    }
    return(-1);
}

int32_t komodo_DEXfind(uint32_t timestamp,uint32_t shorthash) // change to open hashtables for each KOMODO_DEX_LOCALHEARTBEAT/2
{
return(komodo_DEXrecentquotefind(timestamp,RecentHashes,(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)),shorthash));
}

int32_t komodo_DEXadd(uint32_t now,uint32_t timestamp,uint32_t shorthash,uint8_t *msg,int32_t len)
{
    int32_t ind,total; uint32_t totalhash;
    // changes to allocate structure, place in openhashtable
    komodo_DEXpurge(now - KOMODO_DEX_QUOTETIME);
    if ( (ind= komodo_DEXrecentquoteadd(timestamp,RecentHashes,(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)),shorthash)) >= 0 )
    {
        if ( RecentPackets[ind].size() != len )
            RecentPackets[ind].resize(len);
        memcpy(&RecentPackets[ind][0],msg,len);
        RecentPackets[ind][0] = msg[0] != 0xff ? msg[0] - 1 : msg[0];
        totalhash = komodo_DEXtotal(total);
        DEX_totaladd++;
        fprintf(stderr,"update slot.%d [%d] with %08x, total.%d %08x\n",ind,RecentPackets[ind][0],RecentHashes[ind],total,totalhash);
    } else fprintf(stderr,"unexpected error: no slot available\n");
    return(ind);
}

int32_t komodo_DEXrecentquoteupdate(uint32_t timestamp,uint32_t recentquotes[],int32_t maxquotes,uint32_t shorthash) // peer info
{
    if ( komodo_DEXrecentquotefind(timestamp,recentquotes,maxquotes,shorthash) < 0 )
    {
        komodo_DEXrecentquoteadd(timestamp,recentquotes,maxquotes,shorthash);
        return(1);
    }
    return(0);
}

int32_t komodo_DEXrecentpackets(uint32_t now,CNode *pto,uint32_t recentquotes[],int32_t maxquotes)
{
    int32_t i,n = 0; uint8_t relay,funcid,*msg; uint32_t t;
    for (i=0; i<(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)); i++) // changes to only relevant hashtables
    {
        if ( RecentHashes[i] != 0 && RecentPackets[i].size() >= 6 )
        {
            msg = &RecentPackets[i][0];
            relay = msg[0];
            funcid = msg[1];
            iguana_rwnum(0,&msg[2],sizeof(t),&t);
            if ( relay >= 0 && relay <= KOMODO_DEX_RELAYDEPTH && now <= t+KOMODO_DEX_LOCALHEARTBEAT )
            {
                if ( komodo_DEXrecentquoteupdate(t,recentquotes,maxquotes,RecentHashes[i]) != 0 )
                {
                    pto->PushMessage("DEX",RecentPackets[i]); // pretty sure this will get there -> mark present
                    n++;
                    DEX_totalsent++;
                }
            }
        }
    }
    return(n);
}

int32_t komodo_DEXrecentquotes(uint32_t now,std::vector<uint8_t> &ping,int32_t offset,uint32_t recentquotes[],int32_t maxquotes)
{
    int32_t i; uint16_t n = 0; uint8_t relay,funcid,*msg; uint32_t t; uint32_t recents[4096];
    for (i=0; i<(int32_t)(sizeof(RecentHashes)/sizeof(*RecentHashes)); i++) // changes to only relevant hashtables
    {
        if ( RecentHashes[i] != 0 && RecentPackets[i].size() >= 6 )
        {
            msg = &RecentPackets[i][0];
            relay = msg[0];
            funcid = msg[1];
            iguana_rwnum(0,&msg[2],sizeof(t),&t);
            if ( now < t+10*KOMODO_DEX_LOCALHEARTBEAT+60 )
            {
                recents[n++] = RecentHashes[i];
                //fprintf(stderr,"%08x ",RecentHashes[i]);
                if ( n >= (int32_t)(sizeof(recents)/sizeof(*recents)) )
                    break;
            }
        }
    }
    ping.resize(offset + sizeof(n) + n*sizeof(uint32_t));
    offset += iguana_rwnum(1,&ping[offset],sizeof(n),&n);
    for (i=0; i<n; i++)
        offset += iguana_rwnum(1,&ping[offset],sizeof(recents[i]),&recents[i]);
    return(offset);
}

int32_t komodo_DEXgenquote(uint32_t &shorthash,std::vector<uint8_t> &quote,uint32_t timestamp,uint8_t data[],int32_t datalen)
{
    int32_t i,len = 0; bits256 hash; uint32_t nonce;
    quote.resize(2 + sizeof(uint32_t) + datalen + sizeof(nonce)); // send list of recently added shorthashes
    quote[len++] = KOMODO_DEX_RELAYDEPTH;
    quote[len++] = 'Q';
    len += iguana_rwnum(1,&quote[len],sizeof(timestamp),&timestamp);
    for (i=0; i<datalen; i++)
        quote[len++] = data[i];
    len += sizeof(nonce);
    for (nonce=0; nonce<0xffffffff; nonce++)
    {
        iguana_rwnum(1,&quote[len - sizeof(nonce)],sizeof(nonce),&nonce);
        shorthash = komodo_DEXquotehash(hash,&quote[0],len);
        if ( (hash.uints[1] & KOMODO_DEX_TXPOWMASK) == (0x777 & KOMODO_DEX_TXPOWMASK) )
        {
            fprintf(stderr,"nonce.%u\n",nonce);
            break;
        }
    }
    return(len);
}

int32_t komodo_DEXgenping(std::vector<uint8_t> &ping,uint32_t timestamp,uint32_t *recentquotes,int32_t maxquotes)
{
    int32_t len = 0;
    ping.resize(2 + sizeof(uint32_t));
    ping[len++] = 0;
    ping[len++] = 'P';
    len += iguana_rwnum(1,&ping[len],sizeof(timestamp),&timestamp);
    len = komodo_DEXrecentquotes(timestamp,ping,len,recentquotes,maxquotes);
    return(len);
}

int32_t komodo_DEXgenget(std::vector<uint8_t> &getshorthash,uint32_t timestamp,uint32_t shorthash)
{
    int32_t len = 0;
    getshorthash.resize(2 + 2*sizeof(uint32_t));
    getshorthash[len++] = 0;
    getshorthash[len++] = 'G';
    len += iguana_rwnum(1,&getshorthash[len],sizeof(timestamp),&timestamp);
    len += iguana_rwnum(1,&getshorthash[len],sizeof(shorthash),&shorthash);
    return(len);
}

void komodo_DEXbroadcast(char *hexstr)
{
    std::vector<uint8_t> packet; uint8_t quote[KOMODO_DEX_QUOTESIZE]; int32_t i,len; uint32_t shorthash,timestamp;
    timestamp = (uint32_t)time(NULL);
    len = KOMODO_DEX_QUOTESIZE;
    for (i=0; i<len; i++)
        quote[i] = (rand() >> 11) & 0xff;
    komodo_DEXgenquote(shorthash,packet,timestamp,quote,len);
    komodo_DEXadd(timestamp,timestamp,shorthash,&packet[0],packet.size());
    fprintf(stderr,"issue order %08x!\n",shorthash);
}

void komodo_DEXpoll(CNode *pto)
{
    std::vector<uint8_t> packet; uint32_t i,timestamp,shorthash,len;
    timestamp = (uint32_t)time(NULL);
    if ( (timestamp == Got_Recent_Quote && timestamp > pto->dexlastping) || timestamp > pto->dexlastping+KOMODO_DEX_LOCALHEARTBEAT )
    {
        komodo_DEXgenping(packet,timestamp,pto->recentquotes,(int32_t)(sizeof(pto->recentquotes)/sizeof(*pto->recentquotes)));
        if ( packet.size() > 8 )
        {
            //fprintf(stderr," send ping to %s\n",pto->addr.ToString().c_str());
            pto->PushMessage("DEX",packet);
            pto->dexlastping = timestamp;
        }
        komodo_DEXrecentpackets(timestamp,pto,pto->recentquotes,(int32_t)(sizeof(pto->recentquotes)/sizeof(*pto->recentquotes)));
  //fprintf(stderr," send at %u to (%s)\n",timestamp,pto->addr.ToString().c_str());
    }
    //if ( (rand() % 100) == 0 ) // some delay to allow peer to send updated ping
}

int32_t komodo_DEXprocess(uint32_t now,CNode *pfrom,uint8_t *msg,int32_t len)
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
            DEX_totalrecv++;
            fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
            fprintf(stderr," recv at %u from (%s) >>>>>>>>>> shorthash.%08x total R%d/S%d/A%d dup.%d\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str(),h,DEX_totalrecv,DEX_totalsent,DEX_totaladd,DEX_duplicate);
            if ( (hash.uints[1] & KOMODO_DEX_TXPOWMASK) != (0x777 & KOMODO_DEX_TXPOWMASK) )
                fprintf(stderr,"reject quote due to invalid hash[1] %08x\n",hash.uints[1]);
            else if ( relay <= KOMODO_DEX_RELAYDEPTH || relay == 0xff )
            {
                komodo_DEXrecentquoteupdate(t,pfrom->recentquotes,(int32_t)(sizeof(pfrom->recentquotes)/sizeof(*pfrom->recentquotes)),h);
                if ( komodo_DEXfind(h) < 0 )
                {
                    komodo_DEXadd(now,t,h,msg,len);
                    Got_Recent_Quote = now;
                } else DEX_duplicate++;
            } else fprintf(stderr,"unexpected relay.%d\n",relay);
        }
        else if ( funcid == 'P' )
        {
            std::vector<uint8_t> getshorthash;
            if ( len >= 8 )
            {
                offset = 6;
                offset += iguana_rwnum(0,&msg[offset],sizeof(n),&n);
                if ( offset+n*sizeof(uint32_t) == len )
                {
                    for (flag=i=0; i<n; i++)
                    {
                        offset += iguana_rwnum(0,&msg[offset],sizeof(h),&h);
                        if ( komodo_DEXfind(h) < 0 && komodo_DEXrecentquotefind(t,RequestHashes,(int32_t)(sizeof(RequestHashes)/sizeof(*RequestHashes)),h) < 0 )
                        {
                            komodo_DEXrecentquoteadd(t,RequestHashes,(int32_t)(sizeof(RequestHashes)/sizeof(*RequestHashes)),h);
                            fprintf(stderr,">>>> %08x <<<<< ",h);
                            komodo_DEXgenget(getshorthash,now,h);
                            pfrom->PushMessage("DEX",getshorthash);
                            flag++;
                        }
                        //fprintf(stderr,"%08x ",h);
                    }
                    if ( flag != 0 )
                    {
                        fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
                        fprintf(stderr," recv at %u from (%s) PULL these\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str());
                    } else if ( (0) && n > 0 )
                        fprintf(stderr,"ping from %s\n",pfrom->addr.ToString().c_str());
                } else fprintf(stderr,"ping packetsize error %d != %d, offset.%d n.%d\n",len,offset+n*4,offset,n);
            }
        }
        else if ( funcid == 'G' )
        {
            iguana_rwnum(0,&msg[6],sizeof(h),&h);
            //fprintf(stderr," f.%c t.%u [%d] get.%08x ",funcid,t,relay,h);
            //fprintf(stderr," recv at %u from (%s)\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str());
            if ( (ind= komodo_DEXfind(h)) >= 0 && RecentPackets[ind].size() > 0 )
            {
                //fprintf(stderr,"response.[%d] <- slot.%d\n",(int32_t)RecentPackets[ind].size(),ind);
                if ( komodo_DEXrecentquoteupdate(t,pfrom->recentquotes,(int32_t)(sizeof(pfrom->recentquotes)/sizeof(*pfrom->recentquotes)),h) != 0 )
                {
                    std::vector<uint8_t> response;
                    response = RecentPackets[ind];
                    response[0] = 0; // squelch relaying of 'G' packets
                    pfrom->PushMessage("DEX",response);
                    DEX_totalsent++;
                    return(response.size());
                }
            }
        }
        else if ( funcid == 'B' )
        {
            // set 'D' response based on 1 to 59+ minutes from expiration
        }
        else if ( funcid == 'D' ) // block of shorthashes
        {
            // process them
        }
        else if ( funcid == 'I' )
        {
            // summary info of recent blocks
        }
   }
    return(0);
}

void komodo_DEXmsg(CNode *pfrom,std::vector<uint8_t> request) // received a packet
{
    int32_t len; std::vector<uint8_t> response; bits256 hash; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= request.size()) > 0 )
    {
        komodo_DEXprocess(timestamp,pfrom,&request[0],len);
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

