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
 message format: <relay depth> <funcid> <timestamp> <payload>
 
 <payload> is the datablob for a 'Q' quote or <uint16_t> + n * <uint32_t> for a 'P' ping of recent shorthashes
 
 To achieve very fast performance a hybrid push/poll/pull gossip protocol is used. All new quotes are broadcast KOMODO_DEX_RELAYDEPTH levels deep, this gets the quote to large percentage of nodes assuming <ave peers> ^ KOMODO_DEX_RELAYDEPTH power is approx totalnodes/2. Nodes in the broacast "cone" will receive a new quote in less than a second in most cases.
 
 For efficiency it is better to aim for one third to half of the nodes during the push phase. The higher the KOMODO_DEX_RELAYDEPTH the faster a quote will be broadcast, but the more redundant packets will be sent. If each node would be able to have a full network connectivity map, each node could locally simulate packet propagation and select a subset of peers with the maximum propagation and minimum overlap. However, such an optimization requires up to date and complete network topography and the current method has the advantage of being much simpler and reasonably efficient.
 
 Additionally, all nodes will be broadcasting to their immediate peers, the most recent quotes not known to be known by the destination, which will allow the receiving peer to find any quotes they are missing and request it directly. At the cost of the local broadcasting, all the nodes will be able to directly request any quote they didnt get during the push phase.
 
 For sparsely connected nodes, as the pull process propagates a new quote, they will eventually also see the new quote. Worst case would be the last node in a singly connected chain of peers. Assuming most all nodes will have 3 or more peers, then most all nodes will get a quote broadcast in a few multiples of KOMODO_DEX_LOCALHEARTBEAT
 
 Purgelist[] is needed since there are some pointers still referenced withing the PURGETIME hashtables. the reason is likely due to each timeslot being randomly ordered, so when more than 1 tx are in the same tag during the same second, they end up in a random order. This means the next second is needed before it can be seen what pointers are not referenced anymore. however, scanning for pointer references is slow. therefore the purgelist is made where it simply frees any packet older than the PURGETIME. it could be that a queue would allow for more efficient maintaining of the purgelist, but if there are not so many packets, there is plenty of CPU time and if there are a lot of packets, the purgelist will, be mostly full.
 
 todo:
 implement prioritized routing! both for send and get
 track recent lag, adaptive send/get
 fix sizepriority doubling (if real)
 
 speedup message indices and make it limited by RAM
 get and orderbook rpc call

 later:
 defend against memory overflow
 improve privacy via secretpubkeys, automatic key exchange, get close to bitmessage level privacy in realtime
 parameterize network #defines heartbeat, maxhops, maxlag, relaydepth, peermasksize, hashlog2!, purgetime!
 detect evil peer: 'Q' is directly protected by txpow, G is a fixed size, so it cant be very big and invalid request can be detected. 'P' message will lead to 'G' queries that cannot be answered
 */

uint8_t *komodo_DEX_encrypt(uint8_t **allocatedp,uint8_t *data,int32_t *datalenp,bits256 destpubkey,bits256 privkey);
uint8_t *komodo_DEX_decrypt(uint8_t **allocatedp,uint8_t *data,int32_t *datalenp,bits256 privkey);
void komodo_DEX_pubkey(bits256 &pub0);
void komodo_DEX_privkey(bits256 &priv0);

#define KOMODO_DEX_ROUTESIZE 6 // (relaydepth + funcid + timestamp)

#define KOMODO_DEX_LOCALHEARTBEAT 1
#define KOMODO_DEX_MAXHOPS 10 // most distant node pair after push phase
#define KOMODO_DEX_MAXLAG (60 + KOMODO_DEX_LOCALHEARTBEAT*KOMODO_DEX_MAXHOPS)
#define KOMODO_DEX_RELAYDEPTH ((uint8_t)KOMODO_DEX_MAXHOPS) // increase as <avepeers> root of network size increases
#define KOMODO_DEX_MAXFANOUT ((uint8_t)3)

#define KOMODO_DEX_HASHLOG2 14
#define KOMODO_DEX_HASHSIZE (1 << KOMODO_DEX_HASHLOG2) // effective limit of sustained datablobs/sec
#define KOMODO_DEX_HASHMASK (KOMODO_DEX_HASHSIZE - 1)
#define KOMODO_DEX_PURGETIME 300

#define KOMOD_DEX_PEERMASKSIZE 128
#define KOMODO_DEX_MAXPEERID (KOMOD_DEX_PEERMASKSIZE * 8)
#define SECONDS_IN_DAY (24 * 3600)
#define KOMODO_DEX_PEERPERIOD 300 // must be evenly divisible into SECONDS_IN_DAY
#define KOMODO_DEX_PEEREPOCHS (SECONDS_IN_DAY / KOMODO_DEX_PEERPERIOD)

#define KOMODO_DEX_TAGSIZE 16   // (33 / 2) rounded down
#define KOMODO_DEX_MAXKEYSIZE 34 // destpub 1+33, or tagAB 1+16 + 1 + 16 -> both are 34
#define KOMODO_DEX_MAXINDEX 64
#define KOMODO_DEX_MAXINDICES 4 // [0] destpub, [1] tagA, [2] tagB, [3] two tags order dependent

#define KOMODO_DEX_MAXPACKETSIZE (1 << 20)
#define KOMODO_DEX_MAXPRIORITY 20 // a millionX should be enough, but can be as high as 64 - KOMODO_DEX_TXPOWBITS
#define KOMODO_DEX_TXPOWBITS 1    // should be 17 for approx 1 sec per tx
#define KOMODO_DEX_TXPOWDIVBITS 10 // each doubling of size of datalen, increases minpriority
#define KOMODO_DEX_TXPOWMASK ((1LL << KOMODO_DEX_TXPOWBITS)-1)
//#define KOMODO_DEX_CREATEINDEX_MINPRIORITY 6 // 64x baseline diff -> approx 1 minute if baseline is 1 second diff

struct DEX_datablob
{
    bits256 hash;
    struct DEX_datablob *prevs[KOMODO_DEX_MAXINDICES],*nexts[KOMODO_DEX_MAXINDICES];
    uint8_t peermask[KOMOD_DEX_PEERMASKSIZE];
    uint32_t recvtime,datalen;
    int8_t priority,sizepriority;
    uint8_t numsent,offset;
    uint8_t data[];
};

struct DEX_index
{
    struct DEX_datablob *tip;
    int32_t count;
    uint8_t keylen;
    uint8_t key[KOMODO_DEX_MAXKEYSIZE];
} DEX_tagABs[KOMODO_DEX_MAXINDEX],DEX_tagAs[KOMODO_DEX_MAXINDEX],DEX_tagBs[KOMODO_DEX_MAXINDEX],DEX_destpubs[KOMODO_DEX_MAXINDEX];


