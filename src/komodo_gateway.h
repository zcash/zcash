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

// paxdeposit equivalent in reverse makes opreturn and KMD does the same in reverse
#include "komodo_defs.h"

/*#include "secp256k1/include/secp256k1.h"
#include "secp256k1/include/secp256k1_schnorrsig.h"
#include "secp256k1/include/secp256k1_musig.h"

int32_t dummy_linker_tricker()
{
    secp256k1_context *ctx = 0; std::vector<uint8_t> musig64; CPubKey pk; secp256k1_schnorrsig musig; secp256k1_pubkey combined_pk;
    if ( secp256k1_schnorrsig_parse((const secp256k1_context *)ctx,&musig,(const uint8_t *)&musig64[0]) > 0 && secp256k1_ec_pubkey_parse(ctx,&combined_pk,pk.begin(),33) > 0 )
        return(1);
}*/

int32_t MarmaraValidateCoinbase(int32_t height,CTransaction tx);

int32_t pax_fiatstatus(uint64_t *available,uint64_t *deposited,uint64_t *issued,uint64_t *withdrawn,uint64_t *approved,uint64_t *redeemed,char *base)
{
    int32_t baseid; struct komodo_state *sp; int64_t netliability,maxallowed,maxval;
    *available = *deposited = *issued = *withdrawn = *approved = *redeemed = 0;
    if ( (baseid= komodo_baseid(base)) >= 0 )
    {
        if ( (sp= komodo_stateptrget(base)) != 0 )
        {
            *deposited = sp->deposited;
            *issued = sp->issued;
            *withdrawn = sp->withdrawn;
            *approved = sp->approved;
            *redeemed = sp->redeemed;
            maxval = sp->approved;
            if ( sp->withdrawn > maxval )
                maxval = sp->withdrawn;
            netliability = (sp->issued - maxval) - sp->shorted;
            maxallowed = komodo_maxallowed(baseid);
            if ( netliability < maxallowed )
                *available = (maxallowed - netliability);
            //printf("%llu - %llu %s %.8f %.8f %.8f %.8f %.8f\n",(long long)maxallowed,(long long)netliability,base,dstr(*deposited),dstr(*issued),dstr(*withdrawn),dstr(*approved),dstr(*redeemed));
            return(0);
        } else printf("pax_fiatstatus cant get basesp.%s\n",base);
    } // else printf("pax_fiatstatus illegal base.%s\n",base);
    return(-1);
}

void pax_keyset(uint8_t *buf,uint256 txid,uint16_t vout,uint8_t type)
{
    memcpy(buf,&txid,32);
    memcpy(&buf[32],&vout,2);
    buf[34] = type;
}

struct pax_transaction *komodo_paxfind(uint256 txid,uint16_t vout,uint8_t type)
{
    struct pax_transaction *pax; uint8_t buf[35];
    pthread_mutex_lock(&komodo_mutex);
    pax_keyset(buf,txid,vout,type);
    HASH_FIND(hh,PAX,buf,sizeof(buf),pax);
    pthread_mutex_unlock(&komodo_mutex);
    return(pax);
}

struct pax_transaction *komodo_paxfinds(uint256 txid,uint16_t vout)
{
    struct pax_transaction *pax; int32_t i; uint8_t types[] = { 'I', 'D', 'X', 'A', 'W' };
    for (i=0; i<sizeof(types)/sizeof(*types); i++)
        if ( (pax= komodo_paxfind(txid,vout,types[i])) != 0 )
            return(pax);
    return(0);
}

struct pax_transaction *komodo_paxmark(int32_t height,uint256 txid,uint16_t vout,uint8_t type,int32_t mark)
{
    struct pax_transaction *pax; uint8_t buf[35];
    pthread_mutex_lock(&komodo_mutex);
    pax_keyset(buf,txid,vout,type);
    HASH_FIND(hh,PAX,buf,sizeof(buf),pax);
    if ( pax == 0 )
    {
        pax = (struct pax_transaction *)calloc(1,sizeof(*pax));
        pax->txid = txid;
        pax->vout = vout;
        pax->type = type;
        memcpy(pax->buf,buf,sizeof(pax->buf));
        HASH_ADD_KEYPTR(hh,PAX,pax->buf,sizeof(pax->buf),pax);
        //printf("ht.%d create pax.%p mark.%d\n",height,pax,mark);
    }
    if ( pax != 0 )
    {
        pax->marked = mark;
        //if ( height > 214700 || pax->height > 214700 )
        //    printf("mark ht.%d %.8f %.8f\n",pax->height,dstr(pax->komodoshis),dstr(pax->fiatoshis));

    }
    pthread_mutex_unlock(&komodo_mutex);
    return(pax);
}

void komodo_paxdelete(struct pax_transaction *pax)
{
    return; // breaks when out of order
    pthread_mutex_lock(&komodo_mutex);
    HASH_DELETE(hh,PAX,pax);
    pthread_mutex_unlock(&komodo_mutex);
}

void komodo_gateway_deposit(char *coinaddr,uint64_t value,char *symbol,uint64_t fiatoshis,uint8_t *rmd160,uint256 txid,uint16_t vout,uint8_t type,int32_t height,int32_t otherheight,char *source,int32_t approved) // assetchain context
{
    struct pax_transaction *pax; uint8_t buf[35]; int32_t addflag = 0; struct komodo_state *sp; char str[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN],*s;
    //if ( KOMODO_PAX == 0 )
    //    return;
    //if ( strcmp(symbol,ASSETCHAINS_SYMBOL) != 0 )
    //    return;
    sp = komodo_stateptr(str,dest);
    pthread_mutex_lock(&komodo_mutex);
    pax_keyset(buf,txid,vout,type);
    HASH_FIND(hh,PAX,buf,sizeof(buf),pax);
    if ( pax == 0 )
    {
        pax = (struct pax_transaction *)calloc(1,sizeof(*pax));
        pax->txid = txid;
        pax->vout = vout;
        pax->type = type;
        memcpy(pax->buf,buf,sizeof(pax->buf));
        HASH_ADD_KEYPTR(hh,PAX,pax->buf,sizeof(pax->buf),pax);
        addflag = 1;
        if ( 0 && ASSETCHAINS_SYMBOL[0] == 0 )
        {
            int32_t i; for (i=0; i<32; i++)
                printf("%02x",((uint8_t *)&txid)[i]);
            printf(" v.%d [%s] kht.%d ht.%d create pax.%p symbol.%s source.%s\n",vout,ASSETCHAINS_SYMBOL,height,otherheight,pax,symbol,source);
        }
    }
    pthread_mutex_unlock(&komodo_mutex);
    if ( coinaddr != 0 )
    {
        strcpy(pax->coinaddr,coinaddr);
        if ( value != 0 )
            pax->komodoshis = value;
        if ( symbol != 0 )
            strcpy(pax->symbol,symbol);
        if ( source != 0 )
            strcpy(pax->source,source);
        if ( fiatoshis != 0 )
            pax->fiatoshis = fiatoshis;
        if ( rmd160 != 0 )
            memcpy(pax->rmd160,rmd160,20);
        if ( height != 0 )
            pax->height = height;
        if ( otherheight != 0 )
            pax->otherheight = otherheight;
    }
    else
    {
        pax->marked = height;
        //printf("pax.%p MARK DEPOSIT ht.%d other.%d\n",pax,height,otherheight);
    }
}

int32_t komodo_rwapproval(int32_t rwflag,uint8_t *opretbuf,struct pax_transaction *pax)
{
    int32_t i,len = 0;
    if ( rwflag == 1 )
    {
        for (i=0; i<32; i++)
            opretbuf[len++] = ((uint8_t *)&pax->txid)[i];
        opretbuf[len++] = pax->vout & 0xff;
        opretbuf[len++] = (pax->vout >> 8) & 0xff;
    }
    else
    {
        for (i=0; i<32; i++)
            ((uint8_t *)&pax->txid)[i] = opretbuf[len++];
        //for (i=0; i<32; i++)
        //    printf("%02x",((uint8_t *)&pax->txid)[31-i]);
        pax->vout = opretbuf[len++];
        pax->vout += ((uint32_t)opretbuf[len++] << 8);
        //printf(" txid v.%d\n",pax->vout);
    }
    len += iguana_rwnum(rwflag,&opretbuf[len],sizeof(pax->komodoshis),&pax->komodoshis);
    len += iguana_rwnum(rwflag,&opretbuf[len],sizeof(pax->fiatoshis),&pax->fiatoshis);
    len += iguana_rwnum(rwflag,&opretbuf[len],sizeof(pax->height),&pax->height);
    len += iguana_rwnum(rwflag,&opretbuf[len],sizeof(pax->otherheight),&pax->otherheight);
    if ( rwflag != 0 )
    {
        memcpy(&opretbuf[len],pax->rmd160,20), len += 20;
        for (i=0; i<4; i++)
            opretbuf[len++] = pax->source[i];
    }
    else
    {
        memcpy(pax->rmd160,&opretbuf[len],20), len += 20;
        for (i=0; i<4; i++)
            pax->source[i] = opretbuf[len++];
    }
    return(len);
}

int32_t komodo_issued_opreturn(char *base,uint256 *txids,uint16_t *vouts,int64_t *values,int64_t *srcvalues,int32_t *kmdheights,int32_t *otherheights,int8_t *baseids,uint8_t *rmd160s,uint8_t *opretbuf,int32_t opretlen,int32_t iskomodo)
{
    struct pax_transaction p,*pax; int32_t i,n=0,j,len=0,incr,height,otherheight; uint8_t type,rmd160[20]; uint64_t fiatoshis; char symbol[KOMODO_ASSETCHAIN_MAXLEN];
    //if ( KOMODO_PAX == 0 )
    //    return(0);
    incr = 34 + (iskomodo * (2*sizeof(fiatoshis) + 2*sizeof(height) + 20 + 4));
    //41e77b91cb68dc2aa02fa88550eae6b6d44db676a7e935337b6d1392d9718f03cb0200305c90660400000000fbcbeb1f000000bde801006201000058e7945ad08ddba1eac9c9b6c8e1e97e8016a2d152

    // 41e94d736ec69d88c08b5d238abeeca609c02357a8317e0d56c328bcb1c259be5d0200485bc80200000000404b4c000000000059470200b80b000061f22ba7d19fe29ac3baebd839af8b7127d1f9075553440046bb4cc7a3b5cd39dffe7206507a3482a00780e617f68b273cce9817ed69298d02001069ca1b0000000080f0fa02000000005b470200b90b000061f22ba7d19fe29ac3baebd839af8b7127d1f90755

    //for (i=0; i<opretlen; i++)
    //    printf("%02x",opretbuf[i]);
    //printf(" opretlen.%d (%s)\n",opretlen,base);
    //printf(" opretlen.%d vs %d incr.%d (%d)\n",opretlen,(int32_t)(2*sizeof(fiatoshis) + 2*sizeof(height) + 20 + 2),incr,opretlen/incr);
    //if ( ASSETCHAINS_SYMBOL[0] == 0 || strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0 )
    {
        type = opretbuf[0];
        opretbuf++, opretlen--;
        for (n=0; n<opretlen/incr; n++)
        {
            if ( iskomodo != 0 )
            {
                memset(&p,0,sizeof(p));
                len += komodo_rwapproval(0,&opretbuf[len],&p);
                if ( values != 0 && srcvalues != 0 && kmdheights != 0 && otherheights != 0 && baseids != 0 && rmd160s != 0 )
                {
                    txids[n] = p.txid;
                    vouts[n] = p.vout;
                    values[n] = (strcmp("KMD",base) == 0) ? p.komodoshis : p.fiatoshis;
                    srcvalues[n] = (strcmp("KMD",base) == 0) ? p.fiatoshis : p.komodoshis;
                    kmdheights[n] = p.height;
                    otherheights[n] = p.otherheight;
                    memcpy(&rmd160s[n * 20],p.rmd160,20);
                    baseids[n] = komodo_baseid(p.source);
                    if ( 0 )
                    {
                        char coinaddr[64];
                        bitcoin_address(coinaddr,60,&rmd160s[n * 20],20);
                        printf(">>>>>>> %s: (%s) fiat %.8f kmdheight.%d other.%d -> %s %.8f\n",type=='A'?"approvedA":"issuedX",baseids[n]>=0?CURRENCIES[baseids[n]]:"???",dstr(p.fiatoshis),kmdheights[n],otherheights[n],coinaddr,dstr(values[n]));
                    }
                }
            }
            else
            {
                for (i=0; i<4; i++)
                    base[i] = opretbuf[opretlen-4+i];
                for (j=0; j<32; j++)
                {
                    ((uint8_t *)&txids[n])[j] = opretbuf[len++];
                    //printf("%02x",((uint8_t *)&txids[n])[j]);
                }
                vouts[n] = opretbuf[len++];
                vouts[n] = (opretbuf[len++] << 8) | vouts[n];
                baseids[n] = komodo_baseid(base);
                if ( (pax= komodo_paxfinds(txids[n],vouts[n])) != 0 )
                {
                    values[n] = (strcmp("KMD",base) == 0) ? pax->komodoshis : pax->fiatoshis;
                    srcvalues[n] = (strcmp("KMD",base) == 0) ? pax->fiatoshis : pax->komodoshis;
                    kmdheights[n] = pax->height;
                    otherheights[n] = pax->otherheight;
                    memcpy(&rmd160s[n * 20],pax->rmd160,20);
                }
            }
            //printf(" komodo_issued_opreturn issuedtxid v%d i.%d opretlen.%d\n",vouts[n],n,opretlen);
        }
    }
    return(n);
}

int32_t komodo_paxcmp(char *symbol,int32_t kmdheight,uint64_t value,uint64_t checkvalue,uint64_t seed)
{
    int32_t ratio;
    if ( seed == 0 && checkvalue != 0 )
    {
        ratio = ((value << 6) / checkvalue);
        if ( ratio >= 60 && ratio <= 67 )
            return(0);
        else
        {
            if ( ASSETCHAINS_SYMBOL[0] != 0 )
                printf("ht.%d ignore mismatched %s value %lld vs checkvalue %lld -> ratio.%d\n",kmdheight,symbol,(long long)value,(long long)checkvalue,ratio);
            return(-1);
        }
    }
    else if ( checkvalue != 0 )
    {
        ratio = ((value << 10) / checkvalue);
        if ( ratio >= 1023 && ratio <= 1025 )
            return(0);
    }
    return(value != checkvalue);
}

uint64_t komodo_paxtotal()
{
    struct pax_transaction *pax,*pax2,*tmp,*tmp2; char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN],*str; int32_t i,ht; int64_t checktoshis; uint64_t seed,total = 0; struct komodo_state *basesp;
    if ( KOMODO_PASSPORT_INITDONE == 0 ) //KOMODO_PAX == 0 ||
        return(0);
    if ( komodo_isrealtime(&ht) == 0 )
        return(0);
    else
    {
        HASH_ITER(hh,PAX,pax,tmp)
        {
            if ( pax->marked != 0 )
                continue;
            if ( pax->type == 'A' || pax->type == 'D' || pax->type == 'X' )
                str = pax->symbol;
            else str = pax->source;
            basesp = komodo_stateptrget(str);
            if ( basesp != 0 && pax->didstats == 0 )
            {
                if ( pax->type == 'I' && (pax2= komodo_paxfind(pax->txid,pax->vout,'D')) != 0 )
                {
                    if ( pax2->fiatoshis != 0 )
                    {
                        pax->komodoshis = pax2->komodoshis;
                        pax->fiatoshis = pax2->fiatoshis;
                        basesp->issued += pax->fiatoshis;
                        pax->didstats = 1;
                        if ( strcmp(str,ASSETCHAINS_SYMBOL) == 0 )
                            printf("########### %p issued %s += %.8f kmdheight.%d %.8f other.%d\n",basesp,str,dstr(pax->fiatoshis),pax->height,dstr(pax->komodoshis),pax->otherheight);
                        pax2->marked = pax->height;
                        pax->marked = pax->height;
                    }
                }
                else if ( pax->type == 'W' )
                {
                    //bitcoin_address(coinaddr,addrtype,rmd160,20);
                    if ( (checktoshis= komodo_paxprice(&seed,pax->height,pax->source,(char *)"KMD",(uint64_t)pax->fiatoshis)) != 0 )
                    {
                        if ( komodo_paxcmp(pax->source,pax->height,pax->komodoshis,checktoshis,seed) != 0 )
                        {
                            pax->marked = pax->height;
                            //printf("WITHDRAW.%s mark <- %d %.8f != %.8f\n",pax->source,pax->height,dstr(checktoshis),dstr(pax->komodoshis));
                        }
                        else if ( pax->validated == 0 )
                        {
                            pax->validated = pax->komodoshis = checktoshis;
                            //int32_t j; for (j=0; j<32; j++)
                            //    printf("%02x",((uint8_t *)&pax->txid)[j]);
                            //if ( strcmp(str,ASSETCHAINS_SYMBOL) == 0 )
                            //    printf(" v%d %p got WITHDRAW.%s kmd.%d ht.%d %.8f -> %.8f/%.8f\n",pax->vout,pax,pax->source,pax->height,pax->otherheight,dstr(pax->fiatoshis),dstr(pax->komodoshis),dstr(checktoshis));
                        }
                    }
                }
            }
        }
    }
    komodo_stateptr(symbol,dest);
    HASH_ITER(hh,PAX,pax,tmp)
    {
        pax->ready = 0;
        if ( 0 && pax->type == 'A' )
            printf("%p pax.%s <- %s marked.%d %.8f -> %.8f validated.%d approved.%d\n",pax,pax->symbol,pax->source,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis),pax->validated != 0,pax->approved != 0);
        if ( pax->marked != 0 )
            continue;
        if ( strcmp(symbol,pax->symbol) == 0 || pax->type == 'A' )
        {
            if ( pax->marked == 0 )
            {
                if ( komodo_is_issuer() != 0 )
                {
                    if ( pax->validated != 0 && pax->type == 'D' )
                    {
                        total += pax->fiatoshis;
                        pax->ready = 1;
                    }
                }
                else if ( pax->approved != 0 && pax->type == 'A' )
                {
                    if ( pax->validated != 0 )
                    {
                        total += pax->komodoshis;
                        pax->ready = 1;
                    }
                    else
                    {
                        seed = 0;
                        checktoshis = komodo_paxprice(&seed,pax->height,pax->source,(char *)"KMD",(uint64_t)pax->fiatoshis);
                        //printf("paxtotal PAX_fiatdest ht.%d price %s %.8f -> KMD %.8f vs %.8f\n",pax->height,pax->symbol,(double)pax->fiatoshis/COIN,(double)pax->komodoshis/COIN,(double)checktoshis/COIN);
                        //printf(" v%d %.8f k.%d ht.%d\n",pax->vout,dstr(pax->komodoshis),pax->height,pax->otherheight);
                        if ( seed != 0 && checktoshis != 0 )
                        {
                            if ( checktoshis == pax->komodoshis )
                            {
                                total += pax->komodoshis;
                                pax->validated = pax->komodoshis;
                                pax->ready = 1;
                            } else pax->marked = pax->height;
                        }
                    }
                }
                if ( 0 && pax->ready != 0 )
                    printf("%p (%c) pax.%s marked.%d %.8f -> %.8f validated.%d approved.%d ready.%d ht.%d\n",pax,pax->type,pax->symbol,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis),pax->validated != 0,pax->approved != 0,pax->ready,pax->height);
            }
        }
    }
    //printf("paxtotal %.8f\n",dstr(total));
    return(total);
}

