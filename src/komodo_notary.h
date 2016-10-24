
#define KOMODO_ELECTION_GAP 1000
#define KOMODO_PUBKEYS_HEIGHT(height) ((int32_t)(((((height)+KOMODO_ELECTION_GAP*.5)/KOMODO_ELECTION_GAP) + 1) * KOMODO_ELECTION_GAP))

struct nutxo_entry { UT_hash_handle hh; uint256 txhash; uint64_t voutmask; int32_t notaryid,height; } *NUTXOS;
struct knotary_entry { UT_hash_handle hh; uint8_t pubkey[33],notaryid; };
struct knotaries_entry { int32_t height,numnotaries; struct knotary_entry *Notaries; } Pubkeys[10000];
struct notarized_checkpoint { uint256 notarized_hash,notarized_desttxid; int32_t nHeight,notarized_height; } *NPOINTS; int32_t NUM_NPOINTS;

int32_t IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,NOTARIZED_HEIGHT,Num_nutxos,KOMODO_NUMNOTARIES = 64;
std::string NOTARY_PUBKEY;
uint8_t NOTARY_PUBKEY33[33];
uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID;

int32_t komodo_ratify_threshold(int32_t height,uint64_t signedmask)
{
    int32_t numnotaries,i,wt = 0;
    numnotaries = Pubkeys[height / KOMODO_ELECTION_GAP].numnotaries;
    for (i=0; i<numnotaries; i++)
        if ( ((1LL << i) & signedmask) != 0 )
            wt++;
    if ( wt > (numnotaries >> 1) || (wt > 7 && (signedmask & 3) != 0) )
        return(1); // N/2+1 || N/3 + devsig
    else return(0);
}

void komodo_nutxoadd(int32_t height,int32_t notaryid,uint256 txhash,uint64_t voutmask,int32_t numvouts)
{
    struct nutxo_entry *np;
    if ( numvouts > 1 && notaryid < 64 )
    {
        pthread_mutex_lock(&komodo_mutex);
        np = (struct nutxo_entry *)calloc(1,sizeof(*np));
        np->height = height;
        np->txhash = txhash;
        np->voutmask = voutmask;
        np->notaryid = notaryid;
        HASH_ADD_KEYPTR(hh,NUTXOS,&np->txhash,sizeof(np->txhash),np);
        printf("Add NUTXO[%d] <- %s notaryid.%d t%u %s %llx\n",Num_nutxos,Notaries[notaryid][0],notaryid,komodo_txtime(txhash),txhash.ToString().c_str(),(long long)voutmask);
        //if ( addflag != 0 )
        //    komodo_stateupdate(height,0,0,notaryid,txhash,voutmask,numvouts,0,0);
        Num_nutxos++;
        pthread_mutex_unlock(&komodo_mutex);
    }
}

int32_t komodo_nutxofind(int32_t height,uint256 txhash,int32_t vout)
{
    struct nutxo_entry *np;
    pthread_mutex_lock(&komodo_mutex);
    HASH_FIND(hh,NUTXOS,&txhash,sizeof(txhash),np);
    pthread_mutex_unlock(&komodo_mutex);
    if ( np != 0 && ((1LL << vout) & np->voutmask) != 0 )
        return(np->notaryid);
    return(-1);
}

void komodo_notarysinit(int32_t height,uint8_t pubkeys[64][33],int32_t num)
{
    int32_t k,i,htind; struct knotary_entry *kp; struct knotaries_entry N;
    memset(&N,0,sizeof(N));
    pthread_mutex_lock(&komodo_mutex);
    for (k=0; k<num; k++)
    {
        kp = (struct knotary_entry *)calloc(1,sizeof(*kp));
        memcpy(kp->pubkey,pubkeys[k],33);
        kp->notaryid = k;
        HASH_ADD_KEYPTR(hh,N.Notaries,kp->pubkey,33,kp);
        for (i=0; i<33; i++)
            printf("%02x",pubkeys[k][i]);
        printf(" notarypubs.[%d]\n",k);
    }
    N.numnotaries = num;
    htind = KOMODO_PUBKEYS_HEIGHT(height) / KOMODO_ELECTION_GAP;
    if ( htind == 1 )
        htind = 0;
    for (i=htind; i<sizeof(Pubkeys)/sizeof(*Pubkeys); i++)
    {
        Pubkeys[i] = N;
        Pubkeys[i].height = i * KOMODO_ELECTION_GAP;
    }
    pthread_mutex_unlock(&komodo_mutex);
}

int32_t komodo_chosennotary(int32_t *notaryidp,int32_t height,uint8_t *pubkey33)
{
    // -1 if not notary, 0 if notary, 1 if special notary
    struct knotary_entry *kp; int32_t numnotaries,modval = -1;
    *notaryidp = -1;
    pthread_mutex_lock(&komodo_mutex);
    HASH_FIND(hh,Pubkeys[height/KOMODO_ELECTION_GAP].Notaries,pubkey33,33,kp);
    pthread_mutex_unlock(&komodo_mutex);
    if ( kp != 0 )
    {
        if ( (numnotaries= Pubkeys[height/KOMODO_ELECTION_GAP].numnotaries) > 0 )
        {
            *notaryidp = kp->notaryid;
            modval = ((height % numnotaries) == kp->notaryid);
            //printf("found notary.%d ht.%d modval.%d\n",kp->notaryid,height,modval);
        } else printf("unexpected zero notaries at height.%d\n",height);
    }
    //int32_t i; for (i=0; i<33; i++)
    //    printf("%02x",pubkey33[i]);
    //printf(" ht.%d notary.%d special.%d\n",height,*notaryidp,modval);
    return(modval);
}

void komodo_notarized_update(int32_t nHeight,int32_t notarized_height,uint256 notarized_hash,uint256 notarized_desttxid)
{
    struct notarized_checkpoint *np;
    if ( notarized_height > nHeight )
    {
        printf("komodo_notarized_update REJECT notarized_height %d > %d nHeight\n",notarized_height,nHeight);
        return;
    }
    NPOINTS = (struct notarized_checkpoint *)realloc(NPOINTS,(NUM_NPOINTS+1) * sizeof(*NPOINTS));
    np = &NPOINTS[NUM_NPOINTS++];
    memset(np,0,sizeof(*np));
    np->nHeight = nHeight;
    np->notarized_height = notarized_height;
    np->notarized_hash = notarized_hash;
    np->notarized_desttxid = notarized_desttxid;
}

int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp)
{
    struct notarized_checkpoint *np = 0; int32_t i;
    if ( NUM_NPOINTS > 0 )
    {
        for (i=0; i<NUM_NPOINTS; i++)
        {
            if ( NPOINTS[i].nHeight >= nHeight )
                break;
            np = &NPOINTS[i];
        }
    }
    if ( np != 0 )
    {
        *notarized_hashp = np->notarized_hash;
        *notarized_desttxidp = np->notarized_desttxid;
        return(np->notarized_height);
    }
    memset(notarized_hashp,0,sizeof(*notarized_hashp));
    return(0);
}
