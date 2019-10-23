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

#include "CCdice.h"

// timeout

/*
 in order to implement a dice game, we need a source of entropy, reasonably fast completion time and a way to manage the utxos.

 1. CC vout locks "house" funds with hash(entropy)
 2. bettor submits bet, with entropy, odds, houseid and sends combined amount into another CC vout.
 3. house account sends funds to winner/loser with proof of entropy
 4. if timeout, bettor gets refund

 2. and 3. can be done in mempool

 The house commits to an entropy value by including the hash of the entropy value  in the 'E' transaction.

 To bet, one of these 'E' transactions is used as the first input and its hashed entropy is combined with the unhashed entropy attached to the bet 'B' transaction.

 The house node monitors the 'B' transactions and if it sees one of its own, it creates either a winner 'W' or loser 'L' transaction, with proof of hash of entropy.

 In the even the house node doesnt respond before timeoutblocks, then anybody (including bettor) can undo the bet with funds going back to the house and bettor

 In order for people to play dice, someone (anyone) needs to create a funded dice plan and addfunding with enough utxo to allow players to find one.

createfunding:
 vins.*: normal inputs
 vout.0: CC vout for funding
 vout.1: owner vout
 vout.2: dice marker address vout for easy searching
 vout.3: normal change
 vout.n-1: opreturn 'F' sbits minbet maxbet maxodds timeoutblocks

addfunding (entropy):
 vins.*: normal inputs
 vout.0: CC vout for locked entropy funds
 vout.1: tag to owner address for entropy funds
 vout.2: normal change
 vout.n-1: opreturn 'E' sbits fundingtxid hentropy

bet:
 vin.0: entropy txid from house (must validate vin0 of 'E')
 vins.1+: normal inputs
 vout.0: CC vout for locked entropy
 vout.1: CC vout for locked bet
 vout.2: tag for bettor's address (txfee + odds)
 vout.3: change
 vout.n-1: opreturn 'B' sbits fundingtxid entropy

loser:
 vin.0: normal input
 vin.1: betTx CC vout.0 entropy from bet
 vin.2: betTx CC vout.1 bet amount from bet
 vin.3+: funding CC vout.0 from 'F', 'E', 'W', 'L' or 'T'
 vout.0: funding CC to entropy owner
 vout.1: tag to owner address for entropy funds
 vout.2: change to fundingpk
 vout.n-1: opreturn 'L' sbits fundingtxid hentropy proof

winner:
 same as loser, but vout.2 is winnings
 vout.3: change to fundingpk
 vout.n-1: opreturn 'W' sbits fundingtxid hentropy proof

timeout:
 same as winner, just without hentropy or proof

WARNING: there is an attack vector that precludes betting any large amounts, it goes as follows:
 1. do dicebet to get the house entropy revealed
 2. calculate bettor entropy that would win against the house entropy
 3. reorg the chain and make a big bet using the winning entropy calculated in 2.

 In order to mitigate this, the disclosure of the house entropy needs to be delayed beyond a reasonable reorg depth (notarization). It is recommended for production dice game with significant amounts of money to use such a delayed disclosure method.

 Actually a much better solution to this is possible, which allows to retain the realtime response aspect of dice CC, which is critical to its usage.

What is needed is for the dealer node to track the entropy tx that was already broadcast into the mempool with its entropy revealed. Then before processing a dicebet, it is checked against the already revealed list. If it is found, the dicebet is refunded with proof that a different dicebet was already used to reveal the entropy

 change to hashtables
 validate refund

 */

#include "../compat/endian.h"

#define MAX_ENTROPYUSED 8192
#define DICE_MINUTXOS 15000
extern int32_t KOMODO_INSYNC;

pthread_mutex_t DICE_MUTEX,DICEREVEALED_MUTEX;

struct dicefinish_utxo { uint256 txid; int32_t vout; };

struct dicefinish_info
{
    struct dicefinish_info *prev,*next;
    CTransaction betTx;
    uint256 fundingtxid,bettxid,entropyused,txid;
    uint64_t sbits;
    int64_t winamount;
    int32_t iswin,entropyvout,orphaned;
    uint32_t bettxid_ready,revealed;
    std::string rawtx;
    uint8_t funcid;
} *DICEFINISH_LIST;

struct dicehash_entry
{
    UT_hash_handle hh;
    uint256 bettxid;
} *DICEHASH_TABLE;

struct dice_entropy
{
    UT_hash_handle hh;
    uint256 entropyused,bettxid;
    CTransaction betTx;
    int32_t entropyvout;
} *DICE_ENTROPY;

struct dicehash_entry *_dicehash_find(uint256 bettxid)
{
    struct dicehash_entry *ptr;
    HASH_FIND(hh,DICEHASH_TABLE,&bettxid,sizeof(bettxid),ptr);
    return(ptr);
}

int32_t _dicehash_clear(uint256 bettxid)
{
    struct dicehash_entry *ptr;
    HASH_FIND(hh,DICEHASH_TABLE,&bettxid,sizeof(bettxid),ptr);
    if ( ptr != 0 )
    {
        fprintf(stderr,"delete %s\n",bettxid.GetHex().c_str());
        HASH_DELETE(hh,DICEHASH_TABLE,ptr);
        return(0);
    } else fprintf(stderr,"hashdelete couldnt find %s\n",bettxid.GetHex().c_str());
    return(-1);
}

struct dicehash_entry *_dicehash_add(uint256 bettxid)
{
    struct dicehash_entry *ptr;
    ptr = (struct dicehash_entry *)calloc(1,sizeof(*ptr));
    ptr->bettxid = bettxid;
    HASH_ADD(hh,DICEHASH_TABLE,bettxid,sizeof(bettxid),ptr);
    return(ptr);
}

int32_t _dicerevealed_find(uint256 &oldbettxid,CTransaction &oldbetTx,int32_t &oldentropyvout,uint256 entropyused,uint256 bettxid,int32_t entropyvout)
{
    struct dice_entropy *ptr;
    HASH_FIND(hh,DICE_ENTROPY,&entropyused,sizeof(entropyused),ptr);
    if ( ptr != 0 )
    {
        if ( entropyvout == ptr->entropyvout )
        {
            if ( bettxid == ptr->bettxid )
            {
                //fprintf(stderr,"identical %s E.%s v.%d\n",bettxid.GetHex().c_str(),entropyused.GetHex().c_str(),entropyvout);
                return(entropyvout+1);
            }
            else
            {
                fprintf(stderr,"found identical entropy used.%s %s vs %s v.%d vs %d\n",entropyused.GetHex().c_str(),bettxid.GetHex().c_str(),ptr->bettxid.GetHex().c_str(),entropyvout,ptr->entropyvout);
                oldbettxid = ptr->bettxid;
                oldbetTx = ptr->betTx;
                oldentropyvout = ptr->entropyvout;
                return(-1);
            }
        } else fprintf(stderr,"shared entropy.%s vouts %d vs %d\n",entropyused.GetHex().c_str(),entropyvout,ptr->entropyvout);
    }
    return(0);
}

struct dice_entropy *_dicerevealed_add(uint256 entropyused,uint256 bettxid,CTransaction betTx,int32_t entropyvout)
{
    struct dice_entropy *ptr;
    ptr = (struct dice_entropy *)calloc(1,sizeof(*ptr));
    ptr->entropyused = entropyused;
    ptr->bettxid = bettxid;
    ptr->betTx = betTx;
    ptr->entropyvout = entropyvout;
    HASH_ADD(hh,DICE_ENTROPY,entropyused,sizeof(entropyused),ptr);
    return(ptr);
}

int32_t DiceEntropyUsed(CTransaction &oldbetTx,uint256 &oldbettxid,int32_t &oldentropyvout,uint256 entropyused,uint256 bettxid,CTransaction betTx,int32_t entropyvout)
{
    int32_t retval;
    oldbettxid = zeroid;
    if ( entropyused == zeroid || bettxid == zeroid )
    {
        fprintf(stderr,"null entropyused or bettxid\n");
        return(1);
    }
    pthread_mutex_lock(&DICEREVEALED_MUTEX);
    retval = _dicerevealed_find(oldbettxid,oldbetTx,oldentropyvout,entropyused,bettxid,entropyvout);
    pthread_mutex_unlock(&DICEREVEALED_MUTEX);
    return(retval);
}

bool mySenddicetransaction(std::string res,uint256 entropyused,int32_t entropyvout,uint256 bettxid,CTransaction betTx,uint8_t funcid,struct dicefinish_info *ptr)
{
    CTransaction tx; int32_t i=0,retval=-1,oldentropyvout; uint256 oldbettxid; CTransaction oldbetTx;
    if ( res.empty() == 0 && res.size() > 64 && is_hexstr((char *)res.c_str(),0) > 64 )
    {
        if ( DecodeHexTx(tx,res) != 0 )
        {
            if ( ptr != 0 )
                ptr->txid = tx.GetHash();
            //fprintf(stderr,"%s\n%s\n",res.c_str(),uint256_str(str,tx.GetHash()));
            if ( funcid == 'R' || (retval= DiceEntropyUsed(oldbetTx,oldbettxid,oldentropyvout,entropyused,bettxid,betTx,entropyvout)) >= 0 )
            {
                LOCK(cs_main);
                if ( myAddtomempool(tx) != 0 )
                {
                    RelayTransaction(tx);
                    if ( retval == 0 )
                    {
                        if ( ptr != 0 )
                            ptr->revealed = (uint32_t)time(NULL);
                        pthread_mutex_lock(&DICEREVEALED_MUTEX);
                        _dicerevealed_add(entropyused,bettxid,betTx,entropyvout);
                        pthread_mutex_unlock(&DICEREVEALED_MUTEX);
                        fprintf(stderr,"added.%c to mempool.[%d] and broadcast entropyused.%s bettxid.%s -> %s\n",funcid,i,entropyused.GetHex().c_str(),bettxid.GetHex().c_str(),tx.GetHash().GetHex().c_str());
                    } else fprintf(stderr,"rebroadcast.%c to mempool.[%d] and broadcast entropyused.%s bettxid.%s -> %s\n",funcid,i,entropyused.GetHex().c_str(),bettxid.GetHex().c_str(),tx.GetHash().GetHex().c_str());
                    return(true);
                }
                else
                {
                    RelayTransaction(tx);
                    fprintf(stderr,"rebroadcast.%c and clear [%d] and broadcast entropyused.%s bettxid.%s -> %s\n",funcid,i,entropyused.GetHex().c_str(),bettxid.GetHex().c_str(),tx.GetHash().GetHex().c_str());
                    if ( ptr != 0 )
                    {
                        if ( ptr->rawtx.empty() == 0 )
                            ptr->rawtx.clear();
                        ptr->txid = zeroid;
                    }
                    //fprintf(stderr,"error adding funcid.%c E.%s bet.%s -> %s to mempool, probably Disable replacement feature size.%d\n",funcid,entropyused.GetHex().c_str(),bettxid.GetHex().c_str(),tx.GetHash().GetHex().c_str(),(int32_t)ptr->rawtx.size());
                }
            } else fprintf(stderr,"error duplicate entropyused different bettxid\n");
        } else fprintf(stderr,"error decoding hex\n");
    }
    return(false);
}

