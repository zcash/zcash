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

struct pax_transaction *komodo_paxfind(struct pax_transaction *space,uint256 txid,uint16_t vout)
{
    struct pax_transaction *pax;
    pthread_mutex_lock(&komodo_mutex);
    HASH_FIND(hh,PAX,&txid,sizeof(txid),pax);
    if ( pax != 0 )
        memcpy(space,pax,sizeof(*pax));
    pthread_mutex_unlock(&komodo_mutex);
    return(pax);
}

struct pax_transaction *komodo_paxmark(int32_t height,struct pax_transaction *space,uint256 txid,uint16_t vout,int32_t mark)
{
    struct pax_transaction *pax;
    pthread_mutex_lock(&komodo_mutex);
    HASH_FIND(hh,PAX,&txid,sizeof(txid),pax);
    if ( pax == 0 )
    {
        pax = (struct pax_transaction *)calloc(1,sizeof(*pax));
        pax->txid = txid;
        pax->vout = vout;
        HASH_ADD_KEYPTR(hh,PAX,&pax->txid,sizeof(pax->txid),pax);
        //printf("ht.%d create pax.%p mark.%d\n",height,pax,mark);
    }
    if ( pax != 0 )
    {
        pax->marked = mark;
        //int32_t i; for (i=0; i<32; i++)
        //    printf("%02x",((uint8_t *)&txid)[i]);
        //printf(" paxmark.ht %d vout%d\n",mark,vout);
        memcpy(space,pax,sizeof(*pax));
    }
    pthread_mutex_unlock(&komodo_mutex);
    return(pax);
}

void komodo_gateway_deposit(char *coinaddr,uint64_t value,char *symbol,uint64_t fiatoshis,uint8_t *rmd160,uint256 txid,uint16_t vout,int32_t height,int32_t otherheight) // assetchain context
{
    struct pax_transaction *pax; int32_t addflag = 0; struct komodo_state *sp; char str[16],dest[16];
    sp = komodo_stateptr(str,dest);
    pthread_mutex_lock(&komodo_mutex);
    HASH_FIND(hh,PAX,&txid,sizeof(txid),pax);
    if ( pax == 0 )
    {
        pax = (struct pax_transaction *)calloc(1,sizeof(*pax));
        pax->txid = txid;
        pax->vout = vout;
        HASH_ADD_KEYPTR(hh,PAX,&pax->txid,sizeof(pax->txid),pax);
        addflag = 1;
        if ( ASSETCHAINS_SYMBOL[0] == 0 )
        {
            int32_t i; for (i=0; i<32; i++)
                printf("%02x",((uint8_t *)&txid)[i]);
            printf(" v.%d [%s] kht.%d ht.%d create pax.%p\n",vout,ASSETCHAINS_SYMBOL,height,otherheight,pax);
        }
    }
    pthread_mutex_unlock(&komodo_mutex);
    if ( coinaddr != 0 )
    {
        strcpy(pax->coinaddr,coinaddr);
        pax->komodoshis = value;
        strcpy(pax->symbol,symbol);
        pax->fiatoshis = fiatoshis;
        memcpy(pax->rmd160,rmd160,20);
        pax->height = height;
        pax->otherheight = sp->CURRENT_HEIGHT;//otherheight;
        if ( pax->marked == 0 )
        {
            if ( addflag != 0 )
                printf("[%s] addflag.%d ADD DEPOSIT %s %.8f -> %s TO PAX ht.%d otherht.%d total %.8f\n",ASSETCHAINS_SYMBOL,addflag,symbol,dstr(fiatoshis),coinaddr,height,otherheight,dstr(komodo_paxtotal()));
        }
        //else printf("%p MARKED.%d DEPOSIT %s %.8f -> %s TO PAX ht.%d otherht.%d\n",pax,pax->marked,symbol,dstr(fiatoshis),coinaddr,height,otherheight);
    }
    else
    {
        pax->marked = height;
        printf("pax.%p MARK DEPOSIT ht.%d other.%d\n",pax,height,otherheight);
    }
}

