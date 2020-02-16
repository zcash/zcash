/******************************************************************************
 * Copyright © 2014-2020 The SuperNET Developers.                             *
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

/*
 MAKE SURE YOU NTP sync your node, precise timestamps are assumed
 
 _functions() assume DEX_globalmutex is locked when it is called
 functions() assume that DEX_globalmutes is not locked when it is called and must lock/unlock to call _functions()
 
 message format: <relay depth> <funcid> <timestamp> <payload>
 
 <payload> is the datablob for a 'Q' quote or <uint16_t> + n * <uint32_t> for a 'P' ping of recent shorthashes
 
 To achieve very fast performance a hybrid push/poll/pull gossip protocol is used. All new quotes are broadcast KOMODO_DEX_RELAYDEPTH levels deep, this gets the quote to large percentage of nodes assuming <ave peers> ^ KOMODO_DEX_RELAYDEPTH power is approx totalnodes/2. Nodes in the broacast "cone" will receive a new quote in less than a second in most cases.
 
 For efficiency it is better to aim for one third to half of the nodes during the push phase. The higher the KOMODO_DEX_RELAYDEPTH the faster a quote will be broadcast, but the more redundant packets will be sent. If each node would be able to have a full network connectivity map, each node could locally simulate packet propagation and select a subset of peers with the maximum propagation and minimum overlap. However, such an optimization requires up to date and complete network topography and the current method has the advantage of being much simpler and reasonably efficient.
 
 Additionally, all nodes will be broadcasting to their immediate peers, the most recent quotes not known to be known by the destination, which will allow the receiving peer to find any quotes they are missing and request it directly. At the cost of the local broadcasting, all the nodes will be able to directly request any quote they didnt get during the push phase.
 
 For sparsely connected nodes, as the pull process propagates a new quote, they will eventually also see the new quote. Worst case would be the last node in a singly connected chain of peers. Assuming most all nodes will have 3 or more peers, then most all nodes will get a quote broadcast in a few multiples of KOMODO_DEX_LOCALHEARTBEAT
 
 
todo:
 permissioned list of pubkeys
 ignore lagging REQUEST commands (maybe others too)
    updatentz argv[1] -> system(getblockhash) -> extract last N heights, compare to DEX_list, post changed, cancel if reorged.
    notarizer post list of active coins, register 01pubkey to 03pubkey/handle/address, scan from last notarization, sortition select, identify forks
 
 

 the payload is rejected, so it is in the orderbook falsely. i guess i need to check for such wrong senders and not put it in the orderbook, or just reject it completely [wrong sender broadcast]

payments:
 paying a specific "fee" could allow bypassing the txpow requirement (as you proposed with the paywall)
 One useful feature might be auction type of selling were highest bidder win if there are no new bids given like an hour or so? DEX_auction
 yes, video subscriptions wouldnt be very hard at all
 there is no place to upload as the lifetime is one hour, however one thing i wrote above is you can submit (encrypted) data to a service provider who will store it for you for a specified amount of time, with progress payments along the way
 
 
later:
 detect peer restarted and peerclear
 defend against memory overflow
 defend against pingpong attack with pongbits
 shamirs sharding of data
 parameterize network #defines heartbeat, maxhops, maxlag, relaydepth, peermasksize, hashlog2, purgetime
 detect evil peer: 'Q' is directly protected by txpow, G is a fixed size, so it cant be very big and invalid request can be detected. 'P' message will lead to 'G' queries that cannot be answered, 'R' needs to have high priority
 */

uint8_t *komodo_DEX_encrypt(uint8_t **allocatedp,uint8_t *data,int32_t *datalenp,bits256 pubkey,bits256 privkey);
uint8_t *komodo_DEX_decrypt(uint8_t *senderpub,uint8_t **allocatedp,uint8_t *data,int32_t *datalenp,bits256 privkey);
void komodo_DEX_pubkey(bits256 &pub0);
void komodo_DEX_privkey(bits256 &priv0);
int32_t komodo_DEX_request(int32_t priority,uint32_t shorthash,uint32_t timestamp,char *tagA,char *tagB);

#define KOMODO_DEX_PURGELIST 0

#define KOMODO_DEX_BLAST (iter/3)  // define as iter to make it have 10 different priorities, as 0 to blast diff 0
#define KOMODO_DEX_ROUTESIZE 6 // (relaydepth + funcid + timestamp)

#define KOMODO_DEX_LOCALHEARTBEAT 1
#define KOMODO_DEX_MAXHOPS 10 // most distant node pair after push phase
#define KOMODO_DEX_MAXLAG 60
#define KOMODO_DEX_RELAYDEPTH ((uint8_t)KOMODO_DEX_MAXHOPS) // increase as <avepeers> root of network size increases
#define KOMODO_DEX_MAXFANOUT ((uint8_t)6)

#define KOMODO_DEX_HASHLOG2 14
#define KOMODO_DEX_MAXPERSEC (1 << KOMODO_DEX_HASHLOG2) // effective limit of sustained datablobs/sec
//#define KOMODO_DEX_HASHMASK (KOMODO_DEX_MAXPERSEC - 1)
#define KOMODO_DEX_PURGETIME (3600)
#define KOMODO_DEX_MAXPING (KOMODO_DEX_MAXPERSEC / 17)

#define KOMOD_DEX_PEERMASKSIZE 128
#define KOMODO_DEX_MAXPEERID (KOMOD_DEX_PEERMASKSIZE * 8)
#define SECONDS_IN_DAY (24*3600)
#define KOMODO_DEX_PEERPERIOD KOMODO_DEX_PURGETIME // must be evenly divisible into SECONDS_IN_DAY
#define KOMODO_DEX_PEEREPOCHS (SECONDS_IN_DAY / KOMODO_DEX_PEERPERIOD)

#define KOMODO_DEX_TAGSIZE 16   // (33 / 2) rounded down
#define KOMODO_DEX_MAXKEYSIZE 34 // destpub 1+33, or tagAB 1+16 + 1 + 16 -> both are 34
#define KOMODO_DEX_MAXINDEX 64
#define KOMODO_DEX_MAXINDICES 4 // [0] destpub, [1] tagA, [2] tagB, [3] two tags order dependent

#define KOMODO_DEX_MAXPACKETSIZE (1 << 20)
#define KOMODO_DEX_MAXPRIORITY 32 // a millionX should be enough, but can be as high as 64 - KOMODO_DEX_TXPOWBITS
#define KOMODO_DEX_TXPOWBITS 4    // should be 11 for approx 1 sec per tx
#define KOMODO_DEX_VIPLEVEL 5   // if all are VIP it will try to 100% sync all nodes
#define KOMODO_DEX_CMDPRIORITY (KOMODO_DEX_VIPLEVEL + 2) // minimum extra priority for commands
#define KOMODO_DEX_POLLVIP 30

#define KOMODO_DEX_TXPOWDIVBITS 12 // each doubling of size, increases minpriority
#define KOMODO_DEX_TXPOWMASK ((1LL << KOMODO_DEX_TXPOWBITS)-1)
//#define KOMODO_DEX_CREATEINDEX_MINPRIORITY 6 // 64x baseline diff -> approx 1 minute if baseline is 1 second diff

#define KOMODO_DEX_FILEBUFSIZE 10000
#define KOMODO_DEX_STREAMSIZE 100
#define KOMODO_DEX_ANONSIZE 1024

#define _komodo_DEXquotehash(hash,len) (uint32_t)(((hash).ulongs[0] >> (KOMODO_DEX_TXPOWBITS + komodo_DEX_sizepriority(len))))
#define komodo_DEX_id(ptr) _komodo_DEXquotehash(ptr->hash,ptr->datalen)

#define GENESIS_PUBKEYSTR ((char *)"1259ec21d31a30898d7cd1609f80d9668b4778e3d97e941044b39f0c44d2e51b")
#define GENESIS_PRIVKEYSTR ((char *)"88a71671a6edd987ad9e9097428fc3f169decba3ac8f10da7b24e0ca16803b70")

struct DEX_datablob
{
    UT_hash_handle hh;
    struct DEX_datablob *nexts[KOMODO_DEX_MAXINDICES],*prevs[KOMODO_DEX_MAXINDICES];
    bits256 hash;
    uint8_t peermask[KOMOD_DEX_PEERMASKSIZE];
    uint32_t recvtime,cancelled,lastlist,shorthash;
    int32_t datalen;
    int8_t priority,sizepriority;
    uint8_t numsent,offset,linkmask,requested;
    uint8_t data[];
};

struct DEX_index_list { struct DEX_datablob *nexts[KOMODO_DEX_MAXINDICES],*prevs[KOMODO_DEX_MAXINDICES]; };

struct DEX_index
{
    UT_hash_handle hh;
    struct DEX_datablob *head,*tail;
    uint8_t keylen;
    uint8_t key[KOMODO_DEX_MAXKEYSIZE];
} *DEX_destpubs,*DEX_tagAs,*DEX_tagBs,*DEX_tagABs;

struct DEX_orderbookentry
{
    bits256 hash;
    double price;
    int64_t amountA,amountB;
    uint32_t timestamp,shorthash;
    uint8_t pubkey33[33],priority;
};

// start perf metrics
static double DEX_lag,DEX_lag2,DEX_lag3;
static int64_t DEX_totalsent,DEX_totalrecv,DEX_totaladd,DEX_duplicate;
static int64_t DEX_lookup32,DEX_collision32,DEX_add32,DEX_maxlag;
static int64_t DEX_Numpending,DEX_freed,DEX_truncated;
// end perf metrics

static uint32_t Got_Recent_Quote;
bits256 DEX_pubkey,GENESIS_PUBKEY,GENESIS_PRIVKEY;
pthread_mutex_t DEX_globalmutex;

static struct DEX_globals
{
    int32_t DEX_peermaps[KOMODO_DEX_PEEREPOCHS][KOMODO_DEX_MAXPEERID];
    uint32_t Pendings[KOMODO_DEX_MAXLAG * KOMODO_DEX_MAXPERSEC - 1];
    
    struct DEX_datablob *Hashtables[KOMODO_DEX_PURGETIME];
#if KOMODO_DEX_PURGELIST
    struct DEX_datablob *Purgelist[KOMODO_DEX_MAXPERSEC * KOMODO_DEX_MAXLAG];
    int32_t numpurges;
#endif
    FILE *fp;
} *G;

void komodo_DEX_pubkeyupdate()
{
    komodo_DEX_pubkey(DEX_pubkey);
    /*{
        char str[65];
        fprintf(stderr,"new DEX_pubkey %s\n",bits256_str(str,DEX_pubkey));
    }*/
}

void komodo_DEX_init()
{
    static int32_t onetime; int32_t modval;
    if ( onetime == 0 )
    {
        decode_hex(GENESIS_PUBKEY.bytes,sizeof(GENESIS_PUBKEY),GENESIS_PUBKEYSTR);
        decode_hex(GENESIS_PRIVKEY.bytes,sizeof(GENESIS_PRIVKEY),GENESIS_PRIVKEYSTR);
        pthread_mutex_init(&DEX_globalmutex,0);
        komodo_DEX_pubkeyupdate();
        G = (struct DEX_globals *)calloc(1,sizeof(*G));
        if ( (G->fp= fopen((char *)"DEX.log",(char *)"wb")) == 0 )
        {
            fprintf(stderr,"FATAL ERROR couldnt open DEX.log file\n");
            exit(-1);
        }
        char str[67]; fprintf(stderr,"DEX_pubkey.(01%s) sizeof DEX_globals %ld\n\n",bits256_str(str,DEX_pubkey),sizeof(*G));
        onetime = 1;
    }
}

uint32_t komodo_DEX_listid()
{
    static uint32_t listid;
    return(++listid);
}

int32_t komodo_DEX_islagging()
{
    if ( (DEX_lag > DEX_lag2 && DEX_lag2 > DEX_lag3 && DEX_lag > KOMODO_DEX_MAXLAG/KOMODO_DEX_MAXHOPS && DEX_Numpending >= KOMODO_DEX_MAXPERSEC/2) || DEX_Numpending >= KOMODO_DEX_MAXPERSEC )
        return(1);
    else return(0);
}

int32_t komodo_DEX_sizepriority(uint32_t packetsize)
{
    int32_t n,priority = 0;
    n = (packetsize >> KOMODO_DEX_TXPOWDIVBITS);
    while ( n != 0 )
    {
        priority++;
        n >>= 1;
    }
    if ( 0 && priority != 0 )
        fprintf(stderr,"sizepriority.%d from packetsize.%d\n",priority,packetsize);
    return(priority);
}

int32_t komodo_DEX_countbits(uint64_t h)
{
    int32_t i;
    for (i=0; i<64; i++,h>>=1)
        if ( (h & 1) != 0 )
            return(i);
    return(i);
}

int32_t komodo_DEX_priority(uint64_t h,int32_t packetsize)
{
    int32_t i,sizepriority = komodo_DEX_sizepriority(packetsize);
    h >>= KOMODO_DEX_TXPOWBITS;
    i = komodo_DEX_countbits(h);
    return(i - sizepriority);
}

uint32_t komodo_DEXquotehash(bits256 &hash,uint8_t *msg,int32_t len)
{
    bits256 tmp,tmp2;
    vcalc_sha256(0,tmp.bytes,&msg[1],len-1);
    tmp2 = curve25519(tmp,curve25519_basepoint9());
    vcalc_sha256(0,hash.bytes,tmp2.bytes,sizeof(tmp2));
    return(_komodo_DEXquotehash(hash,len));
}

uint16_t _komodo_DEXpeerpos(uint32_t timestamp,int32_t peerid)
{
    // this needs to maintain the bitposition from epoch to epoch, to preserve the accuracy of the GETBIT()
    int32_t epoch,*peermap; uint16_t i;
    epoch = ((timestamp % SECONDS_IN_DAY) / KOMODO_DEX_PEERPERIOD);
    peermap = G->DEX_peermaps[epoch];
    for (i=1; i<KOMODO_DEX_MAXPEERID; i++)
    {
        if ( peermap[i] == 0 )
        {
            peermap[i] = peerid;
            //fprintf(stderr,"epoch.%d [%d] <- peerid.%d\n",epoch,i,peerid);
            return(i);
        }
        else if ( peermap[i] == peerid )
            return(i);
    }
    fprintf(stderr,"DEX_peerpos t.%u peerid.%d has no space left, seems a sybil attack underway. wait 5 minutes\n",timestamp,peerid);
    return(0xffff);
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
        //fprintf(stderr,"hash32 collision at [%d] %u != %u, at worst causes retransmission of packet\n",ind,hashtable[ind],shorthash);
        DEX_collision32++;
        if ( clearflag != 0 )
            hashtable[ind] = 0;
    }
    return(-1);
}

#define DL_APPENDind(head,add,ind)                                                                    \
DL_APPEND2ind(head,add,prevs,nexts,ind)

#define DL_APPEND2ind(head,add,prevs,nexts,ind)                                                         \
do {                                                                                           \
if (head) {                                                                                  \
(add)->prevs[ind] = (head)->prevs[ind];                                                              \
(head)->prevs[ind]->nexts[ind] = (add);                                                              \
(head)->prevs[ind] = (add);                                                                    \
(add)->nexts[ind] = NULL;                                                                      \
} else {                                                                                     \
(head)=(add);                                                                            \
(head)->prevs[ind] = (head);                                                                   \
(head)->nexts[ind] = NULL;                                                                     \
}                                                                                            \
} while (0)

#define DL_DELETEind(head,del,ind)                                                                    \
DL_DELETE2ind(head,del,prevs,nexts,ind)

#define DL_DELETE2ind(head,del,prevs,nexts,ind)                                                         \
do {                                                                                           \
assert((del)->prevs[ind] != NULL);                                                                 \
if ((del)->prevs[ind] == (del)) {                                                                  \
(head)=NULL;                                                                             \
} else if ((del)==(head)) {                                                                  \
(del)->nexts[ind]->prevs[ind] = (del)->prevs[ind];                                                         \
(head) = (del)->nexts[ind];                                                                    \
} else {                                                                                     \
(del)->prevs[ind]->nexts[ind] = (del)->nexts[ind];                                                         \
if ((del)->nexts[ind]) {                                                                       \
(del)->nexts[ind]->prevs[ind] = (del)->prevs[ind];                                                     \
} else {                                                                                 \
(head)->prevs[ind] = (del)->prevs[ind];                                                          \
}                                                                                        \
}                                                                                            \
} while (0)

#define DL_FOREACHind(tail,el,ind)                                                                    \
DL_FOREACH2ind(tail,el,prevs,ind)

#define DL_FOREACH2ind(tail,el,prevs,ind)                                                              \
for(el=tail;el;el=(el)->prevs[ind])

void _komodo_DEX_enqueue(int32_t ind,struct DEX_index *index,struct DEX_datablob *ptr)
{
    if ( GETBIT(&ptr->linkmask,ind) != 0 )
    {
        fprintf(stderr,"duplicate link attempted ind.%d ptr.%p listid.%d\n",ind,ptr,ptr->lastlist);
        return;
    }
    if ( ptr->datalen < KOMODO_DEX_ROUTESIZE )
    {
        fprintf(stderr,"already truncated datablob cant be linked ind.%d ptr.%p listid.%d\n",ind,ptr,ptr->lastlist);
        return;
    }
    DL_APPENDind(index->head,ptr,ind);
    index->tail = ptr;
    SETBIT(&ptr->linkmask,ind);
}

uint32_t _komodo_DEXtotal(int32_t *histo,int32_t &total)
{
    struct DEX_datablob *ptr,*tmp; int32_t priority; uint32_t modval,n,hash,totalhash = 0;
    total = 0;
    for (modval=0; modval<KOMODO_DEX_PURGETIME; modval++)
    {
        hash = n = 0;
        HASH_ITER(hh,G->Hashtables[modval],ptr,tmp)
        {
            n++;
            hash ^= ptr->shorthash;
            if ( (priority= ptr->priority) > 13 )
                priority = 13;
            histo[priority]++;
        }
        totalhash ^= hash;
        total += n;
    }
    return(totalhash);
}

uint32_t _komodo_DEX_peerclear(int32_t peerpos)
{
    struct DEX_datablob *ptr,*tmp; uint32_t modval,n=0,hash,totalhash = 0;
    for (modval=0; modval<KOMODO_DEX_PURGETIME; modval++)
    {
        HASH_ITER(hh,G->Hashtables[modval],ptr,tmp)
        {
            if ( ptr->priority >= KOMODO_DEX_VIPLEVEL )
            {
                n++;
                CLEARBIT(ptr->peermask,peerpos);
            }
        }
    }
    return(n);
}