// start perf metrics
static double DEX_lag,DEX_lag2,DEX_lag3;
static uint32_t Got_Recent_Quote,DEX_totalsent,DEX_totalrecv,DEX_totaladd,DEX_duplicate;
static uint32_t DEX_lookup32,DEX_collision32,DEX_add32,DEX_maxlag;
static int32_t DEX_Numpending,DEX_freed,DEX_truncated;
// end perf metrics

static int32_t DEX_peermaps[KOMODO_DEX_PEEREPOCHS][KOMODO_DEX_MAXPEERID];
static uint32_t Pendings[KOMODO_DEX_MAXLAG * KOMODO_DEX_HASHSIZE - 1];

static uint32_t Hashtables[KOMODO_DEX_PURGETIME][KOMODO_DEX_HASHSIZE]; // bound with Datablobs
static struct DEX_datablob *Datablobs[KOMODO_DEX_PURGETIME][KOMODO_DEX_HASHSIZE]; // bound with Hashtables
static struct DEX_datablob *Purgelist[KOMODO_DEX_HASHSIZE * 4]; // purge functions depend on this being 4
bits256 DEX_pubkey;
pthread_mutex_t DEX_mutex;

void komodo_DEX_init()
{
    static int32_t onetime;
    if ( onetime == 0 )
    {
        pthread_mutex_init(&DEX_mutex,0);
        komodo_DEX_pubkey(DEX_pubkey);
        char str[67]; fprintf(stderr,"DEX_pubkey.(01%s)\n\n",bits256_str(str,DEX_pubkey));
        onetime = 1;
    }
}

int32_t komodo_DEX_islagging()
{
    if ( DEX_lag > DEX_lag2 && DEX_lag2 > DEX_lag3 && DEX_lag > KOMODO_DEX_MAXLAG/KOMODO_DEX_MAXHOPS )
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

int32_t komodo_DEX_priority(uint64_t h,int32_t packetsize)
{
    int32_t i,sizepriority = komodo_DEX_sizepriority(packetsize);
    h >>= KOMODO_DEX_TXPOWBITS;
    for (i=0; i<64; i++,h>>=1)
        if ( (h & 1) != 0 )
            return(i - sizepriority);
    return(i - sizepriority);
}

uint32_t komodo_DEXquotehash(bits256 &hash,uint8_t *msg,int32_t len)
{
    vcalc_sha256(0,hash.bytes,&msg[1],len-1);
    return(hash.uints[0]);
}

uint16_t komodo_DEXpeerpos(uint32_t timestamp,int32_t peerid)
{
    int32_t epoch,*peermap; uint16_t i;
    epoch = ((timestamp % SECONDS_IN_DAY) / KOMODO_DEX_PEERPERIOD);
    peermap = DEX_peermaps[epoch];
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
    }
    return(-1);
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

int32_t DEX_unlinkindices(struct DEX_datablob *ptr)
{
    int32_t j,ind,n=0; struct DEX_index *index = 0; struct DEX_datablob *prev,*next;
    for (ind=0; ind<KOMODO_DEX_MAXINDICES; ind++)
    {
        prev = ptr->prevs[ind];
        if ( (next= ptr->nexts[ind]) != 0 )
        {
            if ( next->prevs[ind] != ptr && next->prevs[ind] != 0 )
            {
                fprintf(stderr,"warning unlink error next->prev %p != %p\n",next->prevs[ind],ptr);
            }
            next->prevs[ind] = prev;
            ptr->nexts[ind] = 0;
            n++;
            //fprintf(stderr,"unlinked %p from ind.%d n.%d\n",ptr,ind,n);
        }
        switch ( ind )
        {
            case 0: index = DEX_destpubs; break;
            case 1: index = DEX_tagAs; break;
            case 2: index = DEX_tagBs; break;
            case 3: index = DEX_tagABs; break;
            default: fprintf(stderr,"should be impossible\n"); return(-1);
        }
        for (j=0; j<KOMODO_DEX_MAXINDEX; j++) // faster than looking up key!
        {
            if ( index[j].tip == ptr )
            {
                if ( (index[j].tip= next) == 0 )
                    index[j].keylen = 0;
                fprintf(stderr,"delink index.%d tip for ptr.%p, count.%d\n",ind,ptr,index[j].count);
                n++;
                break;
            }
        }
        ptr->prevs[ind] = 0;
    }
    return(n);
}

struct DEX_index *komodo_DEX_indexappend(int32_t ind,struct DEX_index *index,struct DEX_datablob *ptr)
{
    struct DEX_datablob *tip;
    if ( (tip= index->tip) == 0 )
    {
        fprintf(stderr,"DEX_indexappend unexpected null tip\n");
        return(0);
    }
    tip->nexts[ind] = ptr;
    ptr->prevs[ind] = tip;
    index->tip = ptr;
    index->count++;
    //char str[2*KOMODO_DEX_MAXKEYSIZE+1]; fprintf(stderr,"ind.%d %p append key (%s) count.%d\n",ind,index,komodo_DEX_keystr(str,index->key,index->keylen),index->count);
    return(index);
}

struct DEX_index *komodo_DEX_indexcreate(int32_t ind,struct DEX_index *index,uint8_t *key,int8_t keylen,struct DEX_datablob *ptr)
{
    if ( index->tip != 0 )
    {
        fprintf(stderr,"DEX_indexcreate unexpected tip.%p\n",(void *)index->tip);
        return(0);
    }
    memset(index->key,0,sizeof(index->key));
    memcpy(index->key,key,keylen);
    if ( 0 )
    {
        int32_t i;
        for (i=0; i<keylen; i++)
            fprintf(stderr,"%02x",key[i]);
        char str[111]; fprintf(stderr," ind.%d %p index create (%s) len.%d\n",ind,index,komodo_DEX_keystr(str,key,keylen),keylen);
    }
    index->keylen = keylen;
    index->tip = ptr;
    index->count = 1;
    return(index);
}

struct DEX_index *DEX_indexsearch(int32_t ind,int32_t priority,struct DEX_datablob *ptr,int8_t lenA,uint8_t *key,int8_t lenB,uint8_t *tagB)
{
    uint8_t keybuf[KOMODO_DEX_MAXKEYSIZE]; int32_t i,keylen = 0; struct DEX_index *index = 0;
    if ( lenA == 33 )
    {
        keylen = 34;
        keybuf[0] = 33;
        memcpy(&keybuf[1],key,33);
        index = DEX_destpubs;
    }
    else if ( lenB == 0 )
    {
        keylen = lenA+1;
        keybuf[0] = lenA;
        memcpy(&keybuf[1],key,lenA);
        index = DEX_tagAs;
    }
    else if ( lenA == 0 )
    {
        keylen = lenB+1;
        keybuf[0] = lenB;
        memcpy(&keybuf[1],tagB,lenB);
        index = DEX_tagBs;
    }
    else if ( lenA > 0 && lenB > 0 && tagB != 0 && lenA <= KOMODO_DEX_TAGSIZE && lenB <= KOMODO_DEX_TAGSIZE )
    {
        keybuf[keylen++] = lenA;
        memcpy(&keybuf[keylen],key,lenA), keylen += lenA;
        keybuf[keylen++] = lenB;
        memcpy(&keybuf[keylen],tagB,lenB), keylen += lenB;
        index = DEX_tagABs;
    }
    for (i=0; i<KOMODO_DEX_MAXINDEX; i++)
    {
        if ( index[i].tip == 0 )
            break;
        if ( index[i].keylen == keylen && memcmp(index[i].key,keybuf,keylen) == 0 )
        {
            if ( ptr != 0 )
                return(komodo_DEX_indexappend(ind,&index[i],ptr));
            else
            {
                //char str[111]; fprintf(stderr," ind.%d index matched (%s) len.%d\n",ind,komodo_DEX_keystr(str,index[i].key,index[i].keylen),index[i].keylen);
                return(&index[i]);
            }
        }
    }
    if ( ptr == 0 )
        return(0);
    /*if ( priority < KOMODO_DEX_CREATEINDEX_MINPRIORITY ) the category creation needs to be in OP_RETURN, could use the same format data packet
    {
        fprintf(stderr,"new index lenA.%d lenB.%d, insufficient priority.%d < minpriority.%d to create\n",lenA,lenB,priority,KOMODO_DEX_CREATEINDEX_MINPRIORITY);
        return(0);
    }
    else*/ if ( i == KOMODO_DEX_MAXINDEX )
    {
        fprintf(stderr,"new index lenA.%d lenB.%d, max number of indeices.%d created already\n",lenA,lenB,KOMODO_DEX_MAXINDEX);
        return(0);
    }
    return(komodo_DEX_indexcreate(ind,&index[i],keybuf,keylen,ptr));
}

int32_t DEX_updatetips(struct DEX_index *tips[KOMODO_DEX_MAXINDICES],int32_t priority,struct DEX_datablob *ptr,int8_t lenA,uint8_t *tagA,int8_t lenB,uint8_t *tagB,uint8_t *destpub,int8_t plen)
{
    int32_t ind = 0,mask = 0;
    memset(tips,0,sizeof(*tips) * KOMODO_DEX_MAXINDICES);
    if ( lenA == 0 && lenB == 0 && plen == 0 )
        return(0);
    pthread_mutex_lock(&DEX_mutex);
    if ( plen != 0 )
    {
        if ( (tips[ind]= DEX_indexsearch(ind,priority,ptr,plen,destpub,0,0)) == 0 )
            mask |= (1 << (ind+16));
        else mask |= (1 << ind);
    }
    ind++;
    if ( lenA != 0 )
    {
        if ( (tips[ind]= DEX_indexsearch(ind,priority,ptr,lenA,tagA,0,0)) == 0 )
            mask |= (1 << (ind+16));
        else mask |= (1 << ind);
        ind++;
        if ( lenB != 0 )
        {
            if ( (tips[ind]= DEX_indexsearch(ind,priority,ptr,0,0,lenB,tagB)) == 0 )
                mask |= (1 << (ind+16));
            else mask |= (1 << ind);
            ind++;
            if ( (tips[ind]= DEX_indexsearch(ind,priority,ptr,lenA,tagA,lenB,tagB)) == 0 )
                mask |= (1 << (ind+16));
            else mask |= (1 << ind);
        }
    }
    else if ( lenB != 0 )
    {
        ind++;
        if ( (tips[ind]= DEX_indexsearch(ind,priority,ptr,0,0,lenB,tagB)) == 0 ) // must have same ind as lenA case above!
            mask |= (1 << (ind+16));
        else mask |= (1 << ind);
    }
    pthread_mutex_unlock(&DEX_mutex);
    if ( ind >= KOMODO_DEX_MAXINDICES )
    {
        fprintf(stderr,"DEX_updatetips: impossible case ind.%d > KOMODO_DEX_MAXINDICES %d\n",ind,KOMODO_DEX_MAXINDICES);
        exit(1);
    }
    //fprintf(stderr,"tips updated %x ptr.%p numrefs.%d\n",mask,ptr,komodo_DEX_refsearch(ptr));
    return(mask); // err bits are <<= 16
}

struct DEX_datablob *komodo_DEXfind(int32_t &openind,int32_t modval,uint32_t shorthash)
{
    uint32_t hashval; int32_t i,hashind;
    hashind = (shorthash & KOMODO_DEX_HASHMASK);
    openind = -1;
    for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
    {
        if ( (hashval= Hashtables[modval][hashind]) == 0 )
        {
            openind = hashind;
            return(0);
        }
        else if ( hashval == shorthash )
            return(Datablobs[modval][hashind]);
        if ( ++hashind >= KOMODO_DEX_HASHSIZE )
            hashind = 0;
    }
    fprintf(stderr,"hashtable full\n");
    return(0);
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
            //fprintf(stderr,"destpub ");
            //flag++;
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
            //fprintf(stderr,"tagA (%s) ",tagA);
            //flag++;
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
            //fprintf(stderr,"tagB (%s) ",tagB);
            //flag++;
        }
        else
        {
            fprintf(stderr,"reject invalid lenB.%d\n",lenB);
            return(-1);
        }
    }
    if ( amountA != 0 || amountB != 0 )
    {
        //fprintf(stderr,"amountA %.8f amountB %.8f\n",dstr(amountA),dstr(amountB));
        //flag++;
    }
    //if ( flag != 0 )
    //    fprintf(stderr,"\n");
    return(offset);
}