int32_t komodo_issued_opreturn(char *base,uint256 *txids,uint16_t *vouts,int64_t *values,int32_t *kmdheights,int32_t *otherheights,int8_t *baseids,uint8_t *opretbuf,int32_t opretlen,int32_t iskomodo)
{
    int32_t i,n=0,j,len;
    for (i=0; i<4; i++)
        base[i] = opretbuf[opretlen-4+i];
    if ( ASSETCHAINS_SYMBOL[0] == 0 || strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0 )
    {
        opretbuf++, opretlen--;
        for (n=len=0; n<opretlen/34; n++)
        {
            for (j=0; j<32; j++)
            {
                ((uint8_t *)&txids[n])[j] = opretbuf[len++];
                //printf("%02x",((uint8_t *)&txids[n])[j]);
            }
            vouts[n] = opretbuf[len++];
            vouts[n] = (opretbuf[len++] << 8) | vouts[n];
            //printf(" issuedtxid v%d i.%d opretlen.%d\n",vouts[n],n,opretlen);
            if ( iskomodo != 0 )
            {
                uint64_t fiatoshis; int32_t height,otherheight; char symbol[16];
                len += iguana_rwnum(0,&opretbuf[len],sizeof(fiatoshis),&fiatoshis);
                len += iguana_rwnum(0,&opretbuf[len],sizeof(height),&height);
                len += iguana_rwnum(0,&opretbuf[len],sizeof(otherheight),&otherheight);
                for (i=0; opretbuf[len+i]!=0&&i<3; i++)
                    symbol[i] = opretbuf[len+i];
                symbol[i] = 0;
                if ( values != 0 && kmdheights != 0 && otherheights != 0 && baseids != 0 )
                {
                    values[i] = fiatoshis;
                    kmdheights[i] = height;
                    otherheights[i] = otherheight;
                    baseids[i] = komodo_baseid(symbol);
                }
                printf(">>>>>>> iskomodo X: (%s) fiat %.8f kmdheight.%d other.%d\n",symbol,dstr(fiatoshis),height,otherheight);
            }
        }
    }
    return(n);
}

uint64_t komodo_paxtotal()
{
    struct pax_transaction *pax,*tmp; int32_t ht; uint64_t total = 0;
    if ( komodo_isrealtime(&ht) == 0 )
        return(0);
    HASH_ITER(hh,PAX,pax,tmp)
    {
        //printf("pax.%s marked.%d %.8f -> %.8f\n",pax->symbol,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis));
        if ( pax->marked == 0 )
        {
            if ( komodo_is_issuer() != 0 )
                total += pax->fiatoshis;
            else total += pax->komodoshis;
        }
    }
    //printf("paxtotal %.8f\n",dstr(total));
    return(total);
}