int32_t dicefinish_utxosget(int32_t &total,struct dicefinish_utxo *utxos,int32_t max,char *coinaddr)
{
    int32_t n = 0; int64_t threshold = 2 * 10000;
    total = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,coinaddr,false);
    {
        LOCK(mempool.cs);
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
        {
            if ( myIsutxo_spentinmempool(ignoretxid,ignorevin,it->first.txhash,(int32_t)it->first.index) == 0 )
            {
                if ( it->second.satoshis < threshold || it->second.satoshis > 10*threshold )
                    continue;
                total++;
                if ( n < max )
                {
                    if ( utxos != 0 )
                    {
                        utxos[n].txid = it->first.txhash;
                        utxos[n].vout = (int32_t)it->first.index;
                    }
                    n++;
                }
            }
        }
    }
    total -= n;
    return(n);
}

int32_t dice_betspent(char *debugstr,uint256 bettxid)
{
    CSpentIndexValue value,value2;
    CSpentIndexKey key(bettxid,0);
    CSpentIndexKey key2(bettxid,1);
    if ( GetSpentIndex(key,value) != 0 || GetSpentIndex(key2,value2) != 0 )
    {
        //fprintf(stderr,"%s txid.%s already spent\n",debugstr,bettxid.GetHex().c_str());
        return(1);
    }
    {
        //LOCK(mempool.cs);
        if ( myIsutxo_spentinmempool(ignoretxid,ignorevin,bettxid,0) != 0 || myIsutxo_spentinmempool(ignoretxid,ignorevin,bettxid,1) != 0 )
        {
            fprintf(stderr,"%s bettxid.%s already spent in mempool\n",debugstr,bettxid.GetHex().c_str());
            return(-1);
        }
    }
    return(0);
}

void dicefinish_delete(struct dicefinish_info *ptr)
{
    pthread_mutex_lock(&DICE_MUTEX);
    _dicehash_clear(ptr->bettxid);
    pthread_mutex_unlock(&DICE_MUTEX);
    DL_DELETE(DICEFINISH_LIST,ptr);
    free(ptr);
}

void *dicefinish(void *_ptr)
{
    std::vector<uint8_t> mypk; struct CCcontract_info *cp,C; char name[32],coinaddr[64],CCaddr[64]; std::string res; int32_t newht,newblock,entropyvout,numblocks,lastheight=0,vin0_needed,i,n,m,num,iter,result; struct dicefinish_info *ptr,*tmp; uint32_t now; struct dicefinish_utxo *utxos; uint256 hashBlock,entropyused; CPubKey dicepk; CTransaction betTx,finishTx,tx;
    mypk = Mypubkey();
    pubkey2addr(coinaddr,mypk.data());
    cp = CCinit(&C,EVAL_DICE);
    dicepk = GetUnspendable(cp,0);
    GetCCaddress(cp,CCaddr,GetUnspendable(cp,0));
    fprintf(stderr,"start dicefinish thread %s CCaddr.%s\n",coinaddr,CCaddr);
    if ( (newht= KOMODO_INSYNC) == 0 )
        sleep(7);
    sleep(3);
    while ( 1 )
    {
        if ( newht != 0 && lastheight != newht )
        {
            lastheight = newht;
            newblock = 1;
            fprintf(stderr,"dicefinish process lastheight.%d <- newht.%d\n",lastheight,newht);
        } else newblock = 0;
        now = (uint32_t)time(NULL);
        for (iter=-1; iter<=1; iter+=2)
        {
            vin0_needed = 0;
            DL_FOREACH_SAFE(DICEFINISH_LIST,ptr,tmp)
            {
                if ( ptr->iswin != iter )
                    continue;
                if ( ptr->revealed != 0 && now > ptr->revealed+3600 )
                {
                    fprintf(stderr,"purge %s\n",ptr->bettxid.GetHex().c_str());
                    dicefinish_delete(ptr);
                    continue;
                }
                if ( ptr->bettxid_ready == 0 )
                {
                    if ( myGetTransaction(ptr->bettxid,betTx,hashBlock) != 0 && hashBlock != zeroid )
                        ptr->bettxid_ready = (uint32_t)time(NULL);
                    else if ( mytxid_inmempool(ptr->bettxid) != 0 )
                        ptr->bettxid_ready = (uint32_t)time(NULL);
                }
                else if ( newblock != 0 && (myGetTransaction(ptr->bettxid,betTx,hashBlock) == 0 || now > ptr->bettxid_ready+600) )
                {
                    fprintf(stderr,"ORPHANED bettxid.%s\n",ptr->bettxid.GetHex().c_str());
                    dicefinish_delete(ptr);
                    continue;
                }
                else if ( newblock != 0 && ptr->txid != zeroid )
                {
                    if ( myGetTransaction(ptr->txid,finishTx,hashBlock) == 0 )
                    {
                        ptr->orphaned++;
                        fprintf(stderr,"ORPHANED.%d finish txid.%s\n",ptr->orphaned,ptr->txid.GetHex().c_str());
                        if ( ptr->orphaned < 4 )
                            continue;
                        if ( ptr->rawtx.empty() == 0 )
                            ptr->rawtx.clear();
                        ptr->txid = zeroid;
                        unstringbits(name,ptr->sbits);
                        result = 0;
                        ptr->orphaned = 0;
                        res = DiceBetFinish(ptr->funcid,entropyused,entropyvout,&result,0,name,ptr->fundingtxid,ptr->bettxid,ptr->iswin,zeroid,-2);
                        if ( ptr->entropyused == zeroid )
                        {
                            ptr->entropyused = entropyused;
                            ptr->entropyvout = entropyvout;
                        }
                        if ( entropyused != ptr->entropyused || entropyvout != ptr->entropyvout )
                        {
                            fprintf(stderr,"WARNING entropy %s != %s || %d != %d\n",entropyused.GetHex().c_str(),ptr->entropyused.GetHex().c_str(),entropyvout,ptr->entropyvout);
                        }
                        if ( result > 0 )
                        {
                            ptr->rawtx = res;
                            fprintf(stderr,"send refund!\n");
                            mySenddicetransaction(ptr->rawtx,ptr->entropyused,ptr->entropyvout,ptr->bettxid,ptr->betTx,ptr->funcid,ptr);
                        }
                        dicefinish_delete(ptr);
                        continue;
                    }
                }
                if ( ptr->bettxid_ready != 0 )
                {
                    if ( now > ptr->bettxid_ready + 2*3600 )
                    {
                        fprintf(stderr,"purge bettxid_ready %s\n",ptr->bettxid.GetHex().c_str());
                        dicefinish_delete(ptr);
                        continue;
                    }
                    else if ( newblock != 0 )
                    {
                        if ( ptr->txid != zeroid )
                        {
                            CCduration(numblocks,ptr->txid);
                            //fprintf(stderr,"duration finish txid.%s\n",ptr->txid.GetHex().c_str());
                            if ( numblocks == 0 )
                                mySenddicetransaction(ptr->rawtx,ptr->entropyused,ptr->entropyvout,ptr->bettxid,ptr->betTx,ptr->funcid,ptr);
                            else continue;
                        }
                    }
                    if ( ptr->txid == zeroid )
                        vin0_needed++;
                }
            }
            if ( vin0_needed > 0 )
            {
                num = 0;
//fprintf(stderr,"iter.%d vin0_needed.%d\n",iter,vin0_needed);
                utxos = (struct dicefinish_utxo *)calloc(vin0_needed,sizeof(*utxos));
                if ( (n= dicefinish_utxosget(num,utxos,vin0_needed,coinaddr)) > 0 )
                {
//fprintf(stderr,"iter.%d vin0_needed.%d got %d, num 0.0002 %d\n",iter,vin0_needed,n,num);
                    m = 0;
                    DL_FOREACH_SAFE(DICEFINISH_LIST,ptr,tmp)
                    {
                        if ( ptr->iswin != iter )
                            continue;
                        if ( ptr->revealed != 0 && time(NULL) > ptr->revealed+3600 )
                        {
                            fprintf(stderr,"purge2 %s\n",ptr->bettxid.GetHex().c_str());
                            dicefinish_delete(ptr);
                            continue;
                        }
                        if ( ptr->txid != zeroid )
                        {
                            CCduration(numblocks,ptr->txid);
                            //fprintf(stderr,"numblocks %s %d\n",ptr->txid.GetHex().c_str(),numblocks);
                            if ( numblocks > 0 )
                                continue;
                        }
                        if ( ptr->bettxid_ready != 0 && ptr->rawtx.size() == 0 && dice_betspent((char *)"dicefinish",ptr->bettxid) <= 0 && ptr->txid == zeroid )
                        {
                            unstringbits(name,ptr->sbits);
                            result = 0;
                            res = DiceBetFinish(ptr->funcid,entropyused,entropyvout,&result,0,name,ptr->fundingtxid,ptr->bettxid,ptr->iswin,utxos[m].txid,utxos[m].vout);
                            if ( ptr->entropyused == zeroid )
                            {
                                ptr->entropyused = entropyused;
                                ptr->entropyvout = entropyvout;
                            }
                            if ( entropyused != ptr->entropyused || entropyvout != ptr->entropyvout )
                            {
                                fprintf(stderr,"WARNING entropy %s != %s || %d != %d\n",entropyused.GetHex().c_str(),ptr->entropyused.GetHex().c_str(),entropyvout,ptr->entropyvout);
                            }
                            if ( result > 0 )
                            {
                                ptr->rawtx = res;
                                mySenddicetransaction(ptr->rawtx,ptr->entropyused,ptr->entropyvout,ptr->bettxid,ptr->betTx,ptr->funcid,ptr);
                            }
                            else if ( result != -2 )
                            {
                                fprintf(stderr,"error doing the dicefinish %d of %d process %s %s using %s/v%d need %.8f\n",m,n,iter<0?"loss":"win",ptr->bettxid.GetHex().c_str(),utxos[m].txid.GetHex().c_str(),utxos[m].vout,(double)(iter<0 ? 0 : ptr->winamount)/COIN);
                                if ( ptr->rawtx.empty() == 0 )
                                    ptr->rawtx.clear();
                                ptr->txid = zeroid;
                            }
                            if ( ++m >= n )
                                break;
                        }
                        else
                        {
                            //fprintf(stderr,"error ready.%d dicefinish %d of %d process %s %s using need %.8f finish.%s size.%d betspent.%d\n",ptr->bettxid_ready,m,n,iter<0?"loss":"win",ptr->bettxid.GetHex().c_str(),(double)(iter<0 ? 0 : ptr->winamount)/COIN,ptr->txid.GetHex().c_str(),(int32_t)ptr->rawtx.size(),dice_betspent((char *)"dicefinish",ptr->bettxid));
                        }
                    }
                } else if ( system("cc/dapps/sendmany100") != 0 )
                    fprintf(stderr,"error issing cc/dapps/sendmany100\n");
                free(utxos);
            }
        }
        if ( (newht= KOMODO_INSYNC) == 0 || newht == lastheight )
            sleep(3);
        else usleep(500000);
    }
    return(0);
}