int32_t komodo_DEX_tagsextract(char taga[],char tagb[],char destpubstr[],uint8_t destpub33[33],uint8_t *msg,int32_t len) // inefficient func for convenience
{
    uint64_t amountA,amountB; uint8_t tagA[KOMODO_DEX_TAGSIZE+1],tagB[KOMODO_DEX_TAGSIZE+1]; int8_t lenA,lenB,plen; int32_t i,offset;
    taga[0] = tagb[0] = destpubstr[0] = 0;
    memset(tagA,0,sizeof(tagA));
    memset(tagB,0,sizeof(tagB));
    if ( (offset= komodo_DEX_extract(amountA,amountB,lenA,tagA,lenB,tagB,destpub33,plen,msg,len)) < 0 )
        return(-1);
    if ( lenA != 0 )
        strcpy(taga,(char *)tagA);
    if ( lenB != 0 )
        strcpy(tagb,(char *)tagB);
    if ( plen == 33 )
    {
        for (i=0; i<33; i++)
            sprintf(&destpubstr[i<<1],"%02x",destpub33[i]);
        destpubstr[i<<1] = 0;
    }
    return(0);
}

struct DEX_datablob *komodo_DEXadd(int32_t openind,uint32_t now,int32_t modval,bits256 hash,uint32_t shorthash,uint8_t *msg,int32_t len)
{
    int32_t ind,offset,priority; struct DEX_datablob *ptr; struct DEX_index *tips[KOMODO_DEX_MAXINDICES]; uint64_t amountA,amountB; uint8_t tagA[KOMODO_DEX_TAGSIZE+1],tagB[KOMODO_DEX_TAGSIZE+1],destpub33[33]; int8_t lenA,lenB,plen;
    if ( (hash.ulongs[1] & KOMODO_DEX_TXPOWMASK) != (0x777 & KOMODO_DEX_TXPOWMASK) )
    {
        static uint32_t count; char str[65];
        if ( count++ < 10 )
            fprintf(stderr," reject quote due to invalid hash[1] %016llx %s\n",(long long)hash.ulongs[1],bits256_str(str,hash));
        return(0);
    } else priority = komodo_DEX_priority(hash.ulongs[1],len);
    if ( openind < 0 || openind >= KOMODO_DEX_HASHSIZE )
    {
        if ( (ptr= komodo_DEXfind(openind,modval,shorthash)) != 0 )
        {
            if ( shorthash != Hashtables[modval][ind] )
                fprintf(stderr,"%08x %08x collision in hashtable[%d] at %d, openind.%d\n",shorthash,Hashtables[modval][ind],modval,ind,openind);
            return(ptr);
        }
    }
    if ( openind < 0 || Hashtables[modval][openind] != 0 || Datablobs[modval][openind] != 0 )
    {
        fprintf(stderr,"DEXadd openind.%d invalid or non-empty spot %08x %p\n",openind,openind >=0 ? Hashtables[modval][openind] : 0,openind >= 0 ? Datablobs[modval][ind] : 0);
        return(0);
    } else ind = openind;
    memset(tagA,0,sizeof(tagA));
    memset(tagB,0,sizeof(tagB));
    if ( (offset= komodo_DEX_extract(amountA,amountB,lenA,tagA,lenB,tagB,destpub33,plen,&msg[KOMODO_DEX_ROUTESIZE],len-KOMODO_DEX_ROUTESIZE)) < 0 )
        return(0);
    if ( (ptr= (struct DEX_datablob *)calloc(1,sizeof(*ptr) + len)) != 0 )
    {
        ptr->recvtime = now;
        ptr->hash = hash;
        ptr->datalen = len;
        ptr->priority = priority;
        ptr->sizepriority = komodo_DEX_sizepriority(len);
        ptr->offset = offset + KOMODO_DEX_ROUTESIZE; // payload is after relaydepth, funcid, timestamp
        memcpy(ptr->data,msg,len);
        ptr->data[0] = msg[0] != 0xff ? msg[0] - 1 : msg[0];
        Datablobs[modval][ind] = ptr;
        Hashtables[modval][ind] = shorthash;
        DEX_totaladd++;
        if ( (DEX_updatetips(tips,priority,ptr,lenA,tagA,lenB,tagB,destpub33,plen) >> 16) != 0 )
            fprintf(stderr,"update M.%d slot.%d [%d] with %08x error updating tips\n",modval,ind,ptr->data[0],Hashtables[modval][ind]);
        return(ptr);
    }
    fprintf(stderr,"out of memory\n");
    return(0);
}

