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

// paxdeposit equivalent in reverse makes opreturn and KMD does the same in reverse

int32_t pax_fiatstatus(uint64_t *available,uint64_t *deposited,uint64_t *issued,uint64_t *withdrawn,uint64_t *approved,uint64_t *redeemed,char *base)
{
    int32_t baseid; struct komodo_state *sp; int64_t netliability,maxallowed;
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
            netliability = (sp->deposited - sp->withdrawn) - sp->shorted;
            maxallowed = komodo_maxallowed(baseid);
            if ( netliability < maxallowed )
                *available = (maxallowed - netliability);
            //printf("%p %s %.8f %.8f %.8f %.8f %.8f\n",sp,base,dstr(*deposited),dstr(*issued),dstr(*withdrawn),dstr(*approved),dstr(*redeemed));
            return(0);
        } else printf("pax_fiatstatus cant get basesp.%s\n",base);
    } else printf("pax_fiatstatus illegal base.%s\n",base);
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
    struct pax_transaction *pax; int32_t i; uint8_t types[] = { 'D', 'I', 'W', 'A', 'X' };
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
        pax->marked = mark;
    pthread_mutex_unlock(&komodo_mutex);
    return(pax);
}

void komodo_gateway_deposit(char *coinaddr,uint64_t value,char *symbol,uint64_t fiatoshis,uint8_t *rmd160,uint256 txid,uint16_t vout,uint8_t type,int32_t height,int32_t otherheight,char *source,int32_t approved) // assetchain context
{
    struct pax_transaction *pax; uint8_t buf[35]; int32_t addflag = 0; struct komodo_state *sp; char str[16],dest[16],*s;
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
    struct pax_transaction p,*pax; int32_t i,n=0,j,len=0,incr,height,otherheight; uint8_t type,rmd160[20]; uint64_t fiatoshis; char symbol[16];
    incr = 34 + (iskomodo * (2*sizeof(fiatoshis) + 2*sizeof(height) + 20 + 4));
    for (i=0; i<4; i++)
        base[i] = opretbuf[opretlen-4+i];
    //for (i=0; i<opretlen; i++)
    //    printf("%02x",opretbuf[i]);
    //printf(" opretlen.%d (%s)\n",opretlen,base);
    //printf(" opretlen.%d vs %d incr.%d\n",opretlen,(int32_t)(2*sizeof(fiatoshis) + 2*sizeof(height) + 20 + 4),incr);
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

uint64_t komodo_paxtotal()
{
    struct pax_transaction *pax,*pax2,*tmp,*tmp2; char symbol[16],dest[16],*str; int32_t i,ht; int64_t checktoshis; uint64_t seed,total = 0; struct komodo_state *basesp;
    if ( komodo_isrealtime(&ht) == 0 )
        return(0);
    else
    {
        HASH_ITER(hh,PAX,pax,tmp)
        {
            if ( pax->type == 'A' || pax->type == 'D' || pax->type == 'X' )
                str = pax->symbol;
            else str = pax->source;
            basesp = komodo_stateptrget(str);
            if ( basesp != 0 && pax->didstats == 0 && pax->type == 'I' )
            {
                if ( (pax2= komodo_paxfind(pax->txid,pax->vout,'D')) != 0 )
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
            }
        }
    }
    komodo_stateptr(symbol,dest);
    HASH_ITER(hh,PAX,pax,tmp)
    {
        //printf("pax.%s marked.%d %.8f -> %.8f\n",pax->symbol,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis));
        if ( strcmp(symbol,pax->symbol) == 0 )
        {
            if ( pax->marked == 0 )
            {
                if ( komodo_is_issuer() != 0 )
                    total += pax->fiatoshis;
                else if ( pax->approved != 0 )
                {
                    if ( pax->validated != 0 )
                        total += pax->komodoshis;
                    else
                    {
                        seed = 0;
                        checktoshis = komodo_paxprice(&seed,pax->height,pax->source,(char *)"KMD",(uint64_t)pax->fiatoshis);
                        //printf("PAX_fiatdest ht.%d price %s %.8f -> KMD %.8f vs %.8f\n",pax->height,pax->symbol,(double)pax->fiatoshis/COIN,(double)pax->komodoshis/COIN,(double)checktoshis/COIN);
                        //printf(" v%d %.8f k.%d ht.%d\n",pax->vout,dstr(pax->komodoshis),pax->height,pax->otherheight);
                        if ( seed != 0 )
                        {
                            if ( checktoshis >= pax->komodoshis )
                            {
                                total += pax->komodoshis;
                                pax->validated = pax->komodoshis;
                            } else pax->marked = pax->height;
                        }
                    }
                }
            }
        }
    }
    //printf("paxtotal %.8f\n",dstr(total));
    return(total);
}

int32_t komodo_pending_withdraws(char *opretstr)
{
    struct pax_transaction *pax,*tmp; uint8_t opretbuf[16384]; int32_t ht,len=0; uint64_t total = 0;
    if ( komodo_isrealtime(&ht) == 0 || ASSETCHAINS_SYMBOL[0] != 0 )
        return(0);
    HASH_ITER(hh,PAX,pax,tmp)
    {
        //printf("pax %s marked.%u approved.%u\n",pax->symbol,pax->marked,pax->approved);
        if ( pax->marked == 0 && strcmp((char *)"KMD",pax->symbol) == 0 && pax->approved == 0 )
        {
            // add 'A' opreturn entry
            if ( len == 0 )
                opretbuf[len++] = 'A';
            len += komodo_rwapproval(1,&opretbuf[len],pax);
            //printf("%s.(marked.%u approved.%d) %p\n",pax->source,pax->marked,pax->approved,pax);
        }
    }
    if ( len > 0 )
        init_hexbytes_noT(opretstr,opretbuf,len);
    else opretstr[0] = 0;
    fprintf(stderr,"komodo_pending_withdraws len.%d PAXTOTAL %.8f\n",len,dstr(komodo_paxtotal()));
    return(len);
}

int32_t komodo_gateway_deposits(CMutableTransaction *txNew,char *base,int32_t tokomodo)
{
    struct pax_transaction *pax,*tmp; char symbol[16],dest[16]; uint8_t *script,opcode,opret[16384],data[16384]; int32_t i,baseid,ht,len=0,opretlen=0,numvouts=1; struct komodo_state *sp; uint64_t available,deposited,issued,withdrawn,approved,redeemed,mask;
    sp = komodo_stateptr(symbol,dest);
    strcpy(symbol,base);
    PENDING_KOMODO_TX = 0;
    if ( tokomodo == 0 )
    {
        opcode = 'I';
        if ( komodo_isrealtime(&ht) == 0 )
            return(0);
    } else opcode = 'X';
    HASH_ITER(hh,PAX,pax,tmp)
    {
        //printf("pax.%s marked.%d %.8f -> %.8f\n",pax->symbol,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis));
        if ( strcmp(symbol,"KMD") == 0 && pax->approved == 0 )
            continue;
        //else if ( strcmp(symbol,"KMD") != 0 )
        {
#ifdef KOMODO_ASSETCHAINS_WAITNOTARIZE
            struct komodo_state *kmdsp = komodo_stateptrget((char *)"KMD");
            if ( kmdsp != 0 && kmdsp->NOTARIZED_HEIGHT >= pax->height ) // assumes same chain as notarize
                pax->validated = pax->komodoshis; //kmdsp->NOTARIZED_HEIGHT;
#endif
        }
        if ( pax_fiatstatus(&available,&deposited,&issued,&withdrawn,&approved,&redeemed,symbol) != 0 || available < pax->fiatoshis )
        {
            printf("miner: skip %s %.8f when avail %.8f\n",symbol,dstr(pax->fiatoshis),dstr(available));
            continue;
        }
        if ( pax->marked != 0 )
            continue;
        if ( strcmp(pax->symbol,symbol) != 0 || pax->validated == 0 )
        {
            //printf("pax->symbol.%s != %s or null pax->validated %.8f\n",pax->symbol,symbol,dstr(pax->validated));
            continue;
        }
        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
            printf("pax.%s marked.%d %.8f -> %.8f\n",ASSETCHAINS_SYMBOL,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis));
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
            //printf(" vout.%u DEPOSIT %.8f <- pax.%s pending %.8f | ",pax->vout,(double)txNew->vout[numvouts].nValue/COIN,symbol,dstr(PENDING_KOMODO_TX));
        }
        if ( numvouts++ >= 64 )
            break;
    }
    if ( numvouts > 1 )
    {
        if ( tokomodo != 0 )
            strcpy(symbol,(char *)"KMD");
        for (i=0; symbol[i]!=0; i++)
            data[len++] = symbol[i];
        data[len++] = 0;
        opretlen = komodo_opreturnscript(opret,opcode,data,len);
        txNew->vout.resize(numvouts+1);
        txNew->vout[numvouts].nValue = 0;
        txNew->vout[numvouts].scriptPubKey.resize(opretlen);
        script = (uint8_t *)&txNew->vout[numvouts].scriptPubKey[0];
        memcpy(script,opret,opretlen);
        printf("MINER deposits.%d (%s) vouts.%d %.8f opretlen.%d\n",tokomodo,ASSETCHAINS_SYMBOL,numvouts,dstr(PENDING_KOMODO_TX),opretlen);
        return(1);
    }
    return(0);
}