int32_t komodo_gateway_deposits(CMutableTransaction *txNew,char *base,int32_t tokomodo)
{
    struct pax_transaction *pax,*tmp; char symbol[16],dest[16]; uint8_t *script,opcode,opret[10000],data[10000]; int32_t i,baseid,ht,len=0,opretlen=0,numvouts=1; struct komodo_state *sp; uint64_t mask;
    sp = komodo_stateptr(symbol,dest);
    strcpy(symbol,base);
    PENDING_KOMODO_TX = 0;
    if ( tokomodo == 0 )
    {
        opcode = 'I';
        if ( komodo_isrealtime(&ht) == 0 )
            return(0);
    }
    else opcode = 'X';
    HASH_ITER(hh,PAX,pax,tmp)
    {
        //printf("pax.%s marked.%d %.8f -> %.8f\n",pax->symbol,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis));
        if ( pax->marked != 0 || strcmp(pax->symbol,symbol) != 0 )
            continue;
        //if ( ASSETCHAINS_SYMBOL[0] != 0 )
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
        for (i=0; i<32; i++)
        {
            //printf("%02x",((uint8_t *)&pax->txid)[i]);
            data[len++] = ((uint8_t *)&pax->txid)[i];
        }
        data[len++] = pax->vout & 0xff;
        data[len++] = (pax->vout >> 8) & 0xff;
        if ( tokomodo == 0 )
            PENDING_KOMODO_TX += pax->fiatoshis;
        else
        {
            //[{"prev_hash":"5d5c9a49489b558de9e84f991f996dedaae6b9d0f157f82b2fec64662476d5cf","prev_vout":2,"EUR":0.10000000,"fiat":"EUR","kmdheight":57930,"height":153,"KMD":0.78329000,"address":"RDhEGYScNQYetCyG75Kf8Fg61UWPdwc1C5","rmd160":"306c507eea639e7220b3069ed9f49f3bc97eaca1"}]
            len += iguana_rwnum(1,&data[len],sizeof(pax->fiatoshis),&pax->fiatoshis);
            len += iguana_rwnum(1,&data[len],sizeof(pax->height),&pax->height);
            len += iguana_rwnum(1,&data[len],sizeof(pax->otherheight),&pax->otherheight);
            for (i=0; pax->symbol[i]!=0&&i<3; i++) // must be 3 letter currency
                data[len++] = pax->symbol[i];
            data[len++] = 0;
            PENDING_KOMODO_TX += pax->komodoshis;
            printf(" vout.%u DEPOSIT %.8f <- pax.%s pending %.8f | ",pax->vout,(double)txNew->vout[numvouts].nValue/COIN,symbol,dstr(PENDING_KOMODO_TX));
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
    int32_t i,j,n,num,opretlen,offset=1,errs=0,matched=0,kmdheights[64],otherheights[64]; uint256 hash,txids[64]; char symbol[16],base[16]; uint16_t vouts[64]; int8_t baseids[64]; uint8_t *script,opcode; int64_t values[64]; struct pax_transaction *pax,space;
    memset(baseids,0xff,sizeof(baseids));
    memset(values,0,sizeof(values));
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
        strcpy(symbol,"KMD");
    }
    else
    {
        strcpy(symbol,ASSETCHAINS_SYMBOL);
        opcode = 'I';
    }
    if ( script[offset] == opcode && opretlen < block.vtx[0].vout[n-1].scriptPubKey.size() )
    {
        if ( (num= komodo_issued_opreturn(base,txids,vouts,values,kmdheights,otherheights,baseids,&script[offset],opretlen,opcode == 'X')) > 0 )
        {
            for (i=1; i<n-1; i++)
            {
                if ( (pax= komodo_paxfind(&space,txids[i-1],vouts[i-1])) != 0 )
                {
                    if ( ((opcode == 'I' && pax->fiatoshis == block.vtx[0].vout[i].nValue) || (opcode == 'X' && pax->komodoshis == block.vtx[0].vout[i].nValue)) )
                    {
                        if ( pax->marked != 0 )
                            errs++;
                        else matched++;
                        if ( 0 && opcode == 'X' )
                            printf("errs.%d i.%d match %.8f == %.8f\n",errs,i,dstr(pax != 0 ? pax->fiatoshis:-1),dstr(block.vtx[0].vout[i].nValue));
                    }
                    else
                    {
                        hash = block.GetHash();
                        if ( opcode == 'X' )
                        {
                            for (j=0; j<32; j++)
                                printf("%02x",((uint8_t *)&hash)[j]);
                            printf(" ht.%d blockhash X couldnt find vout.[%d]\n",height,i);
                            // validate amount! via fiat chain
                        }
                    }
                }
                else
                {
                    if  ( opcode == 'X' )
                    {
                        matched++;
                        for (j=0; j<32; j++)
                            printf("%02x",((uint8_t *)&txids[i-1])[j]);
                        printf(" cant paxfind X txid\n");
                        // validate amount! via fiat chain
                    } else if ( opcode == 'I' )
                        matched++;
                }
                komodo_paxmark(height,&space,txids[i-1],vouts[i-1],height);
            }
            if ( matched != num )
            {
                // can easily happen depending on order of loading
                if ( height > 60000 )
                    printf("WARNING: ht.%d (%c) matched.%d vs num.%d\n",height,opcode,matched,num);
            }
        }
        //printf("opretlen.%d num.%d\n",opretlen,num);
    }
    return(0);
}

const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout)
{
    uint8_t rmd160[20],addrtype,shortflag,pubkey33[33]; int32_t i,j,n,len,tokomodo,kmdheight,otherheights[64],kmdheights[64]; int8_t baseids[64]; char base[4],coinaddr[64],destaddr[64]; struct pax_transaction space; uint256 txids[64]; uint16_t vouts[64]; uint64_t convtoshis,seed; int64_t fiatoshis,komodoshis,checktoshis,values[64]; struct pax_transaction *pax;
    const char *typestr = "unknown";
    memset(baseids,0xff,sizeof(baseids));
    memset(values,0,sizeof(values));
    memset(kmdheights,0,sizeof(kmdheights));
    memset(otherheights,0,sizeof(otherheights));
    tokomodo = (komodo_is_issuer() == 0);
    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
    {
        for (i=0; i<opretlen; i++)
            printf("%02x",opretbuf[i]);
        printf(" opret[%c] else path tokomodo.%d ht.%d\n",opretbuf[0],tokomodo,height);
    }
    if ( opretbuf[0] == 'D' )
    {
        if ( opretlen == 38 ) // any KMD tx
        {
            iguana_rwnum(0,&opretbuf[34],sizeof(kmdheight),&kmdheight);
            memset(base,0,sizeof(base));
            PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&fiatoshis);
            bitcoin_address(coinaddr,addrtype,rmd160,20);
            checktoshis = PAX_fiatdest(&seed,tokomodo,destaddr,pubkey33,coinaddr,kmdheight,base,fiatoshis);
            typestr = "deposit";
            printf("%s kmdheight.%d vs height.%d check %.8f vs %.8f tokomodo.%d %d seed.%llx\n",ASSETCHAINS_SYMBOL,kmdheight,height,dstr(checktoshis),dstr(value),komodo_is_issuer(),strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0,(long long)seed);
            if ( kmdheight <= height )
            {
                if ( tokomodo == 0 && strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0 )
                {
                    for (i=0; i<32; i++)
                        printf("%02x",((uint8_t *)&txid)[i]);
                    printf(" <- txid.v%u ",vout);
                    for (i=0; i<33; i++)
                        printf("%02x",pubkey33[i]);
                    printf(" checkpubkey check %.8f v %.8f dest.(%s) kmdheight.%d height.%d\n",dstr(checktoshis),dstr(value),destaddr,kmdheight,height);
                    if ( value == checktoshis )//value >= checktoshis || (seed == 0 && diff < .01) )
                    {
                        if ( komodo_paxfind(&space,txid,vout) == 0 )
                        {
                            komodo_gateway_deposit(coinaddr,value,base,fiatoshis,rmd160,txid,vout,kmdheight,height);
                        } else printf("duplicate deposit\n");
                    }
                }
            }
        }
    }
    else if ( opretbuf[0] == 'W' && opretlen >= 38 )
    {
        iguana_rwnum(0,&opretbuf[34],sizeof(kmdheight),&kmdheight);
        memset(base,0,sizeof(base));
        PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&komodoshis);
        bitcoin_address(coinaddr,addrtype,rmd160,20);
        checktoshis = PAX_fiatdest(&seed,tokomodo,destaddr,pubkey33,coinaddr,kmdheight,base,value);
        typestr = "withdraw";
        printf("%s.height.%d vs height.%d check %.8f/%.8f vs %.8f tokomodo.%d %d seed.%llx -> (%s)\n",ASSETCHAINS_SYMBOL,kmdheight,height,dstr(checktoshis),dstr(komodoshis),dstr(value),komodo_is_issuer(),strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0,(long long)seed,coinaddr);
        if ( checktoshis == komodoshis )
        {
            if ( (pax= komodo_paxfind(&space,txid,vout)) == 0 )
            {
                printf("notarize %s %.8f -> %.8f kmd.%d other.%d\n",ASSETCHAINS_SYMBOL,dstr(value),dstr(komodoshis),kmdheight,height);
            } else printf(" %.8f -> %s withdraw already there\n",dstr(value),coinaddr);
        }
    }
    else if ( strncmp((char *)"KMD",(char *)&opretbuf[opretlen-4],3) != 0 || opretlen == 38 )
    {
        if ( tokomodo == 0 && opretbuf[0] == 'I' ) // assetchain coinbase
        {
            if ( (n= komodo_issued_opreturn(base,txids,vouts,values,kmdheights,otherheights,baseids,opretbuf,opretlen,0)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    if ( komodo_paxmark(height,&space,txids[i],vouts[i],height) == 0 )
                        komodo_gateway_deposit(0,0,0,0,0,txids[i],vouts[i],height,0);
                }
            }
        }
    }
    return(typestr);
}