static int _paxorder(const void *a,const void *b)
{
#define pax_a (*(struct pax_transaction **)a)
#define pax_b (*(struct pax_transaction **)b)
    uint64_t aval,bval;
    aval = pax_a->fiatoshis + pax_a->komodoshis + pax_a->height;
    bval = pax_b->fiatoshis + pax_b->komodoshis + pax_b->height;
	if ( bval > aval )
		return(-1);
	else if ( bval < aval )
		return(1);
	return(0);
#undef pax_a
#undef pax_b
}

int32_t komodo_pending_withdraws(char *opretstr) // todo: enforce deterministic order
{
    struct pax_transaction *pax,*pax2,*tmp,*paxes[64]; uint8_t opretbuf[16384*4]; int32_t i,n,ht,len=0; uint64_t total = 0;
    if ( KOMODO_PAX == 0 || KOMODO_PASSPORT_INITDONE == 0 )
        return(0);
    if ( komodo_isrealtime(&ht) == 0 || ASSETCHAINS_SYMBOL[0] != 0 )
        return(0);
    n = 0;
    HASH_ITER(hh,PAX,pax,tmp)
    {
        if ( pax->type == 'W' )
        {
            if ( (pax2= komodo_paxfind(pax->txid,pax->vout,'A')) != 0 )
            {
                if ( pax2->approved != 0 )
                    pax->approved = pax2->approved;
            }
            else if ( (pax2= komodo_paxfind(pax->txid,pax->vout,'X')) != 0 )
                pax->approved = pax->height;
            //printf("pending_withdraw: pax %s marked.%u approved.%u validated.%llu\n",pax->symbol,pax->marked,pax->approved,(long long)pax->validated);
            if ( pax->marked == 0 && pax->approved == 0 && pax->validated != 0 ) //strcmp((char *)"KMD",pax->symbol) == 0 &&
            {
                if ( n < sizeof(paxes)/sizeof(*paxes) )
                {
                    paxes[n++] = pax;
                    //int32_t j; for (j=0; j<32; j++)
                    //    printf("%02x",((uint8_t *)&pax->txid)[j]);
                    //printf(" %s.(kmdht.%d ht.%d marked.%u approved.%d validated %.8f) %.8f\n",pax->source,pax->height,pax->otherheight,pax->marked,pax->approved,dstr(pax->validated),dstr(pax->komodoshis));
                }
            }
        }
    }
    opretstr[0] = 0;
    if ( n > 0 )
    {
        opretbuf[len++] = 'A';
        qsort(paxes,n,sizeof(*paxes),_paxorder);
        for (i=0; i<n; i++)
        {
            if ( len < (sizeof(opretbuf)>>3)*7 )
                len += komodo_rwapproval(1,&opretbuf[len],paxes[i]);
        }
        if ( len > 0 )
            init_hexbytes_noT(opretstr,opretbuf,len);
    }
    //fprintf(stderr,"komodo_pending_withdraws len.%d PAXTOTAL %.8f\n",len,dstr(komodo_paxtotal()));
    return(len);
}

int32_t komodo_gateway_deposits(CMutableTransaction *txNew,char *base,int32_t tokomodo)
{
    struct pax_transaction *pax,*tmp; char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; uint8_t *script,opcode,opret[16384*4],data[16384*4]; int32_t i,baseid,ht,len=0,opretlen=0,numvouts=1; struct komodo_state *sp; uint64_t available,deposited,issued,withdrawn,approved,redeemed,mask,sum = 0;
    if ( KOMODO_PASSPORT_INITDONE == 0 )//KOMODO_PAX == 0 ||
        return(0);
    struct komodo_state *kmdsp = komodo_stateptrget((char *)"KMD");
    sp = komodo_stateptr(symbol,dest);
    strcpy(symbol,base);
    if ( ASSETCHAINS_SYMBOL[0] != 0 && komodo_baseid(ASSETCHAINS_SYMBOL) < 0 )
        return(0);
    PENDING_KOMODO_TX = 0;
    for (i=0; i<3; i++)
    {
        if ( komodo_isrealtime(&ht) != 0 )
            break;
        sleep(1);
    }
    if ( i == 3 )
    {
        if ( tokomodo == 0 )
            printf("%s not realtime ht.%d\n",ASSETCHAINS_SYMBOL,ht);
        return(0);
    }
    if ( tokomodo == 0 )
    {
        opcode = 'I';
    }
    else
    {
        opcode = 'X';
        if ( 1 || komodo_paxtotal() == 0 )
            return(0);
    }
    HASH_ITER(hh,PAX,pax,tmp)
    {
        if ( pax->type != 'D' && pax->type != 'A' )
            continue;
        {
#ifdef KOMODO_ASSETCHAINS_WAITNOTARIZE
            if ( pax->height > 236000 )
            {
                if ( kmdsp != 0 && kmdsp->NOTARIZED_HEIGHT >= pax->height )
                    pax->validated = pax->komodoshis;
                else if ( kmdsp->CURRENT_HEIGHT > pax->height+30 )
                    pax->validated = pax->ready = 0;
            }
            else
            {
                if ( kmdsp != 0 && (kmdsp->NOTARIZED_HEIGHT >= pax->height || kmdsp->CURRENT_HEIGHT > pax->height+30) ) // assumes same chain as notarize
                    pax->validated = pax->komodoshis;
                else pax->validated = pax->ready = 0;
            }
#else
            pax->validated = pax->komodoshis;
#endif
        }
        if ( ASSETCHAINS_SYMBOL[0] != 0 && (pax_fiatstatus(&available,&deposited,&issued,&withdrawn,&approved,&redeemed,symbol) != 0 || available < pax->fiatoshis) )
        {
            //if ( pax->height > 214700 || strcmp(ASSETCHAINS_SYMBOL,symbol) == 0 )
            //    printf("miner.[%s]: skip %s %.8f when avail %.8f deposited %.8f, issued %.8f withdrawn %.8f approved %.8f redeemed %.8f\n",ASSETCHAINS_SYMBOL,symbol,dstr(pax->fiatoshis),dstr(available),dstr(deposited),dstr(issued),dstr(withdrawn),dstr(approved),dstr(redeemed));
            continue;
        }
        /*printf("pax.%s marked.%d %.8f -> %.8f ready.%d validated.%d\n",pax->symbol,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis),pax->ready!=0,pax->validated!=0);
        if ( pax->marked != 0 || (pax->type != 'D' && pax->type != 'A') || pax->ready == 0 )
        {
            printf("reject 2\n");
            continue;
        }*/
        if ( ASSETCHAINS_SYMBOL[0] != 0 && (strcmp(pax->symbol,symbol) != 0 || pax->validated == 0 || pax->ready == 0) )
        {
            if ( strcmp(pax->symbol,ASSETCHAINS_SYMBOL) == 0 )
                printf("pax->symbol.%s != %s or null pax->validated %.8f ready.%d ht.(%d %d)\n",pax->symbol,symbol,dstr(pax->validated),pax->ready,kmdsp->CURRENT_HEIGHT,pax->height);
            pax->marked = pax->height;
            continue;
        }
        if ( pax->ready == 0 )
            continue;
        if ( pax->type == 'A' && ASSETCHAINS_SYMBOL[0] == 0 )
        {
            if ( kmdsp != 0 )
            {
                if ( (baseid= komodo_baseid(pax->symbol)) < 0 || ((1LL << baseid) & sp->RTmask) == 0 )
                {
                    printf("not RT for (%s) %llx baseid.%d %llx\n",pax->symbol,(long long)sp->RTmask,baseid,(long long)(1LL<<baseid));
                    continue;
                }
            } else continue;
        }

        //printf("redeem.%d? (%c) %p pax.%s marked.%d %.8f -> %.8f ready.%d validated.%d approved.%d\n",tokomodo,pax->type,pax,pax->symbol,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis),pax->ready!=0,pax->validated!=0,pax->approved!=0);
        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
            printf("pax.%s marked.%d %.8f -> %.8f\n",ASSETCHAINS_SYMBOL,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis));
        if ( opcode == 'I' )
        {
            sum += pax->fiatoshis;
            if ( sum > available )
                break;
        }
        txNew->vout.resize(numvouts+1);
        txNew->vout[numvouts].nValue = (opcode == 'I') ? pax->fiatoshis : pax->komodoshis;
        txNew->vout[numvouts].scriptPubKey.resize(25);
        script = (uint8_t *)&txNew->vout[numvouts].scriptPubKey[0];
        *script++ = 0x76;
        *script++ = 0xa9;
        *script++ = 20;
        memcpy(script,pax->rmd160,20), script += 20;
        *script++ = 0x88;
        *script++ = 0xac;
        if ( tokomodo == 0 )
        {
            for (i=0; i<32; i++)
                data[len++] = ((uint8_t *)&pax->txid)[i];
            data[len++] = pax->vout & 0xff;
            data[len++] = (pax->vout >> 8) & 0xff;
            PENDING_KOMODO_TX += pax->fiatoshis;
        }
        else
        {
            len += komodo_rwapproval(1,&data[len],pax);
            PENDING_KOMODO_TX += pax->komodoshis;
            printf(" len.%d vout.%u DEPOSIT %.8f <- pax.%s pending ht %d %d %.8f | ",len,pax->vout,(double)txNew->vout[numvouts].nValue/COIN,symbol,pax->height,pax->otherheight,dstr(PENDING_KOMODO_TX));
        }
        if ( numvouts++ >= 64 || sum > COIN )
            break;
    }
    if ( numvouts > 1 )
    {
        if ( tokomodo != 0 )
            strcpy(symbol,(char *)"KMD");
        for (i=0; symbol[i]!=0; i++)
            data[len++] = symbol[i];
        data[len++] = 0;
        for (i=0; i<len; i++)
            printf("%02x",data[i]);
        printf(" <- data[%d]\n",len);
        opretlen = komodo_opreturnscript(opret,opcode,data,len);
        txNew->vout.resize(numvouts+1);
        txNew->vout[numvouts].nValue = 0;
        txNew->vout[numvouts].scriptPubKey.resize(opretlen);
        script = (uint8_t *)&txNew->vout[numvouts].scriptPubKey[0];
        memcpy(script,opret,opretlen);
        for (i=0; i<8; i++)
            printf("%02x",opret[i]);
        printf(" <- opret, MINER deposits.%d (%s) vouts.%d %.8f opretlen.%d\n",tokomodo,ASSETCHAINS_SYMBOL,numvouts,dstr(PENDING_KOMODO_TX),opretlen);
        return(1);
    }
    return(0);
}

const char *banned_txids[] =
{
    "78cb4e21245c26b015b888b14c4f5096e18137d2741a6de9734d62b07014dfca", // vout1 only 233559
    "00697be658e05561febdee1aafe368b821ca33fbb89b7027365e3d77b5dfede5", //234172
    "e909465788b32047c472d73e882d79a92b0d550f90be008f76e1edaee6d742ea", //234187
    "f56c6873748a327d0b92b8108f8ec8505a2843a541b1926022883678fb24f9dc", //234188
    "abf08be07d8f5b3a433ddcca7ef539e79a3571632efd6d0294ec0492442a0204", //234213
    "3b854b996cc982fba8c06e76cf507ae7eed52ab92663f4c0d7d10b3ed879c3b0", //234367
    "fa9e474c2cda3cb4127881a40eb3f682feaba3f3328307d518589024a6032cc4", //234635
    "ca746fa13e0113c4c0969937ea2c66de036d20274efad4ce114f6b699f1bc0f3", //234662
    "43ce88438de4973f21b1388ffe66e68fda592da38c6ef939be10bb1b86387041", //234697
    "0aeb748de82f209cd5ff7d3a06f65543904c4c17387c9d87c65fd44b14ad8f8c", //234899
    "bbd3a3d9b14730991e1066bd7c626ca270acac4127131afe25f877a5a886eb25", //235252
    "fa9943525f2e6c32cbc243294b08187e314d83a2870830180380c3c12a9fd33c", //235253
    "a01671c8775328a41304e31a6693bbd35e9acbab28ab117f729eaba9cb769461", //235265
    "2ef49d2d27946ad7c5d5e4ab5c089696762ff04e855f8ab48e83bdf0cc68726d", //235295
    "c85dcffb16d5a45bd239021ad33443414d60224760f11d535ae2063e5709efee", //235296
    // all vouts banned
    "c4ea1462c207547cd6fb6a4155ca6d042b22170d29801a465db5c09fec55b19d", //246748
    "305dc96d8bc23a69d3db955e03a6a87c1832673470c32fe25473a46cc473c7d1", //247204
    //"43416a0c4da6b1a5c1d375bdbe8f7dc8d44d8f60df593d3376aa8221ec66357e", // vout0 only
    //"1eb295ed54c47f35cbccd7e7e40d03041f1853581da6d41102a9d8813782b6cb",
    //"db121e4012222adfc841824984a2a90b7e5b018dd71307822537d58160195e43",
    //"28f95b8148ac4ae6e09c7380e34422fab41d568a411e53dc94823e36a3d6f386",
    //"01d8c839463bda2f2f6400ede4611357913684927a767422a8560ead1b22557c",
    //"6e4980a9e1bd669f4df04732dc6f11b7773b6de88d1abcf89a6b9007d72ef9ac",
    //"6cc1d0495170bc0e11fd3925297623562e529ea1336b66ea61f8a1159041aed2",
    //"250875424cece9bcd98cb226b09da7671625633d6958589e3a462bad89ad87cc", // missed
    //"ea8659011de52f4dac42cda12326064b7b5013b8492f88e33159884ca299aa05", // missed
    //"ce567928b5490a17244167af161b1d8dd6ff753fef222fe6855d95b2278a35b3", // missed
};

int32_t komodo_checkvout(int32_t vout,int32_t k,int32_t indallvouts)
{
    if ( k < indallvouts )
        return(vout == 1);
    else if ( k == indallvouts || k == indallvouts+1 )
        return(1);
    else return(vout == 0);
}

int32_t komodo_bannedset(int32_t *indallvoutsp,uint256 *array,int32_t max)
{
    int32_t i;
    if ( sizeof(banned_txids)/sizeof(*banned_txids) > max )
    {
        fprintf(stderr,"komodo_bannedset: buffer too small %d vs %d\n",(int32_t)(sizeof(banned_txids)/sizeof(*banned_txids)),max);
        StartShutdown();
    }
    for (i=0; i<sizeof(banned_txids)/sizeof(*banned_txids); i++)
        array[i] = uint256S(banned_txids[i]);
    *indallvoutsp = i-2;
    return(i);
}

void komodo_passport_iteration();

