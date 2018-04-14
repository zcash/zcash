/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
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
    struct pax_transaction *pax,*pax2,*tmp,*paxes[64]; uint8_t opretbuf[16384]; int32_t i,n,ht,len=0; uint64_t total = 0;
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
    struct pax_transaction *pax,*tmp; char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; uint8_t *script,opcode,opret[16384],data[16384]; int32_t i,baseid,ht,len=0,opretlen=0,numvouts=1; struct komodo_state *sp; uint64_t available,deposited,issued,withdrawn,approved,redeemed,mask,sum = 0;
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
    "78cb4e21245c26b015b888b14c4f5096e18137d2741a6de9734d62b07014dfca", //233559
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
};

int32_t komodo_bannedset(int32_t *indallvoutsp,uint256 *array,int32_t max)
{
    int32_t i;
    if ( sizeof(banned_txids)/sizeof(*banned_txids) > max )
    {
        fprintf(stderr,"komodo_bannedset: buffer too small %ld vs %d\n",sizeof(banned_txids)/sizeof(*banned_txids),max);
        exit(-1);
    }
    for (i=0; i<sizeof(banned_txids)/sizeof(*banned_txids); i++)
        array[i] = uint256S(banned_txids[i]);
    *indallvoutsp = i-2;
    return(i);
}

void komodo_passport_iteration();

uint64_t komodo_commission(const CBlock &block)
{
    int32_t i,j,n=0,txn_count; uint64_t total = 0;
    txn_count = block.vtx.size();
    for (i=0; i<txn_count; i++)
    {
        n = block.vtx[i].vout.size();
        for (j=0; j<n; j++)
        {
            //fprintf(stderr,"(%d %.8f).%d ",i,dstr(block.vtx[i].vout[j].nValue),j);
            if ( i != 0 || j != 1 )
            {
                total += block.vtx[i].vout[j].nValue;
            }
        }
    }
    //fprintf(stderr,"txn.%d n.%d commission total %.8f -> %.8f\n",txn_count,n,dstr(total),dstr((total * ASSETCHAINS_COMMISSION) / COIN));
    return((total * ASSETCHAINS_COMMISSION) / COIN);
}

uint32_t komodo_stake(int32_t validateflag,arith_uint256 bnTarget,int32_t nHeight,uint256 txid,int32_t vout,uint32_t blocktime,uint32_t prevtime,char *destaddr)
{
    CBlockIndex *pindex; uint8_t hashbuf[128]; char address[64]; bits256 addrhash; arith_uint256 hashval; uint256 hash,pasthash; int64_t diff=0; int32_t segid,minage,i,iter=0; uint32_t txtime,winner = 0; uint64_t value,coinage,supply = ASSETCHAINS_SUPPLY + nHeight*ASSETCHAINS_REWARD/SATOSHIDEN;
    txtime = komodo_txtime(&value,txid,vout,address);
    if ( value == 0 || txtime == 0 )
        return(0);
    if ( (minage= nHeight*3) > 6000 )
        minage = 6000;
    if ( blocktime > txtime+minage && (pindex= komodo_chainactive(nHeight>200?nHeight-200:1)) != 0 )
    {
        vcalc_sha256(0,(uint8_t *)&addrhash,(uint8_t *)address,(int32_t)strlen(address));
        segid = ((nHeight + addrhash.uints[0]) & 0x3f);
        pasthash = pindex->GetBlockHash();
        memcpy(hashbuf,&pasthash,sizeof(pasthash));
        memcpy(&hashbuf[sizeof(pasthash)],&addrhash,sizeof(addrhash));
        vcalc_sha256(0,(uint8_t *)&hash,hashbuf,(int32_t)sizeof(uint256)*2);
        //fprintf(stderr,"(%s) vs. (%s) %s %.8f txtime.%u\n",address,destaddr,hash.ToString().c_str(),dstr(value),txtime);
        for (iter=0; iter<3600; iter++)
        {
            diff = (iter + blocktime - txtime - minage);
            if ( diff > 3600*24 )
                break;
            coinage = (value * diff) * ((diff >> 16) + 1);
            hashval = arith_uint256(supply * 64) * (UintToArith256(hash) / arith_uint256(coinage+1));
            if ( hashval <= bnTarget )
            {
                winner = 1;
                if ( validateflag == 0 )
                {
                    blocktime += iter;
                    blocktime += segid * 2;
                }
                break;
            }
            if ( validateflag != 0 )
            {
                for (i=31; i>=24; i--)
                    fprintf(stderr,"%02x",((uint8_t *)&hashval)[i]);
                fprintf(stderr," vs target ");
                for (i=31; i>=24; i--)
                    fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
                fprintf(stderr," segid.%d iter.%d winner.%d coinage.%llu %d ht.%d gap.%d %.8f diff.%d\n",segid,iter,winner,(long long)coinage,(int32_t)(blocktime - txtime),nHeight,(int32_t)(blocktime - prevtime),dstr(value),(int32_t)diff);
                break;
            }
        }
        //fprintf(stderr,"iterated until i.%d winner.%d\n",i,winner);
        if ( 0 )
        {
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&hashval)[i]);
            fprintf(stderr," vs ");
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
            fprintf(stderr," segid.%d iter.%d winner.%d coinage.%llu %d ht.%d t.%u %.8f diff.%d\n",segid,iter,winner,(long long)coinage,(int32_t)(blocktime - txtime),nHeight,blocktime,dstr(value),(int32_t)diff);
        }
    }
    if ( nHeight < 2 )
        return(blocktime);
    return(blocktime * winner);
}

