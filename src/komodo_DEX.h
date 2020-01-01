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
#define KOMODO_DEX_MAXHOPS 10 // most distant node pair after push phase
#define KOMODO_DEX_MAXLAG (60 + KOMODO_DEX_LOCALHEARTBEAT*KOMODO_DEX_MAXHOPS)
#define KOMODO_DEX_RELAYDEPTH 3 // increase as <avepeers> root of network size increases
#define KOMODO_DEX_TXPOWMASK 0x1    // should be 0x1ffff for approx 1 sec per tx
#define KOMODO_DEX_PURGETIME 3600

#define KOMODO_DEX_HASHLOG2 13
#define KOMODO_DEX_HASHSIZE (1 << KOMODO_DEX_HASHLOG2) // effective limit of sustained datablobs/sec
#define KOMODO_DEX_HASHMASK (KOMODO_DEX_HASHSIZE - 1)

struct DEX_datablob
{
    bits256 hash;
    uint64_t peermask;
    uint32_t recvtime;
    std::vector<uint8_t> packet;
};
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
    variable length quote
    add 'B' bootstrap query funcid ("blocks" of shorthashes?)
    broadcast api should check existing shorthash and error if collision
    improve komodo_DEXpeerpos
 */

static uint32_t Got_Recent_Quote,DEX_totalsent,DEX_totalrecv,DEX_totaladd,DEX_duplicate,DEX_lookup32,DEX_collision32,DEX_add32; // perf metrics
static uint32_t RequestHashes[KOMODO_DEX_MAXLAG * KOMODO_DEX_HASHSIZE - 1]; // pendings

static uint32_t RecentHashes[KOMODO_DEX_PURGETIME][KOMODO_DEX_HASHSIZE]; // bound with Datablobs
static struct DEX_datablob *Datablobs[KOMODO_DEX_PURGETIME][KOMODO_DEX_HASHSIZE]; // bound with RecentHashes

uint8_t komodo_DEXpeerpos(uint32_t timestamp,int32_t peerid)
{
    if ( peerid >= 64 )
    {
        fprintf(stderr,"need to implement time based peerid.%d -> peerpos mapping\n",peerid);
        exit(1);
    }
    return(peerid);
}

uint32_t komodo_DEXquotehash(bits256 &hash,uint8_t *msg,int32_t len)
{
    vcalc_sha256(0,hash.bytes,&msg[1],len-1);
    return(hash.uints[0]);
}

uint32_t komodo_DEXtotal(int32_t &total)
{
    uint32_t i,j,totalhash = 0;
    total = 0;
    for (j=0; j<KOMODO_DEX_PURGETIME; j++)
    {
        for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
        {
            if ( RecentHashes[j][i] != 0 )
            {
                total++;
                totalhash ^= RecentHashes[j][i];
            }
        }
    }
    return(totalhash);
}

int32_t komodo_DEXpurge(uint32_t cutoff)
{
    static uint32_t prevtotalhash;
    int32_t i,n=0,modval,total; uint8_t relay,funcid,*msg; uint32_t t,totalhash; struct DEX_datablob *ptr;
    modval = (cutoff % KOMODO_DEX_PURGETIME);
    for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
    {
        if ( RecentHashes[modval][i] != 0 )
        {
            if ( (ptr= Datablobs[modval][i]) != 0 )
            {
                msg = &ptr->packet[0];
                relay = msg[0];
                funcid = msg[1];
                iguana_rwnum(0,&msg[2],sizeof(t),&t);
                if ( t != cutoff )
                    fprintf(stderr,"modval.%d unexpected purge.%d t.%u vs cutoff.%u\n",modval,i,t,cutoff);
                else
                {
                    RecentHashes[modval][i] = 0;
                    Datablobs[modval][i] = 0;
                    ptr->packet.clear();
                    del ptr->packet;
                    memset(ptr,0,sizeof(*ptr));
                    free(ptr);
                    n++;
                }
            } else fprintf(stderr,"modval.%d unexpected size.%d %d t.%u vs cutoff.%u\n",modval,(int32_t)ptr->packet.size(),i,t,cutoff);
        }
    }
    totalhash = komodo_DEXtotal(total);
    if ( n != 0 || totalhash != prevtotalhash )
    {
        fprintf(stderr,"DEXpurge.%d for t.%u -> n.%d, total.%d %08x R.%d S.%d A.%d duplicates.%d | L.%d A.%d coll.%d \n",modval,cutoff,n,total,totalhash,DEX_totalrecv,DEX_totalsent,DEX_totaladd,DEX_duplicate,DEX_lookup32,DEX_add32,DEX_collision32);
        prevtotalhash = totalhash;
    }
    return(n);
}