int32_t _komodo_DEX_purgeindex(int32_t ind,struct DEX_index *index,uint32_t cutoff)
{
    uint32_t t; int32_t n=0; struct DEX_datablob *ptr = 0;
    ptr = index->head;
    while ( ptr != 0 )
    {
        if ( GETBIT(&ptr->linkmask,ind) == 0 )
        {
            fprintf(stderr,"unlink attempted for clearbit ind.%d ptr.%p\n",ind,ptr);
            break;
        }
        iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
        if ( t <= cutoff )
        {
            if ( index->tail == index->head )
                index->tail = 0;
            DL_DELETEind(index->head,ptr,ind);
            n++;
            CLEARBIT(&ptr->linkmask,ind);
            if ( ptr->linkmask == 0 )
            {
#if KOMODO_DEX_PURGELIST
                G->Purgelist[G->numpurges++] = ptr;
#else
                free(ptr);
                DEX_freed++;
#endif
             } // else fprintf(stderr,"%p ind.%d linkmask.%x\n",ptr,ind,ptr->linkmask);
             ptr = index->head;
        }
        else
        {
            if ( 0 && t > cutoff )
                fprintf(stderr,"purgeindex.%d cutoff %u got future t.%u\n",ind,cutoff,t);
            break;
        }
    }
    return(n);
}

int32_t _komodo_DEXpurge(uint32_t cutoff)
{
    static uint32_t prevtotalhash,lastadd,lastcutoff;
    int32_t i,n=0,modval,total,offset,h; int64_t lagsum = 0; uint8_t relay,funcid,*msg; uint32_t t,totalhash,purgehash=0; struct DEX_datablob *ptr,*tmp;
    if ( (cutoff % SECONDS_IN_DAY) == (SECONDS_IN_DAY-1) )
    {
        fprintf(stderr,"reset peermaps at end of day!\n");
        memset(G->DEX_peermaps,0,sizeof(G->DEX_peermaps));
    }
    modval = (cutoff % KOMODO_DEX_PURGETIME);
    HASH_ITER(hh,G->Hashtables[modval],ptr,tmp)
    {
        msg = &ptr->data[0];
        relay = msg[0];
        funcid = msg[1];
        iguana_rwnum(0,&msg[2],sizeof(t),&t);
        if ( t <= cutoff )
        {
            if ( ptr->recvtime >= t )
                lagsum += (ptr->recvtime - t);
            purgehash ^= ptr->shorthash;
            HASH_DELETE(hh,G->Hashtables[modval],ptr);
            ptr->datalen = 0;
            CLEARBIT(&ptr->linkmask,KOMODO_DEX_MAXINDICES);
            DEX_truncated++;
            n++;
        } // else fprintf(stderr,"modval.%d unexpected purge.%d t.%u vs cutoff.%u\n",modval,i,t,cutoff);
    }
    //totalhash = _komodo_DEXtotal(total);
    if ( (modval % 60) == 0 ) // n != 0 ||  //totalhash != prevtotalhash )
    {
        int32_t histo[64];
        memset(histo,0,sizeof(histo));
        totalhash = 0;
        total = 0;
        if ( (modval % 60) == 0 )
            totalhash = _komodo_DEXtotal(histo,total);
        fprintf(stderr,"%d: del.%d %08x, RAM.%d %08x R.%lld S.%lld A.%lld dup.%lld | L.%lld A.%lld coll.%lld | lag  %.3f (%.4f %.4f %.4f) err.%lld pend.%lld T/F %lld/%lld | ",modval,n,purgehash,total,totalhash,(long long)DEX_totalrecv,(long long)DEX_totalsent,(long long)DEX_totaladd,(long long)DEX_duplicate,(long long)DEX_lookup32,(long long)DEX_add32,(long long)DEX_collision32,n>0?(double)lagsum/n:0,DEX_lag,DEX_lag2,DEX_lag3,(long long)DEX_maxlag,(long long)DEX_Numpending,(long long)DEX_truncated,(long long)DEX_freed);
        for (i=13; i>=0; i--)
            fprintf(stderr,"%.0f ",(double)histo[i]);//1000.*histo[i]/(total+1)); // expected 1 1 2 5 | 10 10 10 10 10 | 10 9 9 7 5
        fprintf(stderr,"%s %lld/sec\n",komodo_DEX_islagging()!=0?"LAG":"",cutoff>lastcutoff ? (long long)(DEX_totaladd - lastadd)/(cutoff - lastcutoff) : 0);
        if ( cutoff > lastcutoff )
        {
            lastadd = DEX_totaladd;
            prevtotalhash = totalhash;
            lastcutoff = cutoff;
        }
    }
    return(n);
}

int32_t _komodo_DEX_purgeindices(uint32_t cutoff)
{
    int32_t i,j,n=0; uint32_t t; struct DEX_datablob *ptr; struct DEX_index *index = 0,*tmp;
    if ( DEX_destpubs != 0 )
    {
        HASH_ITER(hh,DEX_destpubs,index,tmp)
        {
            n += _komodo_DEX_purgeindex(0,index,cutoff);
        }
    }
    if ( DEX_tagAs != 0 )
    {
        HASH_ITER(hh,DEX_tagAs,index,tmp)
        {
            n += _komodo_DEX_purgeindex(1,index,cutoff);
        }
    }
    if ( DEX_tagBs != 0 )
    {
        HASH_ITER(hh,DEX_tagBs,index,tmp)
        {
            n += _komodo_DEX_purgeindex(2,index,cutoff);
        }
    }
    if ( DEX_tagABs != 0 )
    {
        HASH_ITER(hh,DEX_tagABs,index,tmp)
        {
            n += _komodo_DEX_purgeindex(3,index,cutoff);
        }
    }
#if KOMODO_DEX_PURGELIST
    for (i=0; i<G->numpurges; i++)
    {
        if ( (ptr= G->Purgelist[i]) != 0 )
        {
            iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
            if ( t <= cutoff - KOMODO_DEX_MAXLAG/2 )
            {
                if ( ptr->linkmask == 0 )
                {
                    G->Purgelist[i] = G->Purgelist[--G->numpurges];
                    G->Purgelist[G->numpurges] = 0;
                    i--;
                    free(ptr);
                    DEX_freed++;
                } else fprintf(stderr,"ptr is still accessed? linkmask.%x\n",ptr->linkmask);
            }
        } else fprintf(stderr,"unexpected null ptr at %d of %d\n",i,G->numpurges);
    }
#endif
    return(n);
}

char *komodo_DEX_keystr(char *str,uint8_t *key,int8_t keylen)
{
    int32_t i;
    str[0] = 0;
    if ( keylen == 34 )
    {
        for (i=0; i<33; i++)
            sprintf(&str[i<<1],"%02x",key[i+1]);
        str[i<<1] = 0;
    }
    else
    {
        if ( key[0] == keylen-1 )
        {
            memcpy(str,&key[1],key[0]);
            str[key[0]] = 0;
        }
        else if ( key[0]+key[key[0]+1]+2 == keylen )
        {
            memcpy(str,&key[1],key[0]);
            str[key[0]] = '/';
            memcpy(&str[key[0]+1],&key[key[0]+2],key[key[0]+1]);
            str[key[0]+1+key[key[0]+1]] = 0;
        }
        else if ( keylen != 0 )
            fprintf(stderr,"strange keylen %d vs [%d %d]\n",keylen,key[0],key[key[0]+1]);
    }
    return(str);
}

struct DEX_index *_komodo_DEX_indexfind(int32_t ind,uint8_t *keybuf,int32_t keylen)
{
    struct DEX_index *index = 0;
    switch ( ind )
    {
        case 0: HASH_FIND(hh,DEX_destpubs,keybuf,keylen,index); break;
        case 1: HASH_FIND(hh,DEX_tagAs,keybuf,keylen,index); break;
        case 2: HASH_FIND(hh,DEX_tagBs,keybuf,keylen,index); break;
        case 3: HASH_FIND(hh,DEX_tagABs,keybuf,keylen,index); break;
    }
    return(index);
}

struct DEX_index *_komodo_DEX_indexcreate(int32_t ind,uint8_t *key,int8_t keylen,struct DEX_datablob *ptr)
{
    struct DEX_index *index = (struct DEX_index *)calloc(1,sizeof(*index));
    if ( index == 0 )
    {
        fprintf(stderr,"out of memory\n");
        return(0);
    }
    memcpy(index->key,key,keylen);
    if ( 0 )
    {
        int32_t i;
        for (i=0; i<keylen; i++)
            fprintf(stderr,"%02x",key[i]);
        char str[111]; fprintf(stderr," ind.%d %p index create (%s) len.%d\n",ind,index,komodo_DEX_keystr(str,key,keylen),keylen);
    }
    index->keylen = keylen;
    switch ( ind )
    {
        case 0: HASH_ADD_KEYPTR(hh,DEX_destpubs,index->key,index->keylen,index); break;
        case 1: HASH_ADD_KEYPTR(hh,DEX_tagAs,index->key,index->keylen,index); break;
        case 2: HASH_ADD_KEYPTR(hh,DEX_tagBs,index->key,index->keylen,index); break;
        case 3: HASH_ADD_KEYPTR(hh,DEX_tagABs,index->key,index->keylen,index); break;
    }
    _komodo_DEX_enqueue(ind,index,ptr);
    return(index);
}

struct DEX_index *_DEX_indexsearch(int32_t ind,int32_t priority,struct DEX_datablob *ptr,int8_t lenA,uint8_t *key,int8_t lenB,uint8_t *tagB)
{
    uint8_t keybuf[KOMODO_DEX_MAXKEYSIZE]; int32_t i,keylen = 0; struct DEX_index *index = 0;
    if ( lenA == 33 )
    {
        keylen = 34;
        keybuf[0] = 33;
        memcpy(&keybuf[1],key,33);
        if ( ind != 0 )
            fprintf(stderr,"expected ind.0 instead of %d\n",ind);
        //index = DEX_destpubs;
    }
    else if ( lenB == 0 )
    {
        keylen = lenA+1;
        keybuf[0] = lenA;
        memcpy(&keybuf[1],key,lenA);
        if ( ind != 1 )
            fprintf(stderr,"expected ind.1 instead of %d\n",ind);
        //index = DEX_tagAs;
    }
    else if ( lenA == 0 )
    {
        keylen = lenB+1;
        keybuf[0] = lenB;
        memcpy(&keybuf[1],tagB,lenB);
        if ( ind != 2 )
            fprintf(stderr,"expected ind.2 instead of %d\n",ind);
        //index = DEX_tagBs;
    }
    else if ( lenA > 0 && lenB > 0 && tagB != 0 && lenA <= KOMODO_DEX_TAGSIZE && lenB <= KOMODO_DEX_TAGSIZE )
    {
        keybuf[keylen++] = lenA;
        memcpy(&keybuf[keylen],key,lenA), keylen += lenA;
        keybuf[keylen++] = lenB;
        memcpy(&keybuf[keylen],tagB,lenB), keylen += lenB;
        if ( ind != 3 )
            fprintf(stderr,"expected ind.3 instead of %d\n",ind);
        //index = DEX_tagABs;
    }
    if ( (index= _komodo_DEX_indexfind(ind,keybuf,keylen)) != 0 )
    {
        if ( ptr != 0 )
            _komodo_DEX_enqueue(ind,index,ptr);
        return(index);
    }
    if ( ptr == 0 )
        return(0);
    /*if ( priority < KOMODO_DEX_CREATEINDEX_MINPRIORITY ) the category creation needs to be in OP_RETURN, could use the same format data packet
    {
        fprintf(stderr,"new index lenA.%d lenB.%d, insufficient priority.%d < minpriority.%d to create\n",lenA,lenB,priority,KOMODO_DEX_CREATEINDEX_MINPRIORITY);
        return(0);
    }
     }*/
    return(_komodo_DEX_indexcreate(ind,keybuf,keylen,ptr));
}

int32_t _DEX_updatetips(struct DEX_index *tips[KOMODO_DEX_MAXINDICES],int32_t priority,struct DEX_datablob *ptr,int8_t lenA,uint8_t *tagA,int8_t lenB,uint8_t *tagB,uint8_t *destpub,int8_t plen)
{
    int32_t ind = 0,mask = 0;
    memset(tips,0,sizeof(*tips) * KOMODO_DEX_MAXINDICES);
    if ( lenA == 0 && lenB == 0 && plen == 0 )
        return(0);
    if ( plen == 33 )
    {
        if ( (tips[ind]= _DEX_indexsearch(ind,priority,ptr,plen,destpub,0,0)) == 0 )
            mask |= (1 << (ind+16));
        else mask |= (1 << ind);
    }
    ind++;
    if ( lenA != 0 )
    {
        if ( (tips[ind]= _DEX_indexsearch(ind,priority,ptr,lenA,tagA,0,0)) == 0 )
            mask |= (1 << (ind+16));
        else mask |= (1 << ind);
        ind++;
        if ( lenB != 0 )
        {
            if ( (tips[ind]= _DEX_indexsearch(ind,priority,ptr,0,0,lenB,tagB)) == 0 )
                mask |= (1 << (ind+16));
            else mask |= (1 << ind);
            ind++;
            if ( (tips[ind]= _DEX_indexsearch(ind,priority,ptr,lenA,tagA,lenB,tagB)) == 0 )
                mask |= (1 << (ind+16));
            else mask |= (1 << ind);
        }
    }
    else if ( lenB != 0 )
    {
        ind++;
        if ( (tips[ind]= _DEX_indexsearch(ind,priority,ptr,0,0,lenB,tagB)) == 0 ) // must have same ind as lenA case above!
            mask |= (1 << (ind+16));
        else mask |= (1 << ind);
    }
    if ( ind >= KOMODO_DEX_MAXINDICES )
    {
        fprintf(stderr,"DEX_updatetips: impossible case ind.%d > KOMODO_DEX_MAXINDICES %d\n",ind,KOMODO_DEX_MAXINDICES);
        exit(1);
    }
    if ( ptr != 0 )
    {
        //fprintf(G->fp,"tips updated %x ptr.%p\n",mask,ptr);
        //fflush(G->fp);
    }
    return(mask); // err bits are <<= 16
}

int32_t komodo_DEX_extract(uint64_t &amountA,uint64_t &amountB,int8_t &lenA,uint8_t tagA[KOMODO_DEX_TAGSIZE],int8_t &lenB,uint8_t tagB[KOMODO_DEX_TAGSIZE],uint8_t destpub33[33],int8_t &plen,uint8_t *msg,int32_t len)
{
    int32_t offset = 0;
    if ( len < sizeof(amountA)+sizeof(amountB)+3 )
        return(-1);
    memset(destpub33,0,33);
    offset = iguana_rwnum(0,&msg[offset],sizeof(amountA),&amountA);
    offset += iguana_rwnum(0,&msg[offset],sizeof(amountB),&amountB);
    if ( (plen= msg[offset++]) != 0 )
    {
        if ( plen == 33 )
        {
            memcpy(destpub33,&msg[offset],33);
            offset += 33;
        }
        else
        {
            fprintf(stderr,"reject invalid plen.%d\n",plen);
            return(-1);
        }
    }
    if ( (lenA= msg[offset++]) != 0 )
    {
        if ( lenA <= KOMODO_DEX_TAGSIZE )
        {
            memcpy(tagA,&msg[offset],lenA);
            offset += lenA;
        }
        else
        {
            fprintf(stderr,"reject invalid lenA.%d\n",lenA);
            return(-1);
        }
    }
    if ( (lenB= msg[offset++]) != 0 )
    {
        if ( lenB <= KOMODO_DEX_TAGSIZE )
        {
            memcpy(tagB,&msg[offset],lenB);
            offset += lenB;
        }
        else
        {
            fprintf(stderr,"reject invalid lenB.%d\n",lenB);
            return(-1);
        }
    }
    return(offset);
}

int32_t komodo_DEX_tagsextract(uint64_t &amountA,uint64_t &amountB,char taga[],char tagb[],char pubkeystr[],uint8_t destpub33[33],struct DEX_datablob *ptr)
{
    uint8_t *msg,tagA[KOMODO_DEX_TAGSIZE+1],tagB[KOMODO_DEX_TAGSIZE+1]; int8_t lenA,lenB,plen; int32_t i,offset,len;
    msg = &ptr->data[KOMODO_DEX_ROUTESIZE];
    len = ptr->datalen-KOMODO_DEX_ROUTESIZE;
    taga[0] = tagb[0] = 0;
    if ( pubkeystr != 0 )
        pubkeystr[0] = 0;
    amountA = amountB = 0;
    memset(destpub33,0,33);
    memset(tagA,0,sizeof(tagA));
    memset(tagB,0,sizeof(tagB));
    if ( (offset= komodo_DEX_extract(amountA,amountB,lenA,tagA,lenB,tagB,destpub33,plen,msg,len)) < 0 )
        return(-1);
    if ( lenA != 0 )
        strcpy(taga,(char *)tagA);
    if ( lenB != 0 )
        strcpy(tagb,(char *)tagB);
    if ( plen == 33 && pubkeystr != 0 )
    {
        for (i=0; i<33; i++)
            sprintf(&pubkeystr[i<<1],"%02x",destpub33[i]);
        pubkeystr[i<<1] = 0;
    }
    return(0);
}

struct DEX_datablob *_komodo_DEXfind(int32_t modval,uint32_t shorthash)
{
    uint32_t hashval; int32_t i,hashind; struct DEX_datablob *ptr;
    if ( modval < 0 || modval >= KOMODO_DEX_PURGETIME )
    {
        fprintf(stderr,"DEXfind illegal modval.%d\n",modval);
        return(0);
    }
    HASH_FIND(hh,G->Hashtables[modval],&shorthash,sizeof(shorthash),ptr);
    return(ptr);
}

struct DEX_datablob *_komodo_DEXadd(uint32_t now,int32_t modval,bits256 hash,uint32_t shorthash,uint8_t *msg,int32_t len)
{
    int32_t ind,offset,priority; struct DEX_datablob *ptr; struct DEX_index *tips[KOMODO_DEX_MAXINDICES]; uint64_t amountA,amountB; uint8_t tagA[KOMODO_DEX_TAGSIZE+1],tagB[KOMODO_DEX_TAGSIZE+1],destpub33[33]; int8_t lenA,lenB,plen;
    if ( modval < 0 || modval >= KOMODO_DEX_PURGETIME )
    {
        fprintf(stderr,"komodo_DEXadd illegal modval.%d\n",modval);
        return(0);
    }
    if ( (hash.ulongs[0] & KOMODO_DEX_TXPOWMASK) != (0x777 & KOMODO_DEX_TXPOWMASK) )
    {
        static uint32_t count; char str[65];
        if ( count++ < 10 )
            fprintf(stderr," reject quote due to invalid hash[1] %016llx %s\n",(long long)hash.ulongs[0],bits256_str(str,hash));
        return(0);
    } else priority = komodo_DEX_priority(hash.ulongs[0],len);
    if ( (ptr= _komodo_DEXfind(modval,shorthash)) != 0 )
        return(ptr);
    memset(tagA,0,sizeof(tagA));
    memset(tagB,0,sizeof(tagB));
    if ( (offset= komodo_DEX_extract(amountA,amountB,lenA,tagA,lenB,tagB,destpub33,plen,&msg[KOMODO_DEX_ROUTESIZE],len-KOMODO_DEX_ROUTESIZE)) < 0 )
        return(0);
    if ( (ptr= (struct DEX_datablob *)calloc(1,sizeof(*ptr) + len)) != 0 )
    {
        ptr->recvtime = now;
        ptr->hash = hash;
        ptr->shorthash = shorthash;
        ptr->datalen = len;
        ptr->priority = priority;
        ptr->sizepriority = komodo_DEX_sizepriority(len);
        ptr->offset = offset + KOMODO_DEX_ROUTESIZE; // payload is after relaydepth, funcid, timestamp
        memcpy(ptr->data,msg,len);
        ptr->data[0] = msg[0] != 0xff ? msg[0] - 1 : msg[0];
        {
            HASH_ADD(hh,G->Hashtables[modval],shorthash,sizeof(ptr->shorthash),ptr);
            SETBIT(&ptr->linkmask,KOMODO_DEX_MAXINDICES);
            DEX_totaladd++;
            if ( (_DEX_updatetips(tips,priority,ptr,lenA,tagA,lenB,tagB,destpub33,plen) >> 16) != 0 )
                fprintf(stderr,"update M.%d slot.%d [%d] with %08x error updating tips\n",modval,ind,ptr->data[0],ptr->shorthash);
        }
        return(ptr);
    }
    fprintf(stderr,"out of memory\n");
    return(0);
}