void DiceQueue(int32_t iswin,uint64_t sbits,uint256 fundingtxid,uint256 bettxid,CTransaction betTx,int32_t entropyvout)
{
    static int32_t didinit;
    struct dicefinish_info *ptr; int32_t i,duplicate=0; uint64_t txfee = 10000;
    if ( didinit == 0 )
    {
        if ( pthread_create((pthread_t *)malloc(sizeof(pthread_t)),NULL,dicefinish,0) == 0 )
        {
            pthread_mutex_init(&DICE_MUTEX,NULL);
            pthread_mutex_init(&DICEREVEALED_MUTEX,NULL);
            didinit = 1;
        }
        else
        {
            fprintf(stderr,"error launching dicefinish thread\n");
            return;
        }
    }
    //if ( dice_betspent((char *)"DiceQueue",bettxid) != 0 )
    //    return;
    pthread_mutex_lock(&DICE_MUTEX);
    if ( _dicehash_find(bettxid) == 0 )
    {
        _dicehash_add(bettxid);
        ptr = (struct dicefinish_info *)calloc(1,sizeof(*ptr));
        ptr->fundingtxid = fundingtxid;
        ptr->bettxid = bettxid;
        ptr->betTx = betTx;
        ptr->sbits = sbits;
        ptr->iswin = iswin;
        ptr->winamount = betTx.vout[1].nValue * ((betTx.vout[2].nValue - txfee)+1);
        ptr->entropyvout = entropyvout;
        DL_APPEND(DICEFINISH_LIST,ptr);
        fprintf(stderr,"queued %dx iswin.%d %.8f -> %.8f %s\n",(int32_t)(betTx.vout[2].nValue - txfee),iswin,(double)betTx.vout[1].nValue/COIN,(double)ptr->winamount/COIN,bettxid.GetHex().c_str());
    }
    else
    {
        //fprintf(stderr,"DiceQueue status bettxid.%s already in list\n",bettxid.GetHex().c_str());
        //_dicehash_clear(bettxid);
    }
    pthread_mutex_unlock(&DICE_MUTEX);
}

CPubKey DiceFundingPk(CScript scriptPubKey)
{
    CPubKey pk; uint8_t *ptr,*dest; int32_t i;
    if ( scriptPubKey.size() == 35 )
    {
        dest = (uint8_t *)pk.begin();
        for (i=0; i<33; i++)
            dest[i] = scriptPubKey[i+1];
    } else fprintf(stderr,"DiceFundingPk invalid size.%d\n",(int32_t)scriptPubKey.size());
    return(pk);
}

uint256 DiceHashEntropy(uint256 &entropy,uint256 _txidpriv,int32_t vout,int32_t usevout)
{
    int32_t i; uint8_t _entropy[32],_hentropy[32]; bits256 tmp256,txidpub,txidpriv,mypriv,mypub,ssecret,ssecret2; uint256 hentropy;
    memset(&hentropy,0,32);
    endiancpy(txidpriv.bytes,(uint8_t *)&_txidpriv,32);
    if ( usevout != 0 )
    {
        txidpriv.bytes[1] ^= (vout & 0xff);
        txidpriv.bytes[2] ^= ((vout>>8) & 0xff);
    }
    txidpriv.bytes[0] &= 0xf8, txidpriv.bytes[31] &= 0x7f, txidpriv.bytes[31] |= 0x40;
    txidpub = curve25519(txidpriv,curve25519_basepoint9());

    Myprivkey(tmp256.bytes);
    vcalc_sha256(0,mypriv.bytes,tmp256.bytes,32);
    mypriv.bytes[0] &= 0xf8, mypriv.bytes[31] &= 0x7f, mypriv.bytes[31] |= 0x40;
    mypub = curve25519(mypriv,curve25519_basepoint9());

    ssecret = curve25519(mypriv,txidpub);
    ssecret2 = curve25519(txidpriv,mypub);
    if ( memcmp(ssecret.bytes,ssecret2.bytes,32) == 0 )
    {
        vcalc_sha256(0,(uint8_t *)&_entropy,ssecret.bytes,32);
        vcalc_sha256(0,(uint8_t *)&_hentropy,_entropy,32);
        endiancpy((uint8_t *)&entropy,_entropy,32);
        endiancpy((uint8_t *)&hentropy,_hentropy,32);
    }
    else
    {
        for (i=0; i<32; i++)
            fprintf(stderr,"%02x",ssecret.bytes[i]);
        fprintf(stderr," ssecret\n");
        for (i=0; i<32; i++)
            fprintf(stderr,"%02x",ssecret2.bytes[i]);
        fprintf(stderr," ssecret2 dont match\n");
    }
    memset(tmp256.bytes,0,32);
    //char str[65],str2[65];
    //fprintf(stderr,"generated house hentropy.%s <- entropy.%s\n",uint256_str(str,hentropy),uint256_str(str2,entropy));
    return(hentropy);
}

int32_t dice_5nibbles(uint8_t *fivevals)
{
    return(((int32_t)fivevals[0]<<16) + ((int32_t)fivevals[1]<<12) + ((int32_t)fivevals[2]<<8) + ((int32_t)fivevals[3]<<4) + ((int32_t)fivevals[4]));
}

uint64_t DiceCalc(int64_t bet,int64_t vout2,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks,uint256 houseentropy,uint256 bettorentropy)
{
    uint8_t buf[64],_house[32],_bettor[32],_hash[32],hash[32],hash16[64]; uint64_t odds,winnings; arith_uint256 house,bettor; char str[65],str2[65]; int32_t i,modval;
    if ( vout2 <= 10000 )
    {
        fprintf(stderr,"unexpected vout2.%llu\n",(long long)vout2);
        return(0);
    }
    else odds = (vout2 - 10000);
    if ( bet < minbet || bet > maxbet )
    {
        CCerror = strprintf("bet size violation %.8f",(double)bet/COIN);
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return(0);
    }
    if ( odds > maxodds )
    {
        CCerror = strprintf("invalid odds %d, must be <= %d",odds, maxodds);
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return(0);
    }

    endiancpy(buf,(uint8_t *)&houseentropy,32);
    endiancpy(&buf[32],(uint8_t *)&bettorentropy,32);
    vcalc_sha256(0,(uint8_t *)&_house,buf,64);
    endiancpy((uint8_t *)&house,_house,32);

    endiancpy(buf,(uint8_t *)&bettorentropy,32);
    endiancpy(&buf[32],(uint8_t *)&houseentropy,32);
    vcalc_sha256(0,(uint8_t *)&_bettor,buf,64);
    endiancpy((uint8_t *)&bettor,_bettor,32);
    winnings = 0;
    //fprintf(stderr,"calc house entropy %s vs bettor %s\n",uint256_str(str,*(uint256 *)&house),uint256_str(str2,*(uint256 *)&bettor));
    if ( odds > 1 )
    {
        if ( 0 )
        { // old way
            bettor = (bettor / arith_uint256(odds));
            if ( bettor >= house )
                winnings = bet * (odds+1);
            return(winnings);
        }
        if ( odds > 9999 ) // shouldnt happen
            return(0);
        endiancpy(buf,(uint8_t *)&house,32);
        endiancpy(&buf[32],(uint8_t *)&bettor,32);
        vcalc_sha256(0,(uint8_t *)&_hash,buf,64);
        endiancpy(hash,_hash,32);
        for (i=0; i<32; i++)
        {
            hash16[i<<1] = ((hash[i] >> 4) & 0x0f);
            hash16[(i<<1) + 1] = (hash[i] & 0x0f);
        }
        modval = 0;
        for (i=0; i<12; i++)
        {
            modval = dice_5nibbles(&hash16[i*5]);
            if ( modval < 1000000 )
            {
                modval %= 10000;
                break;
            }
        }
        //fprintf(stderr,"modval %d vs %d\n",modval,(int32_t)(10000/(odds+1)));
        if ( modval < 10000/(odds+1) )
            winnings = bet * (odds+1);
    }
    else if ( bettor >= house )
        winnings = bet * (odds+1);
    return(winnings);
}

CScript EncodeDiceFundingOpRet(uint8_t funcid,uint64_t sbits,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks)
{
    CScript opret; uint8_t evalcode = EVAL_DICE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'F' << sbits << minbet << maxbet << maxodds << timeoutblocks);
    return(opret);
}

uint8_t DecodeDiceFundingOpRet(const CScript &scriptPubKey,uint64_t &sbits,int64_t &minbet,int64_t &maxbet,int64_t &maxodds,int64_t &timeoutblocks)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> minbet; ss >> maxbet; ss >> maxodds; ss >> timeoutblocks) != 0 )
    {
        if ( e == EVAL_DICE && f == 'F' )
            return(f);
    }
    return(0);
}

CScript EncodeDiceOpRet(uint8_t funcid,uint64_t sbits,uint256 fundingtxid,uint256 hash,uint256 proof)
{
    CScript opret; uint8_t evalcode = EVAL_DICE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << sbits << fundingtxid << hash << proof);
    return(opret);
}

uint8_t DecodeDiceOpRet(uint256 txid,const CScript &scriptPubKey,uint64_t &sbits,uint256 &fundingtxid,uint256 &hash,uint256 &proof)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid; int64_t minbet,maxbet,maxodds,timeoutblocks;
    //script = (uint8_t *)scriptPubKey.data();
    //fprintf(stderr,"decode %02x %02x %02x\n",script[0],script[1],script[2]);
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 )//&& script[0] == 0x6a )
    {
        script = (uint8_t *)vopret.data();
        if ( script[0] == EVAL_DICE )
        {
            if ( script[1] == 'F' )
            {
                if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> minbet; ss >> maxbet; ss >> maxodds; ss >> timeoutblocks) != 0 )
                {
                    memset(&hash,0,32);
                    fundingtxid = txid;
                    return('F');
                } else fprintf(stderr,"unmarshal error for F\n");
            }
            else if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> fundingtxid; ss >> hash; ss >> proof) != 0 )
            {
                if ( e == EVAL_DICE && (f == 'R' || f == 'B' || f == 'W' || f == 'L' || f == 'T' || f == 'E') )
                    return(f);
                else fprintf(stderr,"mismatched e.%02x f.(%c)\n",e,f);
            }
        } else fprintf(stderr,"script[0] %02x != EVAL_DICE\n",script[0]);
    } else fprintf(stderr,"not enough opret.[%d]\n",(int32_t)vopret.size());
    return(0);
}

uint256 DiceGetEntropy(CTransaction tx,uint8_t reffuncid)
{
    uint256 hash,fundingtxid,proof; uint64_t sbits; int32_t numvouts;
    if ( (numvouts= tx.vout.size()) > 0 && DecodeDiceOpRet(tx.GetHash(),tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid,hash,proof) == reffuncid )
        return(hash);
    else return(zeroid);
}

uint64_t IsDicevout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v,uint64_t refsbits,uint256 reffundingtxid)
{
    char destaddr[64]; uint8_t funcid; int32_t numvouts; uint64_t sbits; uint256 fundingtxid,hash,proof;
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 && (numvouts= tx.vout.size()) > 0 )
        {
            if ( (funcid= DecodeDiceOpRet(tx.GetHash(),tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid,hash,proof)) != 0 && sbits == refsbits && ((funcid == 'F' && tx.GetHash() == reffundingtxid) || fundingtxid == reffundingtxid) )
                return(tx.vout[v].nValue);
        }
    }
    return(0);
}

