/******************************************************************************
 * Copyright © 2014-2016 The SuperNET Developers.                             *
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

#ifndef H_KOMODO_H
#define H_KOMODO_H

// Todo:
// 0. optimize assetchains
// 1. error check fiat redeem amounts
// 2. net balance limiter
// 3. verify: reorgs

// non komodod (non-hardfork) todo:
// a. automate notarization fee payouts
// b. automated distribution of test REVS snapshot

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include "uthash.h"
#include "utlist.h"

#define GENESIS_NBITS 0x1f00ffff
#define KOMODO_MINRATIFY 7
#define KOMODO_MAXBLOCKS 5000000

FILE *Minerfp;
int8_t Minerids[KOMODO_MAXBLOCKS]; // 5 million blocks
#define KOMODO_ELECTION_GAP ((ASSETCHAINS_SYMBOL[0] == 0) ? 2000 : 100)

struct pax_transaction
{
    UT_hash_handle hh;
    uint256 txid;
    uint64_t komodoshis,fiatoshis;
    int32_t marked,height,otherheight;
    uint16_t vout;
    char symbol[16],coinaddr[64]; uint8_t rmd160[20],shortflag;
};

struct nutxo_entry { UT_hash_handle hh; uint256 txhash; uint64_t voutmask; int32_t notaryid,height; };
struct knotary_entry { UT_hash_handle hh; uint8_t pubkey[33],notaryid; };
struct knotaries_entry { int32_t height,numnotaries; struct knotary_entry *Notaries; };
struct notarized_checkpoint { uint256 notarized_hash,notarized_desttxid; int32_t nHeight,notarized_height; };
pthread_mutex_t komodo_mutex;
int32_t KMDHEIGHT = 43000;

struct pax_transaction *PAX;
int32_t NUM_PRICES; uint32_t *PVALS;
struct knotaries_entry *Pubkeys;
struct nutxo_entry *NUTXOS; int32_t Num_nutxos;
struct notarized_checkpoint *NPOINTS; int32_t NUM_NPOINTS;

struct komodo_state
{
    uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID;
    int32_t CURRENT_HEIGHT,NOTARIZED_HEIGHT;
    // gateway state
} KOMODO_STATES[33];

#include "komodo_globals.h"
#include "komodo_utils.h"

#include "cJSON.c"
#include "komodo_bitcoind.h"
#include "komodo_interest.h"
#include "komodo_pax.h"
#include "komodo_notary.h"
#include "komodo_gateway.h"
#include "komodo_events.h"

void komodo_setkmdheight(int32_t kmdheight)
{
    if ( kmdheight > KMDHEIGHT )
        KMDHEIGHT = kmdheight;
}

struct komodo_state *komodo_stateptr(char *symbol,char *dest)
{
    int32_t baseid; struct komodo_state *sp;
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        strcpy(symbol,"KMD");
        strcpy(dest,"BTC");
        sp = &KOMODO_STATES[0];
    }
    else
    {
        strcpy(symbol,ASSETCHAINS_SYMBOL);
        strcpy(dest,"KMD");
        if ( (baseid= komodo_baseid(ASSETCHAINS_SYMBOL)) >= 0 )
            sp = &KOMODO_STATES[baseid];
        else
        {
            fprintf(stderr,"komodo_stateupdate.(%s) not supported\n",ASSETCHAINS_SYMBOL);
            return(0);
        }
    }
    return(sp);
}

void komodo_currentheight_set(int32_t height)
{
    char symbol[16],dest[16]; struct komodo_state *sp;
    if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp->CURRENT_HEIGHT = height;
}

int32_t komodo_currentheight()
{
    char symbol[16],dest[16]; struct komodo_state *sp;
    if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        return(sp->CURRENT_HEIGHT);
    else return(0);
}

int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest)
{
    static int32_t errs;
    char symbol[16]; int32_t func,ht,notarized_height,matched=0; uint256 notarized_hash,notarized_desttxid; uint8_t pubkeys[64][33];
    if ( (func= fgetc(fp)) != EOF )
    {
        if ( ASSETCHAINS_SYMBOL[0] == 0 && strcmp(symbol,"KMD") == 0 )
            matched = 1;
        else matched = (strcmp(symbol,ASSETCHAINS_SYMBOL) == 0);
        if ( fread(&ht,1,sizeof(ht),fp) != sizeof(ht) )
            errs++;
        //printf("fpos.%ld func.(%d %c) ht.%d ",ftell(fp),func,func,ht);
        if ( func == 'P' )
        {
            if ( (num= fgetc(fp)) < 64 )
            {
                if ( fread(pubkeys,33,num,fp) != num )
                    errs++;
                else
                {
                    printf("updated %d pubkeys at ht.%d\n",num,ht);
                    if ( matched != 0 )
                        komodo_eventadd_pubkeys(sp,symbol,ht,num,pubkeys);
                }
            } else printf("illegal num.%d\n",num);
        }
        else if ( func == 'N' )
        {
            if ( fread(&notarized_height,1,sizeof(notarized_height),fp) != sizeof(notarized_height) )
                errs++;
            if ( fread(&notarized_hash,1,sizeof(notarized_hash),fp) != sizeof(notarized_hash) )
                errs++;
            if ( fread(&notarized_desttxid,1,sizeof(notarized_desttxid),fp) != sizeof(notarized_desttxid) )
                errs++;
            printf("load NOTARIZED %d %s\n",notarized_height,notarized_hash.ToString().c_str());
            if ( matched != 0 )
                komodo_eventadd_notarized(sp,symbol,ht,dest,notarized_hash,notarized_desttxid,notarized_height);
        }
        else if ( func == 'U' )
        {
            uint8_t n,nid; uint256 hash; uint64_t mask;
            n = fgetc(fp);
            nid = fgetc(fp);
            //printf("U %d %d\n",n,nid);
            if ( fread(&mask,1,sizeof(mask),fp) != sizeof(mask) )
                errs++;
            if ( fread(&hash,1,sizeof(hash),fp) != sizeof(hash) )
                errs++;
            if ( matched != 0 )
                komodo_eventadd_utxo(sp,symbol,ht,nid,hash,voutmask,n);
        }
        else if ( func == 'K' )
        {
            int32_t kheight;
            if ( fread(&kheight,1,sizeof(kheight),fp) != sizeof(kheight) )
                errs++;
            if ( matched != 0 )
                komodo_eventadd_kmdheight(sp,symbol,ht,kheight);
        }
        else if ( func == 'R' )
        {
            uint16_t olen,v; uint64_t ovalue; uint256 txid; uint8_t opret[10000];
            if ( fread(&txid,1,sizeof(txid),fp) != sizeof(txid) )
                errs++;
            if ( fread(&v,1,sizeof(v),fp) != sizeof(v) )
                errs++;
            if ( fread(&ovalue,1,sizeof(ovalue),fp) != sizeof(ovalue) )
                errs++;
            if ( fread(&olen,1,sizeof(olen),fp) != sizeof(olen) )
                errs++;
            if ( olen < sizeof(opret) )
            {
                if ( fread(opret,1,olen,fp) != olen )
                    errs++;
                if ( matched != 0 )
                    komodo_eventadd_opreturn(sp,symbol,ht,txid,ovalue,v,opret,opretlen);
            } else printf("illegal olen.%u\n",olen);
        }
        else if ( func == 'D' )
        {
            printf("unexpected function D[%d]\n",ht);
        }
        else if ( func == 'V' )
        {
            int32_t numpvals; uint32_t pvals[128];
            numpvals = fgetc(fp);
            if ( numpvals*sizeof(uint32_t) <= sizeof(pvals) && fread(pvals,sizeof(uint32_t),numpvals,fp) == numpvals )
            {
                if ( matched != 0 )
                    komodo_eventadd_pricefeed(sp,symbol,height,pvals,numpvals);
                //printf("load pvals ht.%d numpvals.%d\n",ht,numpvals);
            } else printf("error loading pvals[%d]\n",numpvals);
        }
        else printf("illegal func.(%d %c)\n",func,func);
        return(func);
    } else return(-1);
}
                    
void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,uint32_t *pvals,uint8_t numpvals,int32_t KMDheight,uint64_t opretvalue,uint8_t *opretbuf,uint16_t opretlen,uint16_t vout)
{
    static FILE *fp; static int32_t errs;
    struct komodo_state *sp; char fname[512],fname2[512],symbol[16],dest[16]; int32_t ht,func; uint8_t num,pubkeys[64][33];
    if ( (sp= komodo_stateptr(symbol,dest)) == 0 )
        return;
    if ( fp == 0 )
    {
#ifdef WIN32
        sprintf(fname,"%s\\%s",GetDataDir(false).string().c_str(),(char *)"komodostate");
        sprintf(fname2,"%s\\%s",GetDataDir(false).string().c_str(),(char *)"minerids");
#else
        sprintf(fname,"%s/%s",GetDataDir(false).string().c_str(),(char *)"komodostate");
        sprintf(fname2,"%s/%s",GetDataDir(false).string().c_str(),(char *)"minerids");
#endif
        memset(Minerids,0xfe,sizeof(Minerids));
        if ( (Minerfp= fopen(fname2,"rb+")) == 0 )
        {
            if ( (Minerfp= fopen(fname2,"wb")) != 0 )
            {
                fwrite(Minerids,1,sizeof(Minerids),Minerfp);
                fclose(Minerfp);
            }
            Minerfp = fopen(fname2,"rb+");
        }
        if ( Minerfp != 0 && fread(Minerids,1,sizeof(Minerids),Minerfp) != sizeof(Minerids) )
            printf("read error Minerids\n");
        if ( (fp= fopen(fname,"rb+")) != 0 )
        {
            while ( komodo_parsestatefile(sp,fp,symbol,dest) >= 0 )
                ;
        } else fp = fopen(fname,"wb+");
        printf("fname.(%s) fpos.%ld\n",fname,ftell(fp));
        KOMODO_INITDONE = (uint32_t)time(NULL);
    }
    if ( height <= 0 )
    {
        //printf("early return: stateupdate height.%d\n",height);
        return;
    }
    if ( fp != 0 ) // write out funcid, height, other fields, call side effect function
    {
        //printf("fpos.%ld ",ftell(fp));
        if ( KMDheight > 0 )
        {
            fputc('K',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            if ( fwrite(&KMDheight,1,sizeof(KMDheight),fp) != sizeof(KMDheight) )
                errs++;
            komodo_eventadd_kmdheight(sp,symbol,height,KMDheight);
        }
        else if ( opretbuf != 0 && opretlen > 0 )
        {
            uint16_t olen = opretlen;
            fputc('R',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            if ( fwrite(&txhash,1,sizeof(txhash),fp) != sizeof(txhash) )
                errs++;
            if ( fwrite(&vout,1,sizeof(vout),fp) != sizeof(vout) )
                errs++;
            if ( fwrite(&opretvalue,1,sizeof(opretvalue),fp) != sizeof(opretvalue) )
                errs++;
            if ( fwrite(&olen,1,sizeof(olen),fp) != olen )
                errs++;
            if ( fwrite(opretbuf,1,olen,fp) != olen )
                errs++;
            //printf("ht.%d R opret[%d]\n",height,olen);
            //komodo_opreturn(height,opretvalue,opretbuf,olen,txhash,vout);
            komodo_eventadd_opreturn(sp,symbol,height,txhash,opretvalue,vout,opretbuf,olen);
        }
        else if ( notarypubs != 0 && numnotaries > 0 )
        {
            //printf("ht.%d func P[%d] errs.%d\n",height,numnotaries,errs);
            fputc('P',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            fputc(numnotaries,fp);
            if ( fwrite(notarypubs,33,numnotaries,fp) != numnotaries )
                errs++;
            komodo_eventadd_pubkeys(sp,symbol,height,numnotaries,notarypubs);
        }
        else if ( voutmask != 0 && numvouts > 0 )
        {
            //printf("ht.%d func U %d %d errs.%d hashsize.%ld\n",height,numvouts,notaryid,errs,sizeof(txhash));
            fputc('U',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            fputc(numvouts,fp);
            fputc(notaryid,fp);
            if ( fwrite(&voutmask,1,sizeof(voutmask),fp) != sizeof(voutmask) )
                errs++;
            if ( fwrite(&txhash,1,sizeof(txhash),fp) != sizeof(txhash) )
                errs++;
            komodo_eventadd_utxo(sp,symbol,height,notaryid,txhash,voutmask,numvouts);
            //komodo_nutxoadd(height,notaryid,txhash,voutmask,numvouts);
        }
//#ifdef KOMODO_PAX
        else if ( pvals != 0 && numpvals > 0 )
        {
            int32_t i,nonz = 0;
            for (i=0; i<32; i++)
                if ( pvals[i] != 0 )
                    nonz++;
            if ( nonz >= 32 )
            {
                fputc('V',fp);
                if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                    errs++;
                fputc(numpvals,fp);
                if ( fwrite(pvals,sizeof(uint32_t),numpvals,fp) != numpvals )
                    errs++;
                komodo_eventadd_pricefeed(sp,symbol,height,pvals,numpvals);
                //printf("ht.%d V numpvals[%d]\n",height,numpvals);
            }
            //printf("save pvals height.%d numpvals.%d\n",height,numpvals);
        }
//#endif
        else if ( height != 0 )
        {
            //printf("ht.%d func N ht.%d errs.%d\n",height,NOTARIZED_HEIGHT,errs);
            fputc('N',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            if ( fwrite(&sp->NOTARIZED_HEIGHT,1,sizeof(sp->NOTARIZED_HEIGHT),fp) != sizeof(sp->NOTARIZED_HEIGHT) )
                errs++;
            if ( fwrite(&sp->NOTARIZED_HASH,1,sizeof(sp->NOTARIZED_HASH),fp) != sizeof(sp->NOTARIZED_HASH) )
                errs++;
            if ( fwrite(&sp->NOTARIZED_DESTTXID,1,sizeof(sp->NOTARIZED_DESTTXID),fp) != sizeof(sp->NOTARIZED_DESTTXID) )
                errs++;
            komodo_eventadd_notarized(sp,symbol,height,dest,sp->NOTARIZED_HASH,sp->NOTARIZED_DESTTXID,sp->NOTARIZED_HEIGHT);
            //komodo_notarized_update(height,NOTARIZED_HEIGHT,NOTARIZED_HASH,NOTARIZED_DESTTXID);
        }
        fflush(fp);
    }
}

int32_t komodo_voutupdate(int32_t *isratificationp,int32_t notaryid,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,uint256 txhash,int32_t i,int32_t j,uint64_t *voutmaskp,int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value)
{
    static uint256 zero; int32_t opretlen,nid,k,len = 0; uint256 kmdtxid,desttxid; uint8_t crypto777[33];
    if ( scriptlen == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
    {
        decode_hex(crypto777,33,(char *)CRYPTO777_PUBSECPSTR);
        /*for (k=0; k<33; k++)
            printf("%02x",crypto777[k]);
        printf(" crypto777 ");
        for (k=0; k<scriptlen; k++)
            printf("%02x",scriptbuf[k]);
        printf(" <- script ht.%d i.%d j.%d cmp.%d\n",height,i,j,memcmp(crypto777,scriptbuf+1,33));*/
        if ( memcmp(crypto777,scriptbuf+1,33) == 0 )
        {
            *specialtxp = 1;
            //printf(">>>>>>>> ");
        }
        else if ( komodo_chosennotary(&nid,height,scriptbuf + 1) >= 0 )
        {
            //printf("found notary.k%d\n",k);
            if ( notaryid < 64 )
            {
                if ( notaryid < 0 )
                {
                    notaryid = nid;
                    *voutmaskp |= (1LL << j);
                }
                else if ( notaryid != nid )
                {
                    //for (i=0; i<33; i++)
                    //    printf("%02x",scriptbuf[i+1]);
                    //printf(" %s mismatch notaryid.%d k.%d\n",ASSETCHAINS_SYMBOL,notaryid,nid);
                    notaryid = 64;
                    *voutmaskp = 0;
                }
                else *voutmaskp |= (1LL << j);
            }
        }
    }
    if ( scriptbuf[len++] == 0x6a )
    {
        if ( (opretlen= scriptbuf[len++]) == 0x4c )
            opretlen = scriptbuf[len++];
        else if ( opretlen == 0x4d )
        {
            opretlen = scriptbuf[len++];
            opretlen = (opretlen << 8) + scriptbuf[len++];
        }
        if ( j == 1 && opretlen >= 32*2+4 && strcmp(ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,(char *)&scriptbuf[len+32*2+4]) == 0 )
        {
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&kmdtxid);
            len += iguana_rwnum(0,&scriptbuf[len],4,(uint8_t *)notarizedheightp);
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&desttxid);
            if ( *notarizedheightp > sp->NOTARIZED_HEIGHT && *notarizedheightp < height )
            {
                printf("ht.%d NOTARIZED.%d %s.%s %sTXID.%s (%s)\n",height,*notarizedheightp,ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,kmdtxid.ToString().c_str(),ASSETCHAINS_SYMBOL[0]==0?"BTC":"KMD",desttxid.ToString().c_str(),(char *)&scriptbuf[len]);
                sp->NOTARIZED_HEIGHT = *notarizedheightp;
                sp->NOTARIZED_HASH = kmdtxid;
                sp->NOTARIZED_DESTTXID = desttxid;
                komodo_stateupdate(height,0,0,0,zero,0,0,0,0,0,0,0,0,0);
            } else printf("reject ht.%d NOTARIZED.%d %s.%s DESTTXID.%s (%s)\n",height,*notarizedheightp,ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,kmdtxid.ToString().c_str(),desttxid.ToString().c_str(),(char *)&scriptbuf[len]);
        }
        else if ( i == 0 && j == 1 && opretlen == 149 )
            komodo_paxpricefeed(height,&scriptbuf[len],opretlen);
        else
        {
            //int32_t k; for (k=0; k<scriptlen; k++)
            //    printf("%02x",scriptbuf[k]);
            //printf(" <- script ht.%d i.%d j.%d value %.8f\n",height,i,j,dstr(value));
            if ( opretlen >= 32*2+4 && strcmp(ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,(char *)&scriptbuf[len+32*2+4]) == 0 )
            {
                iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&kmdtxid);
                for (k=0; k<32; k++)
                    if ( scriptbuf[len+k] != 0 )
                        break;
                if ( k == 32 )
                {
                    *isratificationp = 1;
                    printf("ISRATIFICATION (%s)\n",(char *)&scriptbuf[len+32*2+4]);
                }
            }
            if ( *isratificationp == 0 )
                komodo_stateupdate(height,0,0,0,txhash,0,0,0,0,0,value,&scriptbuf[len],opretlen,j);
        }
    }
    return(notaryid);
}