int32_t komodo_DEXgenget(std::vector<uint8_t> &getshorthash,uint32_t timestamp,uint32_t shorthash,int32_t modval)
{
    int32_t len = 0;
    getshorthash.resize(2 + 2*sizeof(uint32_t) + sizeof(modval));
    getshorthash[len++] = 0;
    getshorthash[len++] = 'G';
    len += iguana_rwnum(1,&getshorthash[len],sizeof(timestamp),&timestamp);
    len += iguana_rwnum(1,&getshorthash[len],sizeof(shorthash),&shorthash);
    len += iguana_rwnum(1,&getshorthash[len],sizeof(modval),&modval);
    return(len);
}

int32_t komodo_DEXgenping(uint8_t funcid,std::vector<uint8_t> &ping,uint32_t timestamp,int32_t modval,uint32_t *recents,uint16_t n)
{
    int32_t i,len = 0;
    if ( modval < 0 || modval >= KOMODO_DEX_PURGETIME )
    {
        fprintf(stderr,"komodo_DEXgenping illegal modval.%d\n",modval);
        return(-1);
    }
    ping.resize(2 + sizeof(timestamp) + sizeof(n) + sizeof(modval) + n*sizeof(uint32_t));
    ping[len++] = 0;
    ping[len++] = funcid;
    len += iguana_rwnum(1,&ping[len],sizeof(timestamp),&timestamp);
    len += iguana_rwnum(1,&ping[len],sizeof(n),&n);
    len += iguana_rwnum(1,&ping[len],sizeof(modval),&modval);
    for (i=0; i<n; i++)
        len += iguana_rwnum(1,&ping[len],sizeof(recents[i]),&recents[i]);
    return(len);
}

int32_t komodo_DEXgenquote(uint8_t funcid,int32_t priority,bits256 &hash,uint32_t &shorthash,std::vector<uint8_t> &quote,uint32_t timestamp,uint8_t hdr[],int32_t hdrlen,uint8_t data[],int32_t datalen)
{
    int32_t i,j,len = 0; uint64_t h; uint32_t nonce = rand();
    quote.resize(2 + sizeof(uint32_t) + hdrlen + datalen + sizeof(nonce)); // send list of recently added shorthashes
    quote[len++] = KOMODO_DEX_RELAYDEPTH;
    quote[len++] = funcid;
    len += iguana_rwnum(1,&quote[len],sizeof(timestamp),&timestamp);
    for (i=0; i<hdrlen; i++)
        quote[len++] = hdr[i];
    if ( data != 0 )
    {
        for (i=0; i<datalen; i++)
            quote[len++] = data[i];
    }
    len += sizeof(nonce);
#if KOMODO_DEX_TXPOWMASK
    for (i=0; i<0xffffffff; i++,nonce++)
    {
        timestamp = (uint32_t)time(NULL);
        iguana_rwnum(1,&quote[2],sizeof(timestamp),&timestamp);
        iguana_rwnum(1,&quote[len - sizeof(nonce)],sizeof(nonce),&nonce);
        shorthash = komodo_DEXquotehash(hash,&quote[0],len);
        h = hash.ulongs[0];
        if ( (h & KOMODO_DEX_TXPOWMASK) == (0x777 & KOMODO_DEX_TXPOWMASK) )
        {
            h >>= KOMODO_DEX_TXPOWBITS;
            for (j=0; j<priority; j++,h>>=1)
                if ( (h & 1) != 0 )
                    break;
            if ( j < priority )
            {
                //fprintf(stderr,"i.%u j.%d failed priority.%d ulongs[0] %016llx\n",i,j,priority,(long long)hash.ulongs[0]);
                continue;
            }
            if ( i > 10000000 )
                fprintf(stderr,"nonce calc: i.%u j.%d priority.%d ulongs[0] %016llx\n",i,j,priority,(long long)hash.ulongs[0]);
            break;
        }
    }
#else
    iguana_rwnum(1,&quote[len - sizeof(nonce)],sizeof(nonce),&nonce);
    shorthash = komodo_DEXquotehash(hash,&quote[0],len);
#endif
    return(len);
}

int32_t komodo_DEXpacketsend(CNode *peer,uint8_t peerpos,struct DEX_datablob *ptr,uint8_t resp0)
{
    std::vector<uint8_t> packet; int32_t i;
    //fprintf(stderr,"packet send %p datalen.%d\n",ptr,ptr->datalen);
    if ( ptr->datalen < KOMODO_DEX_ROUTESIZE || ptr->datalen > KOMODO_DEX_MAXPACKETSIZE )
    {
        fprintf(stderr,"illegal datalen.%d\n",ptr->datalen);
        return(-1);
    }
    //SETBIT(ptr->peermask,peerpos); // pretty sure this will get there -> mark present
    packet.resize(ptr->datalen);
    memcpy(&packet[0],ptr->data,ptr->datalen);
    packet[0] = resp0;
    peer->PushMessage("DEX",packet);
    DEX_totalsent++;
    return(ptr->datalen);
}

int32_t _komodo_DEXmodval(uint32_t now,const int32_t modval,CNode *peer)
{
    static uint32_t recents[16][KOMODO_DEX_MAXPERSEC],sendbuf[KOMODO_DEX_MAXPING];
    std::vector<uint8_t> packet; int32_t i,j,n=0,mult,p,vip=0,maxp=0,sum=0; uint16_t peerpos,num[16]; uint8_t priority,relay,funcid,*msg; uint32_t t,h; struct DEX_datablob *ptr=0,*tmp;
    if ( modval < 0 || modval >= KOMODO_DEX_PURGETIME || (peerpos= _komodo_DEXpeerpos(now,peer->id)) == 0xffff )
        return(-1);
    memset(num,0,sizeof(num));
    HASH_ITER(hh,G->Hashtables[modval],ptr,tmp)
    {
        n++;
        h = ptr->shorthash;
        if ( ptr->datalen >= KOMODO_DEX_ROUTESIZE && ptr->datalen < KOMODO_DEX_MAXPACKETSIZE )
        {
            msg = &ptr->data[0];
            relay = msg[0];
            funcid = msg[1];
            iguana_rwnum(0,&msg[2],sizeof(t),&t);
            if ( now < t+KOMODO_DEX_MAXLAG || ptr->priority >= KOMODO_DEX_VIPLEVEL || ptr->requested > 0 ) //|| now < ptr->recvtime+KOMODO_DEX_MAXHOPS/2+1 )
            {
                if ( GETBIT(ptr->peermask,peerpos) == 0 || ptr->requested > 0 )
                {
                    if ( (p= ptr->priority) >= 16 )
                        p = 15;
                    if ( p < 0 )
                    {
                        fprintf(stderr,"unexpected negative priority.%d\n",p);
                        continue;
                    }
                    if ( p > maxp )
                        maxp = p;
                    if ( num[p] >= (int32_t)(sizeof(recents[p])/sizeof(*recents[p])) )
                    {
                        fprintf(stderr,"num[%d] %d is full\n",p,num[p]);
                        continue;
                    }
                    /*if ( GETBIT(ptr->peermask,peerpos) == 0 && ptr->priority >= KOMODO_DEX_VIPLEVEL )
                    {
                        //fprintf(G->fp,"%08x ",ptr->shorthash);
                        vip++;
                    }*/
                    recents[p][num[p]++] = h;
                    if ( ptr->requested > 0 )
                    {
                        //fprintf(G->fp,"%08x.R%d.%d ",ptr->shorthash,ptr->requested,GETBIT(ptr->peermask,peerpos));
                        ptr->requested--;
                        vip++;
                    }
                    if ( ptr->numsent < KOMODO_DEX_MAXFANOUT )
                    {
                        if ( (relay >= 0 && relay <= KOMODO_DEX_RELAYDEPTH && now < t+KOMODO_DEX_LOCALHEARTBEAT) )
                        {
                            if ( komodo_DEX_islagging() == 0 )
                            {
                                komodo_DEXpacketsend(peer,peerpos,ptr,ptr->data[0]);
                                ptr->numsent++;
                            }
                        }
                    }
                }
            }
        } else fprintf(stderr,"ptr.%p %08x with illegal size %d\n",ptr,ptr->shorthash,ptr->datalen);
    }
    if ( vip != 0 )
    {
        //fprintf(G->fp,"missing vip.%d dexmodval.%d peer.%d n.%d\n",vip,modval,peerpos,n);
        //fflush(G->fp);
    }
    for (p=15; p>=0; p--)
    {
        if ( num[p] != 0 )
        {
            if ( (mult= num[p]/KOMODO_DEX_MAXPING) > 1 ) // reduce conflict with pings from other peers
            {
                i = (rand() % mult);
                for (n=0; i<num[p]; i+=mult,n++)
                    sendbuf[n] = recents[p][i];
                if ( komodo_DEXgenping('P',packet,now,modval,sendbuf,n) > 0 )
                    peer->PushMessage("DEX",packet);
                sum += n;
            }
            else
            {
                if ( komodo_DEXgenping('P',packet,now,modval,recents[p],num[p]) > 0 )
                    peer->PushMessage("DEX",packet);
                sum += num[p];
            }
            if ( komodo_DEX_islagging() != 0 )
                return(sum);
        }
    }
    return(sum);
}

uint8_t *komodo_DEX_datablobdecrypt(bits256 *senderpub,uint8_t **allocatedp,int32_t *newlenp,struct DEX_datablob *ptr,bits256 pubkey,char *taga)
{
    static bits256 zero;
    bits256 priv0; uint8_t *decoded; char str[65],str2[65];
    *allocatedp = 0;
    *newlenp = 0;
    memset(priv0.bytes,0,sizeof(priv0));
    memset(senderpub,0,sizeof(*senderpub));
    //if ( bits256_nonz(pubkey) != 0 )
    {
        if ( memcmp(pubkey.bytes,DEX_pubkey.bytes,32) == 0 && strcmp(taga,"inbox") == 0 )
            komodo_DEX_privkey(priv0);
        else priv0 = GENESIS_PRIVKEY;
        *newlenp = ptr->datalen - 4 - ptr->offset;
        if ( (decoded= komodo_DEX_decrypt(senderpub->bytes,allocatedp,&ptr->data[ptr->offset],newlenp,priv0)) != 0 )
        {
            if ( memcmp(&priv0,&GENESIS_PRIVKEY,32) == 0 && memcmp(senderpub,&pubkey,32) != 0 && memcmp(pubkey.bytes,zero.bytes,sizeof(zero)) != 0 && memcmp(pubkey.bytes,&GENESIS_PUBKEY,32) != 0 )
            {
                fprintf(stderr,"senderpub %s != pubkey %s\n",bits256_str(str,*senderpub),bits256_str(str2,pubkey));
                *newlenp = -1;
                decoded = 0;
            }
        } //else fprintf(stderr,"decrypt error senderpub.%s\n",bits256_str(str,*senderpub));
        memset(priv0.bytes,0,sizeof(priv0));
    }
    return(decoded);
}

int32_t _komodo_DEX_decryptdata(uint8_t *data,int32_t maxlen,struct DEX_datablob *ptr)
{
    uint8_t *decoded,*allocated=0; bits256 zero,senderpub; int32_t newlen = 0;
    memset(zero.bytes,0,sizeof(zero));
    if ( (decoded= komodo_DEX_datablobdecrypt(&senderpub,&allocated,&newlen,ptr,zero,(char *)"")) != 0 )
    {
        if ( newlen < maxlen )
            memcpy(data,decoded,newlen);
        else newlen = -1;
    }
    if ( allocated != 0 )
        free(allocated);
    return(newlen);
}

int32_t komodo_DEX_cancelupdate(struct DEX_datablob *ptr,char *tagA,char *tagB,bits256 senderpub,uint32_t cutoff)
{
    uint64_t amountA,amountB; char taga[KOMODO_DEX_MAXKEYSIZE+1],tagb[KOMODO_DEX_MAXKEYSIZE+1]; uint8_t pubkey33[33];
    if ( komodo_DEX_tagsextract(amountA,amountB,taga,tagb,0,pubkey33,ptr) < 0 )
        return(-2);
    if ( pubkey33[0] != 0x01 || memcmp(pubkey33+1,senderpub.bytes,32) != 0 )
    {
        fprintf(stderr,"illegal trying to cancel another pubkey datablob! banscore this\n");
        return(-3);
    }
    if ( tagA[0] != 0 && strcmp(tagA,taga) != 0 )
    {
        //fprintf(stderr,"tagA %s mismatch %s\n",tagA,taga);
        return(-4);
    }
    if ( tagB[0] != 0 && strcmp(tagB,tagb) != 0 )
    {
        //fprintf(stderr,"tagB %s mismatch %s\n",tagB,tagb);
        return(-5);
    }
    if ( ptr->cancelled != 0 )
        return(0);
    else
    {
        ptr->cancelled = cutoff;
        //fprintf(stderr,"(%08x) cancel at %u\n",ptr->shorthash,ptr->cancelled);
        return(1);
    }
}

int32_t _komodo_DEX_cancelid(uint32_t shorthash,bits256 senderpub,uint32_t cutoff)
{
    int32_t modval; struct DEX_datablob *ptr;
    for (modval=0; modval<KOMODO_DEX_PURGETIME; modval++)
    {
        if ( (ptr= _komodo_DEXfind(modval,shorthash)) != 0 )
            return(komodo_DEX_cancelupdate(ptr,(char *)"",(char *)"",senderpub,cutoff));
    }
    return(-1);
}

int32_t _komodo_DEX_cancelpubkey(char *tagA,char *tagB,uint8_t *cancelkey33,uint32_t cutoff)
{
    struct DEX_datablob *ptr = 0; struct DEX_index *index; bits256 senderpub; uint32_t t; int32_t ind=0,n = 0;
    if ( cancelkey33[0] != 0x01 )
    {
        fprintf(stderr,"komodo_DEX_cancelpubkey: (%s,%s) illegal pubkey[0] %02x\n",tagA,tagB,cancelkey33[0]);
        return(-1);
    }
    memcpy(senderpub.bytes,cancelkey33+1,32);
    if ( (index= _DEX_indexsearch(0,0,0,33,cancelkey33,0,0)) != 0 )
    {
        for (ptr=index->tail; ptr!=0; ptr=ptr->prevs[ind])
        {
            iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
            if ( t < cutoff-KOMODO_DEX_MAXHOPS && komodo_DEX_cancelupdate(ptr,tagA,tagB,senderpub,cutoff) >= 0 )
            {
                //fprintf(stderr,"cancel ptr.%p %08x t.%u cutoff.%u (%s,%s) cancelled.%u\n",ptr,ptr->shorthash,t,cutoff-KOMODO_DEX_MAXHOPS,tagA,tagB,ptr->cancelled);
                n++;
            }
            if ( ptr == index->head )
                break;
       }
    }
    return(n);
}

int32_t _komodo_DEX_locatorsextract(int32_t sendflag,uint32_t shorthash,int32_t modval,int32_t priority)
{
    static bits256 zero;
    uint8_t *allocated=0,*decoded; int32_t i,j,n=0,numrequests,newlen=0; bits256 senderpub; uint64_t locator; struct DEX_datablob *refptr,*ptr;
    if ( (refptr= _komodo_DEXfind(modval,shorthash)) == 0 )
        return(-1);
    if ( (decoded= komodo_DEX_datablobdecrypt(&senderpub,&allocated,&newlen,refptr,zero,(char *)"")) != 0 && (newlen & 7) == 0 )
    {
        numrequests = 50 + priority * 10;
        if ( numrequests > 250 )
            numrequests = 250;
        for (i=sizeof(uint64_t),j=0; i<newlen; i+=8,j++)
        {
            iguana_rwnum(0,&decoded[j*8 + 8],sizeof(locator),&locator);
            if ( (ptr= _komodo_DEXfind((int32_t)(locator >> 32) % KOMODO_DEX_PURGETIME,(uint32_t)locator)) != 0 )
            {
                ptr->requested = numrequests;
                //fprintf(stderr,"%u ",ptr->shorthash);
                n++;
            }
            else if ( sendflag != 0 )
            {
                // send request to peer
            }
        }
    }
    if ( allocated != 0 )
        free(allocated);
    //if ( n > 0 )
    //    fprintf(stderr," set %d ptrs requests.%d\n",n,numrequests);
    return(n);
}