int32_t komodo_check_deposit(int32_t height,const CBlock& block) // verify above block is valid pax pricing
{
    int32_t i,j,n,num,opretlen,offset=1,errs=0,matched=0,kmdheights[64],otherheights[64]; uint256 hash,txids[64]; char symbol[16],base[16]; uint16_t vouts[64]; int8_t baseids[64]; uint8_t *script,opcode,rmd160s[64*20]; uint64_t available,deposited,issued,withdrawn,approved,redeemed; int64_t values[64],srcvalues[64]; struct pax_transaction *pax;
    memset(baseids,0xff,sizeof(baseids));
    memset(values,0,sizeof(values));
    memset(srcvalues,0,sizeof(srcvalues));
    memset(rmd160s,0,sizeof(rmd160s));
    memset(kmdheights,0,sizeof(kmdheights));
    memset(otherheights,0,sizeof(otherheights));
    n = block.vtx[0].vout.size();
    script = (uint8_t *)block.vtx[0].vout[n-1].scriptPubKey.data();
    if ( n <= 2 || script[0] != 0x6a )
        return(0);
    offset += komodo_scriptitemlen(&opretlen,&script[offset]);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        //for (i=0; i<opretlen; i++)
        //    printf("%02x",script[i]);
        //printf(" height.%d checkdeposit n.%d [%02x] [%c] %d vs %d\n",height,n,script[0],script[offset],script[offset],'X');
        opcode = 'X';
        strcpy(symbol,(char *)"KMD");
    }
    else
    {
        strcpy(symbol,ASSETCHAINS_SYMBOL);
        opcode = 'I';
    }
    if ( script[offset] == opcode && opretlen < block.vtx[0].vout[n-1].scriptPubKey.size() )
    {
        if ( (num= komodo_issued_opreturn(base,txids,vouts,values,srcvalues,kmdheights,otherheights,baseids,rmd160s,&script[offset],opretlen,opcode == 'X')) > 0 )
        {
            for (i=1; i<n-1; i++)
            {
                if ( (pax= komodo_paxfinds(txids[i-1],vouts[i-1])) != 0 )
                {
                    pax->type = opcode;
                    if ( opcode == 'I' && pax_fiatstatus(&available,&deposited,&issued,&withdrawn,&approved,&redeemed,symbol) != 0 || available < pax->fiatoshis )
                    {
                        printf("checkdeposit: skip %s %.8f when avail %.8f\n",pax->symbol,dstr(pax->fiatoshis),dstr(available));
                        continue;
                    }
                    if ( ((opcode == 'I' && (pax->fiatoshis == 0 || pax->fiatoshis == block.vtx[0].vout[i].nValue)) || (opcode == 'X' && (pax->komodoshis == 0 || pax->komodoshis == block.vtx[0].vout[i].nValue))) )
                    {
                        if ( pax->marked != 0 && height >= 80820 )
                        {
                            printf("%c errs.%d i.%d match %.8f vs %.8f pax.%p\n",opcode,errs,i,dstr(opcode == 'I' ? pax->fiatoshis : pax->komodoshis),dstr(block.vtx[0].vout[i].nValue),pax);
                            errs++;
                        } else matched++;
                    }
                    else
                    {
                        for (j=0; j<32; j++)
                            printf("%02x",((uint8_t *)&txids[i-1])[j]);
                        printf(" cant paxfind %c txid\n",opcode);
                        printf("%c errs.%d i.%d match %.8f vs %.8f pax.%p\n",opcode,errs,i,dstr(opcode == 'I' ? pax->fiatoshis : pax->komodoshis),dstr(block.vtx[0].vout[i].nValue),pax);
                    }
                }
                else
                {
                    hash = block.GetHash();
                    for (j=0; j<32; j++)
                        printf("%02x",((uint8_t *)&hash)[j]);
                    printf(" ht.%d blockhash X couldnt find vout.[%d]\n",height,i);
                }
            }
            if ( matched != num )
            {
                printf("WOULD REJECT %s: ht.%d (%c) matched.%d vs num.%d\n",symbol,height,opcode,matched,num);
                // can easily happen depending on order of loading
                if ( height > 100000 ) //&& opcode == 'X' )
                {
                    printf("REJECT: ht.%d (%c) matched.%d vs num.%d\n",height,opcode,matched,num);
                    return(-1);
                }
            }
        }
        //printf("opretlen.%d num.%d\n",opretlen,num);
    }
    return(0);
}