uint32_t komodo_DEXtotal(int32_t *histo,int32_t &total)
{
    struct DEX_datablob *ptr; int32_t priority; uint32_t i,j,n,hash,totalhash = 0;
    total = 0;
    for (j=0; j<KOMODO_DEX_PURGETIME; j++)
    {
        hash = n = 0;
        for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
        {
            if ( Hashtables[j][i] != 0 )
            {
                n++;
                hash ^= Hashtables[j][i];
                if ( (ptr= Datablobs[j][i]) != 0 )
                {
                    if ( (priority= ptr->priority) > 13 )
                        priority = 13;
                    histo[priority]++;
                }
            }
        }
        totalhash ^= hash;
        total += n;
    }
    return(totalhash);
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

int32_t komodo_DEXgenping(std::vector<uint8_t> &ping,uint32_t timestamp,int32_t modval,uint32_t *recents,uint16_t n)
{
    int32_t i,len = 0;
    if ( modval < 0 || modval >= KOMODO_DEX_PURGETIME )
    {
        fprintf(stderr,"komodo_DEXgenping illegal modval.%d\n",modval);
        return(-1);
    }
    ping.resize(2 + sizeof(timestamp) + sizeof(n) + sizeof(modval) + n*sizeof(uint32_t));
    ping[len++] = 0;
    ping[len++] = 'P';
    len += iguana_rwnum(1,&ping[len],sizeof(timestamp),&timestamp);
    len += iguana_rwnum(1,&ping[len],sizeof(n),&n);
    len += iguana_rwnum(1,&ping[len],sizeof(modval),&modval);
    for (i=0; i<n; i++)
        len += iguana_rwnum(1,&ping[len],sizeof(recents[i]),&recents[i]);
    return(len);
}

int32_t komodo_DEXgenquote(int32_t priority,bits256 &hash,uint32_t &shorthash,std::vector<uint8_t> &quote,uint32_t timestamp,uint8_t hdr[],int32_t hdrlen,uint8_t data[],int32_t datalen)
{
    int32_t i,j,len = 0; uint64_t h; uint32_t nonce = rand();
    quote.resize(2 + sizeof(uint32_t) + hdrlen + datalen + sizeof(nonce)); // send list of recently added shorthashes
    quote[len++] = KOMODO_DEX_RELAYDEPTH;
    quote[len++] = 'Q';
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
        iguana_rwnum(1,&quote[len - sizeof(nonce)],sizeof(nonce),&nonce);
        shorthash = komodo_DEXquotehash(hash,&quote[0],len);
        h = hash.ulongs[1];
        if ( (h & KOMODO_DEX_TXPOWMASK) == (0x777 & KOMODO_DEX_TXPOWMASK) )
        {
            h >>= KOMODO_DEX_TXPOWBITS;
            for (j=0; j<priority; j++,h>>=1)
                if ( (h & 1) != 0 )
                    break;
            if ( j < priority )
            {
                //fprintf(stderr,"i.%u j.%d failed priority.%d uints[1] %016llx\n",i,j,priority,(long long)hash.ulongs[1]);
                continue;
            }
            if ( i > 100000 )
                fprintf(stderr,"i.%u j.%d priority.%d uints[1] %016llx\n",i,j,priority,(long long)hash.ulongs[1]);
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
    std::vector<uint8_t> packet;
    SETBIT(ptr->peermask,peerpos); // pretty sure this will get there -> mark present
    packet.resize(ptr->datalen);
    memcpy(&packet[0],ptr->data,ptr->datalen);
    packet[0] = resp0;
    peer->PushMessage("DEX",packet);
    DEX_totalsent++;
    return(ptr->datalen);
}

int32_t komodo_DEXmodval(uint32_t now,const int32_t modval,CNode *peer)
{
    static uint32_t recents[KOMODO_DEX_HASHSIZE];
    std::vector<uint8_t> packet; int32_t i,j; uint16_t peerpos,n = 0; uint8_t relay,funcid,*msg; uint32_t t; struct DEX_datablob *ptr;
    if ( modval < 0 || modval >= KOMODO_DEX_PURGETIME || (peerpos= komodo_DEXpeerpos(now,peer->id)) == 0xffff )
        return(-1);
    for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
    {
        if ( Hashtables[modval][i] != 0 && (ptr= Datablobs[modval][i]) != 0 )
        {
            msg = &ptr->data[0];
            relay = msg[0];
            funcid = msg[1];
            iguana_rwnum(0,&msg[2],sizeof(t),&t);
            if ( now < t+KOMODO_DEX_MAXLAG )
            {
                if ( GETBIT(ptr->peermask,peerpos) == 0 )
                {
                    // add priorities, track max priority
                    recents[n++] = Hashtables[modval][i];
                    if ( ptr->numsent < KOMODO_DEX_MAXFANOUT && DEX_Numpending < KOMODO_DEX_HASHSIZE/8 )
                    {
                        if ( relay >= 0 && relay <= KOMODO_DEX_RELAYDEPTH && now < t+KOMODO_DEX_LOCALHEARTBEAT )
                        {
                            // sort by priority, across all peers?
                            komodo_DEXpacketsend(peer,peerpos,ptr,ptr->data[0]);
                            ptr->numsent++;
                        }
                    }
                }
            }
        }
    }
    if ( n > 0 )
    {
        if ( komodo_DEXgenping(packet,now,modval,recents,n) > 0 ) // send only max priority
            peer->PushMessage("DEX",packet);
    }
    return(n);
}

// due to timing issues and no locks operations, the linked lists might still refer to ptr, so it is freed in a 2 step process.

int32_t komodo_DEX_purgelist(struct DEX_datablob *refptr)
{
    int32_t i,ind,n=0; uint32_t now,t; struct DEX_datablob *ptr,*prev,*next;
    now = (uint32_t)time(NULL);
    for (i=0; i<(int32_t)(sizeof(Purgelist)/sizeof(*Purgelist)); i++)
    {
        if ( (ptr= Purgelist[i]) != 0 )
        {
            iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
            if ( now > t+KOMODO_DEX_PURGETIME )
            {
                //fprintf(stderr,"modval %d i.%d t.%u lag.%d -> purge it\n",modval,i,t,now-t);
                Purgelist[i] = 0;
                DEX_freed++;
                memset(ptr,0,sizeof(*ptr));
                free(ptr);
            }
        }
    }
    return(n);
}

int32_t komodo_DEXpurge(uint32_t cutoff)
{
    static uint32_t prevtotalhash,lastadd,lastcutoff;
    int32_t i,n=0,modval,total,offset; int64_t lagsum = 0; uint8_t relay,funcid,*msg; uint32_t t,hash,totalhash,purgehash=0; struct DEX_datablob *ptr;
    if ( (cutoff % SECONDS_IN_DAY) == (SECONDS_IN_DAY-1) )
    {
        fprintf(stderr,"reset peermaps at end of day!\n");
        memset(DEX_peermaps,0,sizeof(DEX_peermaps));
    }
    modval = (cutoff % KOMODO_DEX_PURGETIME);
    pthread_mutex_lock(&DEX_mutex);
    for (i=0; i<KOMODO_DEX_HASHSIZE; i++)
    {
        if ( (hash= Hashtables[modval][i]) != 0 )
        {
            if ( (ptr= Datablobs[modval][i]) != 0 )
            {
                msg = &ptr->data[0];
                relay = msg[0];
                funcid = msg[1];
                iguana_rwnum(0,&msg[2],sizeof(t),&t);
                if ( t != cutoff )
                    fprintf(stderr,"modval.%d unexpected purge.%d t.%u vs cutoff.%u\n",modval,i,t,cutoff);
                else
                {
                    if ( DEX_unlinkindices(ptr) < 0 )
                        fprintf(stderr,"error unlinking ptr\n");
                    if ( ptr->recvtime < t )
                        fprintf(stderr,"timewarped recvtime lag.%d\n",ptr->recvtime - t);
                    else lagsum += (ptr->recvtime - t);
                    purgehash ^= hash;
                    Hashtables[modval][i] = 0;
                    Datablobs[modval][i] = 0;
                    ptr->datalen = 0;
                    if ( 0 && realloc(ptr,sizeof(*ptr)) != ptr ) // no point for syscall overhead, will be freed
                        fprintf(stderr,"ptr truncation changed the ptr\n");
                    DEX_truncated++;
                    if ( Purgelist[(i<<2) + (modval&3)] != 0 )
                        fprintf(stderr,"purgelist collision at i.%d modval.%d\n",i,modval);
                    Purgelist[(i<<2) + (modval&3)] = ptr;
                    n++;
                }
            } else fprintf(stderr,"modval.%d unexpected size.%d %d t.%u vs cutoff.%u\n",modval,ptr->datalen,i,t,cutoff);
        }
    }
    pthread_mutex_unlock(&DEX_mutex);
    //totalhash = komodo_DEXtotal(total);
    if ( n != 0 || (modval % 60) == 0 )//totalhash != prevtotalhash )
    {
        int32_t histo[64];
        memset(histo,0,sizeof(histo));
        totalhash = komodo_DEXtotal(histo,total);
        fprintf(stderr,"purge.%d -> n.%d %08x, total.%d %08x R.%d S.%d A.%d dup.%d | L.%d A.%d coll.%d | avelag  %.3f (%.4f %.4f %.4f) errlag.%d pend.%d T/F %d/%d | %d/sec ",modval,n,purgehash,total,totalhash,DEX_totalrecv,DEX_totalsent,DEX_totaladd,DEX_duplicate,DEX_lookup32,DEX_add32,DEX_collision32,n>0?(double)lagsum/n:0,DEX_lag,DEX_lag2,DEX_lag3,DEX_maxlag,DEX_Numpending,DEX_truncated,DEX_freed,(DEX_totaladd - lastadd)/(cutoff - lastcutoff));
        for (i=13; i>=0; i--)
            fprintf(stderr,"%.0f ",1000.*histo[i]/(total+1)); // expected 1 1 2 5 | 10 10 10 10 10 | 10 9 9 7 5
        fprintf(stderr,"%s\n",komodo_DEX_islagging()!=0?"LAGGING":"");
        lastadd = DEX_totaladd;
        prevtotalhash = totalhash;
        lastcutoff = cutoff;
    }
    komodo_DEX_purgelist(0);
    return(n);
}

void komodo_DEXpoll(CNode *pto)
{
    static uint32_t purgetime;
    std::vector<uint8_t> packet; uint32_t i,now,shorthash,len,ptime,modval;
    now = (uint32_t)time(NULL);
    ptime = now - KOMODO_DEX_PURGETIME + 3;;
    //pthread_mutex_lock(&DEX_mutex);
    if ( ptime > purgetime )
    {
        if ( purgetime == 0 )
        {
            purgetime = ptime;
        }
        else
        {
            for (; purgetime<ptime; purgetime++)
                komodo_DEXpurge(purgetime);
        }
        DEX_Numpending *= 0.995; // decay pending to compensate for hashcollision remnants
    }
    if ( (now == Got_Recent_Quote && now > pto->dexlastping) || now >= pto->dexlastping+KOMODO_DEX_LOCALHEARTBEAT )
    {
        for (i=0; i<KOMODO_DEX_MAXLAG/2; i++)
        {
            modval = (now + 1 - i) % KOMODO_DEX_PURGETIME;
            if ( komodo_DEXmodval(now,modval,pto) > 0 )
                pto->dexlastping = now;
        }
    }
    //pthread_mutex_unlock(&DEX_mutex);
}

int32_t komodo_DEXprocess(uint32_t now,CNode *pfrom,uint8_t *msg,int32_t len)
{
    int32_t i,j,ind,m,offset,flag,modval,openind,lag,priority; uint16_t n,peerpos; uint32_t t,h; uint8_t funcid,relay=0; bits256 hash; struct DEX_datablob *ptr;
    peerpos = komodo_DEXpeerpos(now,pfrom->id);
    //fprintf(stderr,"peer.%d msg[%d] %c\n",peerpos,len,msg[1]);
    if ( len >= KOMODO_DEX_ROUTESIZE && peerpos != 0xffff && len < KOMODO_DEX_MAXPACKETSIZE )
    {
        relay = msg[0];
        funcid = msg[1];
        iguana_rwnum(0,&msg[2],sizeof(t),&t);
        modval = (t % KOMODO_DEX_PURGETIME);
        lag = (now - t);
        if ( lag < 0 )
            lag = 0;
        if ( t > now+KOMODO_DEX_LOCALHEARTBEAT )
        {
            fprintf(stderr,"reject packet from future t.%u vs now.%u\n",t,now);
        }
        else if ( lag > KOMODO_DEX_MAXLAG )
        {
            DEX_maxlag++;
            //fprintf(stderr,"reject packet with too big lag t.%u vs now.%u lag.%d\n",t,now,lag);
        }
        else if ( funcid == 'Q' )
        {
            h = komodo_DEXquotehash(hash,msg,len);
            DEX_totalrecv++;
            //fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
            //fprintf(stderr," recv at %u from (%s) >>>>>>>>>> shorthash.%08x %016llx total R%d/S%d/A%d dup.%d\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str(),h,(long long)hash.ulongs[1],DEX_totalrecv,DEX_totalsent,DEX_totaladd,DEX_duplicate);
            if ( (hash.ulongs[1] & KOMODO_DEX_TXPOWMASK) != (0x777 & KOMODO_DEX_TXPOWMASK) )
            {
                static uint32_t count;
                if ( count++ < 10 )
                    fprintf(stderr,"reject quote due to invalid hash[1] %016llx\n",(long long)hash.ulongs[1]);
            }
            else if ( komodo_DEX_priority(hash.ulongs[1],len) < 0 )
            {
                static uint32_t count;
                if ( count++ < 10 )
                    fprintf(stderr,"reject quote due to insufficient priority.%d for size.%d, needed %d\n",komodo_DEX_priority(hash.ulongs[1],0),len,komodo_DEX_sizepriority(len));
                
            }
            else if ( relay <= KOMODO_DEX_RELAYDEPTH || relay == 0xff )
            {
                if ( (ptr= komodo_DEXfind(openind,modval,h)) == 0 )
                {
                    if ( (ptr= komodo_DEXadd(openind,now,modval,hash,h,msg,len)) != 0 )
                    {
                        if ( komodo_DEXfind32(Pendings,(int32_t)(sizeof(Pendings)/sizeof(*Pendings)),h,1) >= 0 )
                        {
                            if ( DEX_Numpending > 0 )
                                DEX_Numpending--;
                        }
                        Got_Recent_Quote = now;
                        if ( lag > 0 )
                        {
                            if ( DEX_lag == 0. )
                                DEX_lag = DEX_lag2 = DEX_lag3 = lag;
                            else
                            {
                                DEX_lag = (DEX_lag * 0.995) + (0.005 * lag);
                                DEX_lag2 = (DEX_lag2 * 0.999) + (0.001 * lag);
                                DEX_lag3 = (DEX_lag3 * 0.9999) + (0.0001 * lag);
                            }
                        }
                    }
                } else DEX_duplicate++;
                if ( ptr != 0 )
                    SETBIT(ptr->peermask,peerpos);
            } else fprintf(stderr,"unexpected relay.%d\n",relay);
        }
        else if ( funcid == 'P' )
        {
            std::vector<uint8_t> getshorthash;
            if ( len >= 12 )
            {
                offset = KOMODO_DEX_ROUTESIZE;
                offset += iguana_rwnum(0,&msg[offset],sizeof(n),&n);
                offset += iguana_rwnum(0,&msg[offset],sizeof(m),&m);
                if ( offset+n*sizeof(uint32_t) == len && m >= 0 && m < KOMODO_DEX_PURGETIME )
                {
                    for (flag=i=0; i<n; i++)
                    {
                        if ( DEX_Numpending > KOMODO_DEX_HASHSIZE )
                            break;
                        offset += iguana_rwnum(0,&msg[offset],sizeof(h),&h);
                        if ( (ptr= komodo_DEXfind(openind,m,h)) != 0 )
                            continue;
                        if ( komodo_DEXfind32(Pendings,(int32_t)(sizeof(Pendings)/sizeof(*Pendings)),h,0) < 0 )
                        {
                            komodo_DEXadd32(Pendings,(int32_t)(sizeof(Pendings)/sizeof(*Pendings)),h);
                            //fprintf(stderr,">>>> %08x <<<<< ",h);
                            DEX_Numpending++;
                            komodo_DEXgenget(getshorthash,now,h,m);
                            pfrom->PushMessage("DEX",getshorthash);
                            flag++;
                        }
                        //fprintf(stderr,"%08x ",h);
                    }
                    if ( (0) && flag != 0 )
                    {
                        fprintf(stderr," f.%c t.%u [%d] ",funcid,t,relay);
                        fprintf(stderr," recv at %u from (%s) PULL these.%d lag.%d\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str(),flag,lag);
                    } else if ( (0) && n > 0 )
                        fprintf(stderr,"ping from %s\n",pfrom->addr.ToString().c_str());
                } else fprintf(stderr,"ping packetsize error %d != %d, offset.%d n.%d, modval.%d purgtime.%d\n",len,offset+n*4,offset,n,m,KOMODO_DEX_PURGETIME);
            }
        }
        else if ( funcid == 'G' )
        {
            iguana_rwnum(0,&msg[KOMODO_DEX_ROUTESIZE],sizeof(h),&h);
            iguana_rwnum(0,&msg[KOMODO_DEX_ROUTESIZE+sizeof(h)],sizeof(modval),&modval);
            //fprintf(stderr," f.%c t.%u [%d] get.%08x ",funcid,t,relay,h);
            //fprintf(stderr," recv at %u from (%s)\n",(uint32_t)time(NULL),pfrom->addr.ToString().c_str());
            if ( modval >= 0 && modval < KOMODO_DEX_PURGETIME )
            {
                if ( (ptr= komodo_DEXfind(openind,modval,h)) != 0 )
                {
                    if ( GETBIT(ptr->peermask,peerpos) == 0 )
                        return(komodo_DEXpacketsend(pfrom,peerpos,ptr,0)); // squelch relaying of 'G' packets
                }
            } else fprintf(stderr,"illegal modval.%d\n",modval);
        }
    }
    return(0);
}

void komodo_DEXmsg(CNode *pfrom,std::vector<uint8_t> request) // received a packet
{
    int32_t len; std::vector<uint8_t> response; bits256 hash; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= request.size()) > 0 )
    {
        //pthread_mutex_lock(&DEX_mutex);
        komodo_DEXprocess(timestamp,pfrom,&request[0],len);
        //pthread_mutex_unlock(&DEX_mutex);
    }
}

// following is from separate process from cli! implements middle message layer

int32_t komodo_DEX_payloadstr(UniValue &item,uint8_t *data,int32_t datalen,int32_t decrypted)
{
    char *itemstr; int32_t i,j,hexflag = 0;
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
        for (i=0,j=0; i<datalen; i++,j++)
            sprintf(&itemstr[j<<1],"%02x",data[i]);
        itemstr[j<<1] = 0;
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

UniValue komodo_DEX_dataobj(struct DEX_datablob *ptr)
{
    UniValue item(UniValue::VOBJ); bits256 priv0; uint32_t t; bits256 destpubkey; int32_t i,j,dflag=0,newlen; uint8_t *decoded,*allocated=0,destpub33[33]; uint64_t amountA,amountB; char taga[KOMODO_DEX_MAXKEYSIZE+1],tagb[KOMODO_DEX_MAXKEYSIZE+1],destpubstr[67];
    iguana_rwnum(0,&ptr->data[2],sizeof(t),&t);
    iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE],sizeof(amountA),&amountA);
    iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE + sizeof(amountA)],sizeof(amountB),&amountB);
    item.push_back(Pair((char *)"timestamp",(int64_t)t));
    item.push_back(Pair((char *)"id",(int64_t)ptr->hash.uints[0]));
    if ( komodo_DEX_tagsextract(taga,tagb,destpubstr,destpub33,&ptr->data[KOMODO_DEX_ROUTESIZE],ptr->datalen-KOMODO_DEX_ROUTESIZE) >= 0 )
    {
        item.push_back(Pair((char *)"tagA",taga));
        item.push_back(Pair((char *)"tagB",tagb));
        item.push_back(Pair((char *)"destpub",destpubstr));
    }
    memcpy(destpubkey.bytes,destpub33+1,32);
    komodo_DEX_payloadstr(item,&ptr->data[ptr->offset],ptr->datalen-4-ptr->offset,0);
    if ( memcmp(destpubkey.bytes,DEX_pubkey.bytes,32) == 0 )
    {
        komodo_DEX_privkey(priv0);
        newlen = ptr->datalen-4-ptr->offset;
        if ( (decoded= komodo_DEX_decrypt(&allocated,&ptr->data[ptr->offset],&newlen,priv0)) != 0 )
            komodo_DEX_payloadstr(item,decoded,newlen,1);
        if ( allocated != 0 )
            free(allocated), allocated = 0;
        memset(priv0.bytes,0,sizeof(priv0));
    }
    item.push_back(Pair((char *)"amountA",dstr(amountA)));
    item.push_back(Pair((char *)"amountB",dstr(amountB)));
    item.push_back(Pair((char *)"priority",komodo_DEX_priority(ptr->hash.ulongs[1],ptr->datalen)));
    return(item);
}