int32_t komodo_check_deposit(int32_t height,const CBlock& block,uint32_t prevtime) // verify above block is valid pax pricing
{
    static uint256 array[64]; static int32_t numbanned,indallvouts;
    int32_t i,j,k,n,ht,baseid,txn_count,activation,num,opretlen,offset=1,errs=0,notmatched=0,matched=0,kmdheights[256],otherheights[256]; uint256 hash,txids[256]; char symbol[KOMODO_ASSETCHAIN_MAXLEN],base[KOMODO_ASSETCHAIN_MAXLEN]; uint16_t vouts[256]; int8_t baseids[256]; uint8_t *script,opcode,rmd160s[256*20]; uint64_t total,subsidy,available,deposited,issued,withdrawn,approved,redeemed,seed; int64_t checktoshis,values[256],srcvalues[256]; struct pax_transaction *pax; struct komodo_state *sp; CTransaction tx;
    activation = 235300;
    if ( *(int32_t *)&array[0] == 0 )
        numbanned = komodo_bannedset(&indallvouts,array,(int32_t)(sizeof(array)/sizeof(*array)));
    memset(baseids,0xff,sizeof(baseids));
    memset(values,0,sizeof(values));
    memset(srcvalues,0,sizeof(srcvalues));
    memset(rmd160s,0,sizeof(rmd160s));
    memset(kmdheights,0,sizeof(kmdheights));
    memset(otherheights,0,sizeof(otherheights));
    txn_count = block.vtx.size();
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        for (i=0; i<txn_count; i++)
        {
            if ( i == 0 && txn_count > 1 && block.vtx[txn_count-1].vout.size() > 0 && block.vtx[txn_count-1].vout[0].nValue == 5000 )
            {
                /*
                if ( block.vtx[txn_count-1].vin.size() == 1 && GetTransaction(block.vtx[txn_count-1].vin[0].prevout.hash,tx,hash,false) && block.vtx[0].vout[0].scriptPubKey == tx.vout[block.vtx[txn_count-1].vin[0].prevout.n].scriptPubKey )
                    notmatched = 1;
                */
                if ( block.vtx[txn_count-1].vin.size() == 1 ) {
                    uint256 hashNotaryProofVin = block.vtx[txn_count-1].vin[0].prevout.hash;
                    int fNotaryProofVinTxFound = GetTransaction(hashNotaryProofVin,tx,hash,false);
                    if (!fNotaryProofVinTxFound) {
                        // try to search in the same block
                        BOOST_FOREACH(const CTransaction &txInThisBlock, block.vtx) {
                            if (txInThisBlock.GetHash() == hashNotaryProofVin) {
                                fNotaryProofVinTxFound = 1;
                                tx = txInThisBlock;
                                hash = block.GetHash();
                                break;
                            }
                        }
                    }
                    if ( fNotaryProofVinTxFound && block.vtx[0].vout[0].scriptPubKey == tx.vout[block.vtx[txn_count-1].vin[0].prevout.n].scriptPubKey )
                        {
                            notmatched = 1;
                        }
                }  
            }
            n = block.vtx[i].vin.size();
            for (j=0; j<n; j++)
            {
                for (k=0; k<numbanned; k++)
                {
                    if ( block.vtx[i].vin[j].prevout.hash == array[k] && komodo_checkvout(block.vtx[i].vin[j].prevout.n,k,indallvouts) != 0 ) //(block.vtx[i].vin[j].prevout.n == 1 || k >= indallvouts)  )
                    {
                        printf("banned tx.%d being used at ht.%d txi.%d vini.%d\n",k,height,i,j);
                        return(-1);
                    }
                }
            }
        }
    }
    if ( height > 0 && ASSETCHAINS_MARMARA != 0 && (height & 1) == 0 )
    {
        if ( MarmaraValidateCoinbase(height,block.vtx[0]) < 0 )
        {
            fprintf(stderr,"MARMARA error ht.%d constrains even height blocks to pay 100%% to CC in vout0 with opreturn\n",height);
            return(-1);
        }
    }
    // we don't want these checks in VRSC, leave it at the Sapling upgrade
    if ( ASSETCHAINS_SYMBOL[0] == 0 ||
         ((ASSETCHAINS_COMMISSION != 0 || ASSETCHAINS_FOUNDERS_REWARD) && height > 1) ||
         NetworkUpgradeActive(height, Params().GetConsensus(), Consensus::UPGRADE_SAPLING) )
    {
        n = block.vtx[0].vout.size();
        int64_t val,prevtotal = 0; int32_t strangeout=0,overflow = 0;
        total = 0;
        for (i=1; i<n; i++)
        {
            script = (uint8_t *)&block.vtx[0].vout[i].scriptPubKey[0];
            if ( (val= block.vtx[0].vout[i].nValue) < 0 || val >= MAX_MONEY )
            {
                overflow = 1;
                break;
            }
            if ( i > 1 && script[0] != 0x6a && val < 5000 )
                strangeout++;
            total += val;
            if ( total < prevtotal || (val != 0 && total == prevtotal) )
            {
                overflow = 1;
                break;
            }
            prevtotal = total;
        }
        if ( ASSETCHAINS_SYMBOL[0] == 0 )
        {
            if ( overflow != 0 || total > COIN/10 )
            {
                if ( height >= activation )
                {
                    if ( height > 800000 )
                        fprintf(stderr,">>>>>>>> <<<<<<<<<< ht.%d illegal nonz output %.8f n.%d\n",height,dstr(block.vtx[0].vout[1].nValue),n);
                    return(-1);
                }
            }
            else if ( block.nBits == KOMODO_MINDIFF_NBITS && total > 0 ) // to deal with fee stealing
            {
                fprintf(stderr,"notary mined ht.%d with extra %.8f\n",height,dstr(total));
                if ( height > KOMODO_NOTARIES_HEIGHT1 )
                    return(-1);
            }
            if ( strangeout != 0 || notmatched != 0 )
            {
                if ( 0 && strcmp(NOTARY_PUBKEY.c_str(),"03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828") == 0 )
                    fprintf(stderr,">>>>>>>>>>>>> DUST ht.%d strangout.%d notmatched.%d <<<<<<<<<\n",height,strangeout,notmatched);
                if ( height > 1000000 && strangeout != 0 )
                    return(-1);
            }
            else if ( height > 814000 )
            {
                script = (uint8_t *)&block.vtx[0].vout[0].scriptPubKey[0];
                //int32_t notary = komodo_electednotary(&num,script+1,height,0);
                //if ( (-1 * (komodo_electednotary(&num,script+1,height,0) >= 0) * (height > 1000000)) < 0 )
                //    fprintf(stderr, ">>>>>>> FAILED BLOCK.%d notary.%d insync.%d\n",height,notary,KOMODO_INSYNC);
                //else
                //    fprintf(stderr, "<<<<<<< VALID BLOCK.%d notary.%d insync.%d\n",height,notary,KOMODO_INSYNC);
                return(-1 * (komodo_electednotary(&num,script+1,height,0) >= 0) * (height > 1000000));
            }
        }
        else
        {
            checktoshis = 0;
            if ( (ASSETCHAINS_COMMISSION != 0 || ASSETCHAINS_FOUNDERS_REWARD) && height > 1 )
            {
                if ( (checktoshis= komodo_checkcommission((CBlock *)&block,height)) < 0 )
                {
                    fprintf(stderr,"ht.%d checktoshis %.8f overflow.%d total %.8f strangeout.%d\n",height,dstr(checktoshis),overflow,dstr(total),strangeout);
                    return(-1);
                }
            }
            if ( height > 1 && checktoshis == 0 )
            {
                checktoshis = ((uint64_t)GetBlockSubsidy(height, Params().GetConsensus()) - block.vtx[0].vout[0].nValue);
                // some pools will need to change their pool fee to be (poolfee % - txfees)
                //checktoshis += txn_count * 0.001; // rely on higher level validations to prevent emitting more coins than actual txfees
            }
            if ( height >= 2 && (overflow != 0 || total > checktoshis || strangeout != 0) )
            {
                fprintf(stderr,"checkdeposit: ht.%d checktoshis %.8f overflow.%d total %.8f strangeout.%d\n",height,dstr(checktoshis),overflow,dstr(total),strangeout);
                if ( strangeout != 0 )
                    fprintf(stderr,">>>>>>>>>>>>> %s DUST ht.%d strangeout.%d notmatched.%d <<<<<<<<<\n",ASSETCHAINS_SYMBOL,height,strangeout,notmatched);
                return(-1);
            }
        }
    }
    return(0);
}

const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout,char *source)
{
    uint8_t rmd160[20],rmd160s[64*20],addrtype,shortflag,pubkey33[33]; int32_t didstats,i,j,n,kvheight,len,tokomodo,kmdheight,otherheights[64],kmdheights[64]; int8_t baseids[64]; char base[4],coinaddr[64],destaddr[64]; uint256 txids[64]; uint16_t vouts[64]; uint64_t convtoshis,seed; int64_t fee,fiatoshis,komodoshis,checktoshis,values[64],srcvalues[64]; struct pax_transaction *pax,*pax2; struct komodo_state *basesp; double diff;
    const char *typestr = "unknown";
    if ( ASSETCHAINS_SYMBOL[0] != 0 && komodo_baseid(ASSETCHAINS_SYMBOL) < 0 && opretbuf[0] != 'K' )
    {
        //printf("komodo_opreturn skip %s\n",ASSETCHAINS_SYMBOL);
        return("assetchain");
    }
    memset(baseids,0xff,sizeof(baseids));
    memset(values,0,sizeof(values));
    memset(srcvalues,0,sizeof(srcvalues));
    memset(rmd160s,0,sizeof(rmd160s));
    memset(kmdheights,0,sizeof(kmdheights));
    memset(otherheights,0,sizeof(otherheights));
    tokomodo = (komodo_is_issuer() == 0);
    if ( opretbuf[0] == 'K' && opretlen != 40 )
    {
        komodo_kvupdate(opretbuf,opretlen,value);
        return("kv");
    }
    else if ( ASSETCHAINS_SYMBOL[0] == 0 && KOMODO_PAX == 0 )
        return("nopax");
    if ( opretbuf[0] == 'D' )
    {
        tokomodo = 0;
        if ( opretlen == 38 ) // any KMD tx
        {
            iguana_rwnum(0,&opretbuf[34],sizeof(kmdheight),&kmdheight);
            memset(base,0,sizeof(base));
            PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&fiatoshis);
            bitcoin_address(coinaddr,addrtype,rmd160,20);
            checktoshis = PAX_fiatdest(&seed,tokomodo,destaddr,pubkey33,coinaddr,kmdheight,base,fiatoshis);
            if ( komodo_paxcmp(base,kmdheight,value,checktoshis,kmdheight < 225000 ? seed : 0) != 0 )
                checktoshis = PAX_fiatdest(&seed,tokomodo,destaddr,pubkey33,coinaddr,height,base,fiatoshis);
            typestr = "deposit";
            if ( 0 && strcmp("NOK",base) == 0 )
            {
                printf("[%s] %s paxdeposit height.%d vs kmdheight.%d\n",ASSETCHAINS_SYMBOL,base,height,kmdheight);
                printf("(%s) (%s) kmdheight.%d vs height.%d check %.8f vs %.8f tokomodo.%d %d seed.%llx\n",ASSETCHAINS_SYMBOL,base,kmdheight,height,dstr(checktoshis),dstr(value),komodo_is_issuer(),strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0,(long long)seed);
                for (i=0; i<32; i++)
                    printf("%02x",((uint8_t *)&txid)[i]);
                printf(" <- txid.v%u ",vout);
                for (i=0; i<33; i++)
                    printf("%02x",pubkey33[i]);
                printf(" checkpubkey check %.8f v %.8f dest.(%s) kmdheight.%d height.%d\n",dstr(checktoshis),dstr(value),destaddr,kmdheight,height);
            }
            if ( strcmp(base,ASSETCHAINS_SYMBOL) == 0 && (kmdheight > 195000 || kmdheight <= height) )
            {
                didstats = 0;
                if ( komodo_paxcmp(base,kmdheight,value,checktoshis,kmdheight < 225000 ? seed : 0) == 0 )
                {
                    if ( (pax= komodo_paxfind(txid,vout,'D')) == 0 )
                    {
                        if ( (basesp= komodo_stateptrget(base)) != 0 )
                        {
                            basesp->deposited += fiatoshis;
                            didstats = 1;
                            if ( 0 && strcmp(base,ASSETCHAINS_SYMBOL) == 0 )
                                printf("########### %p deposited %s += %.8f kmdheight.%d %.8f\n",basesp,base,dstr(fiatoshis),kmdheight,dstr(value));
                        } else printf("cant get stateptr.(%s)\n",base);
                        komodo_gateway_deposit(coinaddr,value,base,fiatoshis,rmd160,txid,vout,'D',kmdheight,height,(char *)"KMD",0);
                    }
                    if ( (pax= komodo_paxfind(txid,vout,'D')) != 0 )
                    {
                        pax->height = kmdheight;
                        pax->validated = value;
                        pax->komodoshis = value;
                        pax->fiatoshis = fiatoshis;
                        if ( didstats == 0 && pax->didstats == 0 )
                        {
                            if ( (basesp= komodo_stateptrget(base)) != 0 )
                            {
                                basesp->deposited += fiatoshis;
                                didstats = 1;
                                if ( 0 && strcmp(base,ASSETCHAINS_SYMBOL) == 0 )
                                    printf("########### %p depositedB %s += %.8f/%.8f kmdheight.%d/%d %.8f/%.8f\n",basesp,base,dstr(fiatoshis),dstr(pax->fiatoshis),kmdheight,pax->height,dstr(value),dstr(pax->komodoshis));
                            }
                        } //
                        if ( didstats != 0 )
                            pax->didstats = 1;
                        if ( (pax2= komodo_paxfind(txid,vout,'I')) != 0 )
                        {
                            pax2->fiatoshis = pax->fiatoshis;
                            pax2->komodoshis = pax->komodoshis;
                            pax->marked = pax2->marked = pax->height;
                            pax2->height = pax->height = height;
                            if ( pax2->didstats == 0 )
                            {
                                if ( (basesp= komodo_stateptrget(base)) != 0 )
                                {
                                    basesp->issued += pax2->fiatoshis;
                                    pax2->didstats = 1;
                                    if ( 0 && strcmp(base,"USD") == 0 )
                                        printf("########### %p issueda %s += %.8f kmdheight.%d %.8f other.%d [%d]\n",basesp,base,dstr(pax2->fiatoshis),pax2->height,dstr(pax2->komodoshis),pax2->otherheight,height);
                                }
                            }
                        }
                    }
                }
                else
                {
                    if ( (pax= komodo_paxfind(txid,vout,'D')) != 0 )
                        pax->marked = checktoshis;
                    if ( kmdheight > 238000 && (kmdheight > 214700 || strcmp(base,ASSETCHAINS_SYMBOL) == 0) ) //seed != 0 &&
                        printf("pax %s deposit %.8f rejected kmdheight.%d %.8f KMD check %.8f seed.%llu\n",base,dstr(fiatoshis),kmdheight,dstr(value),dstr(checktoshis),(long long)seed);
                }
            } //else printf("[%s] %s paxdeposit height.%d vs kmdheight.%d\n",ASSETCHAINS_SYMBOL,base,height,kmdheight);
        } //else printf("unsupported size.%d for opreturn D\n",opretlen);
    }
    else if ( opretbuf[0] == 'I' )
    {
        tokomodo = 0;
        if ( strncmp((char *)"KMD",(char *)&opretbuf[opretlen-4],3) != 0 && strncmp(ASSETCHAINS_SYMBOL,(char *)&opretbuf[opretlen-4],3) == 0 )
        {
            if ( (n= komodo_issued_opreturn(base,txids,vouts,values,srcvalues,kmdheights,otherheights,baseids,rmd160s,opretbuf,opretlen,0)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    if ( baseids[i] < 0 )
                    {
                        static uint32_t counter;
                        if ( counter++ < 0 )
                            printf("%d of %d illegal baseid.%d, this can be ignored\n",i,n,baseids[i]);
                        continue;
                    }
                    bitcoin_address(coinaddr,60,&rmd160s[i*20],20);
                    komodo_gateway_deposit(coinaddr,0,ASSETCHAINS_SYMBOL,0,0,txids[i],vouts[i],'I',height,0,CURRENCIES[baseids[i]],0);
                    komodo_paxmark(height,txids[i],vouts[i],'I',height);
                    if ( (pax= komodo_paxfind(txids[i],vouts[i],'I')) != 0 )
                    {
                        pax->type = opretbuf[0];
                        strcpy(pax->source,(char *)&opretbuf[opretlen-4]);
                        if ( (pax2= komodo_paxfind(txids[i],vouts[i],'D')) != 0 && pax2->fiatoshis != 0 && pax2->komodoshis != 0 )
                        {
                            // realtime path?
                            pax->fiatoshis = pax2->fiatoshis;
                            pax->komodoshis = pax2->komodoshis;
                            pax->marked = pax2->marked = pax2->height;
                            if ( pax->didstats == 0 )
                            {
                                if ( (basesp= komodo_stateptrget(CURRENCIES[baseids[i]])) != 0 )
                                {
                                    basesp->issued += pax->fiatoshis;
                                    pax->didstats = 1;
                                    pax->height = pax2->height;
                                    pax->otherheight = height;
                                    if ( 1 && strcmp(CURRENCIES[baseids[i]],"USD") == 0 )
                                        printf("########### %p issuedb %s += %.8f kmdheight.%d %.8f other.%d [%d]\n",basesp,CURRENCIES[baseids[i]],dstr(pax->fiatoshis),pax->height,dstr(pax->komodoshis),pax->otherheight,height);
                                }
                            }
                        }
                    }
                    if ( (pax= komodo_paxmark(height,txids[i],vouts[i],'I',height)) != 0 )
                        komodo_paxdelete(pax);
                    if ( (pax= komodo_paxmark(height,txids[i],vouts[i],'D',height)) != 0 )
                        komodo_paxdelete(pax);
                }
            } //else printf("opreturn none issued?\n");
        }
    }
    else if ( height < 236000 && opretbuf[0] == 'W' && strncmp(ASSETCHAINS_SYMBOL,(char *)&opretbuf[opretlen-4],3) == 0 )//&& opretlen >= 38 )
    {
        if ( komodo_baseid((char *)&opretbuf[opretlen-4]) >= 0 && strcmp(ASSETCHAINS_SYMBOL,(char *)&opretbuf[opretlen-4]) == 0 )
        {
            for (i=0; i<opretlen; i++)
                printf("%02x",opretbuf[i]);
            printf(" [%s] reject obsolete withdraw request.%s\n",ASSETCHAINS_SYMBOL,(char *)&opretbuf[opretlen-4]);
            return(typestr);
        }
        tokomodo = 1;
        iguana_rwnum(0,&opretbuf[34],sizeof(kmdheight),&kmdheight);
        memset(base,0,sizeof(base));
        PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&komodoshis);
        bitcoin_address(coinaddr,addrtype,rmd160,20);
        checktoshis = PAX_fiatdest(&seed,tokomodo,destaddr,pubkey33,coinaddr,kmdheight,base,value);
        typestr = "withdraw";
        //printf(" [%s] WITHDRAW %s.height.%d vs height.%d check %.8f/%.8f vs %.8f tokomodo.%d %d seed.%llx -> (%s) len.%d\n",ASSETCHAINS_SYMBOL,base,kmdheight,height,dstr(checktoshis),dstr(komodoshis),dstr(value),komodo_is_issuer(),strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0,(long long)seed,coinaddr,opretlen);
        didstats = 0;
        //if ( komodo_paxcmp(base,kmdheight,komodoshis,checktoshis,seed) == 0 )
        {
            if ( value != 0 && ((pax= komodo_paxfind(txid,vout,'W')) == 0 || pax->didstats == 0) )
            {
                if ( (basesp= komodo_stateptrget(base)) != 0 )
                {
                    basesp->withdrawn += value;
                    didstats = 1;
                    if ( 0 && strcmp(base,ASSETCHAINS_SYMBOL) == 0 )
                        printf("########### %p withdrawn %s += %.8f check %.8f\n",basesp,base,dstr(value),dstr(checktoshis));
                }
                if ( 0 && strcmp(base,"RUB") == 0 && (pax == 0 || pax->approved == 0) )
                    printf("notarize %s %.8f -> %.8f kmd.%d other.%d\n",ASSETCHAINS_SYMBOL,dstr(value),dstr(komodoshis),kmdheight,height);
            }
            komodo_gateway_deposit(coinaddr,0,(char *)"KMD",value,rmd160,txid,vout,'W',kmdheight,height,source,0);
            if ( (pax= komodo_paxfind(txid,vout,'W')) != 0 )
            {
                pax->type = opretbuf[0];
                strcpy(pax->source,base);
                strcpy(pax->symbol,"KMD");
                pax->height = kmdheight;
                pax->otherheight = height;
                pax->komodoshis = komodoshis;
            }
        } // else printf("withdraw %s paxcmp ht.%d %d error value %.8f -> %.8f vs %.8f\n",base,kmdheight,height,dstr(value),dstr(komodoshis),dstr(checktoshis));
        // need to allocate pax
    }
    else if ( height < 236000 && tokomodo != 0 && opretbuf[0] == 'A' && ASSETCHAINS_SYMBOL[0] == 0 )
    {
        tokomodo = 1;
        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
        {
            for (i=0; i<opretlen; i++)
                printf("%02x",opretbuf[i]);
            printf(" opret[%c] else path tokomodo.%d ht.%d before %.8f opretlen.%d\n",opretbuf[0],tokomodo,height,dstr(komodo_paxtotal()),opretlen);
        }
        if ( (n= komodo_issued_opreturn(base,txids,vouts,values,srcvalues,kmdheights,otherheights,baseids,rmd160s,opretbuf,opretlen,1)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                //for (j=0; j<32; j++)
                //    printf("%02x",((uint8_t *)&txids[i])[j]);
                //printf(" v%d %.8f %.8f k.%d ht.%d base.%d\n",vouts[i],dstr(values[i]),dstr(srcvalues[i]),kmdheights[i],otherheights[i],baseids[i]);
                if ( baseids[i] < 0 )
                {
                    for (i=0; i<opretlen; i++)
                        printf("%02x",opretbuf[i]);
                    printf(" opret[%c] else path tokomodo.%d ht.%d before %.8f opretlen.%d\n",opretbuf[0],tokomodo,height,dstr(komodo_paxtotal()),opretlen);
                    //printf("baseids[%d] %d\n",i,baseids[i]);
                    if ( (pax= komodo_paxfind(txids[i],vouts[i],'W')) != 0 || (pax= komodo_paxfind(txids[i],vouts[i],'X')) != 0 )
                    {
                        baseids[i] = komodo_baseid(pax->symbol);
                        printf("override neg1 with (%s)\n",pax->symbol);
                    }
                    if ( baseids[i] < 0 )
                        continue;
                }
                didstats = 0;
                seed = 0;
                checktoshis = komodo_paxprice(&seed,kmdheights[i],CURRENCIES[baseids[i]],(char *)"KMD",(uint64_t)values[i]);
                //printf("PAX_fiatdest ht.%d price %s %.8f -> KMD %.8f vs %.8f\n",kmdheights[i],CURRENCIES[baseids[i]],(double)values[i]/COIN,(double)srcvalues[i]/COIN,(double)checktoshis/COIN);
                if ( srcvalues[i] == checktoshis )
                {
                    if ( (pax= komodo_paxfind(txids[i],vouts[i],'A')) == 0 )
                    {
                        bitcoin_address(coinaddr,60,&rmd160s[i*20],20);
                        komodo_gateway_deposit(coinaddr,srcvalues[i],CURRENCIES[baseids[i]],values[i],&rmd160s[i*20],txids[i],vouts[i],'A',kmdheights[i],otherheights[i],CURRENCIES[baseids[i]],kmdheights[i]);
                        if ( (pax= komodo_paxfind(txids[i],vouts[i],'A')) == 0 )
                            printf("unexpected null pax for approve\n");
                        else pax->validated = checktoshis;
                        if ( (pax2= komodo_paxfind(txids[i],vouts[i],'W')) != 0 )
                            pax2->approved = kmdheights[i];
                        komodo_paxmark(height,txids[i],vouts[i],'W',height);
                        //komodo_paxmark(height,txids[i],vouts[i],'A',height);
                        if ( values[i] != 0 && (basesp= komodo_stateptrget(CURRENCIES[baseids[i]])) != 0 )
                        {
                            basesp->approved += values[i];
                            didstats = 1;
                            //printf("pax.%p ########### %p approved %s += %.8f -> %.8f/%.8f kht.%d %d\n",pax,basesp,CURRENCIES[baseids[i]],dstr(values[i]),dstr(srcvalues[i]),dstr(checktoshis),kmdheights[i],otherheights[i]);
                        }
                        //printf(" i.%d (%s) <- %.8f ADDFLAG APPROVED\n",i,coinaddr,dstr(values[i]));
                    }
                    else if ( pax->didstats == 0 && srcvalues[i] != 0 )
                    {
                        if ( (basesp= komodo_stateptrget(CURRENCIES[baseids[i]])) != 0 )
                        {
                            basesp->approved += values[i];
                            didstats = 1;
                            //printf("pax.%p ########### %p approved %s += %.8f -> %.8f/%.8f kht.%d %d\n",pax,basesp,CURRENCIES[baseids[i]],dstr(values[i]),dstr(srcvalues[i]),dstr(checktoshis),kmdheights[i],otherheights[i]);
                        }
                    } //else printf(" i.%d of n.%d pax.%p baseids[] %d\n",i,n,pax,baseids[i]);
                    if ( (pax= komodo_paxfind(txids[i],vouts[i],'A')) != 0 )
                    {
                        pax->type = opretbuf[0];
                        pax->approved = kmdheights[i];
                        pax->validated = checktoshis;
                        if ( didstats != 0 )
                            pax->didstats = 1;
                        //if ( strcmp(CURRENCIES[baseids[i]],ASSETCHAINS_SYMBOL) == 0 )
                        //printf(" i.%d approved.%d <<<<<<<<<<<<< APPROVED %p\n",i,kmdheights[i],pax);
                    }
                }
            }
        } //else printf("n.%d from opreturns\n",n);
        //printf("extra.[%d] after %.8f\n",n,dstr(komodo_paxtotal()));
    }
    else if ( height < 236000 && opretbuf[0] == 'X' && ASSETCHAINS_SYMBOL[0] == 0 )
    {
        tokomodo = 1;
        if ( (n= komodo_issued_opreturn(base,txids,vouts,values,srcvalues,kmdheights,otherheights,baseids,rmd160s,opretbuf,opretlen,1)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                if ( baseids[i] < 0 )
                    continue;
                bitcoin_address(coinaddr,60,&rmd160s[i*20],20);
                komodo_gateway_deposit(coinaddr,0,0,0,0,txids[i],vouts[i],'X',height,0,(char *)"KMD",0);
                komodo_paxmark(height,txids[i],vouts[i],'W',height);
                komodo_paxmark(height,txids[i],vouts[i],'A',height);
                komodo_paxmark(height,txids[i],vouts[i],'X',height);
                if ( (pax= komodo_paxfind(txids[i],vouts[i],'X')) != 0 )
                {
                    pax->type = opretbuf[0];
                    if ( height < 121842 ) // fields got switched around due to legacy issues and approves
                        value = srcvalues[i];
                    else value = values[i];
                    if ( baseids[i] >= 0 && value != 0 && (basesp= komodo_stateptrget(CURRENCIES[baseids[i]])) != 0 )
                    {
                        basesp->redeemed += value;
                        pax->didstats = 1;
                        if ( strcmp(CURRENCIES[baseids[i]],ASSETCHAINS_SYMBOL) == 0 )
                            printf("ht.%d %.8f ########### %p redeemed %s += %.8f %.8f kht.%d ht.%d\n",height,dstr(value),basesp,CURRENCIES[baseids[i]],dstr(value),dstr(srcvalues[i]),kmdheights[i],otherheights[i]);
                    }
                }
                if ( (pax= komodo_paxmark(height,txids[i],vouts[i],'W',height)) != 0 )
                    komodo_paxdelete(pax);
                if ( (pax= komodo_paxmark(height,txids[i],vouts[i],'A',height)) != 0 )
                    komodo_paxdelete(pax);
                if ( (pax= komodo_paxmark(height,txids[i],vouts[i],'X',height)) != 0 )
                    komodo_paxdelete(pax);
            }
        } //else printf("komodo_issued_opreturn returned %d\n",n);
    }
    return(typestr);
}