int32_t _komodo_DEX_commandprocessor(struct DEX_datablob *ptr,int32_t addedflag,int32_t peerpos)
{
    char _taga[KOMODO_DEX_MAXKEYSIZE+1],_tagb[KOMODO_DEX_MAXKEYSIZE+1],taga[KOMODO_DEX_MAXKEYSIZE+1],tagb[KOMODO_DEX_MAXKEYSIZE+1],str[65]; uint8_t pubkey33[33],*decoded,*allocated; bits256 pubkey,senderpub; uint32_t t,shorthash; int32_t modval,lenA,lenB,n,newlen=0; uint64_t amountA,amountB;
    if ( ptr->priority < KOMODO_DEX_CMDPRIORITY )
        return(-1);
    if ( komodo_DEX_tagsextract(amountA,amountB,taga,tagb,0,pubkey33,ptr) < 0 )
        return(-2);
    if ( pubkey33[0] != 0x01 )
        return(-3);
    memcpy(pubkey.bytes,pubkey33+1,32);
    //if ( memcmp(pubkey.bytes,DEX_pubkey.bytes,32) == 0 )
    //    addedflag = 1;
    if ( addedflag != 0 )
    {
        if ( (decoded= komodo_DEX_datablobdecrypt(&senderpub,&allocated,&newlen,ptr,pubkey,taga)) != 0 && newlen > 0 )
        {
            if ( ptr->data[1] == 'R' )
            {
                if ( newlen == sizeof(uint32_t)*2 )
                {
                    iguana_rwnum(0,decoded,sizeof(shorthash),&shorthash);
                    iguana_rwnum(0,decoded+sizeof(uint32_t),sizeof(t),&t);
                    /*if ( shorthash == 0xffffffff && t == 0xffffffff )
                        n = _komodo_DEX_peerclear(peerpos);
                    else*/ n = _komodo_DEX_locatorsextract(0,shorthash,t % KOMODO_DEX_PURGETIME,ptr->priority);
                    fprintf(stderr,"received REQUEST command for (%s/%s) %08x t.%u -> updated %d ptrs\n",taga,tagb,shorthash,t,n);
                } else fprintf(stderr,"newlen.%d != 8 for 'R'\n",newlen);
            }
            else if ( ptr->data[1] == 'A' )
            {
                //fprintf(stderr,"got anonsend newlen.%d\n",newlen);
            }
            else if ( ptr->data[1] == 'X' )
            {
                if ( strcmp(taga,"cancel") != 0 )
                    fprintf(stderr,"expected tagA cancel, but got (%s)\n",taga);
                else
                {
                    iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
                    //fprintf(stderr,"funcid.%c decoded %d bytes tagA.(%s)\n",ptr->data[1],newlen,taga);
                    if ( newlen == 4 )
                    {
                        iguana_rwnum(0,decoded,sizeof(shorthash),&shorthash);
                        _komodo_DEX_cancelid(shorthash,senderpub,t);
                    }
                    else if ( newlen == 33 && decoded[0] == 0x01 ) // depends on pubkey format
                    {
                        if ( memcmp(&decoded[1],senderpub.bytes,32) == 0 )
                        {
                            _komodo_DEX_cancelpubkey((char *)"",(char *)"",decoded,t);
                        } else fprintf(stderr,"unexpected payload mismatch senderpub\n");
                    }
                    else if ( newlen < KOMODO_DEX_MAXKEYSIZE )
                    {
                        lenA = decoded[0];
                        lenB = decoded[lenA+1];
                        if ( 0 )
                        {
                            int32_t i;
                            for (i=0; i<newlen; i++)
                                fprintf(stderr,"%02x",decoded[i]);
                            fprintf(stderr," decoded cancel scan for (%s,%s)\n",_taga,_tagb);
                        }
                        if ( lenA+lenB+2 == newlen && lenA < KOMODO_DEX_TAGSIZE && lenB < KOMODO_DEX_TAGSIZE )
                        {
                            memset(_taga,0,sizeof(_taga));
                            memcpy(_taga,&decoded[1],lenA);
                            memset(_tagb,0,sizeof(_tagb));
                            memcpy(_tagb,&decoded[2+lenA],lenB);
                            pubkey33[0] = 0x01;
                            memcpy(&pubkey33[1],senderpub.bytes,32);
                            _komodo_DEX_cancelpubkey(_taga,_tagb,pubkey33,t);
                        } else fprintf(stderr,"skip lenA.%d lenB.%d vs newlen.%d\n",lenA,lenB,newlen);
                    }
                }
            } else fprintf(stderr,"unsupported funcid.%d (%c)\n",ptr->data[1],ptr->data[1]);
        } else fprintf(stderr,"decode error, newlen.%d\n",newlen);
        if ( allocated != 0 )
            free(allocated);
    }
    return(newlen);
}

int32_t _komodo_DEXprocess(uint32_t now,CNode *pfrom,uint8_t *msg,int32_t len)
{
    static uint32_t cache[2],pongbuf[KOMODO_DEX_MAXPING];
    int32_t i,j,ind,m,p,tmpval,haves,offset,flag,modval,lag,priority,addedflag=0; uint16_t n,peerpos; uint32_t t,h; uint8_t funcid,relay=0; bits256 hash; struct DEX_datablob *ptr;
    peerpos = _komodo_DEXpeerpos(now,pfrom->id);
    //fprintf(stderr,"peer.%d msg[%d] %c\n",peerpos,len,msg[1]);
    if ( len > KOMODO_DEX_ROUTESIZE+sizeof(uint32_t) && peerpos != 0xffff && len < KOMODO_DEX_MAXPACKETSIZE )
    {
        relay = msg[0];
        funcid = msg[1];
        iguana_rwnum(0,&msg[2],sizeof(t),&t);
        modval = (t % KOMODO_DEX_PURGETIME);
        lag = (now - t);
        if ( lag < 0 )
            lag = 0;
        h = komodo_DEXquotehash(hash,msg,len);
        priority = komodo_DEX_priority(hash.ulongs[0],len);
        if ( t > now+KOMODO_DEX_LOCALHEARTBEAT )
        {
            fprintf(stderr,"reject packet from future t.%u vs now.%u\n",t,now);
        }
        /*else if ( lag >= KOMODO_DEX_MAXLAG && priority < KOMODO_DEX_VIPLEVEL )
        {
            DEX_maxlag++;
            //fprintf(stderr,"reject packet with too big lag t.%u vs now.%u lag.%d\n",t,now,lag);
        }*/
        else if ( funcid == 'Q' || funcid == 'X' || funcid == 'R' || funcid == 'A' )
        {
            DEX_totalrecv++;
            //fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
            //fprintf(G->fp," recv modval.%d from (%s) relay.%d p.%d shorthash.%08x %016llx\n",modval,pfrom->addr.ToString().c_str(),relay,priority,h,(long long)hash.ulongs[0]);
            if ( (hash.ulongs[0] & KOMODO_DEX_TXPOWMASK) != (0x777 & KOMODO_DEX_TXPOWMASK) )
            {
                static uint32_t count;
                if ( count++ < 10 )
                    fprintf(stderr,"reject quote due to invalid hash[0] %016llx\n",(long long)hash.ulongs[0]);
            }
            else if ( priority < 0 )
            {
                static uint32_t count;
                if ( count++ < 10 )
                    fprintf(stderr,"reject quote due to insufficient priority.%d for size.%d, needed %d\n",komodo_DEX_priority(hash.ulongs[0],0),len,komodo_DEX_sizepriority(len));
                
            }
            else if ( relay <= KOMODO_DEX_RELAYDEPTH || relay == 0xff )
            {
                if ( (ptr= _komodo_DEXfind(modval,h)) == 0 )
                {
                    if ( (ptr= _komodo_DEXadd(now,modval,hash,h,msg,len)) != 0 )
                    {
                        addedflag = 1;
                        if ( komodo_DEXfind32(G->Pendings,(int32_t)(sizeof(G->Pendings)/sizeof(*G->Pendings)),h,1) >= 0 )
                        {
                            if ( DEX_Numpending > 0 )
                                DEX_Numpending--;
                        }
                        Got_Recent_Quote = now;
                        if ( lag > 0 && lag < KOMODO_DEX_MAXLAG )
                        {
                            if ( DEX_lag == 0. )
                                DEX_lag = DEX_lag2 = DEX_lag3 = lag;
                            else
                            {
                                DEX_lag = (DEX_lag * 0.999) + (0.001 * lag);
                                DEX_lag2 = (DEX_lag2 * 0.9999) + (0.0001 * lag);
                                DEX_lag3 = (DEX_lag3 * 0.99999) + (0.00001 * lag);
                            }
                        }
                        if ( cache[0] != now )
                        {
                            cache[0] = now;
                            cache[1] = ptr->priority;
                        }
                        else if ( ptr->priority > cache[1] )
                            cache[1] = ptr->priority;
                    } else fprintf(stderr,"error adding %08x\n",h);
                }
                else
                {
                    DEX_duplicate++;
                }
                if ( ptr != 0 )
                {
                    SETBIT(ptr->peermask,peerpos);
                    if ( funcid != 'Q' )
                        _komodo_DEX_commandprocessor(ptr,addedflag,peerpos);
                }
            } else fprintf(stderr,"unexpected relay.%d\n",relay);
        }
        else if ( funcid == 'P' || funcid == 'p' )
        {
            std::vector<uint8_t> getshorthash;
            haves = 0;
            if ( len >= 12 && len < KOMODO_DEX_MAXPING*sizeof(uint32_t)+64 )
            {
                offset = KOMODO_DEX_ROUTESIZE;
                offset += iguana_rwnum(0,&msg[offset],sizeof(n),&n);
                offset += iguana_rwnum(0,&msg[offset],sizeof(m),&m);
                if ( offset+n*sizeof(uint32_t) == len && m >= 0 && m < KOMODO_DEX_PURGETIME )
                {
                    for (flag=i=0; i<n; i++)
                    {
                        offset += iguana_rwnum(0,&msg[offset],sizeof(h),&h);
                        if ( (ptr= _komodo_DEXfind(m,h)) != 0 )
                        {
                            SETBIT(ptr->peermask,peerpos);
                            pongbuf[haves++] = h;
                            continue;
                        }
                        if ( funcid == 'p' || DEX_Numpending > KOMODO_DEX_MAXPERSEC )
                            continue;
                        p = komodo_DEX_countbits(h);
                        if ( p < KOMODO_DEX_VIPLEVEL && komodo_DEX_islagging() != 0 && p < cache[1] ) // adjusts for txpowbits and sizebits
                        {
                            //fprintf(stderr,"skip estimated priority.%d with cache[%u %d]\n",komodo_DEX_countbits(h),cache[0],cache[1]);
                            continue;
                        }
                        tmpval = komodo_DEXfind32(G->Pendings,(int32_t)(sizeof(G->Pendings)/sizeof(*G->Pendings)),h,0);
                        if ( tmpval < 0 ) //|| (p >= KOMODO_DEX_VIPLEVEL && (rand() % 10)) )
                        {
                            komodo_DEXadd32(G->Pendings,(int32_t)(sizeof(G->Pendings)/sizeof(*G->Pendings)),h);
                            //fprintf(G->fp,">>>> %d/%08x <<<<< ",m,h);
                            if ( tmpval < 0 )
                                DEX_Numpending++;
                            komodo_DEXgenget(getshorthash,now,h,m);
                            pfrom->PushMessage("DEX",getshorthash);
                            flag++;
                        }
                        //fprintf(G->fp,"%d/%08x ",m,h);
                    }
                    if ( (0) && flag != 0 )
                    {
                        fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
                        fprintf(stderr," recv at %u from (%s) PULL these.%d lag.%d\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str(),flag,lag);
                    } else if ( (0) && n > 0 )
                        fprintf(G->fp,"ping[%d] modval.%d from %s\n",n,m,pfrom->addr.ToString().c_str());
                } else fprintf(stderr,"ping packetsize error %d != %d, offset.%d n.%d, modval.%d purgtime.%d\n",len,offset+n*4,offset,n,m,KOMODO_DEX_PURGETIME);
                if ( funcid == 'P' && haves > 0 )
                {
                    std::vector<uint8_t> pong;
                    if ( komodo_DEXgenping('p',pong,now,m,pongbuf,haves) > 0 )
                        pfrom->PushMessage("DEX",pong);
                }
            } // else banscore this
        }
        else if ( funcid == 'G' )
        {
            iguana_rwnum(0,&msg[KOMODO_DEX_ROUTESIZE],sizeof(h),&h);
            iguana_rwnum(0,&msg[KOMODO_DEX_ROUTESIZE+sizeof(h)],sizeof(modval),&modval);
            //fprintf(G->fp," modval.%d f.%c t.%u [%d] get.%08x\n",modval,funcid,t,relay,h);
            //fprintf(stderr," recv at %u from (%s)\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str());
            if ( modval >= 0 && modval < KOMODO_DEX_PURGETIME )
            {
                if ( (ptr= _komodo_DEXfind(modval,h)) != 0 )
                {
                    //if ( GETBIT(ptr->peermask,peerpos) == 0 ) if a node specifically requests it, send it
                    {
                        return(komodo_DEXpacketsend(pfrom,peerpos,ptr,0)); // squelch relaying of 'G' packets
                    }
                } // else fprintf(stderr,"unexpected request of %08x\n",h);
            } else fprintf(stderr,"illegal modval.%d\n",modval);
        }
    }
    //fflush(G->fp);
    return(0);
}

int32_t komodo_DEX_payloadstr(UniValue &item,uint8_t *data,int32_t datalen,int32_t decrypted)
{
    char *itemstr; int32_t i,hexflag = 0;
    if ( datalen <= 0 )
    {
        item.push_back(Pair((char *)"payload",(char *)""));
        item.push_back(Pair((char *)"hex",1));
        return(-1);
    }
    if ( data[datalen-1] != 0 )
        hexflag = 1;
    else
    {
        for (i=0; i<datalen-1; i++)
            if ( isprint(data[i]) == 0 )
            {
                hexflag = 1;
                break;
            }
    }
    if ( hexflag != 0 )
    {
        itemstr = (char *)calloc(1,datalen*2+1);
        for (i=0; i<datalen; i++)
            sprintf(&itemstr[i<<1],"%02x",data[i]);
        itemstr[i<<1] = 0;
        if ( decrypted == 0 )
        {
            item.push_back(Pair((char *)"payload",itemstr));
            item.push_back(Pair((char *)"hex",1));
        }
        else
        {
            item.push_back(Pair((char *)"decrypted",itemstr));
            item.push_back(Pair((char *)"decryptedhex",1));
        }
        free(itemstr);
    }
    else
    {
        //for (i=0; i<datalen; i++)
        //    fprintf(stderr,"%02x",data[i]);
        //fprintf(stderr," itempush.(%s)\n",(char *)data);
        if ( decrypted == 0 )
        {
            item.push_back(Pair((char *)"payload",(char *)data));
            item.push_back(Pair((char *)"hex",0));
        }
        else
        {
            item.push_back(Pair((char *)"decrypted",(char *)data));
            item.push_back(Pair((char *)"decryptedhex",0));
        }
    }
    return(hexflag);
}

int32_t komodo_DEX_tagsmatch(uint64_t &amountA,uint64_t &amountB,struct DEX_datablob *ptr,uint8_t *tagA,int8_t lenA,uint8_t *tagB,int8_t lenB,uint8_t *destpub,int8_t plen)
{
    char taga[KOMODO_DEX_MAXKEYSIZE+1],tagb[KOMODO_DEX_MAXKEYSIZE+1],pubkeystr[67]; uint8_t destpub33[33];
    if ( komodo_DEX_tagsextract(amountA,amountB,taga,tagb,pubkeystr,destpub33,ptr) < 0 )
        return(-1);
    if ( lenA != 0 && (memcmp(tagA,taga,lenA) != 0 || taga[lenA] != 0) )
    {
        //fprintf(stderr,"komodo_DEX_tagsmatch tagA.%s mismatch vs %s\n",taga,(char *)tagA);
        return(-2);
    }
    if ( lenB != 0 && (memcmp(tagB,tagb,lenB) != 0 || tagb[lenB] != 0) )
    {
        //fprintf(stderr,"komodo_DEX_tagsmatch tagB.%s mismatch vs %s\n",tagb,(char *)tagB);
        return(-3);
    }
    if ( plen != 0 && memcmp(destpub,destpub33,33) != 0 )
    {
        //fprintf(stderr,"komodo_DEX_tagsmatch pubkey.%s mismatch\n",pubkeystr);
        return(-4);
    }
    return(0);
}

int32_t komodo_DEX_decryptbuf(uint8_t *buf,int32_t maxlen,struct DEX_datablob *ptr,bits256 pubkey,char *tagA)
{
    uint8_t *decoded,*allocated=0; int32_t newlen=0; bits256 senderpub;
    if ( (decoded= komodo_DEX_datablobdecrypt(&senderpub,&allocated,&newlen,ptr,pubkey,tagA)) != 0 )
    {
        if ( newlen <= maxlen )
            memcpy(buf,decoded,newlen);
        else newlen = -2;
    }
    if ( allocated != 0 )
        free(allocated), allocated = 0;
    return(newlen);
}

uint8_t *komodo_DEX_anondecode(bits256 *senderpub,uint8_t **allocatedp,uint8_t **allocated2p,uint8_t *data,int32_t *datalenp)
{
    bits256 priv0; uint8_t *decoded=0,*decoded2=0; int32_t i,newlen,newlen2; bits256 sender; char str[65];
    komodo_DEX_privkey(priv0);
    newlen = *datalenp;
    memset(senderpub,0,sizeof(*senderpub));
    if ( (decoded= komodo_DEX_decrypt(sender.bytes,allocatedp,data,&newlen,priv0)) != 0 )
    {
        if ( memcmp(sender.bytes,&GENESIS_PUBKEY,32) == 0 )
        {
            newlen2 = newlen;
            if ( (decoded2= komodo_DEX_decrypt(senderpub->bytes,allocated2p,decoded,&newlen2,priv0)) != 0 )
            {
                for (i=0; i<newlen-1; i++)
                    if ( isprint(decoded2[i]) == 0 )
                        break;
                if ( decoded2[i] == 0 )
                    *datalenp = i;
                else *datalenp = -2;
            } else *datalenp = -4;
        }
        else
        {
            fprintf(stderr,"first encryption not from genesispubkey %s\n",bits256_str(str,sender));
            *datalenp = -3;
        }
    } else *datalenp = -1;
    memset(&priv0,0,sizeof(priv0));
    if ( *datalenp < 0 )
    {
        fprintf(stderr,"anondecode error.%d\n",*datalenp);
        return(0);
    }
    return(decoded2);
}

uint64_t komodo_DEX_convert64(char *numstr)
{
    return((atof(numstr) + 0.0000000049999) * SATOSHIDEN);
}

UniValue komodo_DEX_dataobj(struct DEX_datablob *ptr)
{
    UniValue item(UniValue::VOBJ); uint32_t t; bits256 pubkey,senderpub; int32_t i,j,dflag=0,newlen; uint8_t *decoded,*allocated=0,destpub33[33]; uint64_t amountA,amountB; char taga[KOMODO_DEX_MAXKEYSIZE+1],tagb[KOMODO_DEX_MAXKEYSIZE+1],pubkeystr[67],str[67];
    iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
    iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE],sizeof(amountA),&amountA);
    iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE + sizeof(amountA)],sizeof(amountB),&amountB);
    item.push_back(Pair((char *)"timestamp",(int64_t)t));
    item.push_back(Pair((char *)"id",(int64_t)komodo_DEX_id(ptr)));
    bits256_str(str,ptr->hash);
    item.push_back(Pair((char *)"hash",str));
    if ( komodo_DEX_tagsextract(amountA,amountB,taga,tagb,pubkeystr,destpub33,ptr) >= 0 )
    {
        item.push_back(Pair((char *)"tagA",taga));
        item.push_back(Pair((char *)"tagB",tagb));
        item.push_back(Pair((char *)"pubkey",pubkeystr));
    }
    memcpy(pubkey.bytes,destpub33+1,32);
    komodo_DEX_payloadstr(item,&ptr->data[ptr->offset],ptr->datalen-4-ptr->offset,0);
    if ( (decoded= komodo_DEX_datablobdecrypt(&senderpub,&allocated,&newlen,ptr,pubkey,taga)) != 0 )
    {
        komodo_DEX_payloadstr(item,decoded,newlen,1);
        if ( ptr->data[1] == 'A' && strcmp(taga,(char *)"anon") == 0 )
        {
            uint8_t *anonallocated = 0,*anonallocated2=0; int32_t anonlen = newlen; char *anonmsg,senderstr[67]; bits256 senderpub;
            if ( (anonmsg= (char *)komodo_DEX_anondecode(&senderpub,&anonallocated,&anonallocated2,decoded,&anonlen)) != 0 )
            {
                item.push_back(Pair((char *)"anonmsg",anonmsg));
                senderstr[0] = '0';
                senderstr[1] = '1';
                bits256_str(senderstr+2,senderpub);
                item.push_back(Pair((char *)"anonsender",senderstr));
            }
            if ( anonallocated != 0 )
                free(anonallocated);
            if ( anonallocated2 != 0 )
                free(anonallocated2);
        }
    }
    str[0] = '0';
    str[1] = '1';
    bits256_str(str+2,senderpub);
    item.push_back(Pair((char *)"senderpub",str));
    if ( newlen < 0 )
        item.push_back(Pair((char *)"error","wrong sender"));
    if ( allocated != 0 )
        free(allocated), allocated = 0;
    sprintf(str,"%llu.%08llu",(long long)amountA/COIN,(long long)amountA % COIN);
    item.push_back(Pair((char *)"amountA",str));
    sprintf(str,"%llu.%08llu",(long long)amountB/COIN,(long long)amountB % COIN);
    item.push_back(Pair((char *)"amountB",str));
    item.push_back(Pair((char *)"priority",komodo_DEX_priority(ptr->hash.ulongs[0],ptr->datalen)));
    item.push_back(Pair((char *)"recvtime",(int64_t)ptr->recvtime));
    item.push_back(Pair((char *)"cancelled",(int64_t)ptr->cancelled));
    return(item);
}