int32_t komodo_paxcmp(char *symbol,int32_t kmdheight,uint64_t value,uint64_t checkvalue,uint64_t seed)
{
    int32_t ratio;
    if ( seed == 0 && checkvalue != 0 )
    {
        ratio = ((value << 6) / checkvalue);
        if ( ratio >= 63 && ratio <= 65 )
            return(0);
        else
        {
            if ( kmdheight >= 86150 )
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

const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout,char *source)
{
    uint8_t rmd160[20],rmd160s[64*20],addrtype,shortflag,pubkey33[33]; int32_t didstats,i,j,n,len,tokomodo,kmdheight,otherheights[64],kmdheights[64]; int8_t baseids[64]; char base[4],coinaddr[64],destaddr[64]; uint256 txids[64]; uint16_t vouts[64]; uint64_t convtoshis,seed; int64_t fiatoshis,komodoshis,checktoshis,values[64],srcvalues[64]; struct pax_transaction *pax,*pax2; struct komodo_state *basesp; double diff;
    const char *typestr = "unknown";
    memset(baseids,0xff,sizeof(baseids));
    memset(values,0,sizeof(values));
    memset(srcvalues,0,sizeof(srcvalues));
    memset(rmd160s,0,sizeof(rmd160s));
    memset(kmdheights,0,sizeof(kmdheights));
    memset(otherheights,0,sizeof(otherheights));
    tokomodo = (komodo_is_issuer() == 0);
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
            typestr = "deposit";
            if ( kmdheight <= height )
            {
                didstats = 0;
                if ( strcmp(base,ASSETCHAINS_SYMBOL) == 0 )
                {
                    printf("(%s) (%s) kmdheight.%d vs height.%d check %.8f vs %.8f tokomodo.%d %d seed.%llx\n",ASSETCHAINS_SYMBOL,base,kmdheight,height,dstr(checktoshis),dstr(value),komodo_is_issuer(),strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0,(long long)seed);
                    for (i=0; i<32; i++)
                        printf("%02x",((uint8_t *)&txid)[i]);
                    printf(" <- txid.v%u ",vout);
                    for (i=0; i<33; i++)
                        printf("%02x",pubkey33[i]);
                    printf(" checkpubkey check %.8f v %.8f dest.(%s) kmdheight.%d height.%d\n",dstr(checktoshis),dstr(value),destaddr,kmdheight,height);
                }
                if ( komodo_paxcmp(base,kmdheight,value,checktoshis,seed) == 0 )
                {
                    if ( (pax= komodo_paxfind(txid,vout,'D')) == 0 )
                    {
                        if ( (basesp= komodo_stateptrget(base)) != 0 )
                        {
                            basesp->deposited += fiatoshis;
                            didstats = 1;
                            if ( strcmp(base,ASSETCHAINS_SYMBOL) == 0 )
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
                                if ( strcmp(base,ASSETCHAINS_SYMBOL) == 0 )
                                    printf("########### %p deposited %s += %.8f/%.8f kmdheight.%d/%d %.8f/%.8f\n",basesp,base,dstr(fiatoshis),dstr(pax->fiatoshis),kmdheight,pax->height,dstr(value),dstr(pax->komodoshis));
                            }
                        }
                        if ( didstats != 0 )
                            pax->didstats = 1;
                        if ( (pax2= komodo_paxfind(txid,vout,'I')) != 0 )
                        {
                            pax2->fiatoshis = pax->fiatoshis;
                            pax2->komodoshis = pax->komodoshis;
                            pax->marked = pax2->marked = pax->height;
                            pax->otherheight = height;
                            if ( pax2->didstats == 0 )
                            {
                                if ( (basesp= komodo_stateptrget(base)) != 0 )
                                {
                                    basesp->issued += pax2->fiatoshis;
                                    pax2->didstats = 1;
                                    if ( strcmp(base,ASSETCHAINS_SYMBOL) == 0 )
                                        printf("########### %p issueda %s += %.8f kmdheight.%d %.8f other.%d\n",basesp,base,dstr(pax2->fiatoshis),pax2->height,dstr(pax2->komodoshis),pax2->otherheight);
                                }
                            }
                        }
                    }
                }
                else if ( kmdheight > 91800 )
                    printf("pax %s deposit %.8f rejected kmdheight.%d %.8f KMD\n",base,dstr(fiatoshis),kmdheight,dstr(value));
            }
        }
    }
    else if ( opretbuf[0] == 'I' )
    {
        tokomodo = 0;
        if ( strncmp((char *)"KMD",(char *)&opretbuf[opretlen-4],3) != 0 )
        {
            if ( (n= komodo_issued_opreturn(base,txids,vouts,values,srcvalues,kmdheights,otherheights,baseids,rmd160s,opretbuf,opretlen,0)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    if ( baseids[i] < 0 )
                    {
                        printf("%d of %d illegal baseid.%d\n",i,n,baseids[i]);
                        continue;
                    }
                    bitcoin_address(coinaddr,60,&rmd160s[i*20],20);
                    komodo_gateway_deposit(coinaddr,0,0,0,0,txids[i],vouts[i],'I',height,0,CURRENCIES[baseids[i]],0);
                    komodo_paxmark(height,txids[i],vouts[i],'I',height);
                    if ( (pax= komodo_paxfind(txids[i],vouts[i],'I')) != 0 )
                    {
                        pax->type = opretbuf[0];
                        strcpy(pax->source,(char *)&opretbuf[opretlen-4]);
                        if ( (pax2= komodo_paxfind(txids[i],vouts[i],'D')) != 0 )
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
                                    if ( strcmp(CURRENCIES[baseids[i]],ASSETCHAINS_SYMBOL) == 0 )
                                        printf("########### %p issuedb %s += %.8f kmdheight.%d %.8f other.%d\n",basesp,CURRENCIES[baseids[i]],dstr(pax->fiatoshis),pax->height,dstr(pax->komodoshis),pax->otherheight);
                                }
                            }
                        }
                        komodo_paxmark(pax->height,txids[i],vouts[i],'D',height);
                    }
                }
            } else printf("opreturn none issued?\n");
        }
        if ( strcmp(source,ASSETCHAINS_SYMBOL) == 0 )
            printf("source.%s opreturn[I] matches %s\n",source,(char *)&opretbuf[opretlen-4]);
    }
    else if ( opretbuf[0] == 'W' && opretlen >= 38 )
    {
        tokomodo = 1;
        iguana_rwnum(0,&opretbuf[34],sizeof(kmdheight),&kmdheight);
        memset(base,0,sizeof(base));
        PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&komodoshis);
        bitcoin_address(coinaddr,addrtype,rmd160,20);
        checktoshis = PAX_fiatdest(&seed,tokomodo,destaddr,pubkey33,coinaddr,kmdheight,base,value);
        typestr = "withdraw";
        //printf("%s.height.%d vs height.%d check %.8f/%.8f vs %.8f tokomodo.%d %d seed.%llx -> (%s)\n",ASSETCHAINS_SYMBOL,kmdheight,height,dstr(checktoshis),dstr(komodoshis),dstr(value),komodo_is_issuer(),strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0,(long long)seed,coinaddr);
        didstats = 0;
        if ( komodo_paxcmp(base,kmdheight,komodoshis,checktoshis,seed) == 0 )
        {
            if ( value != 0 && ((pax= komodo_paxfind(txid,vout,'W')) == 0 || pax->didstats == 0) )
            {
                if ( (basesp= komodo_stateptrget(base)) != 0 )
                {
                    basesp->withdrawn += value;
                    didstats = 1;
                    if ( strcmp(base,ASSETCHAINS_SYMBOL) == 0 )
                        printf("########### %p withdrawn %s += %.8f\n",basesp,base,dstr(value));
                }
                //printf("notarize %s %.8f -> %.8f kmd.%d other.%d\n",ASSETCHAINS_SYMBOL,dstr(value),dstr(komodoshis),kmdheight,height);
            }
            komodo_gateway_deposit(coinaddr,komodoshis,(char *)"KMD",value,rmd160,txid,vout,'W',kmdheight,height,source,0);
            if ( (pax= komodo_paxfind(txid,vout,'W')) != 0 )
            {
                if ( didstats != 0 )
                    pax->didstats = 1;
                pax->type = opretbuf[0];
                pax->validated = komodoshis;
            }
        }
    }
    else if ( tokomodo != 0 && opretbuf[0] == 'A' )
    {
        tokomodo = 1;
        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
        {
            for (i=0; i<opretlen; i++)
                printf("%02x",opretbuf[i]);
            printf(" opret[%c] else path tokomodo.%d ht.%d before %.8f\n",opretbuf[0],tokomodo,height,dstr(komodo_paxtotal()));
        }
        if ( (n= komodo_issued_opreturn(base,txids,vouts,values,srcvalues,kmdheights,otherheights,baseids,rmd160s,opretbuf,opretlen,1)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                if ( baseids[i] < 0 )
                    continue;
                didstats = 0;
                seed = 0;
                checktoshis = komodo_paxprice(&seed,kmdheights[i],CURRENCIES[baseids[i]],(char *)"KMD",(uint64_t)srcvalues[i]);
                /*printf("PAX_fiatdest ht.%d price %s %.8f -> KMD %.8f vs %.8f\n",kmdheights[i],CURRENCIES[baseids[i]],(double)srcvalues[i]/COIN,(double)values[i]/COIN,(double)checktoshis/COIN);
                for (j=0; j<32; j++)
                    printf("%02x",((uint8_t *)&txids[i])[j]);
                printf(" v%d %.8f k.%d ht.%d base.%d\n",vouts[i],dstr(values[i]),kmdheights[i],otherheights[i],baseids[i]);*/
                if ( (pax= komodo_paxfind(txids[i],vouts[i],'A')) == 0 )
                {
                    bitcoin_address(coinaddr,60,&rmd160s[i*20],20);
                    komodo_gateway_deposit(coinaddr,values[i],CURRENCIES[baseids[i]],srcvalues[i],&rmd160s[i*20],txids[i],vouts[i],'A',kmdheights[i],otherheights[i],CURRENCIES[baseids[i]],kmdheights[i]);
                    komodo_paxmark(height,txids[i],vouts[i],'W',height);
                    komodo_paxmark(height,txids[i],vouts[i],'A',height);
                    if ( srcvalues[i] != 0 && (basesp= komodo_stateptrget(CURRENCIES[baseids[i]])) != 0 )
                    {
                        basesp->approved += srcvalues[i];
                        didstats = 1;
                        if ( strcmp(CURRENCIES[baseids[i]],ASSETCHAINS_SYMBOL) == 0 )
                            printf("########### %p approved %s += %.8f\n",basesp,CURRENCIES[baseids[i]],dstr(srcvalues[i]));
                    }
                    //printf(" i.%d (%s) <- %.8f ADDFLAG APPROVED\n",i,coinaddr,dstr(values[i]));
                }
                else if ( pax->didstats == 0 && srcvalues[i] != 0 )
                {
                    if ( (basesp= komodo_stateptrget(CURRENCIES[baseids[i]])) != 0 )
                    {
                        basesp->approved += srcvalues[i];
                        didstats = 1;
                        if ( strcmp(CURRENCIES[baseids[i]],ASSETCHAINS_SYMBOL) == 0 )
                            printf("########### %p approved %s += %.8f\n",basesp,CURRENCIES[baseids[i]],dstr(srcvalues[i]));
                    }
                } //else printf(" i.%d of n.%d pax.%p baseids[] %d\n",i,n,pax,baseids[i]);
                if ( (pax= komodo_paxfind(txids[i],vouts[i],'A')) != 0 )
                {
                    pax->type = opretbuf[0];
                    pax->approved = kmdheights[i];
                    if ( didstats != 0 )
                        pax->didstats = 1;
                    if ( strcmp(CURRENCIES[baseids[i]],ASSETCHAINS_SYMBOL) == 0 )
                        printf(" i.%d approved.%d <<<<<<<<<<<<< APPROVED %p\n",i,kmdheights[i],pax);
                }
            }
        }
        //printf("extra.[%d] after %.8f\n",n,dstr(komodo_paxtotal()));
    }
    else if ( opretbuf[0] == 'X' )
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
                    if ( baseids[i] >= 0 && srcvalues[i] != 0 && (basesp= komodo_stateptrget(CURRENCIES[baseids[i]])) != 0 )
                    {
                        basesp->redeemed += srcvalues[i];
                        pax->didstats = 1;
                        if ( strcmp(CURRENCIES[baseids[i]],ASSETCHAINS_SYMBOL) == 0 )
                            printf("########### %p redeemed %s += %.8f\n",basesp,CURRENCIES[baseids[i]],dstr(srcvalues[i]));
                    }
                }
            }
        } //else printf("komodo_issued_opreturn returned %d\n",n);
    }
    return(typestr);
}