#define KOMODO_POWMINMULT 16
arith_uint256 komodo_PoWtarget(int32_t *percPoSp,arith_uint256 target,int32_t height,int32_t goalperc)
{
    CBlockIndex *pindex; arith_uint256 bnTarget,hashval,sum,ave; bool fNegative,fOverflow; int32_t i,n,ht,percPoS,diff;
    *percPoSp = percPoS = 0;
    sum = arith_uint256(0);
    ave = sum;
    for (i=n=0; i<100; i++)
    {
        ht = height - 100 + i;
        if ( (pindex= komodo_chainactive(ht)) != 0 )
        {
            bnTarget.SetCompact(pindex->nBits,&fNegative,&fOverflow);
            bnTarget = (bnTarget / arith_uint256(KOMODO_POWMINMULT));
            hashval = UintToArith256(pindex->GetBlockHash());
            if ( hashval <= bnTarget ) // PoW is never as easy as PoS/64, some PoS will be counted as PoW
            {
                sum += hashval;
                n++;
            } else percPoS++;
        }
    }
    *percPoSp = percPoS;
    target = (target / arith_uint256(KOMODO_POWMINMULT));
    if ( n > 0 )
    {
        ave = (sum / arith_uint256(n));
        if ( ave > target )
            ave = target;
    } else return(target);
    if ( percPoS < goalperc ) // increase PoW diff -> lower bnTarget
    {
        bnTarget = (ave * arith_uint256(goalperc)) / arith_uint256(percPoS + goalperc);
        if ( 1 )
        {
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&ave)[i]);
            fprintf(stderr," increase diff -> ");
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
            fprintf(stderr," floor diff ");
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&target)[i]);
            fprintf(stderr," ht.%d percPoS.%d vs goal.%d -> diff %d\n",height,percPoS,goalperc,goalperc - percPoS);
        }
    }
    else if ( percPoS > goalperc ) // decrease PoW diff -> raise bnTarget
    {
        bnTarget = ((ave * arith_uint256(goalperc)) + (target * arith_uint256(percPoS))) / arith_uint256(percPoS + goalperc);
       if ( 1 )
       {
           for (i=31; i>=24; i--)
               fprintf(stderr,"%02x",((uint8_t *)&ave)[i]);
           fprintf(stderr," decrease diff -> ");
           for (i=31; i>=24; i--)
               fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
           fprintf(stderr," floor diff ");
           for (i=31; i>=24; i--)
               fprintf(stderr,"%02x",((uint8_t *)&target)[i]);
           fprintf(stderr," ht.%d percPoS.%d vs goal.%d -> diff %d\n",height,percPoS,goalperc,goalperc - percPoS);
       }
    }
    else bnTarget = ave; // recent ave is perfect
    return(bnTarget);
}