int64_t DiceAmounts(uint64_t &inputs,uint64_t &outputs,struct CCcontract_info *cp,Eval *eval,const CTransaction &tx,uint64_t refsbits,uint256 reffundingtxid)
{
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; uint64_t assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    inputs = outputs = 0;
    for (i=0; i<numvins; i++)
    {
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("always should find vin, but didnt");
            else
            {
                if ( (assetoshis= IsDicevout(cp,vinTx,(int32_t)tx.vin[i].prevout.n,refsbits,reffundingtxid)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsDicevout(cp,tx,i,refsbits,reffundingtxid)) != 0 )
            outputs += assetoshis;
    }
    return(inputs - outputs);
}

bool DiceIsmine(const CScript scriptPubKey)
{
    char destaddr[64],myaddr[64];
    Getscriptaddress(destaddr,scriptPubKey);
    Getscriptaddress(myaddr,CScript() << Mypubkey() << OP_CHECKSIG);
    return(strcmp(destaddr,myaddr) == 0);
}

int32_t DiceIsWinner(uint256 &entropy,int32_t &entropyvout,uint256 txid,CTransaction tx,CTransaction vinTx,uint256 bettorentropy,uint64_t sbits,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks,uint256 fundingtxid)
{
    uint64_t vinsbits,winnings; uint256 vinproof,vinfundingtxid,hentropy,hentropy2; uint8_t funcid;
    //char str[65],str2[65];
    if ( vinTx.vout.size() > 1 && DiceIsmine(vinTx.vout[1].scriptPubKey) != 0 && vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( ((funcid= DecodeDiceOpRet(txid,vinTx.vout[vinTx.vout.size()-1].scriptPubKey,vinsbits,vinfundingtxid,hentropy,vinproof)) == 'E' || funcid == 'W' || funcid == 'L') && sbits == vinsbits && fundingtxid == vinfundingtxid )
        {
            hentropy2 = DiceHashEntropy(entropy,vinTx.vin[0].prevout.hash,vinTx.vin[0].prevout.n,0);
            entropyvout = vinTx.vin[0].prevout.n;
            //fprintf(stderr,"bettxid %s -> vin0 %s/v%d -> %s\n",txid.GetHex().c_str(),vinTx.vin[0].prevout.hash.GetHex().c_str(),entropyvout,entropy.GetHex().c_str());
            if ( hentropy != hentropy2 )
            {
                hentropy2 = DiceHashEntropy(entropy,vinTx.vin[0].prevout.hash,vinTx.vin[0].prevout.n,1);
                //fprintf(stderr,"alt bettxid %s -> vin0 %s/v%d -> %s\n",txid.GetHex().c_str(),vinTx.vin[0].prevout.hash.GetHex().c_str(),entropyvout,entropy.GetHex().c_str());
            }
            if ( hentropy == hentropy2 )
            {
                winnings = DiceCalc(tx.vout[1].nValue,tx.vout[2].nValue,minbet,maxbet,maxodds,timeoutblocks,entropy,bettorentropy);
                //char str[65]; fprintf(stderr,"%s winnings %.8f bet %.8f at odds %d:1\n",uint256_str(str,tx.GetHash()),(double)winnings/COIN,(double)tx.vout[1].nValue/COIN,(int32_t)(tx.vout[2].nValue-10000));
                //fprintf(stderr,"I am house entropy %.8f entropy.(%s) vs %s -> winnings %.8f\n",(double)vinTx.vout[0].nValue/COIN,uint256_str(str,entropy),uint256_str(str2,hash),(double)winnings/COIN);
                if ( winnings == 0 )
                {
                    // queue 'L' losing tx
                    return(-1);
                }
                else
                {
                    // queue 'W' winning tx
                    return(1);
                }
            }
            else
            {
                fprintf(stderr,"both hentropy != hentropy2\n");
            }
        } else fprintf(stderr,"funcid.%c sbits %llx vs %llx cmp.%d\n",funcid,(long long)sbits,(long long)vinsbits,fundingtxid == vinfundingtxid);
    } //else fprintf(stderr,"notmine.%d or not CC.%d\n",DiceIsmine(vinTx.vout[1].scriptPubKey) != 0,vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() != 0);
    return(0);
}

bool DiceVerifyTimeout(CTransaction &betTx,int32_t timeoutblocks)
{
    int32_t numblocks;
    if ( CCduration(numblocks,betTx.GetHash()) <= 0 )
        return(false);
    return(numblocks >= timeoutblocks);
}

bool DiceValidate(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx, uint32_t nIn)
{
    uint256 txid,fundingtxid,vinfundingtxid,vinhentropy,vinproof,hashBlock,hash,proof,entropy; int64_t minbet,maxbet,maxodds,timeoutblocks,odds,winnings; uint64_t vinsbits,refsbits=0,sbits,amount,inputs,outputs,txfee=10000; int32_t numvins,entropyvout,numvouts,preventCCvins,preventCCvouts,i,iswin; uint8_t funcid; CScript fundingPubKey; CTransaction fundingTx,vinTx,vinofvinTx; char CCaddr[64];
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        txid = tx.GetHash();
        if ( (funcid=  DecodeDiceOpRet(txid,tx.vout[numvouts-1].scriptPubKey,refsbits,fundingtxid,hash,proof)) != 0 )
        {
            if ( eval->GetTxUnconfirmed(fundingtxid,fundingTx,hashBlock) == 0 )
                return eval->Invalid("cant find fundingtxid");
            else if ( fundingTx.vout.size() > 0 && DecodeDiceFundingOpRet(fundingTx.vout[fundingTx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) != 'F' )
                return eval->Invalid("fundingTx not valid");
            if ( maxodds > 9999 )
                return eval->Invalid("maxodds too big");
            fundingPubKey = fundingTx.vout[1].scriptPubKey;
            if ( sbits != refsbits )
            {
                fprintf(stderr,"VALIDATION ERROR: sbits %llx != refsbits %llx\n",(long long)sbits,(long long)refsbits);
                //return eval->Invalid("mismatched diceplan");
            }
            switch ( funcid )
            {
                case 'F':
                    //vins.*: normal inputs
                    //vout.0: CC vout for funding
                    //vout.1: normal marker vout for easy searching
                    //vout.2: normal change
                    //vout.n-1: opreturn 'F' sbits minbet maxbet maxodds timeoutblocks
                    return eval->Invalid("unexpected DiceValidate for createfunding");
                    break;
                case 'E': // check sig of vin to match fundingtxid in the 'B' tx
                    //vins.*: normal inputs
                    //vout.0: CC vout for locked entropy funds
                    //vout.1: tag to owner address for entropy funds
                    //vout.2: normal change
                    //vout.n-1: opreturn 'E' sbits fundingtxid hentropy
                    return eval->Invalid("unexpected DiceValidate for addfunding entropy");
                    break;
                case 'B':
                    //vin.0: entropy txid from house
                    //vins.1+: normal inputs
                    //vout.0: CC vout for locked entropy
                    //vout.1: CC vout for locked bet
                    //vout.2: tag for bettor's address (txfee + odds)
                    //vout.3: change
                    //vout.n-1: opreturn 'B' sbits fundingtxid entropy
                    preventCCvouts = 2;
                    preventCCvins = 1;
                    if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
                        return eval->Invalid("vin.0 is normal for bet");
                    else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("vout.0 is normal for bet");
                    else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("vout.1 is normal for bet");
                    else if ( eval->GetTxUnconfirmed(tx.vin[0].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin.0, but didnt for bet");
                    else if ( vinTx.vout[1].scriptPubKey != fundingPubKey )
                        return eval->Invalid("entropy tx not fundingPubKey for bet");
                    else if ( ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,(int64_t)vinTx.vout[tx.vin[0].prevout.n].nValue) == 0 )
                    {
                        fprintf(stderr,"%s prevout.%d %.8f\n",tx.vin[0].prevout.hash.GetHex().c_str(),(int32_t)tx.vin[0].prevout.n,(double)vinTx.vout[tx.vin[0].prevout.n].nValue/COIN);
                        return eval->Invalid("vout[0] != entropy nValue for bet");
                    }
                    else if ( ConstrainVout(tx.vout[1],1,cp->unspendableCCaddr,0) == 0 )
                        return eval->Invalid("vout[1] constrain violation for bet");
                    else if ( tx.vout[2].nValue > txfee+maxodds || tx.vout[2].nValue <= txfee )
                        return eval->Invalid("vout[2] nValue violation for bet");
                    else if ( eval->GetTxUnconfirmed(vinTx.vin[0].prevout.hash,vinofvinTx,hashBlock) == 0 || vinofvinTx.vout.size() < 1 )
                        return eval->Invalid("always should find vinofvin.0, but didnt for bet");
                    else if ( vinTx.vin[0].prevout.hash != fundingtxid )
                    {
                        if ( (int32_t)vinTx.vin[0].prevout.n < 0 || vinofvinTx.vout[vinTx.vin[0].prevout.n].scriptPubKey != fundingPubKey )
                        {
                            uint8_t *ptr0,*ptr1; int32_t i; char str[65],addr0[64],addr1[64];
                            Getscriptaddress(addr0,vinofvinTx.vout[vinTx.vin[0].prevout.n].scriptPubKey);
                            Getscriptaddress(addr1,fundingPubKey);
                            if ( strcmp(addr0,addr1) != 0 )
                            {
                                fprintf(stderr,"%s != %s betTx.%s\n",addr0,addr1,uint256_str(str,txid));
                                fprintf(stderr,"entropyTx.%s v%d\n",uint256_str(str,tx.vin[0].prevout.hash),(int32_t)tx.vin[0].prevout.n);
                                fprintf(stderr,"entropyTx vin0 %s v%d\n",uint256_str(str,vinTx.vin[0].prevout.hash),(int32_t)vinTx.vin[0].prevout.n);
                                ptr0 = (uint8_t *)&vinofvinTx.vout[vinTx.vin[0].prevout.n].scriptPubKey[0];
                                ptr1 = (uint8_t *)&fundingPubKey[0];
                                for (i=0; i<vinofvinTx.vout[vinTx.vin[0].prevout.n].scriptPubKey.size(); i++)
                                    fprintf(stderr,"%02x",ptr0[i]);
                                fprintf(stderr," script vs ");
                                for (i=0; i<fundingPubKey.size(); i++)
                                    fprintf(stderr,"%02x",ptr1[i]);
                                fprintf(stderr," (%c) entropy vin.%d fundingPubKey mismatch %s\n",funcid,vinTx.vin[0].prevout.n,uint256_str(str,vinTx.vin[0].prevout.hash));
                                return eval->Invalid("vin1 of entropy tx not fundingPubKey for bet");
                            }
                        }
                    }
                    if ( (iswin= DiceIsWinner(entropy,entropyvout,txid,tx,vinTx,hash,sbits,minbet,maxbet,maxodds,timeoutblocks,fundingtxid)) != 0 )
                    {
                        // will only happen for fundingPubKey
                        if ( KOMODO_INSYNC != 0 && KOMODO_DEALERNODE != 0 )
                            DiceQueue(iswin,sbits,fundingtxid,txid,tx,entropyvout);
                    }
                    else
                    {
                        //fprintf(stderr,"why does node1 get VALIDATION ERROR: invalid dicebet bettxid %s\n",txid.GetHex().c_str());
                        //return eval->Invalid("invalid dicebet bettxid");
                    }
                    break;
                    // make sure all funding txid are from matching sbits and fundingtxid!!
                case 'L':
                case 'W':
                case 'T':
                    //vin.0: normal input
                    //vin.1: betTx CC vout.0 entropy from bet
                    //vin.2: betTx CC vout.1 bet amount from bet
                    //vin.3+: funding CC vout.0 from 'F', 'E', 'W', 'L' or 'T'
                    //vout.1: tag to owner address for entropy funds
                    preventCCvouts = 1;
                    DiceAmounts(inputs,outputs,cp,eval,tx,sbits,fundingtxid);
                    if ( IsCCInput(tx.vin[1].scriptSig) == 0 || IsCCInput(tx.vin[2].scriptSig) == 0 )
                        return eval->Invalid("vin0 or vin1 normal vin for bet");
                    else if ( tx.vin[1].prevout.hash != tx.vin[2].prevout.hash )
                        return eval->Invalid("vin0 != vin1 prevout.hash for bet");
                    else if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin.0, but didnt for wlt");
                    else if ( vinTx.vout.size() < 3 || DecodeDiceOpRet(tx.vin[1].prevout.hash,vinTx.vout[vinTx.vout.size()-1].scriptPubKey,vinsbits,vinfundingtxid,vinhentropy,vinproof) != 'B' )
                        return eval->Invalid("not betTx for vin0/1 for wlt");
                    else if ( sbits != vinsbits || fundingtxid != vinfundingtxid )
                        return eval->Invalid("sbits or fundingtxid mismatch for wlt");
                    else if ( fundingPubKey != tx.vout[1].scriptPubKey )
                        return eval->Invalid("tx.vout[1] != fundingPubKey for wlt");
                    if ( funcid == 'L' )
                    {
                        //vout.0: funding CC to entropy owner
                        //vout.n-1: opreturn 'L' sbits fundingtxid hentropy proof
                        if ( ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,inputs) == 0 )
                            return eval->Invalid("vout[0] != inputs-txfee for loss");
                        else if ( tx.vout[2].scriptPubKey != fundingPubKey )
                        {
                            if ( tx.vout[2].scriptPubKey.size() == 0 || tx.vout[2].scriptPubKey[0] != 0x6a )
                                return eval->Invalid("vout[2] not send to fundingPubKey for loss");
                        }
                        iswin = -1;
                    }
                    else
                    {
                        //vout.0: funding CC change to entropy owner
                        //vout.2: normal output to bettor's address
                        //vout.n-1: opreturn 'W' sbits fundingtxid hentropy proof
                        odds = vinTx.vout[2].nValue - txfee;
                        if ( ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,0) == 0 )
                            return eval->Invalid("vout[0] != inputs-txfee for win/timeout");
                        else if ( tx.vout[2].scriptPubKey != vinTx.vout[2].scriptPubKey )
                            return eval->Invalid("vout[2] scriptPubKey mismatch for win/timeout");
                        else if ( tx.vout[2].nValue != (odds+1)*vinTx.vout[1].nValue )
                            return eval->Invalid("vout[2] payut mismatch for win/timeout");
                        else if ( inputs != (outputs + tx.vout[2].nValue) && inputs != (outputs + tx.vout[2].nValue+txfee) )
                        {
                            fprintf(stderr,"inputs %.8f != outputs %.8f (%.8f %.8f %.8f %.8f)\n",(double)inputs/COIN,(double)outputs/COIN,(double)tx.vout[0].nValue/COIN,(double)tx.vout[1].nValue/COIN,(double)tx.vout[2].nValue/COIN,(double)tx.vout[3].nValue/COIN);
                            return eval->Invalid("CC funds mismatch for win/timeout");
                        }
                        else if ( tx.vout[3].scriptPubKey != fundingPubKey )
                        {
                            if ( tx.vout[3].scriptPubKey.size() == 0 || tx.vout[3].scriptPubKey[0] != 0x6a )
                                return eval->Invalid("vout[3] not send to fundingPubKey for win/timeout");
                        }
                        iswin = (funcid == 'W');
                    }
                    if ( iswin != 0 )
                    {
                        //char str[65],str2[65];
                        entropy = DiceGetEntropy(vinTx,'B');
                        vcalc_sha256(0,(uint8_t *)&hash,(uint8_t *)&proof,32);
                        //fprintf(stderr,"calculated house hentropy.%s\n",uint256_str(str,hash));
                        //fprintf(stderr,"verify house entropy %s vs bettor %s\n",uint256_str(str,proof),uint256_str(str2,entropy));
                        winnings = DiceCalc(vinTx.vout[1].nValue,vinTx.vout[2].nValue,minbet,maxbet,maxodds,timeoutblocks,proof,entropy);
                        if ( (winnings == 0 && iswin > 0) || (winnings > 0 && iswin < 0) )
                            return eval->Invalid("DiceCalc mismatch for win/loss");
                    }
                    else if ( DiceVerifyTimeout(vinTx,timeoutblocks) == 0 )
                        return eval->Invalid("invalid timeout claim for timeout");
                    break;
                case 'R':
                    if ( eval->GetTxUnconfirmed(tx.vin[0].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin.0, but didnt for refund");
                    else if ( vinTx.vout[tx.vin[0].prevout.n].scriptPubKey != fundingPubKey )
                    {
                        char fundingaddr[64],cmpaddr[64];
                        Getscriptaddress(fundingaddr,fundingPubKey);
                        Getscriptaddress(cmpaddr,vinTx.vout[tx.vin[0].prevout.n].scriptPubKey);
                        if ( strcmp(cmpaddr,fundingaddr) != 0 )
                        {
                            fprintf(stderr,"cmpaddr.%s != fundingaddr.%s\n",cmpaddr,fundingaddr);
                            return eval->Invalid("vin.0 not from fundingPubKey for refund");
                        }
                    }
                    if ( (rand() % 1000) == 0 )
                        fprintf(stderr,"add more validation for refunds\n");
                    break;
                default:
                    fprintf(stderr,"illegal dice funcid.(%c)\n",funcid);
                    return eval->Invalid("unexpected dice funcid");
                    break;
            }
        } else return eval->Invalid("unexpected dice missing funcid");
        return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
    }
    return(true);
}

uint64_t AddDiceInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint64_t total,int32_t maxinputs,uint64_t refsbits,uint256 reffundingtxid)
{
    char coinaddr[64],str[65]; uint64_t threshold,sbits,nValue,totalinputs = 0; uint256 txid,hash,proof,hashBlock,fundingtxid; CTransaction tx; int32_t j,vout,n = 0; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    if ( maxinputs > CC_MAXVINS )
        maxinputs = CC_MAXVINS;
    if ( maxinputs > 0 )
        threshold = total / maxinputs;
    else threshold = total;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( vout != 0 || it->second.satoshis < threshold )
            continue;
        //fprintf(stderr,"(%s) %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        for (j=0; j<mtx.vin.size(); j++)
            if ( txid == mtx.vin[j].prevout.hash && vout == mtx.vin[j].prevout.n )
                break;
        if ( j != mtx.vin.size() )
            continue;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout.size() > 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
        {
            if ( (funcid= DecodeDiceOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid,hash,proof)) != 0 )
            {
                char str[65],sstr[16];
                unstringbits(sstr,sbits);
                if ( sbits == refsbits && (funcid == 'F' && reffundingtxid == txid) || reffundingtxid == fundingtxid )
                {
                    if ( funcid == 'R' || funcid == 'F' || funcid == 'E' || funcid == 'W' || funcid == 'L' || funcid == 'T' )
                    {
                        if ( total != 0 && maxinputs != 0 )
                        {
                            if ( funcid == 'R' )
                                fprintf(stderr,">>>>>>>>>>>> use (%c) %.8f %s %s/v%d\n",funcid,(double)tx.vout[0].nValue/COIN,sstr,uint256_str(str,txid),vout);
                            mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                        }
                        totalinputs += it->second.satoshis;
                        n++;
                        if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                            break;
                    }
                }
            } else fprintf(stderr,"null funcid\n");
        }
    }
    return(totalinputs);
}