int32_t komodo_parsestatefiledata(struct komodo_state *sp,uint8_t *filedata,long *fposp,long datalen,char *symbol,char *dest);

void komodo_stateind_set(struct komodo_state *sp,uint32_t *inds,int32_t n,uint8_t *filedata,long datalen,char *symbol,char *dest)
{
    uint8_t func; long lastK,lastT,lastN,lastV,fpos,lastfpos; int32_t i,count,doissue,iter,numn,numv,numN,numV,numR; uint32_t tmp,prevpos100,offset;
    count = numR = numN = numV = numn = numv = 0;
    lastK = lastT = lastN = lastV = -1;
    for (iter=0; iter<2; iter++)
    {
        for (lastfpos=fpos=prevpos100=i=0; i<n; i++)
        {
            tmp = inds[i];
            if ( (i % 100) == 0 )
                prevpos100 = tmp;
            else
            {
                func = (tmp & 0xff);
                offset = (tmp >> 8);
                fpos = prevpos100 + offset;
                if ( lastfpos >= datalen || (filedata[lastfpos] != func && func != 0) )
                    printf("i.%d/n.%d lastfpos.%ld >= datalen.%ld or [%d] != func.%d\n",i,n,lastfpos,datalen,filedata[lastfpos],func);
                else if ( iter == 0 )
                {
                    switch ( func )
                    {
                        default: case 'P': case 'U': case 'D':
                            inds[i] &= 0xffffff00;
                            break;
                        case 'K':
                            lastK = lastfpos;
                            inds[i] &= 0xffffff00;
                            break;
                        case 'T':
                            lastT = lastfpos;
                            inds[i] &= 0xffffff00;
                            break;
                        case 'N':
                            lastN = lastfpos;
                            numN++;
                            break;
                        case 'V':
                            lastV = lastfpos;
                            numV++;
                            break;
                        case 'R':
                            numR++;
                            break;
                    }
                }
                else
                {
                    doissue = 0;
                    if ( func == 'K' )
                    {
                        if ( lastK == lastfpos )
                            doissue = 1;
                    }
                    else if ( func == 'T' )
                    {
                        if ( lastT == lastfpos )
                            doissue = 1;
                    }
                    else if ( func == 'N' )
                    {
                        if ( numn > numN-128 )
                            doissue = 1;
                        numn++;
                    }
                    else if ( func == 'V' )
                    {
                        if ( KOMODO_PAX != 0 && numv > numV-1440 )
                            doissue = 1;
                        numv++;
                    }
                    else if ( func == 'R' )
                        doissue = 1;
                    if ( doissue != 0 )
                    {
                        //printf("issue %c total.%d lastfpos.%ld\n",func,count,lastfpos);
                        komodo_parsestatefiledata(sp,filedata,&lastfpos,datalen,symbol,dest);
                        count++;
                    }
                }
            }
            lastfpos = fpos;
        }
    }
    printf("numR.%d numV.%d numN.%d count.%d\n",numR,numV,numN,count);
    /*else if ( func == 'K' ) // KMD height: stop after 1st
    else if ( func == 'T' ) // KMD height+timestamp: stop after 1st

    else if ( func == 'N' ) // notarization, scan backwards 1440+ blocks;
    else if ( func == 'V' ) // price feed: can stop after 1440+
    else if ( func == 'R' ) // opreturn:*/
}