int32_t komodo_check_deposit(int32_t height,const CBlock& block,uint32_t prevtime) // verify above block is valid pax pricing
{
    static uint256 array[64]; static int32_t numbanned,indallvouts;
    int32_t i,j,k,n,ht,baseid,txn_count,activation,num,opretlen,offset=1,errs=0,matched=0,kmdheights[256],otherheights[256]; uint256 hash,txids[256]; char symbol[KOMODO_ASSETCHAIN_MAXLEN],base[KOMODO_ASSETCHAIN_MAXLEN]; uint16_t vouts[256]; int8_t baseids[256]; uint8_t *script,opcode,rmd160s[256*20]; uint64_t total,subsidy,available,deposited,issued,withdrawn,approved,redeemed,checktoshis,seed; int64_t values[256],srcvalues[256]; struct pax_transaction *pax; struct komodo_state *sp;
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
            n = block.vtx[i].vin.size();
            for (j=0; j<n; j++)
            {
                for (k=0; k<numbanned; k++)
                {
                    if ( block.vtx[i].vin[j].prevout.hash == array[k] && (block.vtx[i].vin[j].prevout.n == 1 || k >= indallvouts)  )
                    {
                        printf("banned tx.%d being used at ht.%d txi.%d vini.%d\n",k,height,i,j);
                        return(-1);
                    }
                }
            }
        }
    }
    n = block.vtx[0].vout.size();
    script = (uint8_t *)block.vtx[0].vout[n-1].scriptPubKey.data();
    //if ( n <= 2 || script[0] != 0x6a )
    {
        int64_t val,prevtotal = 0; int32_t overflow = 0;
        total = 0;
        for (i=1; i<n; i++)
        {
            if ( (val= block.vtx[0].vout[i].nValue) < 0 || val >= MAX_MONEY )
            {
                overflow = 1;
                break;
            }
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
                //fprintf(stderr,">>>>>>>> <<<<<<<<<< ht.%d illegal nonz output %.8f n.%d\n",height,dstr(block.vtx[0].vout[1].nValue),n);
                if ( height >= activation )
                    return(-1);
            }
            else if ( block.nBits == KOMODO_MINDIFF_NBITS && total > 0 ) // to deal with fee stealing
            {
                fprintf(stderr,"notary mined ht.%d with extra %.8f\n",height,dstr(total));
                if ( height > KOMODO_NOTARIES_HEIGHT1 )
                    return(-1);
            }
        }
        else
        {
            if ( ASSETCHAINS_STAKED != 0 && height >= 2 )
            {
                arith_uint256 bnTarget,hashval; int32_t PoSperc; bool fNegative,fOverflow; CBlockIndex *previndex; uint32_t eligible,isPoS = 0;
                bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
                if ( txn_count > 1 )
                {
                    if ( prevtime == 0 )
                    {
                        if ( (previndex= mapBlockIndex[block.hashPrevBlock]) != 0 )
                            prevtime = (uint32_t)previndex->nTime;
                    }
                    eligible = komodo_stake(1,bnTarget,height,block.vtx[txn_count-1].vin[0].prevout.hash,block.vtx[txn_count-1].vin[0].prevout.n,block.nTime,prevtime,(char *)"");
                    if ( eligible == 0 || eligible > block.nTime )
                    {
                        fprintf(stderr,"PoS failure ht.%d eligible.%u vs blocktime.%u, lag.%d -> check to see if it is PoW block\n",height,eligible,(uint32_t)block.nTime,(int32_t)(eligible - block.nTime));
                    } else isPoS = 1;
                }
                if ( isPoS == 0 && height > 100 )
                {
                    if ( ASSETCHAINS_STAKED == 100 )
                    {
                        fprintf(stderr,"ht.%d 100%% PoS after height 100 rule violated for -ac_staking=100\n",height);
                        return(-1);
                    }
                    // check PoW
                    bnTarget = komodo_PoWtarget(&PoSperc,bnTarget,height,ASSETCHAINS_STAKED);
                    hashval = UintToArith256(block.GetHash());
                    if ( hashval > bnTarget )
                    {
                        /*for (i=31; i>=0; i--)
                            fprintf(stderr,"%02x",((uint8_t *)&hashval)[i]);
                        fprintf(stderr," > ");
                        for (i=31; i>=0; i--)
                            fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
                        fprintf(stderr," ht.%d PoW diff violation PoSperc.%d vs goalperc.%d\n",height,PoSperc,(int32_t)ASSETCHAINS_STAKED);*/
                        return(-1);
                    }
                }
            }
            if ( ASSETCHAINS_OVERRIDE_PUBKEY33[0] != 0 && ASSETCHAINS_COMMISSION != 0 && block.vtx[0].vout.size() > 1 )
            {
                script = (uint8_t *)block.vtx[0].vout[1].scriptPubKey.data();
                if ( script[0] != 33 || script[34] != OP_CHECKSIG || memcmp(script+1,ASSETCHAINS_OVERRIDE_PUBKEY33,33) != 0 )
                    return(-1);
                if ( (checktoshis = komodo_commission(block)) != 0 )
                {
                    if ( block.vtx[0].vout[1].nValue != checktoshis )
                    {
                        fprintf(stderr,"checktoshis %.8f vs actual vout[1] %.8f\n",dstr(checktoshis),dstr(block.vtx[0].vout[1].nValue));
                        return(-1);
                    } else return(0);
                }
            }
            if ( overflow != 0 || total > 0 )
                return(-1);
        }
        return(0);
    }