int64_t DicePlanFunds(uint64_t &entropyval,uint256 &entropytxid,uint64_t refsbits,struct CCcontract_info *cp,CPubKey dicepk,uint256 reffundingtxid, int32_t &entropytxs,bool random)
{
    char coinaddr[64],str[65]; uint64_t sbits; int64_t nValue,sum,totalinputs = 0; uint256 hash,txid,proof,hashBlock,fundingtxid; CScript fundingPubKey; CTransaction tx,vinTx; int32_t vout,first=0,n=0,i=0,pendingbets=0; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    entropyval = 0;
    entropytxid = zeroid;
    if ( myGetTransaction(reffundingtxid,tx,hashBlock) != 0 && tx.vout.size() > 1 && ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,0) != 0 )
    {
        fundingPubKey = tx.vout[1].scriptPubKey;
    } else return(0);
    GetCCaddress(cp,coinaddr,dicepk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    entropyval = 0;
    int loops = 0;
    int numtxs = unspentOutputs.size()/2;
    int startfrom = rand() % (numtxs+1);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( vout != 0 )
            continue;
        sum += it->second.satoshis;
        loops++;
        if (random) {
            if ( loops < startfrom )
                continue;
            if ( (rand() % 100) < 90 )
                continue;
        }
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 )
        {
            if ( (funcid= DecodeDiceOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid,hash,proof)) != 0 && sbits == refsbits )
            {
                if ( funcid == 'B' )
                {
                    pendingbets++;
                    fprintf(stderr,"%d: %s/v%d (%c %.8f) %.8f %.8f\n",n,uint256_str(str,txid),vout,funcid,(double)it->second.satoshis/COIN,(double)totalinputs/COIN,(double)sum/COIN);
                }
                if ( (funcid == 'F' && reffundingtxid == txid) || reffundingtxid == fundingtxid )
                {
                    //fprintf(stderr,"%d: %s/v%d (%c %.8f) %.8f %.8f\n",n,uint256_str(str,txid),vout,funcid,(double)it->second.satoshis/COIN,(double)totalinputs/COIN,(double)sum/COIN);
                    if ( (nValue= IsDicevout(cp,tx,vout,refsbits,reffundingtxid)) >= 10000 && (funcid == 'R' || funcid == 'F' || funcid == 'E' || funcid == 'W' || funcid == 'L' || funcid == 'T')  )
                    {
                        if ( funcid == 'L' || funcid == 'W' || funcid == 'E' )
                            n++;
                        totalinputs += nValue;
                        if ( first == 0 && (funcid == 'E' || funcid == 'W' || funcid == 'L') )
                        {
                            //fprintf(stderr,"check first\n");
                            if ( tx.vout.size() > 1 && fundingPubKey == tx.vout[1].scriptPubKey )
                            {
                                if ( myGetTransaction(tx.vin[0].prevout.hash,vinTx,hashBlock) == 0 || (int32_t)tx.vin[0].prevout.n < 0 )
                                {
                                    fprintf(stderr,"cant find entropy vin0 %s or vin0prev %d vouts[%d], iscoinbase.%d\n",uint256_str(str,tx.vin[0].prevout.hash),(int32_t)tx.vin[0].prevout.n,(int32_t)vinTx.vout.size(),(int32_t)vinTx.vin.size());
                                    continue;
                                }
                                if ( (int32_t)vinTx.vin[0].prevout.n < 0 || vinTx.vout.size() < 2 )
                                {
                                    fprintf(stderr,"skip coinbase or strange entropy tx\n");
                                    continue;
                                }
                                //if ( fundingtxid != tx.vin[0].prevout.hash && vinTx.vout[tx.vin[0].prevout.n].scriptPubKey != fundingPubKey )
                                if ( fundingtxid != tx.vin[0].prevout.hash && vinTx.vout[1].scriptPubKey != fundingPubKey )
                                {
                                    uint8_t *ptr0,*ptr1; int32_t i; char str[65],addr0[64],addr1[64];
                                    Getscriptaddress(addr0,vinTx.vout[1].scriptPubKey);
                                    Getscriptaddress(addr1,fundingPubKey);
                                    if ( strcmp(addr0,addr1) != 0 )
                                    {
                                        ptr0 = (uint8_t *)&vinTx.vout[1].scriptPubKey[0];
                                        ptr1 = (uint8_t *)&fundingPubKey[0];
                                        for (i=0; i<vinTx.vout[1].scriptPubKey.size(); i++)
                                            fprintf(stderr,"%02x",ptr0[i]);
                                        fprintf(stderr," script vs ");
                                        for (i=0; i<fundingPubKey.size(); i++)
                                            fprintf(stderr,"%02x",ptr1[i]);
                                        fprintf(stderr," (%c) entropy vin.%d fundingPubKey mismatch %s %s vs %s\n",funcid,1,uint256_str(str,tx.vin[0].prevout.hash),addr0,addr1);
                                        continue;
                                    }
                                }
                                if ( myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
                                {
                                    entropytxid = txid;
                                    entropyval = tx.vout[0].nValue;
                                    //fprintf(stderr,"funcid.%c first.%d entropytxid.%s val %.8f\n",funcid,first,txid.GetHex().c_str(),(double)entropyval/COIN);
                                    first = 1;
                                    if (random) {
                                        fprintf(stderr, "chosen entropy on loop: %d\n",loops);
                                    }
                                }
                            }
                            else
                            {
                                uint8_t *ptr0,*ptr1; int32_t i; char str[65];
                                const CScript &s0 = tx.vout[1].scriptPubKey;
                                for (i=0; i<tx.vout[1].scriptPubKey.size(); i++)
                                    fprintf(stderr,"%02x",s0[i]);
                                fprintf(stderr," script vs ");
                                for (i=0; i<fundingPubKey.size(); i++)
                                    fprintf(stderr,"%02x",fundingPubKey[i]);
                                fprintf(stderr," (%c) tx vin.%d fundingPubKey mismatch %s\n",funcid,tx.vin[0].prevout.n,uint256_str(str,tx.vin[0].prevout.hash));
                            }
                        }
                    }
                    else if ( 0 && funcid != 'B' )
                        fprintf(stderr,"%s %c refsbits.%llx sbits.%llx nValue %.8f\n",uint256_str(str,txid),funcid,(long long)refsbits,(long long)sbits,(double)nValue/COIN);
                } //else fprintf(stderr,"else case funcid (%c) %d %s vs %s\n",funcid,funcid,uint256_str(str,reffundingtxid),uint256_str(str2,fundingtxid));
            } //else fprintf(stderr,"funcid.%d %c skipped %.8f\n",funcid,funcid,(double)tx.vout[vout].nValue/COIN);
        } i = i + 1;
    }
    if (!random) {
        fprintf(stderr,"pendingbets.%d numentropy tx %d: %.8f\n",pendingbets,n,(double)totalinputs/COIN);
        entropytxs = n;
        return(totalinputs);
    } else {
        return(0);
    }
    //fprintf(stderr,"numentropy tx %d: %.8f\n",n,(double)totalinputs/COIN);
    entropytxs = n;
    return(totalinputs);
}