void komodo_passport_iteration()
{
    static long lastpos[34]; static char userpass[33][1024];
    FILE *fp; int32_t baseid,isrealtime,refid,blocks,longest; struct komodo_state *sp,*refsp; char *retstr,fname[512],*base,symbol[16],dest[16]; uint32_t buf[3]; cJSON *infoobj,*result; uint64_t RTmask = 0;
    while ( KOMODO_INITDONE == 0 )
    {
        fprintf(stderr,"PASSPORT iteration waiting for KOMODO_INITDONE\n");
        sleep(3);
    }
    refsp = komodo_stateptr(symbol,dest);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
        refid = 33;
    else refid = komodo_baseid(ASSETCHAINS_SYMBOL)+1; // illegal base -> baseid.-1 -> 0
    //printf("PASSPORT %s refid.%d\n",ASSETCHAINS_SYMBOL,refid);
    for (baseid=32; baseid>=0; baseid--)
    {
        sp = 0;
        isrealtime = 0;
        base = (char *)CURRENCIES[baseid];
        if ( baseid+1 != refid )
        {
            komodo_statefname(fname,baseid<32?base:(char *)"",(char *)"komodostate");
            komodo_nameset(symbol,dest,base);
            if ( (fp= fopen(fname,"rb")) != 0 && (sp= komodo_stateptrget(symbol)) != 0 )
            {
                fseek(fp,0,SEEK_END);
                if ( ftell(fp) > lastpos[baseid] )
                {
                    if ( 0 && lastpos[baseid] == 0 && strcmp(symbol,"KMD") == 0 )
                        printf("passport refid.%d %s fname.(%s) base.%s\n",refid,symbol,fname,base);
                    fseek(fp,lastpos[baseid],SEEK_SET);
                    while ( komodo_parsestatefile(sp,fp,symbol,dest) >= 0 )
                        ;
                    lastpos[baseid] = ftell(fp);
                    if ( 0 && lastpos[baseid] == 0 && strcmp(symbol,"KMD") == 0 )
                        printf("from.(%s) lastpos[%s] %ld\n",ASSETCHAINS_SYMBOL,CURRENCIES[baseid],lastpos[baseid]);
                } //else fprintf(stderr,"%s.%ld ",CURRENCIES[baseid],ftell(fp));
                fclose(fp);
            }
            komodo_statefname(fname,baseid<32?base:(char *)"",(char *)"realtime");
            if ( (fp= fopen(fname,"rb")) != 0 )
            {
                if ( fread(buf,1,sizeof(buf),fp) == sizeof(buf) )
                {
                    sp->CURRENT_HEIGHT = buf[0];
                    if ( buf[0] != 0 && buf[0] == buf[1] && buf[2] > time(NULL)-60 )
                    {
                        isrealtime = 1;
                        RTmask |= (1LL << baseid);
                        memcpy(refsp->RTbufs[baseid+1],buf,sizeof(refsp->RTbufs[baseid+1]));
                    } //else fprintf(stderr,"%s not RT\n",base);
                } //else fprintf(stderr,"%s size error RT\n",base);
                fclose(fp);
            } //else fprintf(stderr,"%s open error RT\n",base);
        }
        else
        {
            komodo_statefname(fname,baseid<32?base:(char *)"",(char *)"realtime");
            if ( (fp= fopen(fname,"wb")) != 0 )
            {
                buf[0] = (uint32_t)chainActive.Tip()->nHeight;
                buf[1] = (uint32_t)komodo_longestchain();
                if ( buf[0] != 0 && buf[0] == buf[1] )
                {
                    buf[2] = (uint32_t)time(NULL);
                    RTmask |= (1LL << baseid) | 1;
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
    refsp->RTmask = RTmask;
    KOMODO_PASSPORT_INITDONE = 1;
}