UniValue _komodo_DEXget(uint32_t shorthash)
{
    UniValue result; int32_t modval; struct DEX_datablob *ptr;
    for (modval=0; modval<KOMODO_DEX_PURGETIME; modval++)
    {
        if ( (ptr= _komodo_DEXfind(modval,shorthash)) != 0 )
            return(komodo_DEX_dataobj(ptr));
    }
    return(result);
}

UniValue komodo_DEXbroadcast(uint64_t *locatorp,uint8_t funcid,char *hexstr,int32_t priority,char *tagA,char *tagB,char *destpub33,char *volA,char *volB)
{
    UniValue result; struct DEX_datablob *ptr=0; std::vector<uint8_t> packet; bits256 hash,pubkey; uint8_t quote[128],destpub[33],*payload=0,*payload2=0,*allocated=0; int32_t blastflag,i,m=0,ind,explen,len=0,datalen=0,destpubflag=0,slen,modval,iter; uint32_t shorthash,timestamp; uint64_t amountA=0,amountB=0;
    if ( hexstr == 0 || hexstr[0] == 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"broadcasting no payload is not supported"));
        return(result);
    }
    if ( locatorp != 0 )
        *locatorp = 0;
    blastflag = strcmp(hexstr,"ffff") == 0;
    if ( priority < 0 || priority > KOMODO_DEX_MAXPRIORITY )
        priority = KOMODO_DEX_MAXPRIORITY;
    if ( hexstr == 0 || tagA == 0 || tagB == 0 || destpub33 == 0 || volA == 0 || volB == 0 || strlen(tagA) >= KOMODO_DEX_TAGSIZE || strlen(tagB) >= KOMODO_DEX_TAGSIZE )
        return(-1);
    if ( tagA[0] == 0 && tagB[0] == 0 && destpub33[0] == 0 )
        tagA = (char *)"general";
    if ( volA[0] != 0 )
    {
        if ( atof(volA) < 0. )
        {
            fprintf(stderr,"negative volA error\n");
            return(result);
        }
        amountA = komodo_DEX_convert64(volA);
        //fprintf(stderr,"amountA %llu <- %s %.15f\n",(long long)amountA,volA,atof(volA) * SATOSHIDEN + 0.000000009);
    }
    if ( volB[0] != 0 )
    {
        if ( atof(volB) < 0. )
        {
            fprintf(stderr,"negative volB error\n");
            return(result);
        }
        amountB = komodo_DEX_convert64(volB);
    }
    for (iter=0; iter<10; iter++)
    {
        ptr = 0;
        len = 0;
        len = iguana_rwnum(1,&quote[len],sizeof(amountA),&amountA);
        len += iguana_rwnum(1,&quote[len],sizeof(amountB),&amountB);
        if ( is_hexstr(destpub33,0) == 66 )
        {
            decode_hex(destpub,33,destpub33);
            quote[len++] = 33;
            destpubflag = 1;
            if ( destpub[0] != 0x01 )
            {
                fprintf(stderr,"only pubkey starting with 0x01 is allowed, these are available from DEX_stats\n");
                return(-1);
            }
            //fprintf(stderr,"set destpub\n");
            memcpy(&quote[len],destpub,sizeof(destpub)), len += 33;
        } else quote[len++] = 0;
        if ( (slen= strlen(tagA)) > 0 )
        {
            //fprintf(stderr,"tagA (%s)\n",tagA);
            quote[len++] = slen;
            memcpy(&quote[len],tagA,slen), len += slen;
        } else quote[len++] = 0;
        if ( (slen= strlen(tagB)) > 0 )
        {
            //fprintf(stderr,"tagB (%s)\n",tagB);
            quote[len++] = slen;
            memcpy(&quote[len],tagB,slen), len += slen;
        } else quote[len++] = 0;
        if ( blastflag != 0 )
        {
            for (i=len; i<(int32_t)(sizeof(quote)/sizeof(*quote)); i++)
                quote[i] = (rand() >> 11) & 0xff;
            len = i;
        }
        else if ( (datalen= is_hexstr(hexstr,0)) == (int32_t)strlen(hexstr) && datalen > 1 && (datalen&1) == 0 )
        {
            datalen >>= 1;
            //fprintf(stderr,"offset.%d datalen.%d (%s)\n",len,datalen,hexstr);
            payload = (uint8_t *)malloc(datalen);
            decode_hex(payload,datalen,hexstr);
        }
        else if ( (datalen= (int32_t)strlen(hexstr)) > 0 )
        {
            payload = (uint8_t *)hexstr;
            datalen++;
        }
        allocated = 0;
        timestamp = (uint32_t)time(NULL);
        modval = (timestamp % KOMODO_DEX_PURGETIME);
        if ( destpubflag != 0 )
        {
            bits256 priv0;
            komodo_DEX_privkey(priv0);
            if ( strcmp(tagA,"inbox") == 0 )
            {
                for (i=0; i<32; i++)
                    pubkey.bytes[i] = destpub[i + 1];
            } else pubkey = GENESIS_PUBKEY;
            if ( (payload2= komodo_DEX_encrypt(&allocated,payload,&datalen,pubkey,priv0)) == 0 )
            {
                fprintf(stderr,"encryption error for datalen.%d\n",datalen);
                if ( allocated != 0 )
                    free(allocated), allocated = 0;
                memset(priv0.bytes,0,sizeof(priv0));
                return(0);
            }
            else if ( (0) )
            {
                for (i=0; i<datalen; i++)
                    fprintf(stderr,"%02x",payload2[i]);
                fprintf(stderr," payload2[%d] from [%d]\n",datalen,(int32_t)strlen(hexstr));
            }
            memset(priv0.bytes,0,sizeof(priv0));
        } else payload2 = payload;
        explen = (int32_t)(KOMODO_DEX_ROUTESIZE + len + datalen + sizeof(uint32_t));
        if ( (m= komodo_DEXgenquote(funcid,KOMODO_DEX_BLAST + priority + komodo_DEX_sizepriority(explen),hash,shorthash,packet,timestamp,quote,len,payload2,datalen)) != explen )
            fprintf(stderr,"unexpected packetsize n.%d != %d\n",m,explen);
        if ( allocated != 0 )
        {
            free(allocated);
            allocated = 0;
        }
        if ( payload != 0 )
        {
            if ( payload != (uint8_t *)hexstr )
                free(payload);
            payload = 0;
        }
        if ( blastflag != 0 && komodo_DEX_priority(hash.ulongs[0],explen) > priority + KOMODO_DEX_BLAST )
        {
            //fprintf(stderr,"skip harder than specified %d vs %d\n",komodo_DEX_priority(hash.ulongs[0],explen), priority + iter + komodo_DEX_sizepriority(explen));
            continue;
        }
        if ( m > KOMODO_DEX_MAXPACKETSIZE )
        {
            fprintf(stderr,"packetsize.%d > KOMODO_DEX_MAXPACKETSIZE.%d\n",m,KOMODO_DEX_MAXPACKETSIZE);
            return(0);
        }
        {
            pthread_mutex_lock(&DEX_globalmutex);
            iguana_rwnum(0,&packet[2],sizeof(timestamp),&timestamp);
            modval = (timestamp % KOMODO_DEX_PURGETIME);
            if ( (ptr= _komodo_DEXfind(modval,shorthash)) == 0 )
            {
                if ( (ptr= _komodo_DEXadd(timestamp,modval,hash,shorthash,&packet[0],packet.size())) == 0 )
                {
                    char str[65];
                    for (i=0; i<len&&i<64; i++)
                        fprintf(stderr,"%02x",quote[i]);
                    fprintf(stderr," ERROR issue order %08x %016llx %s! packetsize.%d\n",shorthash,(long long)hash.ulongs[0],bits256_str(str,hash),(int32_t)packet.size());
                }
            }
            else
            {
                static uint32_t counter;
                if ( counter++ < 100 )
                    fprintf(stderr," cant issue duplicate order modval.%d t.%u %08x %016llx\n",modval,timestamp,shorthash,(long long)hash.ulongs[0]);
                srand((int32_t)timestamp);
            }
            pthread_mutex_unlock(&DEX_globalmutex);
        }
        if ( blastflag == 0 )
            break;
        else usleep(1000);
    }
    if ( blastflag == 0 && ptr != 0 )
    {
        usleep(1000);
        result = komodo_DEX_dataobj(ptr);
        if ( locatorp != 0 )
        {
            iguana_rwnum(0,&ptr->data[2],sizeof(timestamp),&timestamp);
            *locatorp = ((uint64_t)timestamp << 32) | ptr->shorthash;
        }
        return(result);
    } else return(0);
}

int32_t _komodo_DEX_gettips(struct DEX_index *tips[KOMODO_DEX_MAXINDICES],int8_t &lenA,char *tagA,int8_t &lenB,char *tagB,int8_t &plen,uint8_t *destpub,char *destpub33,uint64_t &minamountA,char *minA,uint64_t &maxamountA,char *maxA,uint64_t &minamountB,char *minB,uint64_t &maxamountB,char *maxB)
{
    memset(tips,0,sizeof(*tips)*KOMODO_DEX_MAXINDICES);
    if ( tagA != 0 )
        lenA = (int32_t)strlen(tagA);
    if ( tagB != 0 )
        lenB = (int32_t)strlen(tagB);
    if ( tagA == 0 || tagB == 0 || destpub33 == 0 || minA == 0 || maxA == 0 || minB == 0 || maxB == 0 || lenA >= KOMODO_DEX_TAGSIZE || lenB >= KOMODO_DEX_TAGSIZE )
        return(-1);
    if ( tagA[0] == 0 && tagB[0] == 0 && destpub33[0] == 0 )
        tagA = (char *)"general";
    if ( minA[0] != 0 )
    {
        if ( atof(minA) < 0. )
        {
            fprintf(stderr,"negative minA error\n");
            return(-2);
        }
        minamountA = komodo_DEX_convert64(minA);
    }
    if ( maxA[0] != 0 )
    {
        if ( atof(maxA) < 0. )
        {
            fprintf(stderr,"negative maxA error\n");
            return(-3);
        }
        maxamountA = komodo_DEX_convert64(maxA);
    }
    if ( minB[0] != 0 )
    {
        if ( atof(minB) < 0. )
        {
            fprintf(stderr,"negative minB error\n");
            return(-4);
        }
        minamountB = komodo_DEX_convert64(minB);
    }
    if ( maxB[0] != 0 )
    {
        if ( atof(maxB) < 0. )
        {
            fprintf(stderr,"negative maxB error\n");
            return(-5);
        }
        maxamountB = komodo_DEX_convert64(maxB);
    }
    if ( minA > maxA || minB > maxB )
    {
        fprintf(stderr,"illegal value range [%.8f %.8f].A [%.8f %.8f].B\n",dstr(minamountA),dstr(maxamountA),dstr(minamountB),dstr(maxamountB));
        return(-6);
    }
    if ( is_hexstr(destpub33,0) == 66 )
    {
        decode_hex(destpub,33,destpub33);
        plen = 33;
    }
    //fprintf(stderr,"DEX_list (%s) (%s)\n",tagA,tagB);
    return(_DEX_updatetips(tips,0,0,lenA,(uint8_t *)tagA,lenB,(uint8_t *)tagB,destpub,plen) & 0xffff);
}

int32_t komodo_DEX_ptrfilter(uint64_t &amountA,uint64_t &amountB,struct DEX_datablob *ptr,int32_t minpriority,int8_t lenA,char *tagA,int8_t lenB,char *tagB,int8_t plen,uint8_t *destpub,uint64_t minamountA,uint64_t maxamountA,uint64_t minamountB,uint64_t maxamountB)
{
    int32_t priority,skipflag = 0;
    amountA = amountB = 0;
    if ( komodo_DEX_tagsmatch(amountA,amountB,ptr,(uint8_t *)tagA,lenA,(uint8_t *)tagB,lenB,destpub,plen) < 0 )
    {
        //fprintf(stderr,"skip %p due to no tagsmatch\n",ptr);
        skipflag = 1;
    }
    else if ( (priority= komodo_DEX_priority(ptr->hash.ulongs[0],ptr->datalen)) < minpriority )
    {
        //fprintf(stderr,"priority.%d < min.%d, skip\n",komodo_DEX_priority(ptr->hash.ulongs[0],ptr->datalen),minpriority);
        skipflag = 2;
    }
    else
    {
        iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE],sizeof(amountA),&amountA);
        iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE + sizeof(amountA)],sizeof(amountB),&amountB);
        if ( amountA < minamountA || amountA > maxamountA )
        {
            fprintf(stderr,"amountA %.8f vs min %.8f max %.8f, skip\n",dstr(amountA),dstr(minamountA),dstr(maxamountA));
            skipflag = 3;
        }
        else if ( amountB < minamountB || amountB > maxamountB )
        {
            fprintf(stderr,"amountB %.8f vs min %.8f max %.8f, skip\n",dstr(amountB),dstr(minamountB),dstr(maxamountB));
            skipflag = 4;
        }
    }
    return(skipflag);
}

UniValue _komodo_DEXlist(uint32_t stopat,int32_t minpriority,char *tagA,char *tagB,char *destpub33,char *minA,char *maxA,char *minB,char *maxB,char *stophashstr)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR);  struct DEX_datablob *ptr; int32_t err,ind,n=0,skipflag; bits256 stophash; struct DEX_index *tips[KOMODO_DEX_MAXINDICES],*index; uint64_t minamountA=0,maxamountA=(1LL<<63),minamountB=0,maxamountB=(1LL<<63),amountA,amountB; int8_t lenA=0,lenB=0,plen=0; uint8_t destpub[33]; uint32_t thislist;
    if ( stophashstr != 0 && is_hexstr(stophashstr,0) == 64 )
        decode_hex(stophash.bytes,32,stophashstr);
    else memset(stophash.bytes,0,32);
    //fprintf(stderr,"DEX_list (%s) (%s)\n",tagA,tagB);
    if ( (err= _komodo_DEX_gettips(tips,lenA,tagA,lenB,tagB,plen,destpub,destpub33,minamountA,minA,maxamountA,maxA,minamountB,minB,maxamountB,maxB)) < 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"errcode",err));
        return(result);
    }
    thislist = komodo_DEX_listid();
    n = 0;
    for (ind=0; ind<KOMODO_DEX_MAXINDICES; ind++)
    {
        if ( (index= tips[ind]) != 0 )
        {
            for (ptr=index->tail; ptr!=0; ptr=ptr->prevs[ind])
            {
                if ( (stopat != 0 && komodo_DEX_id(ptr) == stopat) || memcmp(stophash.bytes,ptr->hash.bytes,32) == 0 )
                    break;
                skipflag = komodo_DEX_ptrfilter(amountA,amountB,ptr,minpriority,lenA,tagA,lenB,tagB,plen,destpub,minamountA,maxamountA,minamountB,maxamountB);
                if ( skipflag == 0 && ptr->lastlist != thislist )
                {
                    ptr->lastlist = thislist;
                    //fprintf(stderr,"%u ",ptr->shorthash);
                    a.push_back(komodo_DEX_dataobj(ptr));
                    n++;
                }
                if ( ptr == index->head )
                    break;
            }
        }
    }
    //fprintf(stderr,"ids\n");
    result.push_back(Pair((char *)"result",(char *)"success"));
    result.push_back(Pair((char *)"matches",a));
    result.push_back(Pair((char *)"tagA",tagA));
    result.push_back(Pair((char *)"tagB",tagB));
    result.push_back(Pair((char *)"pubkey",destpub33));
    result.push_back(Pair((char *)"n",n));
    return(result);
}

// orderbook support

static int _cmp_orderbook(const void *a,const void *b) // bids
{
    int32_t retval = 0;
#define ptr_a (*(struct DEX_orderbookentry **)a)->price
#define ptr_b (*(struct DEX_orderbookentry **)b)->price
    if ( ptr_b > ptr_a+SMALLVAL )
        retval = -1;
    else if ( ptr_b < ptr_a-SMALLVAL )
        retval = 1;
    else
    {
#undef ptr_a
#undef ptr_b
#define ptr_a (*(struct DEX_orderbookentry **)a)->amountA
#define ptr_b (*(struct DEX_orderbookentry **)b)->amountA
        //printf("cmp %.8f vs %.8f <- %p %p\n",dstr(ptr_a),dstr(ptr_b),a,b);
        if ( ptr_b > ptr_a )
            return(1);
        else if ( ptr_b < ptr_a )
            return(-1);
    }
    // printf("%.8f vs %.8f -> %d\n",ptr_a,ptr_b,retval);
    return(retval);
#undef ptr_a
#undef ptr_b
}

static int _revcmp_orderbook(const void *a,const void *b) // asks
{
    int32_t retval = 0;
#define ptr_a (*(struct DEX_orderbookentry **)a)->price
#define ptr_b (*(struct DEX_orderbookentry **)b)->price
    if ( ptr_b > ptr_a+SMALLVAL )
        retval = 1;
    else if ( ptr_b < ptr_a-SMALLVAL )
        retval = -1;
    else
    {
#undef ptr_a
#undef ptr_b
#define ptr_a (*(struct DEX_orderbookentry **)a)->amountA
#define ptr_b (*(struct DEX_orderbookentry **)b)->amountA
        //printf("revcmp %.8f vs %.8f <- %p %p\n",dstr(ptr_a),dstr(ptr_b),a,b);
        if ( ptr_b > ptr_a )
            return(1);
        else if ( ptr_b < ptr_a )
            return(-1);
    }
    // printf("%.8f vs %.8f -> %d\n",ptr_a,ptr_b,retval);
    return(retval);
#undef ptr_a
#undef ptr_b
}