bool DicePlanExists(CScript &fundingPubKey,uint256 &fundingtxid,struct CCcontract_info *cp,uint64_t refsbits,CPubKey dicepk,int64_t &minbet,int64_t &maxbet,int64_t &maxodds,int64_t &timeoutblocks)
{
    char CCaddr[64]; uint64_t sbits=0; uint256 txid,hashBlock; CTransaction tx;
    std::vector<uint256> txids;
    GetCCaddress(cp,CCaddr,dicepk);
    SetCCtxids(txids,cp->normaladdr,false,cp->evalcode,zeroid,'F');
    if ( fundingtxid != zeroid ) // avoid scan unless creating new funding plan
    {
        //fprintf(stderr,"check fundingtxid\n");
        if ( myGetTransaction(fundingtxid,tx,hashBlock) != 0 && tx.vout.size() > 1 && ConstrainVout(tx.vout[0],1,CCaddr,0) != 0 )
        {
            if ( DecodeDiceFundingOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) == 'F' && sbits == refsbits )
            {
                fundingPubKey = tx.vout[1].scriptPubKey;
                return(true);
            } else fprintf(stderr,"error decoding opret or sbits mismatch %llx vs %llx\n",(long long)sbits,(long long)refsbits);
        } else fprintf(stderr,"couldnt get funding tx\n");
        return(false);
    }
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( fundingtxid != zeroid && txid != fundingtxid )
            continue;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout.size() > 0 && ConstrainVout(tx.vout[0],1,CCaddr,0) != 0 )
        {
            if ( DecodeDiceFundingOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) == 'F' )
            {
                if ( sbits == refsbits )
                {
                    fundingPubKey = tx.vout[1].scriptPubKey;
                    fundingtxid = txid;
                    return(true);
                }
            }
        }
    }
    return(false);
}

struct CCcontract_info *Diceinit(CScript &fundingPubKey,uint256 reffundingtxid,struct CCcontract_info *C,char *planstr,uint64_t &txfee,CPubKey &mypk,CPubKey &dicepk,uint64_t &sbits,int64_t &minbet,int64_t &maxbet,int64_t &maxodds,int64_t &timeoutblocks)
{
    struct CCcontract_info *cp; int32_t cmpflag;
    cp = CCinit(C,EVAL_DICE);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    dicepk = GetUnspendable(cp,0);
    sbits = stringbits(planstr);
    if ( reffundingtxid == zeroid )
        cmpflag = 0;
    else cmpflag = 1;
    if ( DicePlanExists(fundingPubKey,reffundingtxid,cp,sbits,dicepk,minbet,maxbet,maxodds,timeoutblocks) != cmpflag )
    {
        fprintf(stderr,"Dice plan (%s) exists.%d vs cmpflag.%d\n",planstr,!cmpflag,cmpflag);
        return(0);
    }
    return(cp);
}

UniValue DiceInfo(uint256 diceid)
{
    UniValue result(UniValue::VOBJ); CPubKey dicepk; uint256 hashBlock,entropytxid; CTransaction vintx; int64_t minbet,maxbet,maxodds,timeoutblocks; uint64_t sbits,funding,entropyval; char str[67],numstr[65]; struct CCcontract_info *cp,C;
    if ( myGetTransaction(diceid,vintx,hashBlock) == 0 )
    {
        fprintf(stderr,"cant find fundingtxid\n");
        ERR_RESULT("cant find fundingtxid");
        return(result);
    }
    if ( vintx.vout.size() > 0 && DecodeDiceFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) == 0 )
    {
        fprintf(stderr,"fundingtxid isnt dice creation txid\n");
        ERR_RESULT("fundingtxid isnt dice creation txid");
        return(result);
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("fundingtxid",uint256_str(str,diceid)));
    unstringbits(str,sbits);
    result.push_back(Pair("name",str));
    result.push_back(Pair("sbits",sbits));
    sprintf(numstr,"%.8f",(double)minbet/COIN);
    result.push_back(Pair("minbet",numstr));
    sprintf(numstr,"%.8f",(double)maxbet/COIN);
    result.push_back(Pair("maxbet",numstr));
    result.push_back(Pair("maxodds",maxodds));
    result.push_back(Pair("timeoutblocks",timeoutblocks));
    cp = CCinit(&C,EVAL_DICE);
    dicepk = GetUnspendable(cp,0);
    int32_t entropytxs;
    funding = DicePlanFunds(entropyval,entropytxid,sbits,cp,dicepk,diceid,entropytxs,false);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    result.push_back(Pair("entropytxs",entropytxs));
    return(result);
}

UniValue DiceList()
{
    UniValue result(UniValue::VARR); std::vector<uint256> txids; struct CCcontract_info *cp,C; uint256 txid,hashBlock; CTransaction vintx; uint64_t sbits; int64_t minbet,maxbet,maxodds,timeoutblocks; char str[65];
    cp = CCinit(&C,EVAL_DICE);
    SetCCtxids(txids,cp->normaladdr,false,cp->evalcode,zeroid,'F');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodeDiceFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) != 0 )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

std::string DiceCreateFunding(uint64_t txfee,char *planstr,int64_t funds,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    uint256 zero; CScript fundingPubKey; CPubKey mypk,dicepk; int64_t a,b,c,d; uint64_t sbits; struct CCcontract_info *cp,C;
    if ( funds < 0 || minbet < 0 || maxbet < 0 || maxodds < 1 || maxodds > 9999 || timeoutblocks < 0 || timeoutblocks > 1440 )
    {
        CCerror = "invalid parameter error";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( funds < 100*COIN )
    {
        CCerror = "dice plan needs at least 100 coins";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    memset(&zero,0,sizeof(zero));
    if ( (cp= Diceinit(fundingPubKey,zero,&C,planstr,txfee,mypk,dicepk,sbits,a,b,c,d)) == 0 )
    {
        CCerror = "Diceinit error in create funding, is your transaction confirmed?";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,funds+3*txfee,60) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,funds,dicepk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(dicepk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeDiceFundingOpRet('F',sbits,minbet,maxbet,maxodds,timeoutblocks)));
    }
    CCerror = "cant find enough inputs";
    fprintf(stderr,"%s\n", CCerror.c_str() );
    return("");
}

std::string DiceAddfunding(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CScript fundingPubKey,scriptPubKey; uint256 entropy,hentropy; CPubKey mypk,dicepk; uint64_t sbits; struct CCcontract_info *cp,C; int64_t minbet,maxbet,maxodds,timeoutblocks;
    if ( amount < 0 )
    {
        CCerror = "amount must be positive";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( (cp= Diceinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,dicepk,sbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 ) {
        CCerror = "Diceinit error in add funding, is your transaction confirmed?";
        return("");
    }
    scriptPubKey = CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG;
    if ( 0 )
    {
        uint8_t *ptr0,*ptr1; int32_t i;
        for (i=0; i<35; i++)
            fprintf(stderr,"%02x",scriptPubKey[i]);
        fprintf(stderr," script vs ");
        for (i=0; i<35; i++)
            fprintf(stderr,"%02x",fundingPubKey[i]);
        fprintf(stderr," funding\n");
    }
    if ( scriptPubKey == fundingPubKey )
    {
        if ( AddNormalinputs2(mtx,amount+2*txfee,60) > 0 )
        {
            hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash,mtx.vin[0].prevout.n,1);
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,dicepk));
            mtx.vout.push_back(CTxOut(txfee,fundingPubKey));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeDiceOpRet('E',sbits,fundingtxid,hentropy,zeroid)));
        } else {
            CCerror = "cant find enough inputs";
            fprintf(stderr,"%s\n", CCerror.c_str() );
        }
    } else {
        CCerror = "only fund creator can add more funds (entropy)";
        fprintf(stderr,"%s\n", CCerror.c_str() );
    }
    return("");
}