int32_t komodo_isratify(int32_t isspecial,int32_t numvalid)
{
    if ( isspecial != 0 && numvalid >= KOMODO_MINRATIFY )
        return(1);
    else return(0);
}

// Special tx have vout[0] -> CRYPTO777
// with more than KOMODO_MINRATIFY pay2pubkey outputs -> ratify
// if all outputs to notary -> notary utxo
// if txi == 0 && 2 outputs and 2nd OP_RETURN, len == 32*2+4 -> notarized, 1st byte 'P' -> pricefeed
// OP_RETURN: 'D' -> deposit, 'W' -> withdraw
void komodo_currentheight_set(int32_t height);

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    static int32_t hwmheight;
    uint64_t signedmask,voutmask;
    uint8_t scriptbuf[4096],pubkeys[64][33]; uint256 kmdtxid,btctxid,txhash;
    int32_t i,j,k,numnotaries,isratification,nid,numvalid,specialtx,notarizedheight,notaryid,len,numvouts,numvins,height,txn_count;
    komodo_init(pindex->nHeight);
    numnotaries = komodo_notaries(pubkeys,pindex->nHeight);
    if ( pindex->nHeight > hwmheight )
        hwmheight = pindex->nHeight;
    else
    {
        printf("hwmheight.%d vs pindex->nHeight.%d reorg.%d\n",hwmheight,pindex->nHeight,hwmheight-pindex->nHeight);
        // reset komodostate
    }
    komodo_currentheight_set(chainActive.Tip()->nHeight);
    if ( komodo_is_issuer() != 0 )
    {
        while ( KOMODO_REALTIME == 0 || time(NULL) <= KOMODO_REALTIME )
        {
            fprintf(stderr,"komodo_connect.(%s) waiting for realtime RT.%u now.%u\n",ASSETCHAINS_SYMBOL,KOMODO_REALTIME,(uint32_t)time(NULL));
            sleep(30);
        }
    }
    KOMODO_REALTIME = KOMODO_INITDONE = (uint32_t)time(NULL);
    if ( pindex != 0 )
    {
        height = pindex->nHeight;
        txn_count = block.vtx.size();
        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
            printf("%s ht.%d connect txn_count.%d\n",ASSETCHAINS_SYMBOL,height,txn_count);
        for (i=0; i<txn_count; i++)
        {
            txhash = block.vtx[i].GetHash();
            numvouts = block.vtx[i].vout.size();
            notaryid = -1;
            voutmask = specialtx = notarizedheight = isratification = 0;
            for (j=0; j<numvouts; j++)
            {
                len = block.vtx[i].vout[j].scriptPubKey.size();
                if ( len <= sizeof(scriptbuf) )
                {
#ifdef KOMODO_ZCASH
                    memcpy(scriptbuf,block.vtx[i].vout[j].scriptPubKey.data(),len);
#else
                    memcpy(scriptbuf,(uint8_t *)&block.vtx[i].vout[j].scriptPubKey[0],len);
#endif
                    notaryid = komodo_voutupdate(&isratification,notaryid,scriptbuf,len,height,txhash,i,j,&voutmask,&specialtx,&notarizedheight,(uint64_t)block.vtx[i].vout[j].nValue);
                    if ( i == 0 && j == 0 && komodo_chosennotary(&nid,height,scriptbuf + 1) >= 0 )
                    {
                        if ( height < sizeof(Minerids)/sizeof(*Minerids) )
                        {
                            if ( (Minerids[height]= nid) >= -1 )
                            {
                                if ( Minerfp != 0 )
                                {
                                    fseek(Minerfp,height,SEEK_SET);
                                    fputc(Minerids[height],Minerfp);
                                    fflush(Minerfp);
                                }
                            }
                        }
                    }
                    if ( 0 && i > 0 )
                    {
                        for (k=0; k<len; k++)
                            printf("%02x",scriptbuf[k]);
                        printf(" <- notaryid.%d ht.%d i.%d j.%d numvouts.%d numvins.%d voutmask.%llx txid.(%s)\n",notaryid,height,i,j,numvouts,numvins,(long long)voutmask,txhash.ToString().c_str());
                    }
                }
            }
            if ( i != 0 && notaryid >= 0 && notaryid < 64 && voutmask != 0 )
            {
                komodo_stateupdate(height,0,0,notaryid,txhash,voutmask,numvouts,0,0,0,0,0,0,0);
                //komodo_nutxoadd(height,notaryid,txhash,voutmask,numvouts);
            }
            signedmask = 0;
            numvins = block.vtx[i].vin.size();
            for (j=0; j<numvins; j++)
            {
                if ( (k= komodo_nutxofind(height,block.vtx[i].vin[j].prevout.hash,block.vtx[i].vin[j].prevout.n)) >= 0 )
                    signedmask |= (1LL << k);
                else if ( 0 && signedmask != 0 )
                    printf("signedmask.%llx but ht.%d i.%d j.%d not found (%s %d)\n",(long long)signedmask,height,i,j,block.vtx[i].vin[j].prevout.hash.ToString().c_str(),block.vtx[i].vin[j].prevout.n);
            }
            if ( signedmask != 0 && (notarizedheight != 0 || specialtx != 0) )
            {
                printf("NOTARY SIGNED.%llx numvins.%d ht.%d txi.%d notaryht.%d specialtx.%d\n",(long long)signedmask,numvins,height,i,notarizedheight,specialtx);
                printf("ht.%d specialtx.%d isratification.%d numvouts.%d signed.%llx numnotaries.%d\n",height,specialtx,isratification,numvouts,(long long)signedmask,numnotaries);
                if ( specialtx != 0 && isratification != 0 && numvouts > 2 )
                {
                    numvalid = 0;
                    memset(pubkeys,0,sizeof(pubkeys));
                    for (j=1; j<numvouts-1; j++)
                    {
                        len = block.vtx[i].vout[j].scriptPubKey.size();
                        if ( len <= sizeof(scriptbuf) )
                        {
#ifdef KOMODO_ZCASH
                            memcpy(scriptbuf,block.vtx[i].vout[j].scriptPubKey.data(),len);
#else
                            memcpy(scriptbuf,(uint8_t *)&block.vtx[i].vout[j].scriptPubKey[0],len);
#endif
                            if ( len == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
                            {
                                memcpy(pubkeys[numvalid++],scriptbuf+1,33);
                                for (k=0; k<33; k++)
                                    printf("%02x",scriptbuf[k+1]);
                                printf(" <- new notary.[%d]\n",j-1);
                            }
                        }
                    }
                    if ( ((signedmask & 1) != 0 && numvalid >= KOMODO_MINRATIFY) || bitweight(signedmask) > (numnotaries>>1) )
                    {
                        memset(&txhash,0,sizeof(txhash));
                        komodo_stateupdate(height,pubkeys,numvalid,0,txhash,0,0,0,0,0,0,0,0,0);
                        printf("RATIFIED! >>>>>>>>>> new notaries.%d newheight.%d from height.%d\n",numvalid,(((height+KOMODO_ELECTION_GAP/2)/KOMODO_ELECTION_GAP)+1)*KOMODO_ELECTION_GAP,height);
                    }
                }
            }
        }
    } else printf("komodo_connectblock: unexpected null pindex\n");
    KOMODO_INITDONE = (uint32_t)time(NULL);
}


#endif