UniValue komodo_DEXbroadcast(char *hexstr,int32_t priority,char *tagA,char *tagB,char *destpub33,char *volA,char *volB)
{
    UniValue result; struct DEX_datablob *ptr=0; std::vector<uint8_t> packet; bits256 hash,destpubkey; uint8_t quote[3700],destpub[33],*payload=0,*payload2=0,*allocated=0; int32_t blastflag,i,m=0,ind,explen,len=0,datalen=0,destpubflag=0,slen,modval,iter,openind; uint32_t shorthash,timestamp; uint64_t amountA=0,amountB=0;
    blastflag = strcmp(hexstr,"ffff") == 0;
    if ( priority < 0 || priority > KOMODO_DEX_MAXPRIORITY )
        priority = KOMODO_DEX_MAXPRIORITY;
    if ( hexstr == 0 || tagA == 0 || tagB == 0 || destpub33 == 0 || volA == 0 || volB == 0 || strlen(tagA) >= KOMODO_DEX_TAGSIZE || strlen(tagB) >= KOMODO_DEX_TAGSIZE )
        return(-1);
    if ( tagA[0] == 0 && tagB[0] == 0 && destpub33[0] == 0 )
        tagA = (char *)"general";
    for (iter=0; iter<10; iter++)
    {
        len = 0;
        if ( volA[0] != 0 )
        {
            if ( atof(volA) < 0. )
            {
                fprintf(stderr,"negative volA error\n");
                return(result);
            }
            amountA = atof(volA) * SATOSHIDEN + 0.0000000049;
        }
        if ( volB[0] != 0 )
        {
            if ( atof(volB) < 0. )
            {
                fprintf(stderr,"negative volB error\n");
                return(result);
            }
            amountB = atof(volB) * SATOSHIDEN + 0.0000000049;
        }
        len = iguana_rwnum(1,&quote[len],sizeof(amountA),&amountA);
        len += iguana_rwnum(1,&quote[len],sizeof(amountB),&amountB);
        if ( is_hexstr(destpub33,0) == 66 )
        {
            decode_hex(destpub,33,destpub33);
            quote[len++] = 33;
            destpubflag = 1;
            if ( destpub[0] != 0x01 )
            {
                fprintf(stderr,"only destpubkey starting with 0x01 is allowed, these are available from DEX_stats\n");
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
        timestamp = (uint32_t)time(NULL);
        modval = (timestamp % KOMODO_DEX_PURGETIME);
        if ( destpubflag != 0 )
        {
            bits256 priv0;
            komodo_DEX_privkey(priv0);
            for (i=0; i<32; i++)
                destpubkey.bytes[i] = destpub[i + 1];
            if ( (payload2= komodo_DEX_encrypt(&allocated,payload,&datalen,destpubkey,priv0)) == 0 )
            {
                fprintf(stderr,"encryption error for datalen.%d\n",datalen);
                if ( allocated != 0 )
                    free(allocated);
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
        if ( (m= komodo_DEXgenquote(iter*0 + priority + komodo_DEX_sizepriority(explen),hash,shorthash,packet,timestamp,quote,len,payload2,datalen)) != explen )
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
        if ( blastflag != 0 && komodo_DEX_priority(hash.ulongs[1],explen) > priority + iter*0 )
        {
            fprintf(stderr,"skip harder than specified %d vs %d\n",komodo_DEX_priority(hash.ulongs[1],explen), priority + iter + komodo_DEX_sizepriority(explen));
            continue;
        }
        if ( m > KOMODO_DEX_MAXPACKETSIZE )
        {
            fprintf(stderr,"packetsize.%d > KOMODO_DEX_MAXPACKETSIZE.%d\n",m,KOMODO_DEX_MAXPACKETSIZE);
            return(0);
        }
        //pthread_mutex_lock(&DEX_mutex);
        if ( (ptr= komodo_DEXfind(openind,modval,shorthash)) == 0 )
        {
            if ( (ptr= komodo_DEXadd(-1,timestamp,timestamp % KOMODO_DEX_PURGETIME,hash,shorthash,&packet[0],packet.size())) == 0 )
            {
                char str[65];
                for (i=0; i<len&&i<64; i++)
                    fprintf(stderr,"%02x",quote[i]);
                fprintf(stderr," ERROR issue order %08x %016llx %s! packetsize.%d\n",shorthash,(long long)hash.ulongs[1],bits256_str(str,hash),(int32_t)packet.size());
            }
        }
        else
        {
            static uint32_t counter;
            if ( counter++ < 100 )
                fprintf(stderr," cant issue duplicate order modval.%d t.%u %08x %016llx\n",modval,timestamp,shorthash,(long long)hash.ulongs[1]);
            srand((int32_t)timestamp);
        }
        //pthread_mutex_unlock(&DEX_mutex);
        if ( blastflag == 0 )
            break;
    }
    if ( blastflag == 0 && ptr != 0 )
    {
        //pthread_mutex_lock(&DEX_mutex);
        result = komodo_DEX_dataobj(ptr);
        //pthread_mutex_unlock(&DEX_mutex);
        return(result);
    } else return(0);
}

UniValue komodo_DEXlist(uint32_t stopat,int32_t minpriority,char *tagA,char *tagB,char *destpub33,char *minA,char *maxA,char *minB,char *maxB)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); char str[2*KOMODO_DEX_MAXKEYSIZE+1]; struct DEX_index *tips[KOMODO_DEX_MAXINDICES],*index; struct DEX_datablob *ptr; uint64_t amountA,amountB; int32_t i,j,ind,n=0,priority; uint32_t t; uint64_t minamountA=0,maxamountA=(1LL<<63),minamountB=0,maxamountB=(1LL<<63); int8_t lenA=0,lenB=0,plen=0; uint8_t destpub[33];
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
            return(result);
        }
        minamountA = atof(minA) * SATOSHIDEN + 0.0000000049;
    }
    if ( maxA[0] != 0 )
    {
        if ( atof(maxA) < 0. )
        {
            fprintf(stderr,"negative maxA error\n");
            return(result);
        }
        maxamountA = atof(maxA) * SATOSHIDEN + 0.0000000049;
    }
    if ( minB[0] != 0 )
    {
        if ( atof(minB) < 0. )
        {
            fprintf(stderr,"negative minB error\n");
            return(result);
        }
        minamountB = atof(minB) * SATOSHIDEN + 0.0000000049;
    }
    if ( maxB[0] != 0 )
    {
        if ( atof(maxB) < 0. )
        {
            fprintf(stderr,"negative maxB error\n");
            return(result);
        }
        maxamountB = atof(maxB) * SATOSHIDEN + 0.0000000049;
    }
    if ( minA > maxA || minB > maxB )
    {
        fprintf(stderr,"illegal value range [%.8f %.8f].A [%.8f %.8f].B\n",dstr(minamountA),dstr(maxamountA),dstr(minamountB),dstr(maxamountB));
        return(-1);
    }
    if ( is_hexstr(destpub33,0) == 66 )
    {
        decode_hex(destpub,33,destpub33);
        plen = 33;
    }
    //fprintf(stderr,"DEX_list (%s) (%s)\n",tagA,tagB);
    //pthread_mutex_lock(&DEX_mutex);
    if ( (DEX_updatetips(tips,0,0,lenA,(uint8_t *)tagA,lenB,(uint8_t *)tagB,destpub,plen) & 0xffff) != 0 )
    {
        for (ind=0; ind<KOMODO_DEX_MAXINDICES; ind++)
        {
            if ( (index= tips[ind]) != 0 )
            {
                ptr = index->tip;
                n = 0;
                while ( ptr != 0 )
                {
                    if ( ptr->hash.uints[0] == stopat )
                    {
                        fprintf(stderr,"reached stopat id\n");
                        break;
                    }
                    if ( (priority= komodo_DEX_priority(ptr->hash.ulongs[1],ptr->datalen)) < minpriority )
                    {
                        //fprintf(stderr,"priority.%d < min.%d, skip\n",komodo_DEX_priority(ptr->hash.ulongs[1]),minpriority);
                        ptr = ptr->prevs[ind];
                        continue;
                    }
                    iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE],sizeof(amountA),&amountA);
                    iguana_rwnum(0,&ptr->data[KOMODO_DEX_ROUTESIZE + sizeof(amountA)],sizeof(amountB),&amountB);
                    if ( amountA < minamountA || amountA > maxamountA )
                    {
                        fprintf(stderr,"amountA %.8f vs min %.8f max %.8f, skip\n",dstr(amountA),dstr(minamountA),dstr(maxamountA));
                        ptr = ptr->prevs[ind];
                        continue;
                    }
                    if ( amountB < minamountB || amountB > maxamountB )
                    {
                        fprintf(stderr,"amountB %.8f vs min %.8f max %.8f, skip\n",dstr(amountB),dstr(minamountB),dstr(maxamountB));
                        ptr = ptr->prevs[ind];
                        continue;
                    }
                    //fprintf(stderr,"DEX_list ind.%d %p ptr.%p prev.%p\n",ind,index,ptr,ptr->prevs[ind]);
                    a.push_back(komodo_DEX_dataobj(ptr));
                    n++;
                    ptr = ptr->prevs[ind];
                }
            }
        }
        result.push_back(Pair((char *)"matches",a));
    }
    //pthread_mutex_unlock(&DEX_mutex);
    result.push_back(Pair((char *)"tagA",tagA));
    result.push_back(Pair((char *)"tagB",tagB));
    result.push_back(Pair((char *)"destpub",destpub33));
    result.push_back(Pair((char *)"n",n));
    return(result);
}

UniValue komodo_DEX_stats()
{
    UniValue result(UniValue::VOBJ); char str[65],pubstr[67];
    bits256_str(pubstr+2,DEX_pubkey);
    pubstr[0] = '0';
    pubstr[1] = '1';
    result.push_back(Pair((char *)"publishable_pubkey",pubstr));
    // add performance stats too
    return(result);
}