std::string DiceBet(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t bet,int32_t odds)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CScript fundingPubKey; CPubKey mypk,dicepk; uint64_t sbits,entropyval,entropyval2; int64_t funding,minbet,maxbet,maxodds,timeoutblocks; uint256 entropytxid,entropytxid2,entropy,hentropy; struct CCcontract_info *cp,C;
    if ( bet < 0 )
    {
        CCerror = "bet must be positive";
        return("");
    }
    if ( odds < 2 || odds > 9999 )
    {
        CCerror = "odds must be between 2 and 9999";
        return("");
    }
    if ( (cp= Diceinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,dicepk,sbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 ) {
        CCerror = "Diceinit error in bet, is your transaction confirmed?";
        return("");
    }
    if ( bet < minbet || bet > maxbet || odds > maxodds )
    {
        CCerror = strprintf("Dice plan %s illegal bet %.8f: minbet %.8f maxbet %.8f or odds %d vs max.%d\n",planstr,(double)bet/COIN,(double)minbet/COIN,(double)maxbet/COIN,(int32_t)odds,(int32_t)maxodds);
        return("");
    }
    int32_t entropytxs=0,emptyvar=0;
    funding = DicePlanFunds(entropyval,entropytxid,sbits,cp,dicepk,fundingtxid,entropytxs,false);
    DicePlanFunds(entropyval2,entropytxid2,sbits,cp,dicepk,fundingtxid,emptyvar,true);
    if ( entropyval2 != 0 && entropytxid2 != zeroid )
    {
        entropyval = entropyval2;
        entropytxid = entropytxid2;
    }
    if ( funding >= 2*bet*odds+txfee && entropyval != 0 )
    {
        if ( entropytxs < 100 ) {
            CCerror = "Your dealer is broke, find a new casino.";
            return("");
        }
        if ( myIsutxo_spentinmempool(ignoretxid,ignorevin,entropytxid,0) != 0 )
        {
            CCerror = "entropy txid is spent";
            return("");
        }
        mtx.vin.push_back(CTxIn(entropytxid,0,CScript()));
        if ( AddNormalinputs(mtx,mypk,bet+2*txfee+odds,60) > 0 )
        {
            hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash,mtx.vin[0].prevout.n,1);
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,entropyval,dicepk));
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,bet,dicepk));
            mtx.vout.push_back(CTxOut(txfee+odds,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeDiceOpRet('B',sbits,fundingtxid,entropy,zeroid)));
        } else CCerror = "cant find enough normal inputs for %.8f, plan funding %.8f\n";
    }
    if ( entropyval == 0 && funding != 0 )
        CCerror = "cant find dice entropy inputs";
    else
        CCerror = "cant find dice input";
    return("");
}

std::string DiceBetFinish(uint8_t &funcid,uint256 &entropyused,int32_t &entropyvout,int32_t *resultp,uint64_t txfee,char *planstr,uint256 fundingtxid,uint256 bettxid,int32_t winlosetimeout,uint256 vin0txid,int32_t vin0vout)
{
    CMutableTransaction savemtx,mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CScript scriptPubKey,fundingPubKey; CTransaction oldbetTx,betTx,entropyTx; uint256 hentropyproof,entropytxid,hashBlock,bettorentropy,entropy,hentropy,oldbettxid; CPubKey mypk,dicepk,fundingpk; struct CCcontract_info *cp,C; int64_t inputs=0,CCchange=0,odds,fundsneeded,minbet,maxbet,maxodds,timeoutblocks; int32_t oldentropyvout,retval=0,iswin=0; uint64_t entropyval,sbits;
    entropyused = zeroid;
    *resultp = 0;
    funcid = 0;
    if ( (cp= Diceinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,dicepk,sbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 )
    {
        CCerror = "Diceinit error in finish, is your transaction confirmed?";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    fundingpk = DiceFundingPk(fundingPubKey);
    scriptPubKey = CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG;
    if ( winlosetimeout != 0 ) // must be dealernode
    {
        if ( scriptPubKey != fundingPubKey )
        {
            //fprintf(stderr,"only dice fund creator can submit winner or loser\n");
            winlosetimeout = 0;
        }
    }
    if ( myGetTransaction(bettxid,betTx,hashBlock) != 0 && myGetTransaction(betTx.vin[0].prevout.hash,entropyTx,hashBlock) != 0 )
    {
        entropytxid = betTx.vin[0].prevout.hash;
        /*if ( dice_betspent((char *)"DiceBetFinish",bettxid) != 0 )
        {
            CCerror = "bettxid already spent";
            fprintf(stderr,"%s\n", CCerror.c_str() );
            return("");
        }*/
        bettorentropy = DiceGetEntropy(betTx,'B');
        if ( winlosetimeout == 0 || (iswin= DiceIsWinner(hentropyproof,entropyvout,bettxid,betTx,entropyTx,bettorentropy,sbits,minbet,maxbet,maxodds,timeoutblocks,fundingtxid)) != 0 )
        {
            if ( vin0txid == zeroid || vin0vout < 0 )
            {
                if ( AddNormalinputs2(mtx,2*txfee,3) == 0 ) // must be a single vin!!
                {
                    CCerror = "no txfee inputs for win/lose";
                    fprintf(stderr,"%s\n", CCerror.c_str() );
                    return("");
                }
            }
            else
            {
                //fprintf(stderr,"use vin0 %s/%d\n",vin0txid.GetHex().c_str(),vin0vout);
                mtx.vin.push_back(CTxIn(vin0txid,vin0vout,CScript()));
            }
            if ( winlosetimeout != 0 ) // dealernode
            {
                entropyused = hentropyproof;
                if ( vin0vout == -2 )
                    retval = -1;
                /*if ( iswin == 0 )
                {
                    retval = -1;
                    fprintf(stderr,"invalid dicebet %s\n",bettxid.GetHex().c_str());
                } else retval = 0;*/
                if ( retval < 0 || (retval= DiceEntropyUsed(oldbetTx,oldbettxid,oldentropyvout,entropyused,bettxid,betTx,entropyvout)) != 0 )
                {
                    if ( retval < 0 )
                    {
                        fprintf(stderr,"orphan that reveals entropy, generate refund tx with proofs\n");
                        // make sure we dont refund wrong amounts
                        mtx.vin.push_back(CTxIn(bettxid,0,CScript()));
                        mtx.vin.push_back(CTxIn(bettxid,1,CScript()));
                        funcid = 'R';
                        mtx.vout.push_back(MakeCC1vout(cp->evalcode,betTx.vout[0].nValue,dicepk));
                        //mtx.vout.push_back(CTxOut(betTx.vout[0].nValue,fundingPubKey));
                        mtx.vout.push_back(CTxOut(txfee,fundingPubKey));
                        mtx.vout.push_back(CTxOut(betTx.vout[1].nValue,betTx.vout[2].scriptPubKey));
                        *resultp = 1;
                        return(FinalizeCCTx(0,cp,mtx,fundingpk,txfee,EncodeDiceOpRet(funcid,sbits,fundingtxid,entropyused,oldbettxid))); // need to change opreturn to include oldbetTx to allow validation
                    }
                    else
                    {
                        CCerror = "DiceBetFinish: duplicate betTx";
                        *resultp = -2; // demote error to warning
                    }
                    //fprintf(stderr,"%s\n", CCerror.c_str() );
                    return("");
                }
                //fprintf(stderr,"set winlosetimeout %d <- %d\n",winlosetimeout,iswin);
                if ( (winlosetimeout= iswin) > 0 )
                    funcid = 'W';
                else funcid = 'L';
            }
            if ( iswin == winlosetimeout ) // dealernode and normal node paths should always get here
            {
                //fprintf(stderr,"iswin.%d matches\n",iswin);
                mtx.vin.push_back(CTxIn(bettxid,0,CScript()));
                mtx.vin.push_back(CTxIn(bettxid,1,CScript()));
                if ( iswin == 0 && funcid != 'L' && funcid != 'W' ) // normal node path
                {
                    if ( DiceVerifyTimeout(betTx,timeoutblocks) == 0 ) // hasnt timed out yet
                    {
                        return("");
                    }
                    else
                    {
                        funcid = 'T';
                        hentropy = hentropyproof = zeroid;
                        iswin = 1;
                        fprintf(stderr,"set timeout win T\n");
                    }
                }
                if ( iswin > 0 && funcid != 0 ) // dealernode 'W' or normal node 'T' path
                {
                    odds = (betTx.vout[2].nValue - txfee);
                    if ( odds < 1 || odds > maxodds )
                    {
                        CCerror = strprintf("illegal odds.%d vs maxodds.%d\n",(int32_t)odds,(int32_t)maxodds);
                        fprintf(stderr,"%s\n", CCerror.c_str() );
                        return("");
                    }
                    CCchange = betTx.vout[0].nValue + betTx.vout[1].nValue;
                    fundsneeded = txfee + (odds+1)*betTx.vout[1].nValue;
                    savemtx = mtx;
                    if ( CCchange >= fundsneeded )
                        CCchange -= fundsneeded;
                    else if ( (inputs= AddDiceInputs(cp,mtx,dicepk,fundsneeded,1,sbits,fundingtxid)) >= fundsneeded )
                    {
                        if ( inputs > fundsneeded )
                            CCchange += (inputs - fundsneeded);
                    }
                    else
                    {
                        mtx = savemtx;
                        if ( (inputs= AddDiceInputs(cp,mtx,dicepk,fundsneeded,60,sbits,fundingtxid)) > 0 )
                        {
                            if ( inputs > fundsneeded )
                                CCchange += (inputs - fundsneeded);
                        }
                        else
                        {
                            CCerror = strprintf("not enough inputs for %.8f\n",(double)fundsneeded/COIN);
                            fprintf(stderr,"%s\n", CCerror.c_str() );
                            return("");
                        }
                    }
                    mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,dicepk));
                    mtx.vout.push_back(CTxOut(txfee,fundingPubKey));
                    mtx.vout.push_back(CTxOut((odds+1) * betTx.vout[1].nValue,betTx.vout[2].scriptPubKey));
                }
                else // dealernode 'L' path
                {
                    funcid = 'L';
                    mtx.vout.push_back(MakeCC1vout(cp->evalcode,betTx.vout[0].nValue + betTx.vout[1].nValue,dicepk));
                    mtx.vout.push_back(CTxOut(txfee,fundingPubKey));
                }
                //fprintf(stderr,"make tx.%c\n",funcid);
                if ( funcid == 'L' || funcid == 'W' ) // dealernode only
                    hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash,mtx.vin[0].prevout.n,1);
                else
                {
                    if ( scriptPubKey != betTx.vout[2].scriptPubKey )
                    {
                        CCerror = strprintf("can only finish your own bettxid\n");
                        fprintf(stderr,"%s\n", CCerror.c_str() );
                        return("");
                    }
                }
                *resultp = 1;
                //char str[65],str2[65];
                //fprintf(stderr,"iswin.%d house entropy %s vs bettor %s\n",iswin,uint256_str(str,hentropyproof),uint256_str(str2,bettorentropy));
                return(FinalizeCCTx(0,cp,mtx,fundingpk,txfee,EncodeDiceOpRet(funcid,sbits,fundingtxid,hentropy,hentropyproof)));
            } else fprintf(stderr,"iswin.%d does not match.%d\n",iswin,winlosetimeout);
        }
        else
        {
            *resultp = -1;
            fprintf(stderr,"iswin.%d winlosetimeout.%d\n",iswin,winlosetimeout);
            return("");
        }
    }
    *resultp = -1;
    return("couldnt find bettx or entropytx");
}