UniValue DEX_orderbookjson(struct DEX_orderbookentry *op)
{
    UniValue item(UniValue::VOBJ); char str[67]; int32_t i;
    if ( op->price >= 0.00000001-SMALLVAL )
        sprintf(str,"%0.8f",op->price + 0.0000000049);
    else sprintf(str,"%0.15f",op->price);
    item.push_back(Pair((char *)"price",str));
    sprintf(str,"%0.8f",((double)op->amountA + 0.0000000049)/SATOSHIDEN);
    item.push_back(Pair((char *)"baseamount",str));
    sprintf(str,"%0.8f",((double)op->amountB + 0.0000000049)/SATOSHIDEN);
    item.push_back(Pair((char *)"relamount",str));
    item.push_back(Pair((char *)"priority",(int64_t)op->priority));
    for (i=0; i<33; i++)
        sprintf(&str[i<<1],"%02x",op->pubkey33[i]);
    str[i<<1] = 0;
    item.push_back(Pair((char *)"pubkey",str));
    item.push_back(Pair((char *)"timestamp",(int64_t)op->timestamp));
    bits256_str(str,op->hash);
    item.push_back(Pair((char *)"hash",str));
    item.push_back(Pair((char *)"id",(int64_t)op->shorthash));
    return(item);
}

struct DEX_orderbookentry *DEX_orderbookentry(struct DEX_datablob *ptr,int32_t revflag,char *base,char *rel)
{
    struct DEX_orderbookentry *op; uint64_t amountA,amountB; double price = 0.;
    char taga[KOMODO_DEX_MAXKEYSIZE+1],tagb[KOMODO_DEX_MAXKEYSIZE+1],pubkeystr[67]; uint8_t destpub33[33];
    if ( komodo_DEX_tagsextract(amountA,amountB,taga,tagb,pubkeystr,destpub33,ptr) == 0 )
    {
        if ( strcmp(taga,base) != 0 || strcmp(tagb,rel) != 0 )
            return(0);
    }
    if ( (op= (struct DEX_orderbookentry *)calloc(1,sizeof(*op))) != 0 )
    {
        memcpy(op->pubkey33,destpub33,33);
        iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE],sizeof(amountA),&amountA);
        iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE + sizeof(amountA)],sizeof(amountB),&amountB);
        if ( revflag == 0 )
        {
            op->amountA = amountA;
            op->amountB = amountB;
            if ( amountA != 0 )
                price = (double)amountB / amountA;
        }
        else
        {
            op->amountA = amountB;
            op->amountB = amountA;
            if ( amountB != 0 )
                price = (double)amountA / amountB;
        }
        op->price = price;
        iguana_rwnum(0,&ptr->data[2],sizeof(op->timestamp),&op->timestamp);
        op->hash = ptr->hash;
        op->shorthash = _komodo_DEXquotehash(ptr->hash,ptr->datalen);
        op->priority = ptr->priority;
        
    }
    return(op);
}

UniValue _komodo_DEXorderbook(int32_t revflag,int32_t maxentries,int32_t minpriority,char *tagA,char *tagB,char *destpub33,char *minA,char *maxA,char *minB,char *maxB)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); struct DEX_orderbookentry *op; std::vector<struct DEX_orderbookentry *>orders; struct DEX_datablob *ptr; uint32_t thislist; int32_t i,err,ind,n=0,skipflag; struct DEX_index *tips[KOMODO_DEX_MAXINDICES],*index; uint64_t minamountA=0,maxamountA=(1LL<<63),minamountB=0,maxamountB=(1LL<<63),amountA,amountB; int8_t lenA=0,lenB=0,plen=0; uint8_t destpub[33];
    if ( maxentries <= 0 )
        maxentries = 10;
    if ( tagA[0] == 0 || tagB[0] == 0 )
    {
        fprintf(stderr,"need both tagA and tagB to specify base/rel for orderbook\n");
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"errcode",-13));
        return(result);
    }
    if ( (err= _komodo_DEX_gettips(tips,lenA,tagA,lenB,tagB,plen,destpub,destpub33,minamountA,minA,maxamountA,maxA,minamountB,minB,maxamountB,maxB)) < 0 )
    {
        //fprintf(stderr,"couldnt find any\n");
        return(a);
    }
    thislist = komodo_DEX_listid();
    n = 0;
    for (ind=KOMODO_DEX_MAXINDICES-1; ind<KOMODO_DEX_MAXINDICES; ind++) // only need tagABs
    {
        if ( (index= tips[ind]) != 0 )
        {
            for (ptr=index->tail; ptr!=0; ptr=ptr->prevs[ind])
            {
                skipflag = komodo_DEX_ptrfilter(amountA,amountB,ptr,minpriority,lenA,tagA,lenB,tagB,plen,destpub,minamountA,maxamountA,minamountB,maxamountB);
                if ( skipflag == 0 && ptr->cancelled == 0 && amountA != 0 && amountB != 0 )
                {
                    if ( ptr->lastlist != thislist && (op= DEX_orderbookentry(ptr,revflag,tagA,tagB)) != 0 ) //
                    {
                        //fprintf(stderr,"ADD n.%d lastlist.%u vs %u\n",n,ptr->lastlist,thislist);
                        ptr->lastlist = thislist;
                        orders.push_back(op);
                        n++;
                    } else fprintf(stderr,"skip ptr->lastlist.%u vs thislist.%d\n",ptr->lastlist,thislist);
                } //else fprintf(stderr,"skipflag.%d cancelled.%u plen.%d amountA %.8f amountB %.8f\n",skipflag,ptr->cancelled,plen,dstr(amountA),dstr(amountB));
                if ( ptr == index->head )
                    break;
            }
        }
    }
    if ( n > 0 )
    {
        //fprintf(stderr,"sort %d orders for %s/%s\n",n,tagA,tagB);
        qsort(&orders[0],n,sizeof(struct DEX_orderbookentry *),revflag != 0 ? _revcmp_orderbook : _cmp_orderbook);
        for (i=0; i<maxentries&&i<n; i++)
        {
            a.push_back(DEX_orderbookjson(orders[i]));
            free(orders[i]);
        }
        for (; i<n; i++)
            free(orders[i]);
    }
    return(a);
}

// general stats

UniValue komodo_DEX_stats()
{
    static uint32_t lastadd,lasttime;
    UniValue result(UniValue::VOBJ); char str[65],pubstr[67],logstr[1024],recvaddr[64]; int32_t i,total,histo[64]; uint32_t now,totalhash,d;
    pubkey2addr(recvaddr,NOTARY_PUBKEY33);
    pthread_mutex_lock(&DEX_globalmutex);
    now = (uint32_t)time(NULL);
    bits256_str(pubstr+2,DEX_pubkey);
    pubstr[0] = '0';
    pubstr[1] = '1';
    result.push_back(Pair((char *)"result",(char *)"success"));
    result.push_back(Pair((char *)"publishable_pubkey",pubstr));
    result.push_back(Pair((char *)"secpkey",(char *)NOTARY_PUBKEY.c_str()));
    result.push_back(Pair((char *)"recvaddr",recvaddr));
    result.push_back(Pair((char *)"recvZaddr",(char *)GetArg("-recvZaddr", "").c_str()));
    result.push_back(Pair((char *)"secpkey",(char *)NOTARY_PUBKEY.c_str()));
    result.push_back(Pair((char *)"handle",(char *)GetArg("-handle", "").c_str()));
    result.push_back(Pair((char *)"txpowbits",(int64_t)KOMODO_DEX_TXPOWBITS));
    result.push_back(Pair((char *)"vip",(int64_t)KOMODO_DEX_VIPLEVEL));
    result.push_back(Pair((char *)"cmdpriority",(int64_t)KOMODO_DEX_CMDPRIORITY));
    memset(histo,0,sizeof(histo));
    totalhash = _komodo_DEXtotal(histo,total);
    sprintf(logstr,"RAM.%d %08x R.%lld S.%lld A.%lld dup.%lld | L.%lld A.%lld coll.%lld | lag (%.4f %.4f %.4f) err.%lld pend.%lld T/F %lld/%lld | ",total,totalhash,(long long)DEX_totalrecv,(long long)DEX_totalsent,(long long)DEX_totaladd,(long long)DEX_duplicate,(long long)DEX_lookup32,(long long)DEX_add32,(long long)DEX_collision32,DEX_lag,DEX_lag2,DEX_lag3,(long long)DEX_maxlag,(long long)DEX_Numpending,(long long)DEX_truncated,(long long)DEX_freed);
    for (i=13; i>=0; i--)
        sprintf(logstr+strlen(logstr),"%.0f ",(double)histo[i]);//1000.*histo[i]/(total+1)); // expected 1 1 2 5 | 10 10 10 10 10 | 10 9 9 7 5
    if ( (d= (now-lasttime)) <= 0 )
        d = 1;
    sprintf(logstr+strlen(logstr),"%s %lld/sec",komodo_DEX_islagging()!=0?"LAG":"",(long long)(DEX_totaladd - lastadd)/d);
    lasttime = now;
    lastadd = DEX_totaladd;
    result.push_back(Pair((char *)"perfstats",logstr));
    pthread_mutex_unlock(&DEX_globalmutex);
    return(result);
}

UniValue komodo_DEXcancel(char *pubkeystr,uint32_t shorthash,char *tagA,char *tagB)
{
    UniValue result(UniValue::VOBJ); uint8_t hex[34],pub33[33]; char hexstr[67],checkstr[67]; int32_t i,lenA,lenB,len=0;
    checkstr[0] = '0';
    checkstr[1] = '1';
    bits256_str(checkstr+2,DEX_pubkey);
    decode_hex(pub33,33,checkstr);
    if ( shorthash != 0 )
    {
        len = iguana_rwnum(1,&hex[len],sizeof(shorthash),&shorthash);
        {
            pthread_mutex_lock(&DEX_globalmutex);
            _komodo_DEX_cancelid(shorthash,DEX_pubkey,(uint32_t)time(NULL));
            pthread_mutex_unlock(&DEX_globalmutex);
        }
    }
    else if ( pubkeystr[0] != 0 )
    {
        if ( strcmp(checkstr,pubkeystr) != 0 )
        {
            result.push_back(Pair((char *)"result",(char *)"error"));
            result.push_back(Pair((char *)"error",(char *)"wrong pubkey"));
            result.push_back(Pair((char *)"pubkey",pubkeystr));
            result.push_back(Pair((char *)"correct pubkey",checkstr));
            return(result);
        }
        decode_hex(hex,33,checkstr);
        len = 33;
        {
            pthread_mutex_lock(&DEX_globalmutex);
            _komodo_DEX_cancelpubkey((char *)"",(char *)"",pub33,(uint32_t)time(NULL));
            pthread_mutex_unlock(&DEX_globalmutex);
        }
    }
    else if ( tagA[0] != 0 && tagB[0] != 0 )
    {
        lenA = (int32_t)strlen(tagA);
        lenB = (int32_t)strlen(tagB);
        if ( lenA >= KOMODO_DEX_TAGSIZE || lenB >= KOMODO_DEX_TAGSIZE )
        {
            result.push_back(Pair((char *)"result",(char *)"error"));
            result.push_back(Pair((char *)"error",(char *)"too long tags"));
            result.push_back(Pair((char *)"tagA",tagA));
            result.push_back(Pair((char *)"tagB",tagB));
            return(result);
        }
        hex[len++] = lenA;
        memcpy(&hex[len],tagA,lenA), len += lenA;
        hex[len++] = lenB;
        memcpy(&hex[len],tagB,lenB), len += lenB;
        {
            pthread_mutex_lock(&DEX_globalmutex);
            _komodo_DEX_cancelpubkey(tagA,tagB,pub33,(uint32_t)time(NULL));
            pthread_mutex_unlock(&DEX_globalmutex);
        }
    }
    for (i=0; i<len; i++)
        sprintf(&hexstr[i<<1],"%02x",hex[i]);
    hexstr[i<<1] = 0;
    //fprintf(stderr,"broadcancel command (%s)\n",hexstr);
    return(komodo_DEXbroadcast(0,'X',hexstr,KOMODO_DEX_CMDPRIORITY,(char *)"cancel",(char *)"",checkstr,(char *)"",(char *)""));
}

// from rpc calls

UniValue komodo_DEXget(uint32_t shorthash)
{
    UniValue result;
    pthread_mutex_lock(&DEX_globalmutex);
    result = _komodo_DEXget(shorthash);
    pthread_mutex_unlock(&DEX_globalmutex);
    return(result);
}

UniValue komodo_DEXlist(uint32_t stopat,int32_t minpriority,char *tagA,char *tagB,char *destpub33,char *minA,char *maxA,char *minB,char *maxB,char *stophashstr)
{
    UniValue result;
    pthread_mutex_lock(&DEX_globalmutex);
    result = _komodo_DEXlist(stopat,minpriority,tagA,tagB,destpub33,minA,maxA,minB,maxB,stophashstr);
    pthread_mutex_unlock(&DEX_globalmutex);
    return(result);
}

UniValue komodo_DEXorderbook(int32_t revflag,int32_t maxentries,int32_t minpriority,char *tagA,char *tagB,char *destpub33,char *minA,char *maxA,char *minB,char *maxB)
{
    UniValue result;
    pthread_mutex_lock(&DEX_globalmutex);
    result = _komodo_DEXorderbook(revflag,maxentries,minpriority,tagA,tagB,destpub33,minA,maxA,minB,maxB);
    pthread_mutex_unlock(&DEX_globalmutex);
    return(result);
}

bits256 komodo_DEX_filehash(FILE *fp,uint64_t offset0,uint64_t rlen,char *fname)
{
    bits256 filehash; uint8_t *data = (uint8_t *)calloc(1,rlen);
    fseek(fp,offset0,SEEK_SET);
    memset(filehash.bytes,0,sizeof(filehash));
    if ( fread(data,1,rlen,fp) == rlen )
        vcalc_sha256(0,filehash.bytes,data,rlen);
    else fprintf(stderr," reading %lld bytes from %s.%llu\n",(long long)rlen,fname,(long long)offset0);
    free(data);
    return(filehash);
}

struct DEX_datablob *_komodo_DEX_latestptr(char *tagA,char *tagB,char *pubkeystr,uint64_t offset0)
{
    struct DEX_index *tips[KOMODO_DEX_MAXINDICES],*index; struct DEX_datablob *latestptr=0,*ptr = 0; uint64_t minamountA,maxamountA,minamountB,maxamountB,amountA,amountB; uint8_t pubkey33[33]; int8_t lenA,lenB,plen; int32_t errflag,ind=0; uint32_t t,latest=0;
    errflag = _komodo_DEX_gettips(tips,lenA,tagA,lenB,(char *)tagB,plen,pubkey33,pubkeystr,minamountA,(char *)"",maxamountA,(char *)"",minamountB,(char *)"",maxamountB,(char *)"");
    if ( (errflag & 0xffff) > 0 )
    {
        if ( (index= tips[ind]) != 0 ) // pubkey list should be shortest, on average
        {
            for (ptr=index->tail; ptr!=0; ptr=ptr->prevs[ind])
            {
                if ( ptr->cancelled == 0 && komodo_DEX_tagsmatch(amountA,amountB,ptr,(uint8_t *)tagA,lenA,(uint8_t *)tagB,lenB,pubkey33,plen) == 0 )
                {
                    if ( strcmp(tagA,(char *)"slices") != 0 || amountA/COIN == offset0 )
                    {
                        iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
                        if ( t > latest )
                        {
                            latest = t;
                            latestptr = ptr;
                        }
                    }
                    //if ( strcmp(tagA,(char *)"slices") == 0 )
                    //    fprintf(stderr,"slices amountA %llu vs %llu\n",(long long)amountA/COIN,(long long)offset0);
                }
                if ( ptr == index->head )
                    break;
            }
        } else fprintf(stderr,"gettips error.%d\n",errflag);
    }
    return(latestptr);
}

int32_t komodo_DEX_locatorsload(uint64_t *locators,uint64_t *offset0p,int32_t *numlocatorsp,char *locatorfname)
{
    FILE *fp; int32_t i,j,errflag=0,len,lag; uint64_t locator; uint32_t now = (uint32_t)time(NULL);
    *numlocatorsp = 0;
    if ( (fp= fopen(locatorfname,(char *)"rb")) != 0 )
    {
        errflag = 0;
        fseek(fp,0,SEEK_END);
        len = (int32_t)ftell(fp);
        rewind(fp);
        if ( fread(offset0p,1,sizeof(*offset0p),fp) != sizeof(*offset0p) )
            errflag++;
        for (i=sizeof(*offset0p),j=0; i<len; i+=8,j++)
        {
            locator = 0;
            if ( fread(&locator,1,sizeof(locator),fp) != sizeof(locator) )
                errflag++;
            iguana_rwnum(0,(uint8_t *)&locator,sizeof(locators[j]),&locators[j]);
            locator = locators[j];
            lag = (int32_t)(now - (uint32_t)(locator >> 32));
            if ( lag > KOMODO_DEX_PURGETIME-KOMODO_DEX_MAXLAG )
            {
                //fprintf(stderr,"purged locator[%d] %u %08x, lag %d\n",j,(uint32_t)(locator >> 32),(uint32_t)locator,lag);
                locators[j] = 0;
            }
        }
        *numlocatorsp = j;
        fclose(fp), fp = 0;
    }
    if ( errflag != 0 )
        fprintf(stderr,"locators load numerrs.%d\n",errflag);
    return(-errflag);
}

int32_t komodo_DEX_request(int32_t priority,uint32_t shorthash,uint32_t timestamp,char *tagA,char *tagB)
{
    uint8_t hex[16]; char str[67],hexstr[33]; int32_t i,n=0;
    iguana_rwnum(1,&hex[0],sizeof(shorthash),&shorthash);
    iguana_rwnum(1,&hex[sizeof(shorthash)],sizeof(timestamp),&timestamp);
    for (i=0; i<sizeof(uint32_t)*2; i++)
        sprintf(&hexstr[i<<1],"%02x",hex[i]);
    hexstr[i<<1] = 0;
    str[0] = '0';
    str[1] = '1';
    bits256_str(str+2,DEX_pubkey);
    komodo_DEXbroadcast(0,'R',hexstr,priority+KOMODO_DEX_CMDPRIORITY,tagA,tagB,str,(char *)"",(char *)"");
    return(_komodo_DEX_locatorsextract(1,shorthash,timestamp % KOMODO_DEX_PURGETIME,priority));
}