int32_t komodo_DEXadd32(uint32_t hashtable[],int32_t hashsize,uint32_t shorthash)
{
    int32_t ind = (shorthash % hashsize);
    hashtable[ind] = shorthash;
    DEX_add32++;
    return(ind);
}

int32_t komodo_DEXfind32(uint32_t hashtable[],int32_t hashsize,uint32_t shorthash,int32_t clearflag)
{
    int32_t ind = (shorthash % hashsize);
    DEX_lookup32++;
    if ( hashtable[ind] == shorthash )
    {
        if ( clearflag != 0 )
            hashtable[ind] = 0;
        return(ind);
    }
    else if ( hashtable[ind] != 0 )
    {
        //fprintf(stderr,"hash32 collision at [%d] %u != %u\n",ind,hashtable[ind],shorthash);
        DEX_collision32++;
    }
    return(-1);
}

int32_t komodo_DEXfind(int32_t &openind,int32_t modval,uint32_t shorthash)
{
    uint32_t hashval; int32_t i,hashind;
    hashind = (shorthash & KOMODO_DEX_HASHMASK);
    openind = -1;
    for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
    {
        if ( (hashval= RecentHashes[modval][hashind]) == 0 )
        {
            //fprintf(stderr,"empty slot {M.%d %d}.%08x\n",modval,hashind,hashval);
            openind = hashind;
            return(-1);
        }
        else if ( hashval == shorthash )
            return(hashind);
        //fprintf(stderr,"{M.%d %d}.%08x ",modval,hashind,hashval);
        if ( ++hashind >= KOMODO_DEX_HASHSIZE )
            hashind = 0;
    }
    fprintf(stderr,"hashtable full\n");
    return(-1);
}

int32_t komodo_DEXadd(int32_t openind,uint32_t now,int32_t modval,bits256 hash,uint32_t shorthash,uint8_t *msg,int32_t len)
{
    int32_t ind; struct DEX_datablob *ptr;
    if ( openind < 0 || openind >= KOMODO_DEX_HASHSIZE )
    {
        if ( (ind= komodo_DEXfind(openind,modval,shorthash)) >= 0 )
        {
            fprintf(stderr,"%08x collision in hashtable[%d] at %d, openind.%d\n",shorthash,modval,ind,openind);
            return(ind);
        }
    }
    if ( openind < 0 || RecentHashes[modval][openind] != 0 || Datablobs[modval][openind] != 0 )
    {
        fprintf(stderr,"DEXadd openind.%d invalid or non-empty spot %08x %p\n",openind,openind >=0 ? RecentHashes[modval][openind] : 0,openind >= 0 ? Datablobs[modval][ind] : 0);
        return(-1);
    } else ind = openind;
    if ( (ptr= (struct DEX_datablob *)calloc(1,sizeof(*ptr))) != 0 )
    {
        ptr->recvtime = now;
        ptr->hash = hash;
        ptr->packet.resize(len);
        memcpy(&ptr->packet[0],msg,len);
        ptr->packet[0] = msg[0] != 0xff ? msg[0] - 1 : msg[0];
        Datablobs[modval][ind] = ptr;
        RecentHashes[modval][ind] = shorthash;
        DEX_totaladd++;
        //fprintf(stderr,"update M.%d slot.%d [%d] with %08x\n",modval,ind,ptr->packet[0],RecentHashes[modval][ind]);
        return(ind);
    }
    fprintf(stderr,"out of memory\n");
    return(-1);
}

int32_t komodo_DEXrecentpackets(uint32_t now,CNode *peer)
{
    int32_t i,j,k,modval,peerpos,n = 0; uint8_t relay,funcid,*msg; uint32_t t; struct DEX_datablob *ptr;
    peerpos = komodo_DEXpeerpos(now,peer->id);
    for (j=0; j<KOMODO_DEX_MAXHOPS*KOMODO_DEX_LOCALHEARTBEAT+1; j++)
    {
        modval = (now + 1 - j) % KOMODO_DEX_PURGETIME;
        for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
        {
            if ( RecentHashes[modval][i] != 0 )
            {
                if ( (ptr= Datablobs[modval][i]) != 0 )
                {
                    //fprintf(stderr,"found ptr at modval.%d i.%d\n",modval,i);
                    msg = &ptr->packet[0];
                    relay = msg[0];
                    funcid = msg[1];
                    iguana_rwnum(0,&msg[2],sizeof(t),&t);
                    if ( relay >= 0 && relay <= KOMODO_DEX_RELAYDEPTH && now <= t+KOMODO_DEX_LOCALHEARTBEAT )
                    {
                        if ( GETBIT(&ptr->peermask,peerpos) == 0 )
                        {
                            SETBIT(&ptr->peermask,peerpos);
                            //fprintf(stderr,"send packet.%08x to peerpos.%d\n",RecentHashes[modval][i],peerpos);
                            peer->PushMessage("DEX",ptr->packet); // pretty sure this will get there -> mark present
                            n++;
                            DEX_totalsent++;
                        }
                    }
                } else fprintf(stderr,"missing ptr at modval.%d i.%d\n",modval,i);
            }
        }
    }
    return(n);
}