/*
    //fprintf(stderr,"ht.%d n.%d nValue %.8f (%d %d %d)\n",height,n,dstr(block.vtx[0].vout[1].nValue),KOMODO_PAX,komodo_isrealtime(&ht),KOMODO_PASSPORT_INITDONE);
    offset += komodo_scriptitemlen(&opretlen,&script[offset]);
    //printf("offset.%d opretlen.%d [%02x %02x %02x %02x]\n",offset,opretlen,script[0],script[1],script[2],script[3]);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        opcode = 'X';
        if ( height >= 235300 )
            return(-1);
        strcpy(symbol,(char *)"KMD");
        if ( komodo_isrealtime(&ht) == 0 || KOMODO_PASSPORT_INITDONE == 0 ) // init time already in DB
            return(0);
    }
    else
    {
        strcpy(symbol,ASSETCHAINS_SYMBOL);
        opcode = 'I';
        if ( (baseid= komodo_baseid(symbol)) < 0 )
        {
            if ( block.vtx[0].vout.size() != 1 )
            {
                printf("%s has more than one coinbase?\n",symbol);
                return(-1);
            }
            return(0);
        }
        while ( KOMODO_PASSPORT_INITDONE == 0 )
            komodo_passport_iteration();
        komodo_passport_iteration();
        // grandfathering preexisting
        if ( height < 777 )
            return(0);
        if ( baseid == USD ) // 6820 in balance calcs
        {
            if ( height <= 6821 || height <= 2000 || height == 2968 || height == 2960 || height == 2649 || height == 3275 || height == 3282 || height == 3328 || height == 3468 )
            return(0);
        }
        else if ( baseid == EUR )
        {
            if ( height < 1200 || height == 1408 || height == 1111 || height == 1094 || height == 1092 || height == 1080 || height == 1053 || height == 1005 || height == 822 || height == 821 || height == 810 || height == 797 || height == 790 || height == 787 || height == 785 || height == 1313 || height == 1288 || height == 1263 || height == 1262 || height <= 211 || height == 210 || height == 188 || height == 185 || height == 182 || height == 181 || height == 171 || height == 170 || height == 169 || height == 168 || height == 167 || height == 166 || height == 165 || height == 164 || height == 163 || height == 162 )
                return(0);
        }
        else if ( baseid == JPY )
        {
            if ( height == 1038 || height == 1039 || height == 1040 || height == 1041 || height == 1014 || height == 998 || height == 973 || height == 139 || height == 971 || height == 141 || height == 816 || height == 814 || height == 803 || height == 142 || height == 782 || height == 145 || height == 181 || height == 186 || height == 192 || height == 190 || height == 189 || height == 255 || height == 218 || height == 233 || height == 259 || height == 278 || height == 361 || height == 367 || height == 733 || height == 688 || height == 468 )
                return(0);
        }
        else if ( baseid == GBP )
        {
            if ( height == 1025 || height == 1026 || height == 1027 || height == 1028 || height == 997 || height == 970 || height == 953 || height == 951 || height == 803 || height == 800 || height == 786 || height == 123 || height == 771 || height == 137 || height == 139 || height == 140 || height == 146 || height == 194 || height == 200 || height == 202 || height == 203 || height == 205 || height == 210 || height == 216 || height == 271 || height == 233 || height == 294 )
                return(0);
        }
        else if ( baseid == AUD )
        {
            if ( height == 1012 || height == 1013 || height == 1014 || height == 985 || height == 958 || height == 936 || height == 933 || height == 790 || height == 788 || height == 778 || height == 124 || height == 777 || height == 122 || height == 140 || height == 142 || height == 143 || height == 186 || height == 196 || height == 198 || height == 201 || height == 210 || height == 192 )
                return(0);
        }
        else if ( baseid == CAD )
        {
            if ( height < 1000 || height == 836 || height == 781 || height == 779 || height == 798 || height == 794 || height == 1181 || height == 1179 || height == 1159 || height == 1134 || height == 1132 || height == 121 || height == 970 || height == 119 || height == 299 || height == 302 || height == 303 || height == 306 || height == 308 || height == 355 || height == 362 || height == 367 || height == 363 || height == 364 || height == 380 || height == 373 || height == 390 || height == 437 || height == 777 )
                return(0);
        }
        else if ( baseid == CHF )
        {
            if ( height == 987 || height == 988 || height == 989 || height == 960 || height == 931 || height == 914 || height == 122 || height == 912 || height == 141 || height == 780 || height == 145 || height == 777 || height == 151 || height == 775 || height == 765 || height == 195 || height == 764 || height == 737 || height == 194 || height == 193 || height == 198 || height == 207 || height == 216 || height == 227 || height == 256 || height == 260 || height == 276 || height == 289 || height == 292 || height == 349 )
                return(0);
        }
        else if ( baseid == NZD )
        {
            if ( height < 1000 || height == 787 || height == 786 || height == 784 || height == 785 || height == 777 || height == 1113 || height == 1083 || height == 1067 || height == 265 || height == 1064 || height == 920 || height == 913 || height == 124 || height == 911 || height == 903 || height == 900 || height == 879 || height == 122 || height == 266 || height == 269 || height == 270 || height == 278 || height == 307 || height == 318 || height == 322 || height == 335 || height == 395 || height == 406 || height == 314 )
                return(0);
        }
        else if ( baseid == CNY )
        {
            if ( height == 987 || height == 988 || height == 989 || height == 982 || height == 957 || height == 931 || height == 119 || height == 929 || height == 134 || height == 782 || height == 137 || height == 775 || height == 143 || height == 772 || height == 186 || height == 763 || height == 761 || height == 734 || height == 183 || height == 190 || height == 192 || height == 211 || height == 203 || height == 226 || height == 263 || height == 264 || height == 277 || height == 342 || height == 613 || height == 525 )
                return(0);
        }
        else if ( baseid == RUB )
        {
            if ( height == 819 || height == 812 || height == 810 || height == 802 || height == 801 || height == 1074 || height == 1040 || height == 1014 || height == 163 || height == 1013 || height == 877 || height == 867 || height == 863 || height == 834 || height == 158 || height == 126 || height == 106 || height == 105 || height == 174 || height == 175 || height == 176 || height == 177 || height == 181 || height == 183 || height == 184 || height == 187 )
                return(0);
        }
        else if ( baseid == MXN )
        {
            if ( height == 1079 || height == 1080 || height == 1081 || height == 1069 || height == 1048 || height == 1028 || height == 121 || height == 1027 || height == 146 || height == 894 || height == 891 || height == 148 || height == 882 || height == 879 || height == 151 || height == 856 || height == 158 || height == 822 || height == 197 || height == 203 || height == 207 || height == 209 || height == 213 || height == 217 || height == 218 || height == 226 || height == 283 || height == 302 || height == 319 || height == 318 || height == 569 )
                return(0);
        }
        else if ( baseid == BRL )
        {
            if ( height == 1003 || height == 1004 || height == 1005 || height == 972 || height == 943 || height == 922 || height == 118 || height == 920 || height == 145 || height == 787 || height == 783 || height == 147 || height == 781 || height == 774 || height == 148 || height == 771 || height == 154 || height == 751 || height == 156 || height == 188 || height == 193 || height == 195 || height == 199 || height == 203 || height == 225 || height == 230 || height == 262 || height == 266 || height == 284 || height == 300 || height == 640 )
                return(0);
        }
        else if ( baseid == INR )
        {
            if ( height == 998 || height == 999 || height == 1000 || height == 788 || height == 786 || height == 775 || height == 753 || height == 125 || height == 153 || height == 155 || height == 159 || height == 204 || height == 206 || height == 211 || height == 217 || height == 220 || height == 229 || height == 265 || height == 292 || height == 300 )
                return(0);
        }
        else if ( baseid == PLN )
        {
            if ( height == 1029 || height == 1030 || height == 1031 || height == 1009 || height == 984 || height == 960 || height == 959 || height == 798 || height == 794 || height == 792 || height == 782 || height == 775 || height == 755 || height == 122 || height == 120 || height == 158 || height == 160 || height == 163 || height == 225 || height == 217 || height == 219 || height == 229 || height == 234 || height == 286 || height == 231 || height == 292 )
                return(0);
        }
        else if ( baseid == HKD )
        {
            if ( height == 1013 || height == 1014 || height == 1015 || height == 1002 || height == 979 || height == 947 || height == 123 || height == 946 || height == 153 || height == 811 || height == 808 || height == 806 || height == 155 || height == 795 || height == 157 || height == 773 || height == 205 || height == 208 || height == 210 || height == 211 || height == 215 || height == 221 || height == 231 || height == 255 || height == 273 || height == 293 || height == 310 || height == 654 )
                return(0);
        }
        else if ( baseid == TRY )
        {
            if ( height == 1022 || height == 1023 || height == 1051 || height == 991 || height == 974 || height == 949 || height == 141 || height == 948 || height == 783 || height == 153 || height == 781 || height == 779 || height == 776 || height == 768 || height == 765 || height == 121 || height == 142 || height == 144 || height == 143 || height == 149 || height == 204 || height == 210 || height == 216 || height == 229 || height == 280 || height == 145 )
                return(0);
        }
        else if ( baseid == NOK )
        {
            if ( height == 987 || height == 988 || height == 989 || height == 962 || height == 911 || height == 907 || height == 125 || height == 765 || height == 150 || height == 762 || height == 760 || height == 753 || height == 152 || height == 751 || height == 151 || height == 154 || height == 160 || height == 195 || height == 203 || height == 207 || height == 211 || height == 220 || height == 236 || height == 253 || height == 278 || height == 283 )
                return(0);
        }
        else if ( baseid == ZAR )
        {
            if ( height == 979 || height == 980 || height == 981 || height == 956 || height == 937 || height == 906 || height == 905 || height == 122 || height == 756 || height == 754 || height == 752 || height == 749 || height == 742 || height == 739 || height == 120 || height == 151 || height == 153 || height == 157 || height == 159 || height == 206 || height == 213 || height == 216 || height == 222 || height == 277 || height == 287 || height == 215 )
                return(0);
        }
        else if ( baseid == SEK )
        {
            if ( height == 960 || height == 961 || height == 962 || height == 943 || height == 925 || height == 896 || height == 147 || height == 894 || height == 759 || height == 751 || height == 749 || height == 742 || height == 740 || height == 716 || height == 123 || height == 148 || height == 150 || height == 151 || height == 157 || height == 199 || height == 200 || height == 201 || height == 206 || height == 256 || height == 257 || height == 268 )
                return(0);
        }
        else if ( baseid == CZK )
        {
            if ( height < 1000 || height == 1084 || height == 1085 || height == 1086 || height == 778 || height == 1079 || height == 1054 || height == 1032 || height == 121 || height == 1030 || height == 182 || height == 876 || height == 873 || height == 871 || height == 184 || height == 862 || height == 185 || height == 861 || height == 186 || height == 838 || height == 238 || height == 242 || height == 246 || height == 248 || height == 250 || height == 251 || height == 256 || height == 282 || height == 322 || height == 343 || height == 433 || height == 323 || height == 276 )
                return(0);
        }
        else if ( baseid == HUF )
        {
            if ( height < 2200 || height == 1531 || height == 1528 || height == 1530 || height == 1451 || height == 1448 || height == 1442 || height == 1418 || height == 1407 || height == 1406 || height == 1392 || height == 1391 || height == 1387 || height == 1383 || height == 1335 || height == 1337 || height == 1338 || height == 1649 || height == 2378 || height == 2376 || height == 2371 || height == 1330 || height == 2326 || height == 1344 || height == 2305 || height == 2304 || height == 2171 || height == 2168 || height == 2166 || height == 2156 || height == 2135 || height == 1343 || height == 1342 || height == 1341 || height == 1340 || height == 1339 || height == 1336 || height == 1334 || height == 1333 || height == 1346 || height == 1400 || height == 1525 || height == 1509 )
                return(0);
        }
        else if ( baseid == DKK )
        {
            if ( height == 1241 || height == 945 || height == 898 || height == 894 || height == 892 || height == 866 || height == 857 || height == 853 || height == 834 || height == 812 || height == 792 || height == 784 || height == 778 || height == 783 || height == 1214 || height == 1186 || height == 1158 || height == 126 || height == 1156 || height == 329 || height == 1021 || height == 1016 || height == 1014 || height == 333 || height == 1003 || height == 1002 || height == 978 || height == 332 || height == 337 || height == 381 || height == 390 || height == 386 || height == 401 || height == 389 || height == 396 || height == 428 || height == 473 || height == 490 || height == 493 )
                return(0);
        }
        else if ( baseid == ILS )
        {
            if ( height == 1017 || height == 1018 || height == 1019 || height == 1010 || height == 985 || height == 954 || height == 120 || height == 952 || height == 143 || height == 819 || height == 817 || height == 808 || height == 145 || height == 786 || height == 155 || height == 753 || height == 151 || height == 149 || height == 199 || height == 193 || height == 201 || height == 209 || height == 284 || height == 305 || height == 676 || height == 702 || height == 406 )
                return(0);
        }
        else if ( baseid == MYR )
        {
            if ( height == 1020 || height == 1021 || height == 1022 || height == 1015 || height == 984 || height == 950 || height == 949 || height == 823 || height == 820 || height == 818 || height == 812 || height == 809 || height == 126 || height == 789 || height == 144 || height == 746 || height == 146 || height == 147 || height == 148 || height == 150 || height == 205 || height == 210 || height == 215 || height == 219 || height == 222 || height == 290 || height == 242 || height == 312 )
                return(0);
        }
        else if ( baseid == PHP )
        {
            if ( height == 859 || height == 817 || height == 812 || height == 811 || height == 792 || height == 779 || height == 778 || height == 781 || height == 1154 || height == 1114 || height == 1077 || height == 1076 || height == 931 || height == 927 || height == 925 || height == 918 || height == 891 || height == 127 || height == 125 || height == 272 || height == 275 || height == 276 || height == 278 || height == 328 || height == 330 || height == 335 || height == 344 || height == 315 )
                return(0);
        }
        else if ( baseid == KRW )
        {
            if ( height == 1021 || height == 1022 || height == 1023 || height == 1010 || height == 950 || height == 916 || height == 138 || height == 914 || height == 140 || height == 785 || height == 782 || height == 781 || height == 142 || height == 775 || height == 148 || height == 772 || height == 751 || height == 147 || height == 146 || height == 159 || height == 175 || height == 181 || height == 184 || height == 186 || height == 188 || height == 196 || height == 209 || height == 238 )
                return(0);
        }
        else if ( baseid == RON )
        {
            if ( height == 1008 || height == 1009 || height == 1010 || height == 1003 || height == 973 || height == 940 || height == 939 || height == 806 || height == 802 || height == 800 || height == 793 || height == 789 || height == 768 || height == 123 || height == 141 || height == 143 || height == 144 || height == 150 || height == 197 || height == 200 || height == 202 || height == 217 || height == 240 || height == 276 )
                return(0);
        }
        else if ( baseid == SGD )
        {
            if ( height == 1016 || height == 1017 || height == 1018 || height == 1010 || height == 984 || height == 951 || height == 950 || height == 801 || height == 798 || height == 792 || height == 788 || height == 122 || height == 769 || height == 120 || height == 158 || height == 160 || height == 164 || height == 169 || height == 229 || height == 234 || height == 246 || height == 297 || height == 299 || height == 316 || height == 233 )
                return(0);
        }
        else if ( baseid == THB )
        {
            if ( height == 977 || height == 978 || height == 968 || height == 933 || height == 898 || height == 897 || height == 788 || height == 786 || height == 779 || height == 777 || height == 757 || height == 119 || height == 140 || height == 142 || height == 146 || height == 152 || height == 196 || height == 200 || height == 214 || height == 260 || height == 263 )
                return(0);
        }
        else if ( baseid == BGN )
        {
            if ( height == 1044 || height == 1045 || height == 1046 || height == 1030 || height == 995 || height == 960 || height == 959 || height == 830 || height == 822 || height == 819 || height == 810 || height == 809 || height == 781 || height == 124 || height == 122 || height == 189 || height == 193 || height == 200 || height == 194 || height == 250 || height == 256 || height == 260 || height == 280 )
                return(0);
        }
        else if ( baseid == HRK )
        {
            if ( height == 1027 || height == 1028 || height == 1029 || height == 1022 || height == 999 || height == 969 || height == 967 || height == 832 || height == 829 || height == 827 || height == 817 || height == 792 || height == 754 || height == 125 || height == 123 || height == 184 || height == 186 || height == 187 || height == 227 || height == 230 || height == 235 || height == 240 || height == 242 || height == 261 || height == 336 )
                return(0);
        }
        else if ( baseid == IDR )
        {
            if ( height == 836 || height == 1036 || height == 837 || height == 720 || height == 715 || height == 714 || height == 696 || height == 695 || height == 694 || height == 693 || height == 119 || height == 689 || height == 120 || height == 669 || height == 123 || height == 662 || height == 122 || height == 634 || height == 121 || height == 609 || height == 124 || height == 174 || height == 603 || height == 595 || height == 166 || height == 176 || height == 589 || height == 172 || height == 587 || height == 179 || height == 239 || height == 175 || height == 245 || height == 260 )
                return(0);
        }
    }
    if ( script[offset] == opcode && opretlen < block.vtx[0].vout[n-1].scriptPubKey.size() )
    {
        if ( (num= komodo_issued_opreturn(base,txids,vouts,values,srcvalues,kmdheights,otherheights,baseids,rmd160s,&script[offset],opretlen,opcode == 'X')) > 0 )
        {
            for (i=1; i<n-1; i++)
            {
                if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 && (sp= komodo_stateptrget(ASSETCHAINS_SYMBOL)) != 0 && strcmp(CURRENCIES[baseids[i-1]],ASSETCHAINS_SYMBOL) == 0 )
                {
                    int32_t rtflag = 0;
                    while ( (sp->RTmask & (1LL << 32)) == 0 )
                    {
                        if ( rtflag == 0 )
                            printf("[%s] skip checkdeposit.%s not RT %llx\n",ASSETCHAINS_SYMBOL,CURRENCIES[baseids[i-1]],(long long)sp->RTmask);
                        sleep(60);
                        rtflag = 1;
                        //matched++;
                        //continue;
                    }
                    if ( rtflag != 0 )
                        printf("[%s] %s reached RT\n",ASSETCHAINS_SYMBOL,CURRENCIES[baseids[i-1]]);
                }
                if ( (pax= komodo_paxfinds(txids[i-1],vouts[i-1])) != 0 ) // finds... make sure right one
                {
                    pax->type = opcode;
                    if ( opcode == 'I' && (pax_fiatstatus(&available,&deposited,&issued,&withdrawn,&approved,&redeemed,symbol) != 0 || available < pax->fiatoshis) )
                    {
                        printf("checkdeposit.[%s.%d]: skip %s %.8f when avail %.8f deposited %.8f, issued %.8f withdrawn %.8f approved %.8f redeemed %.8f\n",ASSETCHAINS_SYMBOL,height,symbol,dstr(pax->fiatoshis),dstr(available),dstr(deposited),dstr(issued),dstr(withdrawn),dstr(approved),dstr(redeemed));
                        return(-1);
                    }
                    if ( pax->fiatoshis == block.vtx[0].vout[i].nValue )
                    {
                        matched++;
                        if ( pax->marked != 0 && height >= 80820 )
                        {
                            printf(">>>>>>>>>>> %c errs.%d i.%d match %.8f vs %.8f paxmarked.%d kht.%d ht.%d [%s].%d\n",opcode,errs,i,dstr(opcode == 'I' ? pax->fiatoshis : pax->komodoshis),dstr(block.vtx[0].vout[i].nValue),pax->marked,pax->height,pax->otherheight,ASSETCHAINS_SYMBOL,height);
                        }
                        else
                        {
                        }
                        if ( strcmp(ASSETCHAINS_SYMBOL,CURRENCIES[baseids[i-1]]) == 0 )
                        {
                            //check deposit validates AUD.1047 [3] 0.14585530 -> 0.01000000 (0.14585530 0.01000000 0.01000000)
                            char destaddr[64],coinaddr[64]; uint8_t pubkey33[33];
                            checktoshis = PAX_fiatdest(&seed,0,destaddr,pubkey33,coinaddr,pax->height,CURRENCIES[baseids[i-1]],pax->fiatoshis);
                            //checktoshis = komodo_paxprice(&seed,pax->height,CURRENCIES[baseids[i-1]],(char *)"KMD",(uint64_t)pax->komodoshis);
                            if ( komodo_paxcmp(CURRENCIES[baseids[i-1]],pax->height,pax->komodoshis,checktoshis,seed) < 0 )
                            {
                                printf("paxcmp FAIL when check deposit validates %s.%d [%d] %.8f -> %.8f (%.8f %.8f %.8f)\n",CURRENCIES[baseids[i-1]],height,i,dstr(srcvalues[i-1]),dstr(values[i-1]),dstr(pax->komodoshis),dstr(pax->fiatoshis),dstr(block.vtx[0].vout[i].nValue));
                                return(-1);
                            } //else printf("check deposit validates %s.%d [%d] %.8f -> %.8f (%.8f %.8f %.8f)\n",CURRENCIES[baseids[i-1]],height,i,dstr(srcvalues[i-1]),dstr(values[i-1]),dstr(pax->komodoshis),dstr(pax->fiatoshis),dstr(block.vtx[0].vout[i].nValue));
                        }
                    }
                    else if ( strcmp(ASSETCHAINS_SYMBOL,CURRENCIES[baseids[i-1]]) == 0 )
                    {
                        for (j=0; j<32; j++)
                            printf("%02x",((uint8_t *)&txids[i-1])[j]);
                        printf(" cant paxfind %c txid [%d]\n",opcode,height);
                        printf(">>>>>>>>>>> %c errs.%d i.%d match %.8f vs %.8f pax.%p [%s] ht.%d\n",opcode,errs,i,dstr(opcode == 'I' ? pax->fiatoshis : pax->komodoshis),dstr(block.vtx[0].vout[i].nValue),pax,ASSETCHAINS_SYMBOL,height);
                        return(-1);
                    }
                }
                else //if ( kmdheights[i-1] > 0 && otherheights[i-1] > 0 )
                {
                    hash = block.GetHash();
                    for (j=0; j<n; j++)
                        printf("%.8f ",dstr(block.vtx[0].vout[j].nValue));
                    printf("vout values\n");
                    for (j=0; j<32; j++)
                        printf("%02x",((uint8_t *)&txids[i-1])[j]);
                    printf(" kht.%d ht.%d %.8f %.8f couldnt find vout.[%d of %d] ht.%d %s for [%s]\n",kmdheights[i-1],otherheights[i-1],dstr(values[i-1]),dstr(srcvalues[i]),i,n,height,ASSETCHAINS_SYMBOL,CURRENCIES[baseids[i-1]]);
                    if ( ASSETCHAINS_SYMBOL[0] != 0 || height >= activation )
                        return(-1);
                }
            }
            if ( ASSETCHAINS_SYMBOL[0] == 0 )
            {
                //if ( height > 0 && (height < chainActive.Tip()->nHeight || (height >= chainActive.Tip()->nHeight && komodo_isrealtime(&ht) != 0)) && matched != num )
                //{
                   // printf("WOULD REJECT %s: ht.%d (%c) matched.%d vs num.%d tip.%d isRT.%d\n",symbol,height,opcode,matched,num,(int32_t)chainActive.Tip()->nHeight,komodo_isrealtime(&ht));
                    // can easily happen depending on order of loading
                    //if ( height > 200000 )
                    //{
                    //    printf("REJECT: ht.%d (%c) matched.%d vs num.%d\n",height,opcode,matched,num);
                    //    return(-1);
                    //}
                //} // disabled 'X' path
            }
            else
            {
                if ( height > 0 && (height < chainActive.Tip()->nHeight || (height >= chainActive.Tip()->nHeight && komodo_isrealtime(&ht) != 0)) && matched != num )
                {
                    printf("REJECT %s: ht.%d (%c) matched.%d vs num.%d tip.%d isRT.%d\n",symbol,height,opcode,matched,num,(int32_t)chainActive.Tip()->nHeight,komodo_isrealtime(&ht));
                    return(-1);
                }
            }
        }
        else
        {
            for (i=0; i<n; i++)
                printf("%.8f ",dstr(block.vtx[0].vout[i].nValue));
            printf("no opreturn entries to check ht.%d %s\n",height,ASSETCHAINS_SYMBOL);
            if ( ASSETCHAINS_SYMBOL[0] != 0 || height >= activation )
                return(-1);
        }
        //printf("opretlen.%d num.%d\n",opretlen,num);
    }
    else
    {
        for (i=0; i<opretlen&&i<100; i++)
            printf("%02x",script[i]);
        printf(" height.%d checkdeposit n.%d [%02x] [%c] %d opcode.%d len.%d ",height,n,script[0],script[offset],script[offset],opcode,opretlen);
        printf("not proper vout with opreturn format %s ht.%d cmp.%d %d\n",ASSETCHAINS_SYMBOL,height,script[offset] == opcode,(int32_t)block.vtx[0].vout[n-1].scriptPubKey.size());
        return(-1);
    }
    return(0);*/
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
        fprintf(stderr,"inds.%p validate %s fsize.%ld datalen.%ld n.%ld lastfpos.%ld\n",inds,indfname,fsize,datalen,fsize / sizeof(uint32_t),lastfpos);
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
                fprintf(stderr,"datalen.%ld validated %ld -> indcounter %u, prevpos100 %u offset.%ld\n",datalen,validated,indcounter,prevpos100,indcounter * sizeof(uint32_t));
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

void komodo_passport_iteration()
{
    static long lastpos[34]; static char userpass[33][1024]; static uint32_t lasttime,callcounter;
    int32_t maxseconds = 10;
    FILE *fp; uint8_t *filedata; long fpos,datalen,lastfpos; int32_t baseid,limit,n,ht,isrealtime,expired,refid,blocks,longest; struct komodo_state *sp,*refsp; char *retstr,fname[512],*base,symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; uint32_t buf[3],starttime; cJSON *infoobj,*result; uint64_t RTmask = 0;
    expired = 0;
    while ( KOMODO_INITDONE == 0 )
    {
        fprintf(stderr,"[%s] PASSPORT iteration waiting for KOMODO_INITDONE\n",ASSETCHAINS_SYMBOL);
        sleep(3);
    }
    refsp = komodo_stateptr(symbol,dest);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
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
                buf[0] = (uint32_t)chainActive.Tip()->nHeight;
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
    komodo_paxtotal();
    refsp->RTmask |= RTmask;
    if ( expired == 0 && KOMODO_PASSPORT_INITDONE == 0 )
    {
        KOMODO_PASSPORT_INITDONE = 1;
        printf("done PASSPORT %s refid.%d\n",ASSETCHAINS_SYMBOL,refid);
    }
}