int32_t komodo_DEX_locatorsync(int32_t &needrequest,int32_t &written,FILE *fp,uint64_t locator,long offset,bits256 senderpub,char *tagA)
{
    uint32_t t,h; struct DEX_datablob *fragptr; int32_t fraglen,errflag=0; uint8_t buf[KOMODO_DEX_FILEBUFSIZE];
    t = locator >> 32;
    h = locator & 0xffffffff;
    {
        pthread_mutex_lock(&DEX_globalmutex);
        fragptr = _komodo_DEXfind(t % KOMODO_DEX_PURGETIME,h);
        pthread_mutex_unlock(&DEX_globalmutex);
    }
    errflag = 0;
    if ( fragptr != 0 )
    {
        if ( (fraglen= komodo_DEX_decryptbuf(buf,sizeof(buf),fragptr,senderpub,(char *)tagA)) > 0 )
        {
            fseek(fp,offset,SEEK_SET);
            if ( fwrite(buf,1,fraglen,fp) != fraglen )
            {
                fprintf(stderr,"write error offset %ld %ld\n",offset,ftell(fp));
                errflag = 1;
            }
            else
            {
                written++;
                //fprintf(stderr,"write %s:%ld [%d] sizepriority.%d\n",fname,i*sizeof(buf)+offset0,fraglen,komodo_DEX_sizepriority(fragptr->datalen));
            }
        }
        else
        {
            fprintf(stderr,"error decrypting into buf for offset of %ld, fraglen.%d datalen.%d h.%u\n",offset,fraglen,fragptr->datalen,h);
            errflag = 1;
        }
    }
    else
    {
        errflag = 1;
        //fprintf(stderr,"%s: missing t.%u h.%08x\n",fname,t % KOMODO_DEX_PURGETIME,h);
        needrequest = 1;
    }
    return(-errflag);
}

UniValue komodo_DEXsubscribe(int32_t &cmpflag,char *origfname,int32_t priority,uint32_t shorthash,char *publisher,int32_t sliceid)
{
    static uint64_t locators[KOMODO_DEX_MAXPACKETSIZE/sizeof(uint64_t)+1],zero[4];
    static uint64_t prevlocators[KOMODO_DEX_MAXPACKETSIZE/sizeof(uint64_t)+1];
    UniValue result(UniValue::VOBJ); FILE *fp; int32_t i,j,n,num,written=0,numprev,fraglen,errflag,modval,requestflag=0,missing=0,len=0,newlen=0; bits256 senderpub,pubkey,filehash; uint8_t tagA[KOMODO_DEX_TAGSIZE+1],tagB[KOMODO_DEX_TAGSIZE+1],pubkey33[33],*decoded,*allocated=0,hex[8]; struct DEX_datablob *fragptr,*ptr = 0; char str[67],pubkeystr[67],fname[512],tagBstr[33],fullfname[512],locatorfname[512]; bits256 checkhash; uint32_t t,h; uint64_t locator,amountA,amountB,mult,prevoffset0,offset0=0; int8_t lenA,lenB,plen;
    cmpflag = 0;
    if ( sliceid < 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"negative sliceid"));
        result.push_back(Pair((char *)"sliceid",(int64_t)sliceid));
        return(result);
    }
    if ( publisher == 0 || publisher[0] == 0 || strlen(publisher) > 66 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"need publisher pubkey for sub"));
        return(result);
    }
    mult = KOMODO_DEX_FILEBUFSIZE * KOMODO_DEX_STREAMSIZE;
    if ( sliceid > 0 )
    {
        offset0 = ((uint64_t)sliceid - 1) * mult;
        sprintf(fname,"%s.%llu",origfname,(long long)offset0);
    } else strcpy(fname,origfname);
    {
        pthread_mutex_lock(&DEX_globalmutex);
        memset(checkhash.bytes,0,sizeof(checkhash));
        if ( (ptr= _komodo_DEX_latestptr(sliceid == 0 ? (char *)"files" : (char *)"slices",origfname,publisher,offset0)) != 0 )
        {
            if ( (decoded= komodo_DEX_datablobdecrypt(&senderpub,&allocated,&newlen,ptr,*(bits256 *)&zero,sliceid == 0 ? (char *)"files" : (char *)"slices")) != 0 && newlen == 32 )
            {
                memcpy(checkhash.bytes,decoded,32);
                //for (i=0; i<32; i++)
                //    fprintf(stderr,"%02x",decoded[i]);
            }
            if ( allocated != 0 )
                free(allocated), allocated = 0;
            //fprintf(stderr," datalen.%d for %s sliceid.%d newlen.%d\n",ptr->datalen,fname,sliceid,newlen);
        }
        if ( shorthash == 0 )
        {
            if ( sliceid == 0 )
                sprintf(tagBstr,"locators");
            else sprintf(tagBstr,"%llu",(long long)offset0);
            if ( (ptr= _komodo_DEX_latestptr(origfname,tagBstr,publisher,0)) != 0 )
                shorthash = ptr->shorthash;
            //fprintf(stderr,"fname.%s auto search %s %s %s shorthash.%08x sliceid.%d\n",fname,origfname,tagBstr,publisher,shorthash,sliceid);
        }
        if ( shorthash != 0 )
        {
            for (modval=0; modval<KOMODO_DEX_PURGETIME; modval++)
            {
                if ( (ptr= _komodo_DEXfind(modval,shorthash)) != 0 )
                    break;
            }
        }
        pthread_mutex_unlock(&DEX_globalmutex);
    }
    if ( ptr == 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"couldnt find id"));
        result.push_back(Pair((char *)"id",(int64_t)shorthash));
        return(result);
    }
    memset(tagA,0,sizeof(tagA));
    memset(tagB,0,sizeof(tagB));
    if ( komodo_DEX_extract(amountA,amountB,lenA,tagA,lenB,tagB,pubkey33,plen,&ptr->data[KOMODO_DEX_ROUTESIZE],ptr->datalen-KOMODO_DEX_ROUTESIZE) < 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"couldnt extract tags"));
        return(result);
    }
    if ( strcmp((char *)tagA,origfname) != 0 || strcmp((char *)tagB,tagBstr) != 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"tags mismatch"));
        result.push_back(Pair((char *)"tagA",(char *)tagA));
        result.push_back(Pair((char *)"filename",origfname));
        result.push_back(Pair((char *)"tagB",(char *)tagB));
        result.push_back(Pair((char *)"sliceid",(int64_t)sliceid));
        return(result);
    }
    memcpy(pubkey.bytes,pubkey33+1,32);
    if ( (decoded= komodo_DEX_datablobdecrypt(&senderpub,&allocated,&newlen,ptr,pubkey,(char *)tagA)) != 0 && (newlen & 7) == 0 )
    {
        iguana_rwnum(0,&decoded[0],sizeof(offset0),&offset0);
        for (i=sizeof(offset0),j=0; i<newlen; i+=8,j++)
            iguana_rwnum(0,&decoded[j*8 + 8],sizeof(locators[j]),&locators[j]);
        num = j;
        result.push_back(Pair((char *)"fname",fname));
        result.push_back(Pair((char *)"id",(int64_t)shorthash));
        str[0] = '0';
        str[1] = '1';
        bits256_str(str+2,senderpub);
        result.push_back(Pair((char *)"senderpub",str));
        result.push_back(Pair((char *)"filesize",(int64_t)amountA));
        result.push_back(Pair((char *)"fragments",(int64_t)amountB));
        result.push_back(Pair((char *)"numlocators",(int64_t)(newlen-sizeof(uint64_t))/sizeof(uint64_t)));
        sprintf(locatorfname,"%s.%s.locators",fname,str);
        sprintf(fullfname,"%s.%s",fname,str);
        //fprintf(stderr,"orig %s fname %s locator %s full %s num.%d\n",origfname,fname,locatorfname,fullfname,num);
        if ( amountB*sizeof(uint64_t)+sizeof(uint64_t) == newlen )
        {
            if ( komodo_DEX_locatorsload(prevlocators,&prevoffset0,&numprev,locatorfname) == 0 )
            {
                if ( offset0 == prevoffset0 )
                {
                    for (i=0; i<num&&i<numprev; i++)
                    {
                        if ( locators[i] == prevlocators[i] )
                            locators[i] = 0;
                        //else fprintf(stderr,"%llx vs %llx ",(long long)locators[i],(long long)prevlocators[i]);
                    }
                } // else fprintf(stderr,"prevoffset0.%llu != offset0.%llu\n",(long long)prevoffset0,(long long)offset0);
            } else fprintf(stderr,"prevlocators read errors for %s\n",fname);
            if ( (fp= fopen(fullfname,(char *)"rb+")) == 0 )
                fp = fopen(fullfname,(char *)"wb");
            if ( fp != 0 )
            {
                for (i=0; i<(int32_t)amountB; i++)
                {
                    if ( (locator= locators[i]) == 0 ) // we already had it from previous rpc call
                    {
                        locators[i] = prevlocators[i];
                        continue;
                    }
                    if ( komodo_DEX_locatorsync(requestflag,written,fp,locator,i*KOMODO_DEX_FILEBUFSIZE,senderpub,(char *)tagA) < 0 )
                    {
                        missing++;
                        locators[i] = 0;
                    }
                }
                fclose(fp), fp = 0;
                if ( (fp= fopen(fullfname,"rb")) != 0 )
                {
                    fseek(fp,0,SEEK_END);
                    filehash = komodo_DEX_filehash(fp,0,ftell(fp),fullfname);
                    result.push_back(Pair((char *)"filehash",bits256_str(str,filehash)));
                    result.push_back(Pair((char *)"checkhash",bits256_str(str,checkhash)));
                    if ( missing == 0 && ftell(fp) == amountA )
                    {
                        result.push_back(Pair((char *)"result",(char *)"success"));
                        if ( memcmp(checkhash.bytes,zero,sizeof(checkhash)) != 0 && memcmp(checkhash.bytes,filehash.bytes,sizeof(checkhash)) != 0 )
                            result.push_back(Pair((char *)"warning",(char *)"extract and compare filehash"));
                        else cmpflag = 1;
                    }
                    else
                    {
                        result.push_back(Pair((char *)"result",(char *)"error"));
                        if ( missing != 0 )
                        {
                            result.push_back(Pair((char *)"error",(char *)"missing fragments"));
                            result.push_back(Pair((char *)"missing",(int64_t)missing));
                        }
                        if ( (ftell(fp)-offset0) != amountA )
                        {
                            result.push_back(Pair((char *)"error",(char *)"wrong size"));
                            result.push_back(Pair((char *)"actual_filesize",(int64_t)(ftell(fp)-offset0)));
                        }
                    }
                    fclose(fp), fp = 0;
                } else fprintf(stderr,"couldnt read %s\n",fullfname);
            } else fprintf(stderr,"couldnt open %s\n",fullfname);
            if ( (fp= fopen(locatorfname,(char *)"wb")) != 0 )
            {
                fwrite(&offset0,1,sizeof(offset0),fp);
                fwrite(locators,sizeof(locators[0]),num,fp);
                fclose(fp), fp = 0;
            }
        }
        else
        {
            result.push_back(Pair((char *)"result",(char *)"error"));
            result.push_back(Pair((char *)"error",(char *)"couldnt decrypt fragment"));
            result.push_back(Pair((char *)"newlen",newlen));
        }
    }
    else
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"couldnt decrypt payload"));
    }
    if ( newlen < 0 )
    {
        result.push_back(Pair((char *)"error","wrong sender"));
        str[0] = '0';
        str[1] = '1';
        bits256_str(str+2,senderpub);
        result.push_back(Pair((char *)"senderpub",str));
    }
    if ( allocated != 0 )
        free(allocated), allocated = 0;
    if ( written > 0 )
        fprintf(stderr,"number of fragments written: %d\n",written);
    if ( requestflag != 0 )
    {
        iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
        n = komodo_DEX_request(priority,shorthash,t,(char *)origfname,(char *)"request");
        result.push_back(Pair((char *)"status","request sent to get missing blocks"));
        result.push_back(Pair((char *)"n",n));
    }
    return(result);
}

UniValue komodo_DEXpublish(char *fname,int32_t priority,int32_t sliceid)
{
    static uint8_t locators[KOMODO_DEX_MAXPACKETSIZE];
    UniValue result(UniValue::VOBJ); FILE *fp,*oldfp=0; uint64_t locator,filesize=0,volA,offset0=0,prevoffset0; long fsize; int32_t i,rlen,rescan=0,n,cmpflag,numprev,oldn=0,numlocators=0,changed=0,mult; bits256 filehash; uint8_t buf[KOMODO_DEX_FILEBUFSIZE],oldbuf[KOMODO_DEX_FILEBUFSIZE],zeros[sizeof(uint64_t)]; char bufstr[sizeof(buf)*2+1],pubkeystr[67],str[65],fname2[512],volAstr[16],volBstr[16],locatorfname[512],oldfname[512],*hexstr;
    if ( sliceid < 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"negative sliceid"));
        result.push_back(Pair((char *)"sliceid",(int64_t)sliceid));
        return(result);
    }
    mult = KOMODO_DEX_FILEBUFSIZE * KOMODO_DEX_STREAMSIZE;
    pubkeystr[0] = '0';
    pubkeystr[1] = '1';
    bits256_str(&pubkeystr[2],DEX_pubkey);
    if ( sliceid > 0 )
    {
        offset0 = ((uint64_t)sliceid - 1) * mult;
        sprintf(oldfname,"%s.%llu.%s",fname,(long long)offset0,pubkeystr);
    } else sprintf(oldfname,"%s.%s",fname,pubkeystr);
    sprintf(locatorfname,"%s.locators",oldfname);
    if ( (fp= fopen(oldfname,"rb")) == 0 )
        rescan = 1;
    else fclose(fp), fp = 0;
    if ( (fp= fopen(locatorfname,"rb")) == 0 )
        rescan = 1;
    else fclose(fp), fp = 0;
    //fprintf(stderr,"fnames (%s): (%s) and (%s) rescan.%d\n",fname,oldfname,locatorfname,rescan);
    if ( strlen(fname) >= KOMODO_DEX_TAGSIZE )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"filename longer than 15 chars"));
        result.push_back(Pair((char *)"filename",fname));
        return(result);
    }
    else if ( (fp= fopen(fname,(char *)"rb")) == 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"file not found"));
        result.push_back(Pair((char *)"filename",fname));
        return(result);
    }
    fseek(fp,0,SEEK_END);
    fsize = ftell(fp);
    if ( sliceid == 0 )
    {
        if ( fsize/sizeof(buf) > (sizeof(locators)-sizeof(uint64_t))/sizeof(uint64_t) )
        {
            result.push_back(Pair((char *)"result",(char *)"error"));
            result.push_back(Pair((char *)"error",(char *)"file too big"));
            result.push_back(Pair((char *)"filename",fname));
            result.push_back(Pair((char *)"filesize",(int64_t)fsize));
            fclose(fp), fp = 0;
            return(result);
        }
        komodo_DEXsubscribe(cmpflag,fname,priority,0,pubkeystr,0);
    }
    else
    {
        fsize -= offset0;
        if ( fsize > mult )
            fsize = mult;
        rescan = 1;
        //komodo_DEXsubscribe(cmpflag,fname,priority,0,pubkeystr,sliceid);
    }
    memset(locators,0,sizeof(locators));
    if ( rescan == 0 && komodo_DEX_locatorsload((uint64_t *)&locators[sizeof(offset0)],&prevoffset0,&numprev,locatorfname) == 0 )
    {
        if ( (oldfp= fopen(oldfname,"rb")) != 0 && rescan == 0 )
        {
            fseek(oldfp,0,SEEK_END);
            oldn = (int32_t)(ftell(oldfp) / sizeof(buf));
        }
    } else rescan = 1;
    n = (int32_t)(fsize / sizeof(buf));
    if ( n < 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"sliceid beyond end of file"));
        result.push_back(Pair((char *)"sliceid",(int64_t)sliceid));
        result.push_back(Pair((char *)"filesize",(int64_t)fsize));
        result.push_back(Pair((char *)"streamstart",(int64_t)sliceid*mult));
        fclose(fp), fp = 0;
        return(result);
    }
    //fprintf(stderr,"rescan.%d offset0.%llu vs prev %llu numprev.%d oldn.%d\n",rescan,(long long)offset0,(long long)prevoffset0,numprev,oldn);
    if ( sliceid != 0 && n > KOMODO_DEX_STREAMSIZE )
        n = KOMODO_DEX_STREAMSIZE;
    iguana_rwnum(1,&locators[0],sizeof(offset0),&offset0);
    for (volA=0; volA<=n; volA++)
    {
        if ( sliceid != 0 && volA >= KOMODO_DEX_STREAMSIZE )
            break;
        fseek(fp,volA * sizeof(buf) + offset0,SEEK_SET);
        if ( volA == n )
            rlen = (fsize - volA*sizeof(buf));
        else rlen = sizeof(buf);
        if ( rescan == 0 && volA < numprev )
        {
            iguana_rwnum(0,&locators[volA*sizeof(uint64_t) + sizeof(uint64_t)],sizeof(locator),&locator);
            //fprintf(stderr,"%d of %d: %llx\n",(int32_t)volA,numprev,(long long)locator);
            if ( locator != 0 )
            {
                numlocators++;
                if ( rlen > 0 )
                    filesize += rlen;
                continue;
            }
        }
        //fprintf(stderr,"%d of %d: rlen.%d\n",(int32_t)volA,n,rlen);
        if ( rlen > 0 )
        {
            filesize += rlen;
            if ( oldfp != 0 )
                fseek(oldfp,ftell(fp) - offset0,SEEK_SET);
            if ( fread(buf,1,rlen,fp) == rlen )
            {
                iguana_rwnum(0,&locators[volA*sizeof(uint64_t) + sizeof(uint64_t)],sizeof(locator),&locator);
                if ( locator == 0 || oldfp == 0 || fread(oldbuf,1,rlen,oldfp) != rlen || memcmp(buf,oldbuf,rlen) != 0 )
                {
                    for (i=0; i<rlen; i++)
                        sprintf(&bufstr[i<<1],"%02x",buf[i]);
                    bufstr[i<<1] = 0;
                    sprintf(volAstr,"%llu.%08llu",(long long)volA/COIN,(long long)volA % COIN);
                    komodo_DEXbroadcast(&locator,'Q',bufstr,0*KOMODO_DEX_VIPLEVEL/2,fname,(char *)"data",pubkeystr,volAstr,(char *)"");
                    //fprintf(stderr,".");
                    iguana_rwnum(1,&locators[volA*sizeof(uint64_t) + sizeof(uint64_t)],sizeof(locator),&locator);
                    changed++;
                    //fprintf(stderr,"broadcast locator.%d of %d: t.%u h.%08x %llx fraglen.%d\n",(int32_t)volA,n,(uint32_t)(locator >> 32) % KOMODO_DEX_PURGETIME,(uint32_t)locator,(long long)*(uint64_t *)&locators[volA*sizeof(uint64_t) + sizeof(uint64_t)],rlen);
                }
                else
                {
                    locator = *(uint64_t *)&locators[volA*sizeof(uint64_t) + sizeof(uint64_t)];
                    //fprintf(stderr,"recycle locator.%d of %d: m.%d %08x %llx\n",(int32_t)volA,n,(uint32_t)(locator >> 32) % KOMODO_DEX_PURGETIME,(uint32_t)locator,(long long)locator);
                }
                numlocators++;
            }
            else
            {
                result.push_back(Pair((char *)"result",(char *)"error"));
                result.push_back(Pair((char *)"error",(char *)"file read error"));
                result.push_back(Pair((char *)"filename",fname));
                fclose(fp), fp = 0;
                if ( oldfp != 0 )
                    fclose(oldfp), oldfp = 0;;
                return(result);
            }
        }
    }
    if ( sliceid == 0 )
        filehash = komodo_DEX_filehash(fp,0,fsize,fname);
    else filehash = komodo_DEX_filehash(fp,offset0,filesize,fname);
    if ( changed != 0 )
    {
        hexstr = (char *)calloc(1,65+(numlocators+1)*sizeof(uint64_t)*2+1);
        init_hexbytes_noT(hexstr,locators,(int32_t)((numlocators+1) * sizeof(uint64_t)));
        sprintf(volAstr,"%llu.%08llu",(long long)filesize/COIN,(long long)filesize % COIN);
        sprintf(volBstr,"%llu.%08llu",(long long)numlocators/COIN,(long long)numlocators % COIN);
        if ( sliceid == 0 )
        {
            komodo_DEXbroadcast(0,'Q',hexstr,priority+KOMODO_DEX_VIPLEVEL,fname,(char *)"locators",pubkeystr,volAstr,volBstr);
            bits256_str(hexstr,filehash);
            komodo_DEXbroadcast(0,'Q',hexstr,priority+KOMODO_DEX_VIPLEVEL,(char *)"files",fname,pubkeystr,volAstr,volBstr);
            //fprintf(stderr,"broadcast fname.(%s) (%s) filehash.(%s)\n",fname,hexstr,bits256_str(str,filehash));
        }
        else
        {
            sprintf(str,"%llu",(long long)offset0);
            komodo_DEXbroadcast(0,'Q',hexstr,priority+KOMODO_DEX_VIPLEVEL,fname,str,pubkeystr,volAstr,volBstr);
            sprintf(fname2,"%s.%llu",fname,(long long)offset0);
            bits256_str(hexstr,filehash);
            sprintf(volAstr,"%llu",(long long)offset0);
            komodo_DEXbroadcast(0,'Q',hexstr,priority+KOMODO_DEX_VIPLEVEL,(char *)"slices",fname,pubkeystr,volAstr,volBstr);
            //fprintf(stderr,"broadcast fname.(%s) (%s) filehash.(%s)\n",fname,hexstr,bits256_str(str,filehash));
        }
        free(hexstr);
    }
    fclose(fp), fp = 0;
    if ( oldfp != 0 )
        fclose(oldfp), oldfp = 0;
    if ( 0 && changed == 0 )
    {
        result.push_back(Pair((char *)"result",(char *)"success"));
        result.push_back(Pair((char *)"status",(char *)"no changes"));
        result.push_back(Pair((char *)"filename",fname));
        result.push_back(Pair((char *)"sliceid",(int64_t)sliceid));
        result.push_back(Pair((char *)"filesize",(int64_t)fsize));
        result.push_back(Pair((char *)"filehash",bits256_str(str,filehash)));
        return(result);
    }
    return(komodo_DEXsubscribe(cmpflag,fname,priority,0,pubkeystr,sliceid));
}