void komodo_passport_iteration()
{
    static long lastpos[34]; static char userpass[33][1024];
    FILE *fp; int32_t baseid,isrealtime,refid,blocks,longest; struct komodo_state *sp,*refsp; char *retstr,fname[512],*base,symbol[16],dest[16]; uint32_t buf[3]; cJSON *infoobj,*result; uint64_t RTmask = 0;
    refsp = komodo_stateptr(symbol,dest);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
        refid = 33;
    else refid = komodo_baseid(ASSETCHAINS_SYMBOL)+1; // illegal base -> baseid.-1 -> 0
    //printf("PASSPORT %s refid.%d\n",ASSETCHAINS_SYMBOL,refid);
    for (baseid=0; baseid<=32; baseid++)
    {
        sp = 0;
        isrealtime = 0;
        base = (char *)CURRENCIES[baseid];
        if ( baseid+1 != refid )
        {
            komodo_statefname(fname,baseid<32?base:(char *)"",(char *)"komodostate");
            komodo_nameset(symbol,dest,base);
            //port = komodo_port(base,10,&magic) + 1;
            if ( (fp= fopen(fname,"rb")) != 0 && (sp= komodo_stateptrget(symbol)) != 0 )
            {
                //printf("refid.%d %s fname.(%s) base.%s\n",refid,symbol,fname,base);
                fseek(fp,0,SEEK_END);
                if ( ftell(fp) > lastpos[baseid] )
                {
                    fseek(fp,lastpos[baseid],SEEK_SET);
                    while ( komodo_parsestatefile(sp,fp,symbol,dest) >= 0 )
                        ;
                    lastpos[baseid] = ftell(fp);
                    //printf("from.(%s) lastpos[%s] %ld\n",ASSETCHAINS_SYMBOL,CURRENCIES[baseid],lastpos[baseid]);
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
            } else fprintf(stderr,"%s open error RT\n",base);
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
    refsp->RTmask = RTmask;
}