int32_t komodo_DEXrecentquotes(uint32_t now,std::vector<uint8_t> &ping,int32_t offset,CNode *peer)
{
    static uint32_t recents[KOMODO_DEX_HASHSIZE * KOMODO_DEX_MAXLAG];
    int32_t i,j,modval,peerpos; uint16_t n = 0; uint8_t relay,funcid,*msg; uint32_t t; struct DEX_datablob *ptr;
    peerpos = komodo_DEXpeerpos(now,peer->id);
    for (j=0; j<KOMODO_DEX_MAXLAG; j++)
    {
        modval = (now + 1 - j) % KOMODO_DEX_PURGETIME;
        for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
        {
            if ( RecentHashes[modval][i] != 0 && (ptr= Datablobs[modval][i]) != 0 )
            {
                msg = &ptr->packet[0];
                relay = msg[0];
                funcid = msg[1];
                iguana_rwnum(0,&msg[2],sizeof(t),&t);
                if ( now < t+KOMODO_DEX_MAXLAG )
                {
                    if ( GETBIT(&ptr->peermask,peerpos) == 0 )
                    {
                        recents[n++] = RecentHashes[modval][i];
                        //fprintf(stderr,"%08x ",RecentHashes[modval][i]);
                        if ( n >= (int32_t)(sizeof(recents)/sizeof(*recents)) )
                        {
                            fprintf(stderr,"recents array filled\n");
                            break;
                        }
                    }
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

int32_t komodo_DEXgenquote(bits256 &hash,uint32_t &shorthash,std::vector<uint8_t> &quote,uint32_t timestamp,uint8_t data[],int32_t datalen)
{
    int32_t i,len = 0; uint32_t nonce;
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
            if ( nonce > 1000 )
                fprintf(stderr,"nonce.%u\n",nonce);
            break;
        }
    }
    return(len);
}

int32_t komodo_DEXgenping(std::vector<uint8_t> &ping,uint32_t timestamp,CNode *peer)
{
    int32_t len = 0;
    ping.resize(2 + sizeof(uint32_t));
    ping[len++] = 0;
    ping[len++] = 'P';
    len += iguana_rwnum(1,&ping[len],sizeof(timestamp),&timestamp);
    len = komodo_DEXrecentquotes(timestamp,ping,len,peer);
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
    std::vector<uint8_t> packet; bits256 hash; uint8_t quote[1024]; int32_t i,len; uint32_t shorthash,timestamp;
    timestamp = (uint32_t)time(NULL);
    len = (int32_t)(sizeof(quote)/sizeof(*quote));
    for (i=0; i<len; i++)
        quote[i] = (rand() >> 11) & 0xff;
    komodo_DEXgenquote(hash,shorthash,packet,timestamp,quote,len);
    komodo_DEXadd(-1,timestamp,timestamp % KOMODO_DEX_PURGETIME,hash,shorthash,&packet[0],packet.size());
    //fprintf(stderr,"issue order %08x!\n",shorthash);
}

void komodo_DEXpoll(CNode *pto)
{
    static uint32_t purgetime;
    std::vector<uint8_t> packet; uint32_t i,timestamp,shorthash,len,ptime;
    timestamp = (uint32_t)time(NULL);
    ptime = timestamp - KOMODO_DEX_PURGETIME + KOMODO_DEX_MAXLAG;
    if ( ptime > purgetime )
    {
        if ( purgetime == 0 )
            purgetime = ptime;
        else
        {
            for (; purgetime<ptime; purgetime++)
                komodo_DEXpurge(purgetime);
        }
    }
    if ( (timestamp == Got_Recent_Quote && timestamp > pto->dexlastping) || timestamp > pto->dexlastping+KOMODO_DEX_LOCALHEARTBEAT )
    {
        komodo_DEXgenping(packet,timestamp,pto);
        if ( packet.size() > 8 )
        {
            //fprintf(stderr," send ping to %s\n",pto->addr.ToString().c_str());
            pto->PushMessage("DEX",packet);
            pto->dexlastping = timestamp;
        }
        komodo_DEXrecentpackets(timestamp,pto);
  //fprintf(stderr," send at %u to (%s)\n",timestamp,pto->addr.ToString().c_str());
    }
}

int32_t komodo_DEXprocess(uint32_t now,CNode *pfrom,uint8_t *msg,int32_t len)
{
    int32_t i,j,ind,offset,peerpos,flag,modval,openind; uint16_t n; uint32_t t,h; uint8_t funcid,relay=0; bits256 hash; struct DEX_datablob *ptr;
    if ( len >= 6 )
    {
        peerpos = komodo_DEXpeerpos(now,pfrom->id);
        relay = msg[0];
        funcid = msg[1];
        iguana_rwnum(0,&msg[2],sizeof(t),&t);
        modval = (t % KOMODO_DEX_PURGETIME);
        if ( t > now+KOMODO_DEX_LOCALHEARTBEAT )
        {
            fprintf(stderr,"reject packet from future t.%u vs now.%u\n",t,now);
        }
        else if ( t < now-KOMODO_DEX_MAXLAG )
        {
            fprintf(stderr,"reject packet with too big lag t.%u vs now.%u lag.%d\n",t,now,now-t);
        }
        else if ( funcid == 'Q' )
        {
            h = komodo_DEXquotehash(hash,msg,len);
            DEX_totalrecv++;
            //fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
            //fprintf(stderr," recv at %u from (%s) >>>>>>>>>> shorthash.%08x total R%d/S%d/A%d dup.%d\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str(),h,DEX_totalrecv,DEX_totalsent,DEX_totaladd,DEX_duplicate);
            if ( (hash.uints[1] & KOMODO_DEX_TXPOWMASK) != (0x777 & KOMODO_DEX_TXPOWMASK) )
                fprintf(stderr,"reject quote due to invalid hash[1] %08x\n",hash.uints[1]);
            else if ( relay <= KOMODO_DEX_RELAYDEPTH || relay == 0xff )
            {
                if ( (ind= komodo_DEXfind(openind,modval,h)) < 0 )
                {
                    ind = komodo_DEXadd(openind,now,modval,hash,h,msg,len);
                    if ( ind >= 0 )
                        komodo_DEXfind32(RequestHashes,(int32_t)(sizeof(RequestHashes)/sizeof(*RequestHashes)),h,1);
                    Got_Recent_Quote = now;
                } else DEX_duplicate++;
                if ( (ptr= Datablobs[modval][ind]) != 0 )
                    SETBIT(&ptr->peermask,peerpos);
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
                        if ( komodo_DEXfind32(RequestHashes,(int32_t)(sizeof(RequestHashes)/sizeof(*RequestHashes)),h,0) < 0 )
                        {
                            for (j=0; j<KOMODO_DEX_MAXLAG; j++)
                            {
                                modval = (now + 1 - j) % KOMODO_DEX_PURGETIME;
                                if ( (ind= komodo_DEXfind(openind,modval,h)) >= 0 )
                                    break;
                            }
                            if ( j == KOMODO_DEX_MAXLAG )
                            {
                                komodo_DEXadd32(RequestHashes,(int32_t)(sizeof(RequestHashes)/sizeof(*RequestHashes)),h);
                                //fprintf(stderr,">>>> %08x <<<<< ",h);
                                komodo_DEXgenget(getshorthash,now,h);
                                pfrom->PushMessage("DEX",getshorthash);
                                flag++;
                            }
                        }
                        //fprintf(stderr,"%08x ",h);
                    }
                    if ( (0) && flag != 0 )
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
            for (j=0; j<KOMODO_DEX_MAXLAG; j++)
            {
                modval = (now + 1 - j) % KOMODO_DEX_PURGETIME;
                if ( (ind= komodo_DEXfind(openind,modval,h)) >= 0 && (ptr= Datablobs[modval][ind]) != 0 )
                {
                    if ( GETBIT(&ptr->peermask,peerpos) == 0 )
                    {
                        //fprintf(stderr,"send packet.%08x to peerpos.%d\n",h,peerpos);
                        SETBIT(&ptr->peermask,peerpos);
                        std::vector<uint8_t> response;
                        response = ptr->packet;
                        response[0] = 0; // squelch relaying of 'G' packets
                        pfrom->PushMessage("DEX",response);
                        DEX_totalsent++;
                        return(ptr->packet.size());
                    }
                }
            }
        }
        else if ( funcid == 'B' )
        {
            // set 'D' response based on 1 to 59+ minutes from expiration
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

