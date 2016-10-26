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

// Todo: handle reorg: clear all entries above reorged height

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>

void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,uint32_t *pvals,uint8_t numpvals,int32_t kheight,uint64_t opretvalue,uint8_t *opretbuf,uint16_t opretlen);
void komodo_init();
int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp);
char *komodo_issuemethod(char *method,char *params,uint16_t port);

int32_t NOTARIZED_HEIGHT,Num_nutxos,KMDHEIGHT = 34000;
uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID;
pthread_mutex_t komodo_mutex;
char KMDUSERPASS[1024]; uint16_t BITCOIND_PORT = 7771;

#include "komodo_utils.h"
#include "cJSON.c"
#include "komodo_bitcoind.h"
#include "komodo_interest.h"
#include "komodo_pax.h"
#include "komodo_notary.h"
#include "komodo_gateway.h"

void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,uint32_t *pvals,uint8_t numpvals,int32_t KMDheight,uint64_t opretvalue,uint8_t *opretbuf,uint16_t opretlen)
{
    static FILE *fp; static int32_t errs; char fname[512]; int32_t ht,func; uint8_t num,pubkeys[64][33];
#ifdef WIN32
    sprintf(fname,"%s\\%s",GetDataDir(false).string().c_str(),(char *)"komodostate");
#else
    sprintf(fname,"%s/%s",GetDataDir(false).string().c_str(),(char *)"komodostate");
#endif
    if ( fp == 0 )
    {
        if ( (fp= fopen(fname,"rb+")) != 0 )
        {
            while ( (func= fgetc(fp)) != EOF )
            {
                if ( fread(&ht,1,sizeof(ht),fp) != sizeof(ht) )
                    errs++;
                //printf("func.(%d %c) ht.%d\n",func,func,ht);
                if ( func == 'P' )
                {
                    if ( (num= fgetc(fp)) < 64 )
                    {
                        if ( fread(pubkeys,33,num,fp) != num )
                            errs++;
                        else
                        {
                            printf("updated %d pubkeys at ht.%d\n",num,ht);
                            komodo_notarysinit(ht,pubkeys,num);
                        }
                    } else printf("illegal num.%d\n",num);
                    //printf("P[%d]\n",num);
                }
                else if ( func == 'N' )
                {
                    if ( fread(&NOTARIZED_HEIGHT,1,sizeof(NOTARIZED_HEIGHT),fp) != sizeof(NOTARIZED_HEIGHT) )
                        errs++;
                    if ( fread(&NOTARIZED_HASH,1,sizeof(NOTARIZED_HASH),fp) != sizeof(NOTARIZED_HASH) )
                        errs++;
                    if ( fread(&NOTARIZED_DESTTXID,1,sizeof(NOTARIZED_DESTTXID),fp) != sizeof(NOTARIZED_DESTTXID) )
                        errs++;
                    printf("load NOTARIZED %d %s\n",NOTARIZED_HEIGHT,NOTARIZED_HASH.ToString().c_str());
                    komodo_notarized_update(ht,NOTARIZED_HEIGHT,NOTARIZED_HASH,NOTARIZED_DESTTXID);
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
                    komodo_nutxoadd(ht,nid,hash,mask,n);
                }
                else if ( func == 'K' )
                {
                    int32_t kheight;
                    if ( fread(&kheight,1,sizeof(kheight),fp) != sizeof(kheight) )
                        errs++;
                    if ( kheight > KMDHEIGHT )
                    {
                        KMDHEIGHT = kheight;
                        printf("KMDHEIGHT <- %d\n",kheight);
                    }
                }
                else if ( func == 'O' )
                {
                    uint16_t olen; uint64_t ovalue; uint8_t opret[10000];
                    if ( fread(&ovalue,1,sizeof(ovalue),fp) != sizeof(ovalue) )
                        errs++;
                    if ( fread(&olen,1,sizeof(olen),fp) != sizeof(olen) )
                        errs++;
                    if ( olen < sizeof(opret) )
                    {
                        if ( fread(opret,1,olen,fp) != olen )
                            errs++;
                        komodo_opreturn(ht,ovalue,opret,olen);
                    } else printf("illegal olen.%u\n",olen);
                }
                else if ( func == 'D' )
                {
                    //printf("D[%d]\n",ht);
                }
//#ifdef KOMODO_PAX
                else if ( func == 'V' )
                {
                    int32_t numpvals; uint32_t pvals[128];
                    numpvals = fgetc(fp);
                    if ( numpvals*sizeof(uint32_t) <= sizeof(pvals) && fread(pvals,sizeof(uint32_t),numpvals,fp) == numpvals )
                    {
                        komodo_pvals(ht,pvals,numpvals);
                        printf("load pvals ht.%d numpvals.%d\n",ht,numpvals);
                    } else printf("error loading pvals[%d]\n",numpvals);
                }
//#endif
                else printf("illegal func.(%d %c)\n",func,func);
            }
        } else fp = fopen(fname,"wb+");
        printf("fname.(%s) fpos.%ld\n",fname,ftell(fp));
    }
    if ( fp != 0 ) // write out funcid, height, other fields, call side effect function
    {
        if ( height < 0 )
        {
            height = -height;
            //printf("func D[%d] errs.%d\n",height,errs);
            fputc('D',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
        }
        else if ( KMDheight > 0 )
        {
            fputc('K',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            if ( fwrite(&KMDheight,1,sizeof(KMDheight),fp) != sizeof(KMDheight) )
                errs++;
        }
        else if ( opretbuf != 0 && opretlen > 0 )
        {
            uint16_t olen;
            fputc('O',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            if ( fwrite(&opretvalue,1,sizeof(opretvalue),fp) != sizeof(opretvalue) )
                errs++;
            olen = opretlen;
            if ( fwrite(&olen,1,sizeof(olen),fp) != olen )
                errs++;
            if ( fwrite(opretbuf,1,opretlen,fp) != opretlen )
                errs++;
            komodo_opreturn(height,opretvalue,opretbuf,opretlen);
        }
        else if ( notarypubs != 0 && numnotaries > 0 )
        {
            //printf("func P[%d] errs.%d\n",numnotaries,errs);
            fputc('P',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            fputc(numnotaries,fp);
            if ( fwrite(notarypubs,33,numnotaries,fp) != numnotaries )
                errs++;
            komodo_notarysinit(height,notarypubs,numnotaries);
        }
        else if ( voutmask != 0 && numvouts > 0 )
        {
            //printf("func U %d %d errs.%d hashsize.%ld\n",numvouts,notaryid,errs,sizeof(txhash));
            fputc('U',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            fputc(numvouts,fp);
            fputc(notaryid,fp);
            if ( fwrite(&voutmask,1,sizeof(voutmask),fp) != sizeof(voutmask) )
                errs++;
            if ( fwrite(&txhash,1,sizeof(txhash),fp) != sizeof(txhash) )
                errs++;
            komodo_nutxoadd(height,notaryid,txhash,voutmask,numvouts);
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
                komodo_pvals(height,pvals,numpvals);
            }
            //printf("save pvals height.%d numpvals.%d\n",height,numpvals);
        }
//#endif
        else if ( height != 0 )
        {
            //printf("func N ht.%d errs.%d\n",NOTARIZED_HEIGHT,errs);
            fputc('N',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            if ( fwrite(&NOTARIZED_HEIGHT,1,sizeof(NOTARIZED_HEIGHT),fp) != sizeof(NOTARIZED_HEIGHT) )
                errs++;
            if ( fwrite(&NOTARIZED_HASH,1,sizeof(NOTARIZED_HASH),fp) != sizeof(NOTARIZED_HASH) )
                errs++;
            if ( fwrite(&NOTARIZED_DESTTXID,1,sizeof(NOTARIZED_DESTTXID),fp) != sizeof(NOTARIZED_DESTTXID) )
                errs++;
            komodo_notarized_update(height,NOTARIZED_HEIGHT,NOTARIZED_HASH,NOTARIZED_DESTTXID);
        }
        fflush(fp);
    }
}

int32_t komodo_voutupdate(int32_t notaryid,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,uint256 txhash,int32_t i,int32_t j,uint64_t *voutmaskp,int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value)
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
            printf(">>>>>>>> ");
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
                    printf("mismatch notaryid.%d k.%d\n",notaryid,nid);
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
        for (k=0; k<scriptlen; k++)
            printf("%02x",scriptbuf[k]);
        printf(" <- script ht.%d i.%d j.%d\n",height,i,j);
        if ( j == 1 && opretlen >= 32*2+4 && strcmp(KOMODO_SOURCE,(char *)&scriptbuf[len+32*2+4]) == 0 )
        {
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&kmdtxid);
            len += iguana_rwnum(0,&scriptbuf[len],4,(uint8_t *)notarizedheightp);
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&desttxid);
            if ( *notarizedheightp > NOTARIZED_HEIGHT && *notarizedheightp < height )
            {
                printf("ht.%d NOTARIZED.%d KMD.%s BTCTXID.%s (%s)\n",height,*notarizedheightp,kmdtxid.ToString().c_str(),desttxid.ToString().c_str(),(char *)&scriptbuf[len]);
                NOTARIZED_HEIGHT = *notarizedheightp;
                NOTARIZED_HASH = kmdtxid;
                NOTARIZED_DESTTXID = desttxid;
                komodo_stateupdate(height,0,0,0,zero,0,0,0,0,0,0,0,0);
            } else printf("reject ht.%d NOTARIZED.%d %s.%s DESTTXID.%s (%s)\n",height,*notarizedheightp,KOMODO_SOURCE,kmdtxid.ToString().c_str(),desttxid.ToString().c_str(),(char *)&scriptbuf[len]);
        }
        else if ( i == 0 && j == 1 && opretlen == 149 )
            komodo_paxpricefeed(height,&scriptbuf[len],opretlen);
        else
        {
            komodo_stateupdate(0,0,0,0,zero,0,0,0,0,0,value,&scriptbuf[len],opretlen);
        }
    }
    return(notaryid);
}

int32_t komodo_isratify(int32_t isspecial,int32_t numvalid)
{
    if ( isspecial != 0 && numvalid > 13 )
        return(1);
    else return(0);
}

// Special tx have vout[0] -> CRYPTO777
// with more than 13 pay2pubkey outputs -> ratify
// if all outputs to notary -> notary utxo
// if txi == 0 && 2 outputs and 2nd OP_RETURN, len == 32*2+4 -> notarized, 1st byte 'P' -> pricefeed
// OP_RETURN: 'D' -> deposit

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    uint64_t signedmask,voutmask;
    uint8_t scriptbuf[4096],pubkeys[64][33]; uint256 kmdtxid,btctxid,txhash;
    int32_t i,j,k,numvalid,specialtx,notarizedheight,notaryid,len,numvouts,numvins,height,txn_count;
    komodo_init();
#ifdef KOMODO_ISSUER
    komodo_gateway_issuer();
#else
    komodo_gateway_redeemer();
#endif
    if ( pindex != 0 )
    {
        height = pindex->nHeight;
        txn_count = block.vtx.size();
        for (i=0; i<txn_count; i++)
        {
            txhash = block.vtx[i].GetHash();
            numvouts = block.vtx[i].vout.size();
            notaryid = -1;
            voutmask = specialtx = notarizedheight = 0;
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
                    // signedmask is needed here!
                    notaryid = komodo_voutupdate(notaryid,scriptbuf,len,height,txhash,i,j,&voutmask,&specialtx,&notarizedheight,(uint64_t)block.vtx[i].vout[j].nValue);
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
                komodo_stateupdate(height,0,0,notaryid,txhash,voutmask,numvouts,0,0,0,0,0,0);
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
                if ( specialtx != 0 && numvouts > 2 && komodo_ratify_threshold(height,signedmask) > 0 )
                {
                    numvalid = 0;
                    memset(pubkeys,0,sizeof(pubkeys));
                    for (j=1; j<numvouts; j++)
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
                    if ( komodo_isratify(1,numvalid) > 13 )
                    {
                        memset(&txhash,0,sizeof(txhash));
                        komodo_stateupdate(height,pubkeys,numvalid,0,txhash,0,0,0,0,0,0,0,0);
                    }
                    printf("new notaries.%d newheight.%d from height.%d\n",numvouts-1,KOMODO_PUBKEYS_HEIGHT(height),height);
                }
            }
        }
    } else printf("komodo_connectblock: unexpected null pindex\n");
}


#endif