UniValue komodo_DEXstream(char *fname,int32_t priority)
{
    static char prevfname[512]; static int32_t prevsliceid;
    UniValue result(UniValue::VOBJ); FILE *fp; uint64_t mult,filesize,offset0; char pubkeystr[67],slicefname[512],tagBstr[33]; int32_t sliceid,n; struct DEX_datablob *ptr;
    if ( strcmp(prevfname,fname) != 0 )
    {
        strcpy(prevfname,fname);
        prevsliceid = 1;
    }
   if ( (fp= fopen(fname,"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        filesize = ftell(fp);
        fclose(fp);
    }
    else
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"file not found"));
        result.push_back(Pair((char *)"filename",fname));
        return(result);
    }
    mult = KOMODO_DEX_FILEBUFSIZE * KOMODO_DEX_STREAMSIZE;
    n = (int32_t)(filesize / mult);
    pubkeystr[0] = '0';
    pubkeystr[1] = '1';
    bits256_str(&pubkeystr[2],DEX_pubkey);
    for (sliceid=prevsliceid; sliceid<=n+1; sliceid++)
    {
        offset0 = ((uint64_t)sliceid - 1) * mult;
        prevsliceid = sliceid;
        sprintf(tagBstr,"%llu",(long long)offset0);
        if ( (ptr= _komodo_DEX_latestptr(fname,tagBstr,pubkeystr,0)) == 0 )
        {
            //fprintf(stderr,"sliceid.%d cant find (%s/%s) %s\n",sliceid,fname,tagBstr,pubkeystr);
            break;
        }
        sprintf(slicefname,"%s.%llu.%s",fname,(long long)offset0,pubkeystr);
        if ( (fp= fopen(slicefname,"rb")) == 0 )
            break;
        fseek(fp,0,SEEK_END);
        if ( ftell(fp) != mult )
        {
            fclose(fp);
            break;
        }
        fclose(fp);
    }
    if ( sliceid > n+1 )
        sliceid = n+1;
    if ( filesize >= offset0+mult )
        return(komodo_DEXpublish(fname,priority,sliceid));
    else
    {
        result.push_back(Pair((char *)"result",(char *)"success"));
        result.push_back(Pair((char *)"warning",(char *)"not enough data to extend stream"));
        result.push_back(Pair((char *)"filename",fname));
        result.push_back(Pair((char *)"filesize",(int64_t)filesize));
        result.push_back(Pair((char *)"offset0",(int64_t)offset0));
        result.push_back(Pair((char *)"available",(int64_t)(filesize-offset0)));
        result.push_back(Pair((char *)"needed",(int64_t)mult));
        sleep(1);
        return(result);
    }
}

FILE *komodo_DEX_streamwrite(char *destfname,FILE *fp,uint64_t wlen,uint64_t offset0)
{
    uint8_t *buf;
    buf = (uint8_t *)malloc(wlen);
    rewind(fp);
    if ( fread(buf,1,wlen,fp) != wlen )
        fprintf(stderr,"error reading %llu slice for %s\n",(long long)wlen,destfname);
    fclose(fp);
    if ( (fp= fopen(destfname,"rb+")) == 0 )
        fp = fopen(destfname,"wb");
    if ( fp != 0 )
    {
        fseek(fp,offset0,SEEK_SET);
        if ( fwrite(buf,1,wlen,fp) != wlen )
            fprintf(stderr,"error writing %llu slice to %s\n",(long long)wlen,destfname);
        fclose(fp);
        fp = 0;
    }
    free(buf);
    return(fp);
}

int md_unlink(char *file)
{
#ifdef _WIN32
    _chmod(file, 0600);
    return( _unlink(file) );
#else
    return(unlink(file));
#endif
}

UniValue komodo_DEXstreamsub(char *fname,int32_t priority,char *pubkeystr)
{
    static char prevfname[512],prevpubkeystr[67]; static int32_t prevsliceid;
    UniValue result(UniValue::VOBJ); FILE *fp=0; uint64_t mult,filesize=0,offset0; char slicefname[512],tagBstr[33]; int32_t sliceid,n,cmpflag; struct DEX_datablob *ptr;
    if ( pubkeystr == 0 || pubkeystr[0] == 0 || strlen(pubkeystr) > 66 )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"need publisher pubkey for sub"));
        return(result);
    }
    if ( strcmp(prevfname,fname) != 0 || strcmp(prevpubkeystr,pubkeystr) != 0 )
    {
        strcpy(prevfname,fname);
        strcpy(prevpubkeystr,pubkeystr);
        prevsliceid = 1;
    }
    mult = KOMODO_DEX_FILEBUFSIZE * KOMODO_DEX_STREAMSIZE;
    n = (int32_t)(filesize / mult);
    for (sliceid=prevsliceid; sliceid<1000*1000; sliceid++) // one TB limit
    {
        prevsliceid = sliceid;
        offset0 = ((uint64_t)sliceid - 1) * mult;
        sprintf(slicefname,"%s.%llu.%s",fname,(long long)offset0,pubkeystr);
        if ( (fp=fopen(slicefname,"rb")) == 0 )
            break;
        else
        {
            fseek(fp,0,SEEK_END);
            if ( (filesize= ftell(fp)) < mult )
            {
                if ( filesize > 0 )
                    fp = komodo_DEX_streamwrite(fname,fp,filesize,offset0); // eats fp
                if ( fp != 0 )
                    fclose(fp);
                fp = 0;
                break;
            }
            if ( filesize == mult )
            {
                result = komodo_DEXsubscribe(cmpflag,fname,priority,0,pubkeystr,sliceid);
                if ( cmpflag == 0 )
                {
                    fclose(fp);
                    fp = 0;
                    return(result);
                }
                else
                {
                    //fprintf(stderr,"streamwrite (%s) offset0.%llu filesize.%llu\n",fname,(long long)offset0,(long long)filesize);
                    fp = komodo_DEX_streamwrite(fname,fp,filesize,offset0); // eats fp
                    if ( sliceid >= 2 )
                    {
                        offset0 = ((uint64_t)sliceid - 2) * mult;
                        sprintf(slicefname,"%s.%llu.%s",fname,(long long)offset0,pubkeystr);
                        md_unlink(slicefname);
                        strcat(slicefname,(char *)".locators");
                        md_unlink(slicefname);
                    }
                }
            }
            if ( fp != 0 )
                fclose(fp);
        }
    }
    return(komodo_DEXsubscribe(cmpflag,fname,priority,0,pubkeystr,sliceid));
}

int32_t komodo_DEX_anonencode(uint8_t *destbuf,int32_t bufsize,char *hexstr,char *message,bits256 destpub)
{
    bits256 priv0; uint8_t *payload,*payload2,*allocated2=0,*allocated = 0; int32_t i,n,datalen=0;
    komodo_DEX_privkey(priv0);
    n = (int32_t)strlen(message) + 1;
    memcpy(destbuf,message,n);
    for (i=n; i<bufsize; i++)
        destbuf[i] = (rand() >> 17) & 0xff;
    datalen = bufsize;
    if ( (payload= komodo_DEX_encrypt(&allocated,(uint8_t *)message,&datalen,destpub,priv0)) == 0 )
    {
        fprintf(stderr,"encryption error for datalen.%d\n",datalen);
        datalen = 0;
    }
    else
    {
        if ( (payload2= komodo_DEX_encrypt(&allocated2,payload,&datalen,destpub,GENESIS_PRIVKEY)) == 0 )
        {
            fprintf(stderr,"encryption error2 for datalen.%d\n",datalen);
            datalen = 0;
        }
        else
        {
            for (i=0; i<datalen; i++)
            {
                //fprintf(stderr,"%02x",payload[i]);
                sprintf(&hexstr[i<<1],"%02x",payload2[i]);
            }
            hexstr[i<<1] = 0;
        }
        fprintf(stderr," payload2[%d] from [%d]\n",datalen,(int32_t)strlen(message));
    }
    if ( allocated != 0 )
        free(allocated), allocated = 0;
    if ( allocated2 != 0 )
        free(allocated2), allocated2 = 0;
    memset(priv0.bytes,0,sizeof(priv0));
    return(datalen);
}

UniValue komodo_DEXanonsend(char *message,int32_t priority,char *destpub33)
{
    UniValue result(UniValue::VOBJ); uint64_t locator; int32_t i,n; uint8_t destbuf[KOMODO_DEX_ANONSIZE+256]; char hexstr[(128+sizeof(destbuf))*2+1],pubkeystr[67]; bits256 destpub;
    if ( destpub33 == 0 || is_hexstr(destpub33,0) != 66 || destpub33[0] != '0' || destpub33[1] != '1' )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"need destpubkey for anonsend"));
        return(result);
    }
    else if ( strlen(message) > KOMODO_DEX_ANONSIZE )
    {
        result.push_back(Pair((char *)"result",(char *)"error"));
        result.push_back(Pair((char *)"error",(char *)"message too long for anonsend"));
        result.push_back(Pair((char *)"maxlength",(int64_t)sizeof(destbuf)-1));
        return(result);
    }
    decode_hex(destpub.bytes,32,destpub33+2);
    komodo_DEX_anonencode(destbuf,KOMODO_DEX_ANONSIZE,hexstr,message,destpub);
    pubkeystr[0] = '0';
    pubkeystr[1] = '1';
    bits256_str(pubkeystr+2,GENESIS_PUBKEY);
    result = komodo_DEXbroadcast(&locator,'A',hexstr,priority + KOMODO_DEX_CMDPRIORITY,(char *)"anon",(char *)"",pubkeystr,(char *)"",(char *)"");
    return(result);
}

UniValue komodo_DEX_notarize(char *coin,int32_t prevheight)
{
    UniValue result(UniValue::VOBJ); uint8_t *decoded,*buf,*allocated=0,data[512]; int32_t height,n,matches,ntzheight,newlen=0,ind=3,lag; uint32_t t,t2; char pubkeystr[67],str[65],tagB[16]; uint32_t ntztime; int8_t lenA; struct DEX_index *tips[KOMODO_DEX_MAXINDICES]; struct DEX_datablob *ptr; bits256 senderpub,ntzhash,blkhash,hash;
    t = (uint32_t)time(NULL);
    lenA = (int8_t)strlen(coin);
    pubkeystr[0] = '0';
    pubkeystr[1] = '1';
    bits256_str(pubkeystr+2,DEX_pubkey);
    {
        pthread_mutex_lock(&DEX_globalmutex);
        if ( (ptr= _komodo_DEX_latestptr(coin,(char *)"notarizations",pubkeystr,0)) != 0 )
        {
            if ( (decoded= komodo_DEX_datablobdecrypt(&senderpub,&allocated,&newlen,ptr,DEX_pubkey,coin)) != 0 && newlen == 40 )
            {
                memcpy(ntzhash.bytes,decoded,32);
                buf = &decoded[32];
                ntzheight = ((int32_t)buf[3] + ((int32_t)buf[2] << 8) + ((int32_t)buf[1] << 16) + ((int32_t)buf[0] << 24));
                buf += sizeof(ntzheight);
                ntztime = ((int32_t)buf[3] + ((int32_t)buf[2] << 8) + ((int32_t)buf[1] << 16) + ((int32_t)buf[0] << 24));
                fprintf(stderr,"last notarization %s.%d (%d) %s t.%u\n",coin,ntzheight,prevheight,bits256_str(str,ntzhash),ntztime);
                for (height=ntzheight+1; height<ntzheight+1440; height++)
                {
                    n = matches = 0;
                    memset(tips,0,sizeof(tips));
                    sprintf(tagB,"%d",height);
                    if ( (_DEX_updatetips(tips,0,0,lenA,(uint8_t *)coin,(int8_t)strlen(tagB),(uint8_t *)tagB,0,0) >> 16) == (1 << ind) )
                        fprintf(stderr,"error getting tips for ht.%d ntzheight.%d\n",height,ntzheight);
                    else if ( tips[ind] != 0 )
                    {
                        memset(blkhash.bytes,0,sizeof(blkhash));
                        for (ptr=tips[ind]->tail; ptr!=0; ptr=ptr->prevs[ind])
                        {
                            if ( ptr->cancelled == 0 )
                            {
                                iguana_rwnum(0,&ptr->data[2],sizeof(t2),&t2);
                                lag = (t - t2);
                                if ( _komodo_DEX_decryptdata(data,sizeof(data),ptr) == sizeof(hash) )
                                {
                                    memcpy(hash.bytes,data,sizeof(hash));
                                    if ( n == 0 )
                                    {
                                        blkhash = hash;
                                        matches = 1;
                                    }
                                    else if ( memcmp(blkhash.bytes,hash.bytes,sizeof(blkhash)) == 0 )
                                        matches++;
                                    n++;
                                }
                            }
                            if ( ptr == tips[ind]->head )
                                break;
                        }
                    }
                    if ( n == 0 )
                        break;
                    fprintf(stderr,"%2d: %s ht.%d ntzht.%d matches.%-2d of %2d blockhash %s lag.%d\n",height-ntzheight,coin,height,ntzheight,matches,n,bits256_str(str,blkhash),lag);
                }
            }
            if ( allocated != 0 )
                free(allocated), allocated = 0;
        }
        //fprintf(stderr,"fname.%s auto search %s %s %s shorthash.%08x sliceid.%d\n",fname,origfname,tagBstr,publisher,shorthash,sliceid);
         pthread_mutex_unlock(&DEX_globalmutex);
    }
    return(result);
}

void komodo_DEXmsg(CNode *pfrom,std::vector<uint8_t> request) // received a packet during interrupt time
{
    int32_t len; std::vector<uint8_t> response; bits256 hash; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= request.size()) > 0 )
    {
        pthread_mutex_lock(&DEX_globalmutex);
        _komodo_DEXprocess(timestamp,pfrom,&request[0],len);
        pthread_mutex_unlock(&DEX_globalmutex);
    }
}

void komodo_DEXpoll(CNode *pto) // from mainloop polling
{
    static uint32_t purgetime;
    std::vector<uint8_t> packet; uint32_t i,now,numiters,shorthash,len,ptime,modval,peerpos;
    now = (uint32_t)time(NULL);
    ptime = now - KOMODO_DEX_PURGETIME + 6;
    pthread_mutex_lock(&DEX_globalmutex);
    peerpos = _komodo_DEXpeerpos(now,pto->id);
    if ( ptime > purgetime )
    {
        if ( purgetime == 0 )
            purgetime = ptime;
        else
        {
            for (; purgetime<ptime; purgetime++)
                _komodo_DEXpurge(purgetime);
            _komodo_DEX_purgeindices(ptime - 3); // call once at the end
        }
        DEX_Numpending *= 0.999; // decay pending to compensate for hashcollision remnants
    }
    if ( (now == Got_Recent_Quote && now > pto->dexlastping) || now >= pto->dexlastping+KOMODO_DEX_LOCALHEARTBEAT )
    {
        if ( ((now + peerpos) % KOMODO_DEX_POLLVIP) == 0 ) // check the VIP packets
        {
            numiters = KOMODO_DEX_PURGETIME - KOMODO_DEX_MAXLAG;
            pto->dexlastping = now;
        } else numiters = KOMODO_DEX_MAXLAG - KOMODO_DEX_MAXHOPS;
        for (i=0; i<numiters; i++)
        {
            modval = (now + 1 - i) % KOMODO_DEX_PURGETIME;
            if ( _komodo_DEXmodval(now,modval,pto) > 0 )
                pto->dexlastping = now;
            if ( komodo_DEX_islagging() != 0 && i > KOMODO_DEX_MAXLAG )
                break;
        }
        pto->dexlastping = now;
    }
    pthread_mutex_unlock(&DEX_globalmutex);
}