void *OS_loadfile(char *fname,uint8_t **bufp,long *lenp,long *allocsizep)
{
    FILE *fp;
    long  filesize,buflen = *allocsizep;
    uint8_t *buf = *bufp;
    *lenp = 0;
    if ( (fp= fopen(fname,"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        filesize = ftell(fp);
        if ( filesize == 0 )
        {
            fclose(fp);
            *lenp = 0;
            printf("OS_loadfile null size.(%s)\n",fname);
            return(0);
        }
        if ( filesize > buflen )
        {
            *allocsizep = filesize;
            *bufp = buf = (uint8_t *)realloc(buf,(long)*allocsizep+64);
        }
        rewind(fp);
        if ( buf == 0 )
            printf("Null buf ???\n");
        else
        {
            if ( fread(buf,1,(long)filesize,fp) != (unsigned long)filesize )
                printf("error reading filesize.%ld\n",(long)filesize);
            buf[filesize] = 0;
        }
        fclose(fp);
        *lenp = filesize;
        //printf("loaded.(%s)\n",buf);
    } //else printf("OS_loadfile couldnt load.(%s)\n",fname);
    return(buf);
}

uint8_t *OS_fileptr(long *allocsizep,char *fname)
{
    long filesize = 0; uint8_t *buf = 0; void *retptr;
    *allocsizep = 0;
    retptr = OS_loadfile(fname,&buf,&filesize,allocsizep);
    return((uint8_t *)retptr);
}

long komodo_stateind_validate(struct komodo_state *sp,char *indfname,uint8_t *filedata,long datalen,uint32_t *prevpos100p,uint32_t *indcounterp,char *symbol,char *dest)
{
    FILE *fp; long fsize,lastfpos=0,fpos=0; uint8_t *inds,func; int32_t i,n; uint32_t offset,tmp,prevpos100 = 0;
    *indcounterp = *prevpos100p = 0;
    if ( (inds= OS_fileptr(&fsize,indfname)) != 0 )
    {
        lastfpos = 0;
        fprintf(stderr,"inds.%p validate %s fsize.%ld datalen.%ld n.%d lastfpos.%ld\n",inds,indfname,fsize,datalen,(int32_t)(fsize / sizeof(uint32_t)),lastfpos);
        if ( (fsize % sizeof(uint32_t)) == 0 )
        {
            n = (int32_t)(fsize / sizeof(uint32_t));
            for (i=0; i<n; i++)
            {
                memcpy(&tmp,&inds[i * sizeof(uint32_t)],sizeof(uint32_t));
                if ( 0 && i > n-10 )
                    printf("%d: tmp.%08x [%c] prevpos100.%u\n",i,tmp,tmp&0xff,prevpos100);
                if ( (i % 100) == 0 )
                    prevpos100 = tmp;
                else
                {
                    func = (tmp & 0xff);
                    offset = (tmp >> 8);
                    fpos = prevpos100 + offset;
                    if ( lastfpos >= datalen || filedata[lastfpos] != func )
                    {
                        printf("validate.%d error (%u %d) prev100 %u -> fpos.%ld datalen.%ld [%d] (%c) vs (%c) lastfpos.%ld\n",i,offset,func,prevpos100,fpos,datalen,lastfpos < datalen ? filedata[lastfpos] : -1,func,filedata[lastfpos],lastfpos);
                        return(-1);
                    }
                }
                lastfpos = fpos;
            }
            *indcounterp = n;
            *prevpos100p = prevpos100;
            if ( sp != 0 )
                komodo_stateind_set(sp,(uint32_t *)inds,n,filedata,fpos,symbol,dest);
            //printf("free inds.%p %s validated[%d] fpos.%ld datalen.%ld, offset %ld vs fsize.%ld\n",inds,indfname,i,fpos,datalen,i * sizeof(uint32_t),fsize);
            free(inds);
            return(fpos);
        } else printf("wrong filesize %s %ld\n",indfname,fsize);
    }
    free(inds);
    fprintf(stderr,"indvalidate return -1\n");
    return(-1);
}

long komodo_indfile_update(FILE *indfp,uint32_t *prevpos100p,long lastfpos,long newfpos,uint8_t func,uint32_t *indcounterp)
{
    uint32_t tmp;
    if ( indfp != 0 )
    {
        tmp = ((uint32_t)(newfpos - *prevpos100p) << 8) | (func & 0xff);
        if ( ftell(indfp)/sizeof(uint32_t) != *indcounterp )
            printf("indfp fpos %ld -> ind.%ld vs counter.%u\n",ftell(indfp),ftell(indfp)/sizeof(uint32_t),*indcounterp);
        //fprintf(stderr,"ftell.%ld indcounter.%u lastfpos.%ld newfpos.%ld func.%02x\n",ftell(indfp),*indcounterp,lastfpos,newfpos,func);
        fwrite(&tmp,1,sizeof(tmp),indfp), (*indcounterp)++;
        if ( (*indcounterp % 100) == 0 )
        {
            *prevpos100p = (uint32_t)newfpos;
            fwrite(prevpos100p,1,sizeof(*prevpos100p),indfp), (*indcounterp)++;
        }
    }
    return(newfpos);
}

int32_t komodo_faststateinit(struct komodo_state *sp,char *fname,char *symbol,char *dest)
{
    FILE *indfp; char indfname[1024]; uint8_t *filedata; long validated=-1,datalen,fpos,lastfpos; uint32_t tmp,prevpos100,indcounter,starttime; int32_t func,finished = 0;
    starttime = (uint32_t)time(NULL);
    safecopy(indfname,fname,sizeof(indfname)-4);
    strcat(indfname,".ind");
    if ( (filedata= OS_fileptr(&datalen,fname)) != 0 )
    {
        if ( 1 )//datalen >= (1LL << 32) || GetArg("-genind",0) != 0 || (validated= komodo_stateind_validate(0,indfname,filedata,datalen,&prevpos100,&indcounter,symbol,dest)) < 0 )
        {
            lastfpos = fpos = 0;
            indcounter = prevpos100 = 0;
            if ( (indfp= fopen(indfname,"wb")) != 0 )
                fwrite(&prevpos100,1,sizeof(prevpos100),indfp), indcounter++;
            fprintf(stderr,"processing %s %ldKB, validated.%ld\n",fname,datalen/1024,validated);
            while ( (func= komodo_parsestatefiledata(sp,filedata,&fpos,datalen,symbol,dest)) >= 0 )
            {
                lastfpos = komodo_indfile_update(indfp,&prevpos100,lastfpos,fpos,func,&indcounter);
            }
            if ( indfp != 0 )
            {
                fclose(indfp);
                if ( (fpos= komodo_stateind_validate(0,indfname,filedata,datalen,&prevpos100,&indcounter,symbol,dest)) < 0 )
                    printf("unexpected komodostate.ind validate failure %s datalen.%ld\n",indfname,datalen);
                else printf("%s validated fpos.%ld\n",indfname,fpos);
            }
            finished = 1;
            fprintf(stderr,"took %d seconds to process %s %ldKB\n",(int32_t)(time(NULL)-starttime),fname,datalen/1024);
        }
        else if ( validated > 0 )
        {
            if ( (indfp= fopen(indfname,"rb+")) != 0 )
            {
                lastfpos = fpos = validated;
                fprintf(stderr,"datalen.%ld validated %ld -> indcounter %u, prevpos100 %u offset.%d\n",datalen,validated,indcounter,prevpos100,(int32_t)(indcounter * sizeof(uint32_t)));
                if ( fpos < datalen )
                {
                    fseek(indfp,indcounter * sizeof(uint32_t),SEEK_SET);
                    if ( ftell(indfp) == indcounter * sizeof(uint32_t) )
                    {
                        while ( (func= komodo_parsestatefiledata(sp,filedata,&fpos,datalen,symbol,dest)) >= 0 )
                        {
                            lastfpos = komodo_indfile_update(indfp,&prevpos100,lastfpos,fpos,func,&indcounter);
                            if ( lastfpos != fpos )
                                fprintf(stderr,"unexpected lastfpos.%ld != %ld\n",lastfpos,fpos);
                        }
                    }
                    fclose(indfp);
                }
                if ( komodo_stateind_validate(sp,indfname,filedata,datalen,&prevpos100,&indcounter,symbol,dest) < 0 )
                    printf("unexpected komodostate.ind validate failure %s datalen.%ld\n",indfname,datalen);
                else
                {
                    printf("%s validated updated from validated.%ld to %ld new.[%ld] -> indcounter %u, prevpos100 %u offset.%ld | elapsed %d seconds\n",indfname,validated,fpos,fpos-validated,indcounter,prevpos100,indcounter * sizeof(uint32_t),(int32_t)(time(NULL) - starttime));
                    finished = 1;
                }
            }
        } else printf("komodo_faststateinit unexpected case\n");
        free(filedata);
        return(finished == 1);
    }
    return(-1);
}

uint64_t komodo_interestsum();

void komodo_passport_iteration()
{
    static long lastpos[34]; static char userpass[33][1024]; static uint32_t lasttime,callcounter,lastinterest;
    int32_t maxseconds = 10;
    FILE *fp; uint8_t *filedata; long fpos,datalen,lastfpos; int32_t baseid,limit,n,ht,isrealtime,expired,refid,blocks,longest; struct komodo_state *sp,*refsp; char *retstr,fname[512],*base,symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; uint32_t buf[3],starttime; uint64_t RTmask = 0; //CBlockIndex *pindex;
    expired = 0;
    while ( 0 && KOMODO_INITDONE == 0 )
    {
        fprintf(stderr,"[%s] PASSPORT iteration waiting for KOMODO_INITDONE\n",ASSETCHAINS_SYMBOL);
        sleep(3);
    }
    if ( komodo_chainactive_timestamp() > lastinterest )
    {
        if ( ASSETCHAINS_SYMBOL[0] == 0 )
            komodo_interestsum();
        //komodo_longestchain();
        lastinterest = komodo_chainactive_timestamp();
    }
    refsp = komodo_stateptr(symbol,dest);
    if ( ASSETCHAINS_SYMBOL[0] == 0 || strcmp(ASSETCHAINS_SYMBOL,"KMDCC") == 0 )
    {
        refid = 33;
        limit = 10000000;
        jumblr_iteration();
    }
    else
    {
        limit = 10000000;
        refid = komodo_baseid(ASSETCHAINS_SYMBOL)+1; // illegal base -> baseid.-1 -> 0
        if ( refid == 0 )
        {
            KOMODO_PASSPORT_INITDONE = 1;
            return;
        }
    }
    /*if ( KOMODO_PAX == 0 )
    {
        KOMODO_PASSPORT_INITDONE = 1;
        return;
    }*/
    starttime = (uint32_t)time(NULL);
    if ( callcounter++ < 1 )
        limit = 10000;
    lasttime = starttime;
    for (baseid=32; baseid>=0; baseid--)
    {
        if ( time(NULL) >= starttime+maxseconds )
            break;
        sp = 0;
        isrealtime = 0;
        base = (char *)CURRENCIES[baseid];
        //printf("PASSPORT %s baseid+1 %d refid.%d\n",ASSETCHAINS_SYMBOL,baseid+1,refid);
        if ( baseid+1 != refid ) // only need to import state from a different coin
        {
            if ( baseid == 32 ) // only care about KMD's state
            {
                refsp->RTmask &= ~(1LL << baseid);
                komodo_statefname(fname,baseid<32?base:(char *)"",(char *)"komodostate");
                komodo_nameset(symbol,dest,base);
                sp = komodo_stateptrget(symbol);
                n = 0;
                if ( lastpos[baseid] == 0 && (filedata= OS_fileptr(&datalen,fname)) != 0 )
                {
                    fpos = 0;
                    fprintf(stderr,"%s processing %s %ldKB\n",ASSETCHAINS_SYMBOL,fname,datalen/1024);
                    while ( komodo_parsestatefiledata(sp,filedata,&fpos,datalen,symbol,dest) >= 0 )
                        lastfpos = fpos;
                    fprintf(stderr,"%s took %d seconds to process %s %ldKB\n",ASSETCHAINS_SYMBOL,(int32_t)(time(NULL)-starttime),fname,datalen/1024);
                    lastpos[baseid] = lastfpos;
                    free(filedata), filedata = 0;
                    datalen = 0;
                }
                else if ( (fp= fopen(fname,"rb")) != 0 && sp != 0 )
                {
                    fseek(fp,0,SEEK_END);
                    //fprintf(stderr,"couldnt OS_fileptr(%s), freading %ldKB\n",fname,ftell(fp)/1024);
                    if ( ftell(fp) > lastpos[baseid] )
                    {
                        if ( ASSETCHAINS_SYMBOL[0] != 0 )
                            printf("%s passport refid.%d %s fname.(%s) base.%s %ld %ld\n",ASSETCHAINS_SYMBOL,refid,symbol,fname,base,ftell(fp),lastpos[baseid]);
                        fseek(fp,lastpos[baseid],SEEK_SET);
                        while ( komodo_parsestatefile(sp,fp,symbol,dest) >= 0 && n < limit )
                        {
                            if ( n == limit-1 )
                            {
                                if ( time(NULL) < starttime+maxseconds )
                                    n = 0;
                                else
                                {
                                    //printf("expire passport loop %s -> %s at %ld\n",ASSETCHAINS_SYMBOL,base,lastpos[baseid]);
                                    expired++;
                                }
                            }
                            n++;
                        }
                        lastpos[baseid] = ftell(fp);
                        if ( 0 && lastpos[baseid] == 0 && strcmp(symbol,"KMD") == 0 )
                            printf("from.(%s) lastpos[%s] %ld isrt.%d\n",ASSETCHAINS_SYMBOL,CURRENCIES[baseid],lastpos[baseid],komodo_isrealtime(&ht));
                    } //else fprintf(stderr,"%s.%ld ",CURRENCIES[baseid],ftell(fp));
                    fclose(fp);
                } else fprintf(stderr,"load error.(%s) %p\n",fname,sp);
                komodo_statefname(fname,baseid<32?base:(char *)"",(char *)"realtime");
                if ( (fp= fopen(fname,"rb")) != 0 )
                {
                    if ( fread(buf,1,sizeof(buf),fp) == sizeof(buf) )
                    {
                        sp->CURRENT_HEIGHT = buf[0];
                        if ( buf[0] != 0 && buf[0] >= buf[1] && buf[2] > time(NULL)-60 )
                        {
                            isrealtime = 1;
                            RTmask |= (1LL << baseid);
                            memcpy(refsp->RTbufs[baseid+1],buf,sizeof(refsp->RTbufs[baseid+1]));
                        }
                        else if ( KOMODO_PAX != 0 && (time(NULL)-buf[2]) > 60 && ASSETCHAINS_SYMBOL[0] != 0 )
                            fprintf(stderr,"[%s]: %s not RT %u %u %d\n",ASSETCHAINS_SYMBOL,base,buf[0],buf[1],(int32_t)(time(NULL)-buf[2]));
                    } //else fprintf(stderr,"%s size error RT\n",base);
                    fclose(fp);
                } //else fprintf(stderr,"%s open error RT\n",base);
            }
        }
        else
        {
            refsp->RTmask &= ~(1LL << baseid);
            komodo_statefname(fname,baseid<32?base:(char *)"",(char *)"realtime");
            if ( (fp= fopen(fname,"wb")) != 0 )
            {
                buf[0] = (uint32_t)chainActive.LastTip()->GetHeight();
                buf[1] = (uint32_t)komodo_longestchain();
                if ( buf[0] != 0 && buf[0] == buf[1] )
                {
                    buf[2] = (uint32_t)time(NULL);
                    RTmask |= (1LL << baseid);
                    memcpy(refsp->RTbufs[baseid+1],buf,sizeof(refsp->RTbufs[baseid+1]));
                    if ( refid != 0 )
                        memcpy(refsp->RTbufs[0],buf,sizeof(refsp->RTbufs[0]));
                }
                if ( fwrite(buf,1,sizeof(buf),fp) != sizeof(buf) )
                    fprintf(stderr,"[%s] %s error writing realtime\n",ASSETCHAINS_SYMBOL,base);
                fclose(fp);
            } else fprintf(stderr,"%s create error RT\n",base);
        }
        if ( sp != 0 && isrealtime == 0 )
            refsp->RTbufs[0][2] = 0;
    }
    //komodo_paxtotal(); // calls komodo_isrealtime(), which calls komodo_longestchain()
    refsp->RTmask |= RTmask;
    if ( expired == 0 && KOMODO_PASSPORT_INITDONE == 0 )
    {
        KOMODO_PASSPORT_INITDONE = 1;
        printf("READY for %s RPC calls at %u! done PASSPORT %s refid.%d\n",ASSETCHAINS_SYMBOL,(uint32_t)time(NULL),ASSETCHAINS_SYMBOL,refid);
    }
}


extern std::vector<uint8_t> Mineropret; // opreturn data set by the data gathering code
#define PRICES_ERRORRATE (COIN / 100)	  // maximum acceptable change, set at 1%
#define PRICES_SIZEBIT0 (sizeof(uint32_t) * 4) // 4 uint32_t unixtimestamp, BTCUSD, BTCGBP and BTCEUR
#define KOMODO_LOCALPRICE_CACHESIZE 13
#define KOMODO_MAXPRICES 2048
#define PRICES_SMOOTHWIDTH 1

#define issue_curl(cmdstr) bitcoind_RPC(0,(char *)"CBCOINBASE",cmdstr,0,0,0)

const char *Cryptos[] = { "KMD", "ETH" }; // must be on binance (for now)
// "LTC", "BCHABC", "XMR", "IOTA", "ZEC", "WAVES",  "LSK", "DCR", "RVN", "DASH", "XEM", "BTS", "ICX", "HOT", "STEEM", "ENJ", "STRAT"
const char *Forex[] =
{ "BGN","NZD","ILS","RUB","CAD","PHP","CHF","AUD","JPY","TRY","HKD","MYR","HRK","CZK","IDR","DKK","NOK","HUF","GBP","MXN","THB","ISK","ZAR","BRL","SGD","PLN","INR","KRW","RON","CNY","SEK","EUR"
}; // must be in ECB list

struct komodo_extremeprice
{
    uint256 blockhash;
    uint32_t pricebits,timestamp;
    int32_t height;
    int16_t dir,ind;
} ExtremePrice;

struct komodo_priceinfo
{
    FILE *fp;
    char symbol[64];
} PRICES[KOMODO_MAXPRICES];

uint32_t PriceCache[KOMODO_LOCALPRICE_CACHESIZE][KOMODO_MAXPRICES];//4+sizeof(Cryptos)/sizeof(*Cryptos)+sizeof(Forex)/sizeof(*Forex)];
int64_t PriceMult[KOMODO_MAXPRICES];
int32_t komodo_cbopretsize(uint64_t flags);

void komodo_PriceCache_shift()
{
    int32_t i;
    for (i=KOMODO_LOCALPRICE_CACHESIZE-1; i>0; i--)
        memcpy(PriceCache[i],PriceCache[i-1],sizeof(PriceCache[i]));
    memcpy(PriceCache[0],Mineropret.data(),Mineropret.size());
}

int32_t _komodo_heightpricebits(uint64_t *seedp,uint32_t *heightbits,CBlock *block)
{
    CTransaction tx; int32_t numvouts; std::vector<uint8_t> vopret;
    tx = block->vtx[0];
    numvouts = (int32_t)tx.vout.size();
    GetOpReturnData(tx.vout[numvouts-1].scriptPubKey,vopret);
    if ( vopret.size() >= PRICES_SIZEBIT0 )
    {
        if ( seedp != 0 )
            memcpy(seedp,&block->hashMerkleRoot,sizeof(*seedp));
        memcpy(heightbits,vopret.data(),vopret.size());
        return((int32_t)(vopret.size()/sizeof(uint32_t)));
    }
    return(-1);
}

// komodo_heightpricebits() extracts the price data in the coinbase for nHeight
int32_t komodo_heightpricebits(uint64_t *seedp,uint32_t *heightbits,int32_t nHeight)
{
    CBlockIndex *pindex; CBlock block;
    if ( seedp != 0 )
        *seedp = 0;
    if ( (pindex= komodo_chainactive(nHeight)) != 0 )
    {
        if ( komodo_blockload(block,pindex) == 0 )
        {
            return(_komodo_heightpricebits(seedp,heightbits,&block));
        }
    }
    fprintf(stderr,"couldnt get pricebits for %d\n",nHeight);
    return(-1);
}

/*
 komodo_pricenew() is passed in a reference price, the change tolerance and the proposed price. it needs to return a clipped price if it is too big and also set a flag if it is at or above the limit
 */
uint32_t komodo_pricenew(char *maxflagp,uint32_t price,uint32_t refprice,int64_t tolerance)
{
    uint64_t highprice,lowprice;
    if ( refprice < 2 )
        return(0);
    highprice = ((uint64_t)refprice * (COIN + tolerance)) / COIN; // calc highest acceptable price
    lowprice = ((uint64_t)refprice * (COIN - tolerance)) / COIN;  // and lowest
    if ( highprice == refprice )
        highprice++;
    if ( lowprice == refprice )
        lowprice--;
    if ( price >= highprice )
    {
        //fprintf(stderr,"high %u vs h%llu l%llu tolerance.%llu\n",price,(long long)highprice,(long long)lowprice,(long long)tolerance);
        if ( price > highprice ) // return non-zero only if we violate the tolerance
        {
            *maxflagp = 2;
            return(highprice);
        }
        *maxflagp = 1;
    }
    else if ( price <= lowprice )
    {
        //fprintf(stderr,"low %u vs h%llu l%llu tolerance.%llu\n",price,(long long)highprice,(long long)lowprice,(long long)tolerance);
        if ( price < lowprice )
        {
            *maxflagp = -2;
            return(lowprice);
        }
        *maxflagp = -1;
    }
    return(0);
}

// komodo_pricecmp() returns -1 if any of the prices are beyond the tolerance
int32_t komodo_pricecmp(int32_t nHeight,int32_t n,char *maxflags,uint32_t *pricebitsA,uint32_t *pricebitsB,int64_t tolerance)
{
    int32_t i; uint32_t newprice;
    for (i=1; i<n; i++)
    {
        if ( (newprice= komodo_pricenew(&maxflags[i],pricebitsA[i],pricebitsB[i],tolerance)) != 0 )
        {
            fprintf(stderr,"ht.%d i.%d/%d %u vs %u -> newprice.%u out of tolerance maxflag.%d\n",nHeight,i,n,pricebitsB[i],pricebitsA[i],newprice,maxflags[i]);
            return(-1);
        }
    }
    return(0);
}

// komodo_priceclamp() clamps any price that is beyond tolerance
int32_t komodo_priceclamp(int32_t n,uint32_t *pricebits,uint32_t *refprices,int64_t tolerance)
{
    int32_t i; uint32_t newprice; char maxflags[KOMODO_MAXPRICES];
    memset(maxflags,0,sizeof(maxflags));
    for (i=1; i<n; i++)
    {
        if ( (newprice= komodo_pricenew(&maxflags[i],pricebits[i],refprices[i],tolerance)) != 0 )
        {
            fprintf(stderr,"priceclamped[%d of %d] %u vs %u -> %u\n",i,n,refprices[i],pricebits[i],newprice);
            pricebits[i] = newprice;
        }
    }
    return(0);
}

// komodo_mineropret() returns a valid pricedata to add to the coinbase opreturn for nHeight
CScript komodo_mineropret(int32_t nHeight)
{
    CScript opret; char maxflags[KOMODO_MAXPRICES]; uint32_t pricebits[KOMODO_MAXPRICES],prevbits[KOMODO_MAXPRICES]; int32_t maxflag,i,n,numzero=0;
    if ( Mineropret.size() >= PRICES_SIZEBIT0 )
    {
        n = (int32_t)(Mineropret.size() / sizeof(uint32_t));
        numzero = 1;
        while ( numzero > 0 )
        {
            memcpy(pricebits,Mineropret.data(),Mineropret.size());
            for (i=numzero=0; i<n; i++)
                if ( pricebits[i] == 0 )
                {
                    fprintf(stderr,"%d ",i);
                    numzero++;
                }
            if ( numzero != 0 )
            {
                fprintf(stderr," komodo_mineropret numzero.%d vs n.%d\n",numzero,n);
                komodo_cbopretupdate(1);
                sleep(61);
            }
        }
        if ( komodo_heightpricebits(0,prevbits,nHeight-1) > 0 )
        {
            memcpy(pricebits,Mineropret.data(),Mineropret.size());
            memset(maxflags,0,sizeof(maxflags));
            if ( komodo_pricecmp(0,n,maxflags,pricebits,prevbits,PRICES_ERRORRATE) < 0 )
            {
                // if the new prices are outside tolerance, update Mineropret with clamped prices
                komodo_priceclamp(n,pricebits,prevbits,PRICES_ERRORRATE);
                //fprintf(stderr,"update Mineropret to clamped prices\n");
                memcpy(Mineropret.data(),pricebits,Mineropret.size());
            }
        }
        int32_t i;
        for (i=0; i<Mineropret.size(); i++)
            fprintf(stderr,"%02x",Mineropret[i]);
        fprintf(stderr," <- Mineropret\n");
        return(opret << OP_RETURN << Mineropret);
    }
    return(opret);
}

/*
 komodo_opretvalidate() is the entire price validation!
 it prints out some useful info for debugging, like the lag from current time and prev block and the prices encoded in the opreturn.
 
 The only way komodo_opretvalidate() doesnt return an error is if maxflag is set or it is within tolerance of both the prior block and the local data. The local data validation only happens if it is a recent block and not a block from the past as the local node is only getting the current price data.
 
 */


void komodo_queuelocalprice(int32_t dir,int32_t height,uint32_t timestamp,uint256 blockhash,int32_t ind,uint32_t pricebits)
{
    fprintf(stderr,"ExtremePrice dir.%d ht.%d ind.%d cmpbits.%u\n",dir,height,ind,pricebits);
    ExtremePrice.dir = dir;
    ExtremePrice.height = height;
    ExtremePrice.blockhash = blockhash;
    ExtremePrice.ind = ind;
    ExtremePrice.timestamp = timestamp;
    ExtremePrice.pricebits = pricebits;
}

int32_t komodo_opretvalidate(const CBlock *block,CBlockIndex * const previndex,int32_t nHeight,CScript scriptPubKey)
{
    int32_t testchain_exemption = 0;
    std::vector<uint8_t> vopret; char maxflags[KOMODO_MAXPRICES]; uint256 bhash; double btcusd,btcgbp,btceur; uint32_t localbits[KOMODO_MAXPRICES],pricebits[KOMODO_MAXPRICES],prevbits[KOMODO_MAXPRICES],newprice; int32_t i,j,prevtime,maxflag,lag,lag2,lag3,n,errflag,iter; uint32_t now;
    now = (uint32_t)time(NULL);
    if ( ASSETCHAINS_CBOPRET != 0 && nHeight > 0 )
    {
        bhash = block->GetHash();
        GetOpReturnData(scriptPubKey,vopret);
        if ( vopret.size() >= PRICES_SIZEBIT0 )
        {
            n = (int32_t)(vopret.size() / sizeof(uint32_t));
            memcpy(pricebits,vopret.data(),Mineropret.size());
            memset(maxflags,0,sizeof(maxflags));
            if ( nHeight > 2 )
            {
                prevtime = previndex->nTime;
                lag = (int32_t)(now - pricebits[0]);
                lag2 = (int32_t)(pricebits[0] - prevtime);
                lag3 = (int32_t)(block->nTime - pricebits[0]);
                if ( lag < -60 ) // avoid data from future
                {
                    fprintf(stderr,"A ht.%d now.%u htstamp.%u %u - pricebits[0] %u -> lags.%d %d %d\n",nHeight,now,prevtime,block->nTime,pricebits[0],lag,lag2,lag3);
                    return(-1);
                }
                if ( lag2 < -60 ) //testchain_exemption ) // must be close to last block timestamp
                {
                    fprintf(stderr,"B ht.%d now.%u htstamp.%u %u - pricebits[0] %u -> lags.%d %d %d vs %d cmp.%d\n",nHeight,now,prevtime,block->nTime,pricebits[0],lag,lag2,lag3,ASSETCHAINS_BLOCKTIME,lag2<-ASSETCHAINS_BLOCKTIME);
                    if ( nHeight > testchain_exemption )
                        return(-1);
                }
                if ( lag3 < -60 || lag3 > ASSETCHAINS_BLOCKTIME )
                {
                    fprintf(stderr,"C ht.%d now.%u htstamp.%u %u - pricebits[0] %u -> lags.%d %d %d\n",nHeight,now,prevtime,block->nTime,pricebits[0],lag,lag2,lag3);
                    if ( nHeight > testchain_exemption )
                        return(-1);
                }
                btcusd = (double)pricebits[1]/10000;
                btcgbp = (double)pricebits[2]/10000;
                btceur = (double)pricebits[3]/10000;
                fprintf(stderr,"ht.%d: lag.%d %.4f USD, %.4f GBP, %.4f EUR, GBPUSD %.6f, EURUSD %.6f, EURGBP %.6f [%d]\n",nHeight,lag,btcusd,btcgbp,btceur,btcusd/btcgbp,btcusd/btceur,btcgbp/btceur,lag2);
                if ( komodo_heightpricebits(0,prevbits,nHeight-1) > 0 )
                {
                    if ( nHeight < testchain_exemption )
                    {
                        for (i=0; i<n; i++)
                            if ( pricebits[i] == 0 )
                                pricebits[i] = prevbits[i];
                    }
                    if ( komodo_pricecmp(nHeight,n,maxflags,pricebits,prevbits,PRICES_ERRORRATE) < 0 )
                    {
                        for (i=1; i<n; i++)
                            fprintf(stderr,"%.4f ",(double)prevbits[i]/10000);
                        fprintf(stderr," oldprices.%d\n",nHeight);
                        for (i=1; i<n; i++)
                            fprintf(stderr,"%.4f ",(double)pricebits[i]/10000);
                        fprintf(stderr," newprices.%d\n",nHeight);

                        fprintf(stderr,"vs prev maxflag.%d cmp error\n",maxflag);
                        return(-1);
                    } // else this is the good case we hope to happen
                } else return(-1);
                if ( lag < ASSETCHAINS_BLOCKTIME && Mineropret.size() >= PRICES_SIZEBIT0 )
                {
                    memcpy(localbits,Mineropret.data(),Mineropret.size());
                    if ( nHeight < testchain_exemption )
                    {
                        for (i=0; i<n; i++)
                            if ( localbits[i] == 0 )
                                localbits[i] = prevbits[i];
                    }
                    for (iter=0; iter<2; iter++) // first iter should just refresh prices if out of tolerance
                    {
                        for (i=1; i<n; i++)
                        {
                            if ( (maxflag= maxflags[i]) != 0 && localbits[i] != 0 )
                            {
                                // make sure local price is moving in right direction
                                fprintf(stderr,"maxflag.%d i.%d localbits.%u vs pricebits.%u prevbits.%u\n",maxflag,i,localbits[i],pricebits[i],prevbits[i]);
                                if ( maxflag > 0 && localbits[i] < prevbits[i] )
                                {
                                    if ( iter == 0 )
                                        break;
                                    // second iteration checks recent prices to see if within local volatility
                                    for (j=0; j<KOMODO_LOCALPRICE_CACHESIZE; j++)
                                        if ( PriceCache[j][i] >= prevbits[i] )
                                        {
                                            fprintf(stderr,"i.%d within recent localprices[%d] %u >= %u\n",i,j,PriceCache[j][i],prevbits[i]);
                                            break;
                                        }
                                    if ( j == KOMODO_LOCALPRICE_CACHESIZE )
                                    {
                                        komodo_queuelocalprice(1,nHeight,block->nTime,bhash,i,prevbits[i]);
                                        break;
                                    }
                                }
                                else if ( maxflag < 0 && localbits[i] > prevbits[i] )
                                {
                                    if ( iter == 0 )
                                        break;
                                    for (j=0; j<KOMODO_LOCALPRICE_CACHESIZE; j++)
                                        if ( PriceCache[j][i] <= prevbits[i] )
                                        {
                                            fprintf(stderr,"i.%d within recent localprices[%d] %u <= prev %u\n",i,j,PriceCache[j][i],prevbits[i]);
                                            break;
                                        }
                                    if ( j == KOMODO_LOCALPRICE_CACHESIZE )
                                    {
                                        komodo_queuelocalprice(-1,nHeight,block->nTime,bhash,i,prevbits[i]);
                                        break;
                                    }
                                }
                            }
                        }
                        if ( i != n )
                        {
                            if ( iter == 0 )
                            {
                                fprintf(stderr,"force update prices\n");
                                komodo_cbopretupdate(1);
                                memcpy(localbits,Mineropret.data(),Mineropret.size());
                            } else return(-1);
                        }
                    }
                }
            }
            if ( bhash == ExtremePrice.blockhash )
            {
                fprintf(stderr,"approved a previously extreme price based on new data ht.%d vs %u vs %u\n",ExtremePrice.height,ExtremePrice.timestamp,(uint32_t)block->nTime);
                memset(&ExtremePrice,0,sizeof(ExtremePrice));
            }
            return(0);
        } else fprintf(stderr,"wrong size %d vs %d, scriptPubKey size %d [%02x]\n",(int32_t)vopret.size(),(int32_t)Mineropret.size(),(int32_t)scriptPubKey.size(),scriptPubKey[0]);
        return(-1);
    }
    return(0);
}

char *nonportable_path(char *str)
{
    int32_t i;
    for (i=0; str[i]!=0; i++)
        if ( str[i] == '/' )
            str[i] = '\\';
    return(str);
}

char *portable_path(char *str)
{
#ifdef _WIN32
    return(nonportable_path(str));
#else
#ifdef __PNACL
    /*int32_t i,n;
     if ( str[0] == '/' )
     return(str);
     else
     {
     n = (int32_t)strlen(str);
     for (i=n; i>0; i--)
     str[i] = str[i-1];
     str[0] = '/';
     str[n+1] = 0;
     }*/
#endif
    return(str);
#endif
}

void *loadfile(char *fname,uint8_t **bufp,long *lenp,long *allocsizep)
{
    FILE *fp;
    long  filesize,buflen = *allocsizep;
    uint8_t *buf = *bufp;
    *lenp = 0;
    if ( (fp= fopen(portable_path(fname),"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        filesize = ftell(fp);
        if ( filesize == 0 )
        {
            fclose(fp);
            *lenp = 0;
            //printf("loadfile null size.(%s)\n",fname);
            return(0);
        }
        if ( filesize > buflen )
        {
            *allocsizep = filesize;
            *bufp = buf = (uint8_t *)realloc(buf,(long)*allocsizep+64);
        }
        rewind(fp);
        if ( buf == 0 )
            printf("Null buf ???\n");
        else
        {
            if ( fread(buf,1,(long)filesize,fp) != (unsigned long)filesize )
                printf("error reading filesize.%ld\n",(long)filesize);
            buf[filesize] = 0;
        }
        fclose(fp);
        *lenp = filesize;
        //printf("loaded.(%s)\n",buf);
    } //else printf("OS_loadfile couldnt load.(%s)\n",fname);
    return(buf);
}

void *filestr(long *allocsizep,char *_fname)
{
    long filesize = 0; char *fname,*buf = 0; void *retptr;
    *allocsizep = 0;
    fname = (char *)malloc(strlen(_fname)+1);
    strcpy(fname,_fname);
    retptr = loadfile(fname,(uint8_t **)&buf,&filesize,allocsizep);
    free(fname);
    return(retptr);
}

cJSON *send_curl(char *url,char *fname)
{
    long fsize; char curlstr[1024],*jsonstr; cJSON *json=0;
    sprintf(curlstr,"wget -q \"%s\" -O %s",url,fname);
    if ( system(curlstr) == 0 )
    {
        if ( (jsonstr= (char *)filestr((long *)&fsize,fname)) != 0 )
        {
            json = cJSON_Parse(jsonstr);
            free(jsonstr);
        }
    }
    return(json);
}

// get_urljson just returns the JSON returned by the URL using issue_curl


/*
const char *Techstocks[] =
{ "AAPL","ADBE","ADSK","AKAM","AMD","AMZN","ATVI","BB","CDW","CRM","CSCO","CYBR","DBX","EA","FB","GDDY","GOOG","GRMN","GSAT","HPQ","IBM","INFY","INTC","INTU","JNPR","MSFT","MSI","MU","MXL","NATI","NCR","NFLX","NTAP","NVDA","ORCL","PANW","PYPL","QCOM","RHT","S","SHOP","SNAP","SPOT","SYMC","SYNA","T","TRIP","TWTR","TXN","VMW","VOD","VRSN","VZ","WDC","XRX","YELP","YNDX","ZEN"
};
const char *Metals[] = { "XAU", "XAG", "XPT", "XPD", };

const char *Markets[] = { "DJIA", "SPX", "NDX", "VIX" };
*/

cJSON *get_urljson(char *url)
{
    char *jsonstr; cJSON *json = 0;
    if ( (jsonstr= issue_curl(url)) != 0 )
    {
        //fprintf(stderr,"(%s) -> (%s)\n",url,jsonstr);
        json = cJSON_Parse(jsonstr);
        free(jsonstr);
    }
    return(json);
}

int32_t get_stockprices(uint32_t now,uint32_t *prices,std::vector<std::string> symbols)
{
    char url[32768],*symbol,*timestr; cJSON *json,*obj; int32_t i,n=0,retval=-1; uint32_t uprice,timestamp;
    sprintf(url,"https://api.iextrading.com/1.0/tops/last?symbols=%s",GetArg("-ac_stocks","").c_str());
    fprintf(stderr,"url.(%s)\n",url);
    if ( (json= get_urljson(url)) != 0 ) //if ( (json= send_curl(url,(char *)"iex")) != 0 ) //
    {
        fprintf(stderr,"stocks.(%s)\n",jprint(json,0));
        if ( (n= cJSON_GetArraySize(json)) > 0 )
        {
            retval = n;
            for (i=0; i<n; i++)
            {
                obj = jitem(json,i);
                if ( (symbol= jstr(obj,(char *)"symbol")) != 0 )
                {
                    uprice = jdouble(obj,(char *)"price")*100 + 0.0049;
                    prices[i] = uprice;
                    /*timestamp = j64bits(obj,(char *)"time");
                    if ( timestamp > now+60 || timestamp < now-ASSETCHAINS_BLOCKTIME )
                    {
                        fprintf(stderr,"time error.%d (%u vs %u)\n",timestamp-now,timestamp,now);
                        retval = -1;
                    }*/
                    if ( symbols[i] != symbol )
                    {
                        retval = -1;
                        fprintf(stderr,"MISMATCH.");
                    }
                    fprintf(stderr,"(%s %u) ",symbol,uprice);
                }
            }
            fprintf(stderr,"numstocks.%d\n",n);
        }
        //https://api.iextrading.com/1.0/tops/last?symbols=AAPL -> [{"symbol":"AAPL","price":198.63,"size":100,"time":1555092606076}]
        free_json(json);
    }
    return(retval);
}

uint32_t get_dailyfx(uint32_t *prices)
{
    //{"base":"USD","rates":{"BGN":1.74344803,"NZD":1.471652701,"ILS":3.6329113924,"RUB":65.1997682296,"CAD":1.3430201462,"USD":1.0,"PHP":52.8641469068,"CHF":0.9970582992,"AUD":1.4129078267,"JPY":110.6792654662,"TRY":5.6523444464,"HKD":7.8499732573,"MYR":4.0824567659,"HRK":6.6232840078,"CZK":22.9862720628,"IDR":14267.4986628633,"DKK":6.6551078624,"NOK":8.6806917454,"HUF":285.131039401,"GBP":0.7626582278,"MXN":19.4183455161,"THB":31.8702085933,"ISK":122.5708682475,"ZAR":14.7033339276,"BRL":3.9750401141,"SGD":1.3573720806,"PLN":3.8286682118,"INR":69.33187734,"KRW":1139.1602781244,"RON":4.2423783206,"CNY":6.7387234801,"SEK":9.3385630237,"EUR":0.8914244963},"date":"2019-03-28"}
    char url[512],*datestr; cJSON *json,*rates; int32_t i; uint32_t datenum=0,price = 0;
    sprintf(url,"https://api.openrates.io/latest?base=USD");
    if ( (json= get_urljson(url)) != 0 ) //if ( (json= send_curl(url,(char *)"dailyfx")) != 0 )
    {
        if ( (rates= jobj(json,(char *)"rates")) != 0 )
        {
            for (i=0; i<sizeof(Forex)/sizeof(*Forex); i++)
            {
                price = jdouble(rates,(char *)Forex[i]) * 10000 + 0.000049;
                fprintf(stderr,"(%s %.4f) ",Forex[i],(double)price/10000);
                prices[i] = price;
            }
        }
        if ( (datestr= jstr(json,(char *)"date")) != 0 )
            fprintf(stderr,"(%s)",datestr);
        fprintf(stderr,"\n");
        free_json(json);
    }
    return(datenum);
}

uint32_t get_binanceprice(const char *symbol)
{
    char url[512]; cJSON *json; uint32_t price = 0;
    sprintf(url,"https://api.binance.com/api/v1/ticker/price?symbol=%sBTC",symbol);
    if ( (json= get_urljson(url)) != 0 ) //if ( (json= send_curl(url,(char *)"bnbprice")) != 0 )
    {
        price = jdouble(json,(char *)"price")*SATOSHIDEN + 0.0000000049;
        free_json(json);
    }
    usleep(100000);
    return(price);
}

int32_t get_cryptoprices(uint32_t *prices,const char *list[],int32_t n,std::vector<std::string> strvec)
{
    int32_t i,errs=0; uint32_t price; char *symbol;
    for (i=0; i<n+strvec.size(); i++)
    {
        if ( i < n )
            symbol = (char *)list[i];
        else symbol = (char *)strvec[i - n].c_str();
        if ( (price= get_binanceprice(symbol)) == 0 )
            errs++;
        fprintf(stderr,"(%s %.8f) ",symbol,(double)price/SATOSHIDEN);
        prices[i] = price;
    }
    fprintf(stderr," errs.%d\n",errs);
    return(-errs);
}

/*uint32_t oldget_stockprice(const char *symbol)
{
    char url[512]; cJSON *json,*obj; uint32_t high,low,price = 0;
    sprintf(url,"https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY&symbol=%s&interval=15min&apikey=%s",symbol,NOTARY_PUBKEY.data()+50);
    if ( (json= get_urljson(url)) != 0 )
    {
        if ( (obj= jobj(json,(char *)"Time Series (15min)")) != 0 )
        {
            high = jdouble(jitem(obj,0),(char *)"2. high")*10000 + 0.000049;
            low = jdouble(jitem(obj,0),(char *)"3. low")*10000 + 0.000049;
            price = (high + low) / 2;
        }
        free_json(json);
    }
    return(price);
}

uint32_t get_currencyprice(const char *symbol)
{
    char url[512]; cJSON *json,*obj; uint32_t price = 0;
    sprintf(url,"https://www.alphavantage.co/query?function=CURRENCY_EXCHANGE_RATE&from_currency=%s&to_currency=USD&apikey=%s",symbol,NOTARY_PUBKEY.data()+50);
    if ( (json= send_curl(url,(char *)"curldata")) != 0 )//get_urljson(url)) != 0 )
    {
        if ( (obj= jobj(jitem(json,0),0)) != 0 )
            price = jdouble(obj,(char *)"5. Exchange Rate")*10000 + 0.000049;
        free_json(json);
    }
    return(price);
}

int32_t get_stocks(const char *list[],int32_t n)
{
    int32_t i,errs=0; uint32_t price;
    for (i=0; i<n; i++)
    {
        if ( (price= get_stockprice(list[i])) == 0 )
            errs++;
        else fprintf(stderr,"(%s %.4f) ",list[i],(double)price/10000);
    }
    fprintf(stderr," errs.%d\n",errs);
    return(-errs);
}*/

// parse the coindesk specific data. yes, if this changes, it will require an update. However, regardless if the format from the data source changes, then the code that extracts it must be changed. One way to mitigate this is to have a large variety of data sources so that there is only a very remote chance that all of them are not available. Certainly the data gathering needs to be made more robust, but it doesnt really affect the proof of concept for the decentralized trustless oracle. The trustlessness is achieved by having all nodes get the oracle data.

int32_t get_btcusd(uint32_t pricebits[4])
{
    cJSON *pjson,*bpi,*obj; char str[512]; double dbtcgbp,dbtcusd,dbtceur; uint64_t btcusd = 0,btcgbp = 0,btceur = 0;
    if ( (pjson= get_urljson((char *)"http://api.coindesk.com/v1/bpi/currentprice.json")) != 0 )
    {
        if ( (bpi= jobj(pjson,(char *)"bpi")) != 0 )
        {
            pricebits[0] = (uint32_t)time(NULL);
            if ( (obj= jobj(bpi,(char *)"USD")) != 0 )
            {
                btcusd = jdouble(obj,(char *)"rate_float") * SATOSHIDEN;
                pricebits[1] = ((btcusd / 10000) & 0xffffffff);
            }
            if ( (obj= jobj(bpi,(char *)"GBP")) != 0 )
            {
                btcgbp = jdouble(obj,(char *)"rate_float") * SATOSHIDEN;
                pricebits[2] = ((btcgbp / 10000) & 0xffffffff);
            }
            if ( (obj= jobj(bpi,(char *)"EUR")) != 0 )
            {
                btceur = jdouble(obj,(char *)"rate_float") * SATOSHIDEN;
                pricebits[3] = ((btceur / 10000) & 0xffffffff);
            }
        }
        free_json(pjson);
        dbtcusd = (double)pricebits[1]/10000;
        dbtcgbp = (double)pricebits[2]/10000;
        dbtceur = (double)pricebits[3]/10000;
        fprintf(stderr,"BTC/USD %.4f, BTC/GBP %.4f, BTC/EUR %.4f GBPUSD %.6f, EURUSD %.6f EURGBP %.6f\n",dbtcusd,dbtcgbp,dbtceur,dbtcusd/dbtcgbp,dbtcusd/dbtceur,dbtcgbp/dbtceur);
        return(0);
    }
    return(-1);
}

// komodo_cbopretupdate() obtains the external price data and encodes it into Mineropret, which will then be used by the miner and validation
// save history, use new data to approve past rejection, where is the auto-reconsiderblock?

int32_t komodo_cbopretsize(uint64_t flags)
{
    int32_t size = 0;
    if ( (ASSETCHAINS_CBOPRET & 1) != 0 )
    {
        size = PRICES_SIZEBIT0;
        if ( (ASSETCHAINS_CBOPRET & 2) != 0 )
            size += (sizeof(Forex)/sizeof(*Forex)) * sizeof(uint32_t);
        if ( (ASSETCHAINS_CBOPRET & 4) != 0 )
            size += (sizeof(Cryptos)/sizeof(*Cryptos) + ASSETCHAINS_PRICES.size()) * sizeof(uint32_t);
        if ( (ASSETCHAINS_CBOPRET & 8) != 0 )
            size += (ASSETCHAINS_STOCKS.size() * sizeof(uint32_t));
    }
    return(size);
}

extern uint256 Queued_reconsiderblock;

void komodo_cbopretupdate(int32_t forceflag)
{
    static uint32_t lasttime,lastbtc,pending;
    static uint32_t pricebits[4],pricebuf[KOMODO_MAXPRICES],forexprices[sizeof(Forex)/sizeof(*Forex)];
    int32_t size; uint32_t flags=0,now; CBlockIndex *pindex;
    if ( Queued_reconsiderblock != zeroid )
    {
        fprintf(stderr,"Queued_reconsiderblock %s\n",Queued_reconsiderblock.GetHex().c_str());
        komodo_reconsiderblock(Queued_reconsiderblock);
        Queued_reconsiderblock = zeroid;
    }
    if ( forceflag != 0 && pending != 0 )
    {
        while ( pending != 0 )
            fprintf(stderr,"pricewait "), sleep(1);
        return;
    }
    pending = 1;
    now = (uint32_t)time(NULL);
    if ( (ASSETCHAINS_CBOPRET & 1) != 0 )
    {
//if ( komodo_nextheight() > 333 ) // for debug only!
//    ASSETCHAINS_CBOPRET = 7;
        size = komodo_cbopretsize(ASSETCHAINS_CBOPRET);
        if ( Mineropret.size() < size )
            Mineropret.resize(size);
        size = PRICES_SIZEBIT0;
        if ( (forceflag != 0 || now > lastbtc+120) && get_btcusd(pricebits) == 0 )
        {
            if ( flags == 0 )
                komodo_PriceCache_shift();
            memcpy(PriceCache[0],pricebits,PRICES_SIZEBIT0);
            flags |= 1;
        }
        if ( (ASSETCHAINS_CBOPRET & 2) != 0 )
        {
            if ( now > lasttime+3600*5 || forexprices[0] == 0 ) // cant assume timestamp is valid for forex price as it is a daily weekday changing thing anyway.
            {
                get_dailyfx(forexprices);
                if ( flags == 0 )
                    komodo_PriceCache_shift();
                flags |= 2;
                memcpy(&PriceCache[0][size/sizeof(uint32_t)],forexprices,sizeof(forexprices));
            }
            size += (sizeof(Forex)/sizeof(*Forex)) * sizeof(uint32_t);
        }
        if ( (ASSETCHAINS_CBOPRET & 4) != 0 )
        {
            if ( forceflag != 0 || flags != 0 )
            {
                get_cryptoprices(pricebuf,Cryptos,(int32_t)(sizeof(Cryptos)/sizeof(*Cryptos)),ASSETCHAINS_PRICES);
                if ( flags == 0 )
                    komodo_PriceCache_shift();
                memcpy(&PriceCache[0][size/sizeof(uint32_t)],pricebuf,(sizeof(Cryptos)/sizeof(*Cryptos)+ASSETCHAINS_PRICES.size()) * sizeof(uint32_t));
                flags |= 4; // very rarely we can see flags == 6 case
            }
            size += (sizeof(Cryptos)/sizeof(*Cryptos)+ASSETCHAINS_PRICES.size()) * sizeof(uint32_t);
        }
        now = (uint32_t)time(NULL);
        if ( (ASSETCHAINS_CBOPRET & 8) != 0 )
        {
            if ( forceflag != 0 || flags != 0 )
            {
                if ( get_stockprices(now,pricebuf,ASSETCHAINS_STOCKS) == ASSETCHAINS_STOCKS.size() )
                {
                    if ( flags == 0 )
                        komodo_PriceCache_shift();
                    memcpy(&PriceCache[0][size/sizeof(uint32_t)],pricebuf,ASSETCHAINS_STOCKS.size() * sizeof(uint32_t));
                    flags |= 8; // very rarely we can see flags == 10 case
                }
            }
            size += (ASSETCHAINS_STOCKS.size()) * sizeof(uint32_t);
        }
        if ( flags != 0 )
        {
            if ( (flags & 1) != 0 )
                lastbtc = now;
            if ( (flags & 2) != 0 )
                lasttime = now;
            memcpy(Mineropret.data(),PriceCache[0],size);
            if ( ExtremePrice.dir != 0 && ExtremePrice.ind > 0 && ExtremePrice.ind < size/sizeof(uint32_t) && now < ExtremePrice.timestamp+3600 )
            {
                fprintf(stderr,"cmp dir.%d PriceCache[0][ExtremePrice.ind] %u >= %u ExtremePrice.pricebits\n",ExtremePrice.dir,PriceCache[0][ExtremePrice.ind],ExtremePrice.pricebits);
                if ( (ExtremePrice.dir > 0 && PriceCache[0][ExtremePrice.ind] >= ExtremePrice.pricebits) || (ExtremePrice.dir < 0 && PriceCache[0][ExtremePrice.ind] <= ExtremePrice.pricebits) )
                {
                    fprintf(stderr,"future price is close enough to allow approving previously rejected block ind.%d %u vs %u\n",ExtremePrice.ind,PriceCache[0][ExtremePrice.ind],ExtremePrice.pricebits);
                    if ( (pindex= komodo_blockindex(ExtremePrice.blockhash)) != 0 )
                        pindex->nStatus &= ~BLOCK_FAILED_MASK;
                    else fprintf(stderr,"couldnt find block.%s\n",ExtremePrice.blockhash.GetHex().c_str());
                }
            }
            // high volatility still strands nodes so we need to check new prices to approve a stuck block
            // scan list of stuck blocks (one?) and auto reconsiderblock if it changed state
            
            //int32_t i; for (i=0; i<Mineropret.size(); i++)
            //    fprintf(stderr,"%02x",Mineropret[i]);
            //fprintf(stderr," <- set Mineropret[%d] size.%d %ld\n",(int32_t)Mineropret.size(),size,sizeof(PriceCache[0]));
        }
    }
    pending = 0;
}

int64_t komodo_pricemult(int32_t ind)
{
    int32_t i,j;
    if ( (ASSETCHAINS_CBOPRET & 1) != 0 && ind < KOMODO_MAXPRICES )
    {
        if ( PriceMult[0] == 0 )
        {
            for (i=0; i<4; i++)
                PriceMult[i] = 10000;
            if ( (ASSETCHAINS_CBOPRET & 2) != 0 )
            {
                for (j=0; j<sizeof(Forex)/sizeof(*Forex); j++)
                    PriceMult[i++] = 10000;
            }
            if ( (ASSETCHAINS_CBOPRET & 4) != 0 )
            {
                for (j=0; j<sizeof(Cryptos)/sizeof(*Cryptos)+ASSETCHAINS_PRICES.size(); j++)
                    PriceMult[i++] = 1;
            }
            if ( (ASSETCHAINS_CBOPRET & 8) != 0 )
            {
                for (j=0; j<ASSETCHAINS_STOCKS.size(); j++)
                    PriceMult[i++] = 1000000;
            }
        }
        return(PriceMult[ind]);
    }
    return(0);
}

char *komodo_pricename(char *name,int32_t ind)
{
    strcpy(name,"error");
    if ( (ASSETCHAINS_CBOPRET & 1) != 0 && ind < KOMODO_MAXPRICES )
    {
        if ( ind < 4 )
        {
            switch ( ind )
            {
                case 0: strcpy(name,"timestamp"); break;
                case 1: strcpy(name,"BTC_USD"); break;
                case 2: strcpy(name,"BTC_GBP"); break;
                case 3: strcpy(name,"BTC_EUR"); break;
                default: return(0); break;
            }
            return(name);
        }
        else
        {
            ind -= 4;
            if ( (ASSETCHAINS_CBOPRET & 2) != 0 )
            {
                if ( ind < 0 )
                    return(0);
                if ( ind < sizeof(Forex)/sizeof(*Forex) )
                {
                    name[0] = 'U', name[1] = 'S', name[2] = 'D', name[3] = '_';
                    strcpy(name+4,Forex[ind]);
                    return(name);
                } else ind -= sizeof(Forex)/sizeof(*Forex);
            }
            if ( (ASSETCHAINS_CBOPRET & 4) != 0 )
            {
                if ( ind < 0 )
                    return(0);
                if ( ind < sizeof(Cryptos)/sizeof(*Cryptos) + ASSETCHAINS_PRICES.size() )
                {
                    if ( ind < sizeof(Cryptos)/sizeof(*Cryptos) )
                        strcpy(name,Cryptos[ind]);
                    else
                    {
                        ind -= (sizeof(Cryptos)/sizeof(*Cryptos));
                        strcpy(name,ASSETCHAINS_PRICES[ind].c_str());
                    }
                    strcat(name,"_BTC");
                    return(name);
                } else ind -= (sizeof(Cryptos)/sizeof(*Cryptos) + ASSETCHAINS_PRICES.size());
            }
            if ( (ASSETCHAINS_CBOPRET & 8) != 0 )
            {
                if ( ind < 0 )
                    return(0);
                if ( ind < ASSETCHAINS_STOCKS.size() )
                {
                    strcpy(name,ASSETCHAINS_STOCKS[ind].c_str());
                    strcat(name,"_USD");
                    return(name);
                } else ind -= ASSETCHAINS_STOCKS.size();
            }
        }
    }
    return(0);
}
// finds index for its symbol name
int32_t komodo_priceind(const char *symbol)
{
    char name[65]; int32_t i,n = (int32_t)(komodo_cbopretsize(ASSETCHAINS_CBOPRET) / sizeof(uint32_t));
    for (i=1; i<n; i++)
    {
        komodo_pricename(name,i);
        if ( strcmp(name,symbol) == 0 )
            return(i);
    }
    return(-1);
}
// returns price value which is in a 10% interval for more than 50% points for the preceding 24 hours
int64_t komodo_pricecorrelated(uint64_t seed,int32_t ind,uint32_t *rawprices,int32_t rawskip,uint32_t *nonzprices,int32_t smoothwidth)
{
    int32_t i,j,k,n,iter,correlation,maxcorrelation=0; int64_t firstprice,price,sum,den,mult,refprice,lowprice,highprice;
    if ( PRICES_DAYWINDOW < 2 || ind >= KOMODO_MAXPRICES )
        return(-1);
    mult = komodo_pricemult(ind);
    if ( nonzprices != 0 )
        memset(nonzprices,0,sizeof(*nonzprices)*PRICES_DAYWINDOW);
    //for (i=0; i<PRICES_DAYWINDOW; i++)
    //    fprintf(stderr,"%u ",rawprices[i*rawskip]);
    //fprintf(stderr,"ind.%d\n",ind);
    for (iter=0; iter<PRICES_DAYWINDOW; iter++)
    {
        correlation = 0;
        i = (iter + seed) % PRICES_DAYWINDOW;
        refprice = rawprices[i*rawskip];
        highprice = (refprice * (COIN + PRICES_ERRORRATE*5)) / COIN;
        lowprice = (refprice * (COIN - PRICES_ERRORRATE*5)) / COIN;
        if ( highprice == refprice )
            highprice++;
        if ( lowprice == refprice )
            lowprice--;
        sum = 0;
        //fprintf(stderr,"firsti.%d: ",i);
        for (j=0; j<PRICES_DAYWINDOW; j++,i++)
        {
            if ( i >= PRICES_DAYWINDOW )
                i = 0;
            if ( (price= rawprices[i*rawskip]) == 0 )
            {
                fprintf(stderr,"null rawprice.[%d]\n",i);
                return(-1);
            }
            if ( price >= lowprice && price <= highprice )
            {
                //fprintf(stderr,"%.1f ",(double)price/10000);
                sum += price;
                correlation++;
                if ( correlation > (PRICES_DAYWINDOW>>1) )
                {
                    if ( nonzprices == 0 )
                        return(refprice * mult);
                    //fprintf(stderr,"-> %.4f\n",(double)sum*mult/correlation);
                    //return(sum*mult/correlation);
                    n = 0;
                    i = (iter + seed) % PRICES_DAYWINDOW;
                    for (k=0; k<PRICES_DAYWINDOW; k++,i++)
                    {
                        if ( i >= PRICES_DAYWINDOW )
                            i = 0;
                        if ( n > (PRICES_DAYWINDOW>>1) )
                            nonzprices[i] = 0;
                        else
                        {
                            price = rawprices[i*rawskip];
                            if ( price < lowprice || price > highprice )
                                nonzprices[i] = 0;
                            else
                            {
                                nonzprices[i] = price;
                                //fprintf(stderr,"(%d %u) ",i,rawprices[i*rawskip]);
                                n++;
                            }
                        }
                    }
                    //fprintf(stderr,"ind.%d iter.%d j.%d i.%d n.%d correlation.%d ref %llu -> %llu\n",ind,iter,j,i,n,correlation,(long long)refprice,(long long)sum/correlation);
                    if ( n != correlation )
                        return(-1);
                    sum = den = n = 0;
                    for (i=0; i<PRICES_DAYWINDOW; i++)
                        if ( nonzprices[i] != 0 )
                            break;
                    firstprice = nonzprices[i];
                    //fprintf(stderr,"firsti.%d: ",i);
                    for (i=0; i<PRICES_DAYWINDOW; i++)
                    {
                        if ( (price= nonzprices[i]) != 0 )
                        {
                            den += (PRICES_DAYWINDOW - i);
                            sum += ((PRICES_DAYWINDOW - i) * (price + firstprice*4)) / 5;
                            n++;
                        }
                    }
                    if ( n != correlation || sum == 0 || den == 0 )
                    {
                        fprintf(stderr,"seed.%llu n.%d vs correlation.%d sum %llu, den %llu\n",(long long)seed,n,correlation,(long long)sum,(long long)den);
                        return(-1);
                    }
                    //fprintf(stderr,"firstprice.%llu weighted -> %.8f\n",(long long)firstprice,((double)(sum*mult) / den) / COIN);
                    return((sum * mult) / den);
                }
            }
        }
        if ( correlation > maxcorrelation )
            maxcorrelation = correlation;
    }
    fprintf(stderr,"ind.%d iter.%d maxcorrelation.%d ref.%llu high.%llu low.%llu\n",ind,iter,maxcorrelation,(long long)refprice,(long long)highprice,(long long)lowprice);
    return(0);
}

int64_t _pairave64(int64_t valA,int64_t valB)
{
    if ( valA != 0 && valB != 0 )
        return((valA + valB) / 2);
    else if ( valA != 0 ) return(valA);
    else return(valB);
}

int64_t _pairdiff64(register int64_t valA,register int64_t valB)
{
    if ( valA != 0 && valB != 0 )
        return(valA - valB);
    else return(0);
}

int64_t balanced_ave64(int64_t buf[],int32_t i,int32_t width)
{
    register int32_t nonz,j; register int64_t sum,price;
    nonz = 0;
    sum = 0;
    for (j=-width; j<=width; j++)
    {
        price = buf[i + j];
        if ( price != 0 )
        {
            sum += price;
            nonz++;
        }
    }
    if ( nonz != 0 )
        sum /= nonz;
    return(sum);
}

void buf_trioave64(int64_t dest[],int64_t src[],int32_t n)
{
    register int32_t i,j,width = 3;
    for (i=0; i<128; i++)
        src[i] = 0;
    //for (i=n-width-1; i>width; i--)
    //	dest[i] = balanced_ave(src,i,width);
    //for (i=width; i>0; i--)
    //	dest[i] = balanced_ave(src,i,i);
    for (i=1; i<width; i++)
        dest[i] = balanced_ave64(src,i,i);
    for (i=width; i<n-width; i++)
        dest[i] = balanced_ave64(src,i,width);
    dest[0] = _pairave64(dest[0],dest[1] - _pairdiff64(dest[2],dest[1]));
    j = width-1;
    for (i=n-width; i<n-1; i++,j--)
        dest[i] = balanced_ave64(src,i,j);
    if ( dest[n-3] != 0. && dest[n-2] != 0. )
        dest[n-1] = ((2 * dest[n-2]) - dest[n-3]);
    else dest[n-1] = 0;
}

void smooth64(int64_t dest[],int64_t src[],int32_t width,int32_t smoothiters)
{
    int64_t smoothbufA[1024],smoothbufB[1024]; int32_t i;
    if ( width < sizeof(smoothbufA)/sizeof(*smoothbufA) )
    {
        buf_trioave64(smoothbufA,src,width);
        for (i=0; i<smoothiters; i++)
        {
            buf_trioave64(smoothbufB,smoothbufA,width);
            buf_trioave64(smoothbufA,smoothbufB,width);
        }
        buf_trioave64(dest,smoothbufA,width);
    } else memcpy(dest,src,width*sizeof(*dest));
}

// http://www.holoborodko.com/pavel/numerical-methods/noise-robust-smoothing-filter/
//const int64_t coeffs[7] = { -2, 0, 18, 32, 18, 0, -2 };
static int cmp_llu(const void *a, const void*b)
{
    if(*(int64_t *)a < *(int64_t *)b) return -1;
    else if(*(int64_t *)a > *(int64_t *)b) return 1;
    else if ( (uint64_t)a < (uint64_t)b ) // jl777 prevent nondeterminism
        return(-1);
    else return(1);
}

static void sort64(int64_t *l, int32_t llen)
{
    qsort(l,llen,sizeof(uint64_t),cmp_llu);
}

static int revcmp_llu(const void *a, const void*b)
{
    if(*(int64_t *)a < *(int64_t *)b) return 1;
    else if(*(int64_t *)a > *(int64_t *)b) return -1;
    else if ( (uint64_t)a < (uint64_t)b ) // jl777 prevent nondeterminism
        return(-1);
    else return(1);
}

static void revsort64(int64_t *l, int32_t llen)
{
    qsort(l,llen,sizeof(uint64_t),revcmp_llu);
}

int64_t komodo_priceave(int64_t *buf,int64_t *correlated,int32_t cskip)
{
    int32_t i,dir=0; int64_t sum=0,nonzprice,price,halfave,thirdave,fourthave,decayprice;
    if ( PRICES_DAYWINDOW < 2 )
        return(0);
    for (i=0; i<PRICES_DAYWINDOW; i++)
    {
        if ( (nonzprice= correlated[i*cskip]) != 0 )
            break;
    }
    if ( nonzprice == 0 )
        return(-1);
    for (i=0; i<PRICES_DAYWINDOW; i++)
    {
        if ( (price= correlated[i*cskip]) != 0 )
            nonzprice = price;
        buf[PRICES_DAYWINDOW+i] = nonzprice;
        sum += nonzprice;
        if ( i == PRICES_DAYWINDOW/2 )
            halfave = (sum / (PRICES_DAYWINDOW/2));
        else if ( i == PRICES_DAYWINDOW/3 )
            thirdave = (sum / (PRICES_DAYWINDOW/3));
        else if ( i == PRICES_DAYWINDOW/4 )
            fourthave = (sum / (PRICES_DAYWINDOW/4));
    }
    memcpy(buf,&buf[PRICES_DAYWINDOW],PRICES_DAYWINDOW*sizeof(*buf));
    price = sum / PRICES_DAYWINDOW;
    return(price);
    if ( halfave == price )
        return(price);
    else if ( halfave > price ) // rising prices
        sort64(buf,PRICES_DAYWINDOW);
    else revsort64(buf,PRICES_DAYWINDOW);
    decayprice = buf[0];
    for (i=0; i<PRICES_DAYWINDOW; i++)
    {
        decayprice = ((decayprice * 97) + (buf[i] * 3)) / 100;
        //fprintf(stderr,"%.4f ",(double)buf[i]/COIN);
    }
    fprintf(stderr,"%ssort half %.8f %.8f %.8f %.8f %.8f %.8f -> %.8f\n",halfave<price?"rev":"",(double)price/COIN,(double)halfave/COIN,(double)thirdave/COIN,(double)fourthave/COIN,(double)decayprice/COIN,(double)buf[PRICES_DAYWINDOW-1]/COIN,(double)(price*7 + halfave*5 + thirdave*3 + fourthave*2 + decayprice + buf[PRICES_DAYWINDOW-1])/(19*COIN));
    return((price*7 + halfave*5 + thirdave*3 + fourthave*2 + decayprice + buf[PRICES_DAYWINDOW-1]) / 19);
}

int32_t komodo_pricesinit()
{
    static int32_t didinit;
    int32_t i,num=0,createflag = 0;
    if ( didinit != 0 )
        return(-1);
    didinit = 1;
    boost::filesystem::path pricefname,pricesdir = GetDataDir() / "prices";
    fprintf(stderr,"pricesinit (%s)\n",pricesdir.string().c_str());
    if (!boost::filesystem::exists(pricesdir))
        boost::filesystem::create_directories(pricesdir), createflag = 1;
    for (i=0; i<KOMODO_MAXPRICES; i++)
    {
        if ( komodo_pricename(PRICES[i].symbol,i) == 0 )
            break;
        //fprintf(stderr,"%s.%d ",PRICES[i].symbol,i);
        if ( i == 0 )
            strcpy(PRICES[i].symbol,"rawprices");
        pricefname = pricesdir / PRICES[i].symbol;
        if ( createflag != 0 )
            PRICES[i].fp = fopen(pricefname.string().c_str(),"wb+");
        else if ( (PRICES[i].fp= fopen(pricefname.string().c_str(),"rb+")) == 0 )
            PRICES[i].fp = fopen(pricefname.string().c_str(),"wb+");
        if ( PRICES[i].fp != 0 )
        {
            num++;
            if ( createflag != 0 )
            {
                fseek(PRICES[i].fp,(2*PRICES_DAYWINDOW+PRICES_SMOOTHWIDTH) * sizeof(int64_t) * PRICES_MAXDATAPOINTS,SEEK_SET);
                fputc(0,PRICES[i].fp);
                fflush(PRICES[i].fp);
            }
        } else fprintf(stderr,"error opening %s createflag.%d\n",pricefname.string().c_str(), createflag);
    }
    if ( i > 0 && PRICES[0].fp != 0 && createflag != 0 )
    {
        fseek(PRICES[0].fp,(2*PRICES_DAYWINDOW+PRICES_SMOOTHWIDTH) * sizeof(uint32_t) * i,SEEK_SET);
        fputc(0,PRICES[0].fp);
        fflush(PRICES[0].fp);
    }
    fprintf(stderr,"pricesinit done i.%d num.%d numprices.%d\n",i,num,(int32_t)(komodo_cbopretsize(ASSETCHAINS_CBOPRET)/sizeof(uint32_t)));
    if ( i != num || i != komodo_cbopretsize(ASSETCHAINS_CBOPRET)/sizeof(uint32_t) )
    {
        fprintf(stderr,"fatal error opening prices files, start shutdown\n");
        StartShutdown();
    }
    return(0);
}

pthread_mutex_t pricemutex;

// PRICES file layouts
// [0] rawprice32 / timestamp
// [1] correlated
// [2] 24hr ave
// [3] to [7] reserved

void komodo_pricesupdate(int32_t height,CBlock *pblock)
{
    static int numprices; static uint32_t *ptr32; static int64_t *ptr64,*tmpbuf;
    int32_t ind,offset,width; int64_t correlated,smoothed; uint64_t seed,rngval; uint32_t rawprices[KOMODO_MAXPRICES],buf[PRICES_MAXDATAPOINTS*2];
    width = PRICES_DAYWINDOW;//(2*PRICES_DAYWINDOW + PRICES_SMOOTHWIDTH);
    if ( numprices == 0 )
    {
        pthread_mutex_init(&pricemutex,0);
        numprices = (int32_t)(komodo_cbopretsize(ASSETCHAINS_CBOPRET) / sizeof(uint32_t));
        ptr32 = (uint32_t *)calloc(sizeof(uint32_t),numprices * width);
        ptr64 = (int64_t *)calloc(sizeof(int64_t),PRICES_DAYWINDOW*PRICES_MAXDATAPOINTS);
        tmpbuf = (int64_t *)calloc(sizeof(int64_t),2*PRICES_DAYWINDOW);
        fprintf(stderr,"prices update: numprices.%d %p %p\n",numprices,ptr32,ptr64);
    }
    if ( _komodo_heightpricebits(&seed,rawprices,pblock) == numprices )
    {
        //for (ind=0; ind<numprices; ind++)
        //    fprintf(stderr,"%u ",rawprices[ind]);
        //fprintf(stderr,"numprices.%d\n",numprices);
        if ( PRICES[0].fp != 0 )
        {
            pthread_mutex_lock(&pricemutex);
            fseek(PRICES[0].fp,height * numprices * sizeof(uint32_t),SEEK_SET);
            if ( fwrite(rawprices,sizeof(uint32_t),numprices,PRICES[0].fp) != numprices )
                fprintf(stderr,"error writing rawprices for ht.%d\n",height);
            else fflush(PRICES[0].fp);
            if ( height > PRICES_DAYWINDOW )
            {
                fseek(PRICES[0].fp,(height-width+1) * numprices * sizeof(uint32_t),SEEK_SET);
                if ( fread(ptr32,sizeof(uint32_t),width*numprices,PRICES[0].fp) == width*numprices )
                {
                    rngval = seed;
                    for (ind=1; ind<numprices; ind++)
                    {
                        if ( PRICES[ind].fp == 0 )
                        {
                            fprintf(stderr,"PRICES[%d].fp is null\n",ind);
                            continue;
                        }
                        offset = (width-1)*numprices + ind;
                        rngval = (rngval*11109 + 13849);
                        if ( (correlated= komodo_pricecorrelated(rngval,ind,&ptr32[offset],-numprices,0,PRICES_SMOOTHWIDTH)) > 0 )
                        {
                            fseek(PRICES[ind].fp,height * sizeof(int64_t) * PRICES_MAXDATAPOINTS,SEEK_SET);
                            memset(buf,0,sizeof(buf));
                            buf[0] = rawprices[ind];
                            buf[1] = rawprices[0]; // timestamp
                            memcpy(&buf[2],&correlated,sizeof(correlated));
                            if ( fwrite(buf,1,sizeof(buf),PRICES[ind].fp) != sizeof(buf) )
                                fprintf(stderr,"error fwrite buf for ht.%d ind.%d\n",height,ind);
                            else if ( height > PRICES_DAYWINDOW*2 )
                            {
                                fseek(PRICES[ind].fp,(height-PRICES_DAYWINDOW+1) * PRICES_MAXDATAPOINTS * sizeof(int64_t),SEEK_SET);
                                if ( fread(ptr64,sizeof(int64_t),PRICES_DAYWINDOW*PRICES_MAXDATAPOINTS,PRICES[ind].fp) == PRICES_DAYWINDOW*PRICES_MAXDATAPOINTS )
                                {
                                    if ( (smoothed= komodo_priceave(tmpbuf,&ptr64[(PRICES_DAYWINDOW-1)*PRICES_MAXDATAPOINTS+1],-PRICES_MAXDATAPOINTS)) > 0 )
                                    {
                                        fseek(PRICES[ind].fp,(height * PRICES_MAXDATAPOINTS + 2) * sizeof(int64_t),SEEK_SET);
                                        if ( fwrite(&smoothed,1,sizeof(smoothed),PRICES[ind].fp) != sizeof(smoothed) )
                                            fprintf(stderr,"error fwrite smoothed for ht.%d ind.%d\n",height,ind);
                                        else fflush(PRICES[ind].fp);
                                    } else fprintf(stderr,"error price_smoothed ht.%d ind.%d\n",height,ind);
                                } else fprintf(stderr,"error fread ptr64 for ht.%d ind.%d\n",height,ind);
                            }
                        } else fprintf(stderr,"error komodo_pricecorrelated for ht.%d ind.%d\n",height,ind);
                    }
                    fprintf(stderr,"height.%d\n",height);
                } else fprintf(stderr,"error reading rawprices for ht.%d\n",height);
            } else fprintf(stderr,"height.%d <= width.%d\n",height,width);
            pthread_mutex_unlock(&pricemutex);
        } else fprintf(stderr,"null PRICES[0].fp\n");
    } else fprintf(stderr,"numprices mismatch, height.%d\n",height);
}

int32_t komodo_priceget(int64_t *buf64,int32_t ind,int32_t height,int32_t numblocks)
{
    FILE *fp; int32_t retval = PRICES_MAXDATAPOINTS;
    pthread_mutex_lock(&pricemutex);
    if ( ind < KOMODO_MAXPRICES && (fp= PRICES[ind].fp) != 0 )
    {
        fseek(fp,height * PRICES_MAXDATAPOINTS * sizeof(int64_t),SEEK_SET);
        if ( fread(buf64,sizeof(int64_t),numblocks*PRICES_MAXDATAPOINTS,fp) != numblocks*PRICES_MAXDATAPOINTS )
            retval = -1;
    }
    pthread_mutex_unlock(&pricemutex);
    return(retval);
}