static uint256 dealer0_fundingtxid;
void *dealer0_loop(void *_arg)
{
    char *planstr = (char *)_arg;
    CTransaction tx,*entropytxs,entropytx; CPubKey mypk,dicepk; uint64_t entropyval; uint256 hashBlock,entropytxid,txid; int32_t height,lastht,numentropytxs,i,n,m,num; CScript fundingPubKey; struct CCcontract_info *cp,C; char coinaddr[64]; std::string res; int64_t minbet,maxbet,maxodds,timeoutblocks; uint64_t refsbits,txfee = 10000;
    if ( (cp= Diceinit(fundingPubKey,dealer0_fundingtxid,&C,planstr,txfee,mypk,dicepk,refsbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 )
    {
        fprintf(stderr,"error initializing dealer0_loop\n");
        StartShutdown();
    }
    fprintf(stderr,"dealer0 node running\n");
    height = lastht = 0;
    entropytxs = (CTransaction *)calloc(sizeof(*entropytxs),DICE_MINUTXOS);
    while ( 1 )
    {
        while ( KOMODO_INSYNC == 0 || (height= KOMODO_INSYNC) == lastht )
        {
            sleep(3);
        }
        lastht = height;
        fprintf(stderr,"New height.%d\n",height);
        DicePlanFunds(entropyval,entropytxid,refsbits,cp,dicepk,dealer0_fundingtxid,numentropytxs,false);
        if ( numentropytxs < DICE_MINUTXOS )
        {
            n = sqrt(DICE_MINUTXOS - numentropytxs);
            //if ( n > 10 )
            //    n = 10;
            for (i=m=0; i<DICE_MINUTXOS - numentropytxs && i<n; i++)
            {
                res = DiceAddfunding(txfee,planstr,dealer0_fundingtxid,COIN);
                if ( res.empty() == 0 && res.size() > 64 && is_hexstr((char *)res.c_str(),0) > 64 )
                {
                    if ( DecodeHexTx(tx,res) != 0 )
                    {
                        LOCK(cs_main);
                        if ( myAddtomempool(tx) != 0 )
                        {
                            fprintf(stderr,"ENTROPY %s: %d of %d, %d\n",tx.GetHash().GetHex().c_str(),i,n,DICE_MINUTXOS - numentropytxs);
                            RelayTransaction(tx);
                            entropytxs[m++] = tx;
                        } else break;
                    } else break;
                } else break;
            }
            for (i=0; i<m; i++)
            {
                tx = entropytxs[i];
                txid = tx.GetHash();
                fprintf(stderr,"check %d of %d: %s\n",i,m,txid.GetHex().c_str());
                while ( 1 )
                {
                    if ( myGetTransaction(txid,entropytx,hashBlock) == 0 || hashBlock == zeroid )
                    {
                        LOCK(cs_main);
                        if ( myAddtomempool(tx) != 0 )
                        {
                            fprintf(stderr,"resend ENTROPY %s: %d of %d\n",txid.GetHex().c_str(),i,m);
                            RelayTransaction(tx);
                        }
                    }
                    else
                    {
                        fprintf(stderr,"found %s in %s\n",txid.GetHex().c_str(),hashBlock.GetHex().c_str());
                        break;
                    }
                    sleep(10);
                }
            }
        }
        pubkey2addr(coinaddr,Mypubkey().data());
        dicefinish_utxosget(num,0,0,coinaddr);
        fprintf(stderr,"have %d 0.0002 utxos, need %d\n",num,DICE_MINUTXOS);
        if ( num < DICE_MINUTXOS ) // this deadlocks, need to put it in a different thread
        {
            char *cmd = (char *)malloc(100 * 128);
            sprintf(cmd,"./komodo-cli -ac_name=%s sendmany \"\"  \"{\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002,\\\"%s\\\":0.0002}\"",ASSETCHAINS_SYMBOL,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr,coinaddr);
            n = sqrt((DICE_MINUTXOS - num) / 100)*2 + 1;
            fprintf(stderr,"num normal 0.0002 utxos.%d < %d -> n.%d\n",num,DICE_MINUTXOS,n);
            for (i=0; i<n; i++)
            {
                fprintf(stderr,"%d of %d: ",i,n);
                if ( system(cmd) != 0 )
                    fprintf(stderr,"system error issuing.(%s)\n",cmd);
            }
            free(cmd);
            if ( (rand() % 100) == 0 )
            {
                fprintf(stderr,"make 0.023 utxos\n");
                if ( system("cc/dapps/sendmany") != 0 )
                    fprintf(stderr,"system error issuing.(cc/dapps/sendmany)\n");
            }
        }
        sleep(60);
    }
    return(0);
}

double DiceStatus(uint64_t txfee,char *planstr,uint256 fundingtxid,uint256 bettxid)
{
    static int32_t didinit; static char _planstr[64];
    CScript fundingPubKey,scriptPubKey; CTransaction spenttx,betTx,entropyTx; uint256 hentropyproof,entropyused,hash,proof,txid,hashBlock,spenttxid,bettorentropy; CPubKey mypk,dicepk,fundingpk; struct CCcontract_info *cp,C; int32_t i,entropyvout,flag,win,num,loss,duplicate=0,result,iswin,vout,n=0; int64_t minbet,maxbet,maxodds,timeoutblocks,sum=0; uint64_t sbits,refsbits; char coinaddr[64]; std::string res; uint8_t funcid;
    if ( (cp= Diceinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,dicepk,refsbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 )
    {
        CCerror = "Diceinit error in status, is your transaction confirmed?";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return(0.);
    }
    win = loss = 0;
    fundingpk = DiceFundingPk(fundingPubKey);
    scriptPubKey = CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG;
    GetCCaddress(cp,coinaddr,dicepk);
    if ( bettxid == zeroid ) // scan
    {
        if ( fundingpk != mypk )
        {
            CCerror = "Diceinit error in status, non-dealer must provide bettxid";
            fprintf(stderr,"%s\n", CCerror.c_str() );
            return(0.);
        }
        std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
        SetCCunspents(unspentOutputs,coinaddr,true);
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
        {
            txid = it->first.txhash;
            vout = (int32_t)it->first.index;
            if ( vout != 0 )
                continue;
            sum += it->second.satoshis;
            if ( myGetTransaction(txid,betTx,hashBlock) != 0 && betTx.vout.size() >= 4 && betTx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 )
            {
                if ( DecodeDiceOpRet(txid,betTx.vout[betTx.vout.size()-1].scriptPubKey,sbits,fundingtxid,hash,proof) == 'B' && sbits == refsbits )
                {
                    if ( myGetTransaction(betTx.vin[0].prevout.hash,entropyTx,hashBlock) != 0 )
                    {
                        flag = KOMODO_DEALERNODE != 0;
                        if ( KOMODO_DEALERNODE != 0 && scriptPubKey == fundingPubKey )
                        {
                            bettorentropy = DiceGetEntropy(betTx,'B');
                            if ( (iswin= DiceIsWinner(hentropyproof,entropyvout,txid,betTx,entropyTx,bettorentropy,sbits,minbet,maxbet,maxodds,timeoutblocks,fundingtxid)) != 0 )
                            {
                                if ( iswin > 0 )
                                    win++;
                                else if ( iswin < 0 )
                                    loss++;
                                n++;
                                DiceQueue(iswin,sbits,fundingtxid,txid,betTx,entropyvout);
                            }
                        }
                        if ( scriptPubKey != fundingPubKey )
                        {
                            fprintf(stderr,"serialized bettxid %d: iswin.%d W.%d L.%d %s/v%d (%c %.8f) %.8f\n",n,iswin,win,loss,txid.GetHex().c_str(),vout,funcid,(double)it->second.satoshis/COIN,(double)sum/COIN);
                            res = DiceBetFinish(funcid,entropyused,entropyvout,&result,txfee,planstr,fundingtxid,txid,scriptPubKey == fundingPubKey,zeroid,-1);
                            if ( result > 0 )
                            {
                                mySenddicetransaction(res,entropyused,entropyvout,txid,betTx,funcid,0);
                                n++;
                                if ( n > 10 )
                                    break;
                            }
                        }
                    } else fprintf(stderr,"bettxid.%s cant find entropyTx.%s\n",txid.GetHex().c_str(),betTx.vin[0].prevout.hash.GetHex().c_str());
                }
            }
        }
        if ( didinit == 0 && KOMODO_DEALERNODE == 0 && scriptPubKey == fundingPubKey )
        {
            strcpy(_planstr,planstr);
            dealer0_fundingtxid = fundingtxid;
            if ( pthread_create((pthread_t *)malloc(sizeof(pthread_t)),NULL,dealer0_loop,_planstr) == 0 )
                didinit = 1;
        }
        return(n);
    }
    else
    {
        char str[65];
        if ( (vout= myIsutxo_spent(spenttxid,bettxid,1)) >= 0 )
        {
            //fprintf(stderr,"bettx is spent\n");
            if ( myGetTransaction(bettxid,betTx,hashBlock) != 0 && myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 2 )
            {
                //fprintf(stderr,"found spenttxid %s\n",uint256_str(str,spenttxid));
                if ( betTx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 || betTx.vout[2].scriptPubKey.IsPayToCryptoCondition() != 0 || spenttx.vout[2].scriptPubKey != betTx.vout[2].scriptPubKey )
                    return(0.);
                else return((double)spenttx.vout[2].nValue/COIN);
            }
            CCerror = "couldnt find bettx or spenttx %s\n",uint256_str(str,spenttxid);
            return(-1.);
        }
        else if ( scriptPubKey == fundingPubKey )
            res = DiceBetFinish(funcid,entropyused,entropyvout,&result,txfee,planstr,fundingtxid,bettxid,1,zeroid,-1);
        else res = DiceBetFinish(funcid,entropyused,entropyvout,&result,txfee,planstr,fundingtxid,bettxid,0,zeroid,-1);
        if ( result > 0 )
        {
            mySenddicetransaction(res,entropyused,entropyvout,bettxid,betTx,funcid,0);
            sleep(1);
            if ( (vout= myIsutxo_spent(spenttxid,bettxid,1)) >= 0 )
            {
                if ( myGetTransaction(txid,betTx,hashBlock) != 0 && myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() >= 2 )
                {
                    if ( funcid == 'L' )//betTx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 || betTx.vout[2].scriptPubKey.IsPayToCryptoCondition() != 0 || spenttx.vout[2].scriptPubKey != betTx.vout[2].scriptPubKey )
                    //if ( spenttx.vout[2].scriptPubKey == fundingPubKey || ((uint8_t *)spenttx.vout[2].scriptPubKey.data())[0] == 0x6a )
                        return(0.);
                    else return((double)spenttx.vout[2].nValue/COIN);
                } else return(0.);
            }
            CCerror = "didnt find dicefinish tx";
        } else CCerror = res;
        return(-1.);
    }
    return(0.);
}
