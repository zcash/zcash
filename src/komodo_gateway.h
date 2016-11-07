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

struct pax_transaction
{
    UT_hash_handle hh;
    uint256 txid;
    uint64_t komodoshis,fiatoshis;
    int32_t marked,height;
    uint16_t vout;
    char symbol[16],coinaddr[64]; uint8_t rmd160[20],shortflag;
} *PAX;

/*uint64_t komodo_paxtotal()
{
    struct pax_transaction *pax,*tmp; uint64_t total = 0;
    HASH_ITER(hh,PAX,pax,tmp)
    {
        if ( pax->marked == 0 )
            total += pax->fiatoshis;
    }
    return(total);
}*/

uint64_t komodo_paxtotal()
{
    struct pax_transaction *pax,*tmp; uint64_t total = 0;
    tmp = 0;
    while ( PAX != 0 && (pax= (struct pax_transaction *)PAX->hh.next) != 0 && pax != tmp )
    {
        if ( pax->marked == 0 )
        {
            if ( komodo_is_issuer() != 0 )
                total += pax->fiatoshis;
            else total += pax->komodoshis;
        }
        tmp = pax;
        pax = (struct pax_transaction *)pax->hh.next;
    }
    return(total);
}

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

struct pax_transaction *komodo_paxmark(struct pax_transaction *space,uint256 txid,uint16_t vout,int32_t mark)
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

void komodo_gateway_deposit(char *coinaddr,uint64_t value,int32_t shortflag,char *symbol,uint64_t fiatoshis,uint8_t *rmd160,uint256 txid,uint16_t vout,int32_t height) // assetchain context
{
    struct pax_transaction *pax; int32_t addflag = 0;
    pthread_mutex_lock(&komodo_mutex);
    HASH_FIND(hh,PAX,&txid,sizeof(txid),pax);
    if ( pax == 0 )
    {
        pax = (struct pax_transaction *)calloc(1,sizeof(*pax));
        pax->txid = txid;
        pax->vout = vout;
        HASH_ADD_KEYPTR(hh,PAX,&pax->txid,sizeof(pax->txid),pax);
    }
    pthread_mutex_unlock(&komodo_mutex);
    if ( coinaddr != 0 )
    {
        strcpy(pax->coinaddr,coinaddr);
        pax->komodoshis = value;
        pax->shortflag = shortflag;
        strcpy(pax->symbol,symbol);
        pax->fiatoshis = fiatoshis;
        memcpy(pax->rmd160,rmd160,20);
        pax->height = height;
        if ( pax->marked == 0 )
            printf("ADD DEPOSIT %s %.8f -> %s TO PAX ht.%d total %.8f\n",symbol,dstr(fiatoshis),coinaddr,height,dstr(komodo_paxtotal()));
        else printf("MARKED.%d DEPOSIT %s %.8f -> %s TO PAX ht.%d\n",pax->marked,symbol,dstr(fiatoshis),coinaddr,height);
    }
    else
    {
        pax->marked = height;
        printf("MARK DEPOSIT ht.%d\n",height);
    }
}

int32_t komodo_issued_opreturn(uint8_t *shortflagp,char *base,uint256 *txids,uint16_t *vouts,uint8_t *opretbuf,int32_t opretlen)
{
    int32_t i,n=0,j,len;
    if ( opretbuf[opretlen-5] == '-' )
        *shortflagp = 1;
    else *shortflagp = 0;
    for (i=0; i<4; i++)
        base[i] = opretbuf[opretlen-4+i];
    if ( (strcmp(base,"KMD") == 0 && ASSETCHAINS_SYMBOL[0] == 0) || strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0 ) // shortflag
    {
        //printf("BASE.(%s) vs (%s)\n",base,ASSETCHAINS_SYMBOL);
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
        }
    }
    return(n);
}

void komodo_gateway_deposits(CMutableTransaction *txNew,int32_t shortflag,char *symbol)
{
    struct pax_transaction *pax,*tmp; uint8_t *script,opcode,opret[10000],data[10000]; int32_t i,len=0,opretlen=0,numvouts=1;
    PENDING_KOMODO_TX = 0;
    if ( strcmp(symbol,"KMD") == 0 )
        opcode = 'I';
    else opcode = 'X';
    tmp = 0;
    while ( PAX != 0 && (pax= (struct pax_transaction *)PAX->hh.next) != 0 && pax != tmp )
    {
        printf("pax.%p marked.%d %.8f -> %.8f\n",pax,pax->marked,dstr(pax->komodoshis),dstr(pax->fiatoshis));
        if ( pax->marked != 0 )
            continue;
        txNew->vout.resize(numvouts+1);
        txNew->vout[numvouts].nValue = pax->fiatoshis;
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
            printf("%02x",((uint8_t *)&pax->txid)[i]);
            data[len++] = ((uint8_t *)&pax->txid)[i];
        }
        data[len++] = pax->vout & 0xff;
        data[len++] = (pax->vout >> 8) & 0xff;
        if ( strcmp(symbol,"KMD") != 0 )
            PENDING_KOMODO_TX += pax->fiatoshis;
        else PENDING_KOMODO_TX += pax->komodoshis;
        printf(" vout.%u DEPOSIT %.8f <- paxdeposit.%s pending %.8f\n",pax->vout,(double)txNew->vout[numvouts].nValue/COIN,symbol,dstr(PENDING_KOMODO_TX));
        if ( numvouts++ >= 64 )
            break;
        tmp = pax;
        pax = (struct pax_transaction *)pax->hh.next;
    }
    if ( numvouts > 1 )
    {
        if ( shortflag != 0 )
            data[len++] = '-';
        for (i=0; symbol[i]!=0; i++)
            data[len++] = symbol[i];
        data[len++] = 0;
        opretlen = komodo_opreturnscript(opret,opcode,data,len);
        txNew->vout.resize(numvouts+1);
        txNew->vout[numvouts].nValue = 0;
        txNew->vout[numvouts].scriptPubKey.resize(opretlen);
        script = (uint8_t *)&txNew->vout[numvouts].scriptPubKey[0];
        memcpy(script,opret,opretlen);
        printf("total numvouts.%d %.8f opretlen.%d\n",numvouts,dstr(PENDING_KOMODO_TX),opretlen);
    }
}

int32_t komodo_check_deposit(int32_t height,const CBlock& block) // verify above block is valid pax pricing
{
    int32_t i,j,n,num,opretlen,offset=1; uint256 hash,txids[64]; uint8_t shortflag; char symbol[16],base[16]; uint16_t vouts[64]; uint8_t *script,opcode; struct pax_transaction *pax,space;
    n = block.vtx[0].vout.size();
    script = (uint8_t *)block.vtx[0].vout[n-1].scriptPubKey.data();
    if ( n <= 2 || script[0] != 0x6a )
        return(0);
    offset += komodo_scriptitemlen(&opretlen,&script[offset]);
    //printf("checkdeposit n.%d [%02x] [%c] %d vs %d\n",n,script[0],script[offset],script[offset],'I');
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
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
        if ( (num= komodo_issued_opreturn(&shortflag,base,txids,vouts,&script[offset],opretlen)) > 0 )
        {
            for (i=1; i<n-1; i++)
            {
                if ( (pax= komodo_paxfind(&space,txids[i-1],vouts[i-1])) != 0 && ((opcode == 'I' && pax->fiatoshis == block.vtx[0].vout[i].nValue) || (opcode == 'X' && pax->komodoshis == block.vtx[0].vout[i].nValue)) )
                {
                    //printf("i.%d match %.8f == %.8f\n",i,dstr(pax != 0 ? pax->fiatoshis:-1),dstr(block.vtx[0].vout[i].nValue));
                    komodo_paxmark(&space,txids[i-1],vouts[i-1],height);
                }
                else
                {
                    hash = block.GetHash();
                    //for (j=0; j<32; j++)
                    //   printf("%02x",((uint8_t *)&hash)[j]);
                    //printf(" ht.%d blockhash couldnt find vout.[%d]\n",height,i);
                    komodo_paxmark(&space,txids[i-1],vouts[i-1],height);
                }
            }
        }
        //printf("opretlen.%d num.%d\n",opretlen,num);
    }
    return(0);
}

const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout)
{
    uint8_t rmd160[20],addrtype,shortflag,pubkey33[33]; int32_t i,j,n,len,tokomodo,kmdheight; char base[4],coinaddr[64],destaddr[64]; struct pax_transaction space; uint256 txids[64]; uint16_t vouts[64]; double diff; uint64_t seed; int64_t fiatoshis,checktoshis; const char *typestr = "unknown";
    tokomodo = (komodo_is_issuer() == 0);
    printf("ASSETCHAIN.(%s) -> tokomodo.%d is_issuer.%d\n",ASSETCHAINS_SYMBOL,tokomodo,komodo_is_issuer());
    if ( opretbuf[0] == 'D' )
    {
        if ( opretlen == 38 ) // any KMD tx
        {
            iguana_rwnum(0,&opretbuf[34],sizeof(kmdheight),&kmdheight);
            memset(base,0,sizeof(base));
            PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&fiatoshis);
            if ( fiatoshis < 0 )
                fiatoshis = -fiatoshis;
            bitcoin_address(coinaddr,addrtype,rmd160,20);
            checktoshis = PAX_fiatdest(&seed,tokomodo,destaddr,pubkey33,coinaddr,kmdheight,base,fiatoshis);
            typestr = "deposit";
            printf("kmdheight.%d vs height.%d check %.8f vs %.8f tokomodo.%d %d seed.%llx\n",kmdheight,height,dstr(checktoshis),dstr(value),komodo_is_issuer(),strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0,(long long)seed);
            diff = ((double)value / checktoshis) - 1.;
            if ( diff < 0. )
                diff = -diff;
            if ( kmdheight <= height )
            {
                if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                {
                    for (i=0; i<opretlen; i++)
                        printf("%02x",opretbuf[i]);
                    printf(" opret[%c] tokomodo.%d\n",opretbuf[0],tokomodo);
                }
                //paxprice seed.0 sum 0.07766840 densum 1000.00000000 basevol 0.01000000 height.59532
                if ( tokomodo == 0 && strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0 && shortflag == ASSETCHAINS_SHORTFLAG )
                {
                    if ( shortflag == 0 )
                    {
                        for (i=0; i<32; i++)
                            printf("%02x",((uint8_t *)&txid)[i]);
                        printf(" <- txid.v%u ",vout);
                        for (i=0; i<33; i++)
                            printf("%02x",pubkey33[i]);
                        printf(" checkpubkey check %.8f v %.8f dest.(%s) kmdheight.%d height.%d\n",dstr(checktoshis),dstr(value),destaddr,kmdheight,height);
                        if ( value >= checktoshis || (seed == 0 && diff < .01) )
                        {
                            if ( komodo_paxfind(&space,txid,vout) == 0 )
                                komodo_gateway_deposit(coinaddr,value,shortflag,base,fiatoshis,rmd160,txid,vout,kmdheight);
                        }
                    }
                    else // short
                    {
                        for (i=0; i<opretlen; i++)
                            printf("%02x",opretbuf[i]);
                        printf(" opret[%c] tokomodo.%d value %.8f vs check %.8f\n",opretbuf[0],tokomodo,dstr(value),dstr(checktoshis));
                        if ( value <= checktoshis || (seed == 0 && diff < .01) )
                        {
                            
                        }
                    }
                }
            }
        }
    }
    else if ( strncmp((char *)"KMD",(char *)&opretbuf[opretlen-4],3) != 0 )
    {
        if ( tokomodo == 0 && opretbuf[0] == 'I' ) // assetchain coinbase
        {
            if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
            {
                for (i=0; i<opretlen; i++)
                    printf("%02x",opretbuf[i]);
                printf(" opret[%c] tokomodo.%d\n",opretbuf[0],tokomodo);
            }
            if ( (n= komodo_issued_opreturn(&shortflag,base,txids,vouts,opretbuf,opretlen)) > 0 && shortflag == ASSETCHAINS_SHORTFLAG )
            {
                for (i=0; i<n; i++)
                {
                    for (j=0; j<32; j++)
                        printf("%02x",((uint8_t *)&txids[i])[j]);
                    printf(" issuedtxid v%d i.%d of n.%d opretlen.%d\n",vouts[i],i,n,opretlen);
                    if ( komodo_paxmark(&space,txids[i],vouts[i],height) == 0 )
                        komodo_gateway_deposit(0,0,0,0,0,0,txids[i],vouts[i],height);
                }
            }
        }
        else if ( tokomodo != 0 && opretbuf[0] == 'X' )
        {
            // verify and update limits
        }
    }
    return(typestr);
}

void komodo_gateway_voutupdate(char *symbol,int32_t isspecial,int32_t height,int32_t txi,bits256 txid,int32_t vout,int32_t numvouts,uint64_t value,uint8_t *script,int32_t len)
{
    int32_t i,opretlen,offset = 0; uint256 zero,utxid; const char *typestr;
    typestr = "unknown";
    memcpy(&utxid,&txid,sizeof(utxid));
    if ( script[offset++] == 0x6a )
    {
        offset += komodo_scriptitemlen(&opretlen,&script[offset]);
        if ( isspecial != 0 && len >= offset+32*2+4 && strcmp((char *)&script[offset+32*2+4],ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL) == 0 )
            typestr = "notarized";
        else if ( txi == 0 && vout == 1 && opretlen == 149 )
        {
            typestr = "pricefeed";
            komodo_paxpricefeed(height,&script[offset],opretlen);
            //printf("height.%d pricefeed len.%d\n",height,opretlen);
        }
        else komodo_stateupdate(height,0,0,0,utxid,0,0,0,0,0,value,&script[offset],opretlen,vout);
    }
    else if ( numvouts >= KOMODO_MINRATIFY )
        typestr = "ratify";
}

int32_t komodo_gateway_tx(char *symbol,int32_t height,int32_t txi,char *txidstr,uint32_t port)
{
    char *retstr,params[256],*hexstr; uint8_t script[10000]; cJSON *oldpub,*newpub,*json,*result,*vouts,*item,*sobj; int32_t vout,n,len,isspecial,retval = -1; uint64_t value; bits256 txid;
    sprintf(params,"[\"%s\", 1]",txidstr);
    if ( (retstr= komodo_issuemethod((char *)"getrawtransaction",params,port)) != 0 )
    {
        if ( (json= cJSON_Parse(retstr)) != 0 )
        {
            if ( (result= jobj(json,(char *)"result")) != 0 )
            {
                oldpub = jobj(result,(char *)"vpub_old");
                newpub = jobj(result,(char *)"vpub_new");
                retval = 0;
                if ( oldpub == 0 && newpub == 0 && (vouts= jarray(&n,result,(char *)"vout")) != 0 )
                {
                    isspecial = 0;
                    txid = jbits256(result,(char *)"txid");
                    for (vout=0; vout<n; vout++)
                    {
                        item = jitem(vouts,vout);
                        value = SATOSHIDEN * jdouble(item,(char *)"value");
                        if ( (sobj= jobj(item,(char *)"scriptPubKey")) != 0 )
                        {
                            if ( (hexstr= jstr(sobj,(char *)"hex")) != 0 )
                            {
                                len = (int32_t)strlen(hexstr) >> 1;
                                if ( vout == 0 && ((memcmp(&hexstr[2],CRYPTO777_PUBSECPSTR,66) == 0 && len == 35) || (memcmp(&hexstr[6],CRYPTO777_RMD160STR,40) == 0 && len == 25)) )
                                    isspecial = 1;
                                else if ( len <= sizeof(script) )
                                {
                                    decode_hex(script,len,hexstr);
                                    komodo_gateway_voutupdate(symbol,isspecial,height,txi,txid,vout,n,value,script,len);
                                }
                            }
                        }
                    }
                }
            } else printf("error getting txids.(%s) %p\n",retstr,result);
            free_json(json);
        }
        free(retstr);
    }
    return(retval);
}

int32_t komodo_gateway_block(char *symbol,int32_t height,uint16_t port)
{
    char *retstr,*retstr2,params[128],*txidstr; int32_t i,n,retval = -1; cJSON *json,*tx=0,*result=0,*result2;
    sprintf(params,"[%d]",height);
    if ( (retstr= komodo_issuemethod((char *)"getblockhash",params,port)) != 0 )
    {
        if ( (result= cJSON_Parse(retstr)) != 0 )
        {
            if ( (txidstr= jstr(result,(char *)"result")) != 0 && strlen(txidstr) == 64 )
            {
                sprintf(params,"[\"%s\"]",txidstr);
                if ( (retstr2= komodo_issuemethod((char *)"getblock",params,port)) != 0 )
                {
                    //printf("getblock.(%s)\n",retstr2);
                    if ( (json= cJSON_Parse(retstr2)) != 0 )
                    {
                        if ( (result2= jobj(json,(char *)"result")) != 0 && (tx= jarray(&n,result2,(char *)"tx")) != 0 )
                        {
                            for (i=0; i<n; i++)
                                if ( komodo_gateway_tx(symbol,height,i,jstri(tx,i),port) < 0 )
                                    break;
                            if ( i == n )
                                retval = 0;
                            else printf("komodo_gateway_block ht.%d error i.%d vs n.%d\n",height,i,n);
                        } else printf("cant get result.%p or tx.%p\n",result,tx);
                        free_json(json);
                    } else printf("cant parse2.(%s)\n",retstr2);
                    free(retstr2);
                } else printf("error getblock %s\n",params);
            } else printf("strlen.%ld (%s)\n",strlen(txidstr),txidstr);
            free_json(result);
        } else printf("couldnt parse.(%s)\n",retstr);
        free(retstr);
    } else printf("error from getblockhash %d\n",height);
    return(retval);
}

void komodo_gateway_iteration(char *symbol)
{
    char *retstr; int32_t i,kmdheight; cJSON *infoobj,*result; uint256 zero; uint16_t port = 7771;
    if ( KMDHEIGHT <= 0 )
        KMDHEIGHT = 1;
    KOMODO_REALTIME = 0;
    if ( (retstr= komodo_issuemethod((char *)"getinfo",0,port)) != 0 )
    {
        if ( (infoobj= cJSON_Parse(retstr)) != 0 )
        {
            if ( (result= jobj(infoobj,(char *)"result")) != 0 && (kmdheight= jint(result,(char *)"blocks")) != 0 )
            {
                //printf("gateway KMDHEIGHT.%d kmdheight.%d\n",KMDHEIGHT,kmdheight);
                for (i=0; i<1000 && KMDHEIGHT<kmdheight; i++,KMDHEIGHT++)
                {
                    if ( (KMDHEIGHT % 10) == 0 )
                    {
                        if ( (KMDHEIGHT % 100) == 0 )
                            fprintf(stderr,"%s.%d ",symbol,KMDHEIGHT);
                        memset(&zero,0,sizeof(zero));
                        komodo_stateupdate(KMDHEIGHT,0,0,0,zero,0,0,0,0,KMDHEIGHT,0,0,0,0);
                    }
                    if ( komodo_gateway_block(symbol,KMDHEIGHT,port) < 0 )
                    {
                        printf("error KMDHEIGHT %d\n",KMDHEIGHT);
                        break;
                    }
                    usleep(10000);
                }
                if ( KMDHEIGHT >= kmdheight )
                    KOMODO_REALTIME = (uint32_t)time(NULL);
            }
            free_json(infoobj);
        }
        free(retstr);
    }
    else
    {
        printf("error from %s\n",symbol);
        sleep(30);
    }
}

void komodo_iteration(char *symbol)
{
    char *retstr,*base,*coinaddr,*txidstr,cmd[512]; uint64_t value,fiatoshis; cJSON *array,*item; int32_t i,n,vout,shortflag,height; uint256 txid; uint8_t rmd160[20],addrtype;
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        //[{"prev_hash":"5d5c9a49489b558de9e84f991f996dedaae6b9d0f157f82b2fec64662476d5cf","prev_vout":2,"EUR":0.78329000,"fiat":"EUR","height":57930,"KMD":0.10000000,"address":"RDhEGYScNQYetCyG75Kf8Fg61UWPdwc1C5"}]
        sprintf(cmd,"{\"agent\":\"dpow\",\"method\":\"pending\",\"fiat\":\"%s\"}",symbol);
        if ( (retstr= issue_curl(cmd)) != 0 )
        {
            if ( (array= cJSON_Parse(retstr)) != 0 )
            {
                if ( (n= cJSON_GetArraySize(array)) > 0 )
                {
                    for (i=0; i<n; i++)
                    {
                        item = jitem(array,i);
                        coinaddr = jstr(item,(char *)"address");
                        value = jdouble(item,(char *)"KMD") * COIN;
                        shortflag = juint(item,(char *)"short");
                        vout = jint(item,(char *)"prev_vout");
                        height = jint(item,(char *)"height");
                        base = jstr(item,(char *)"fiat");
                        txidstr = jstr(item,(char *)"prev_hash");
                        if ( coinaddr != 0 && base != 0 && value > 0 && height > 0 )
                        {
                            fiatoshis = jdouble(item,base) * COIN;
                            decode_hex((uint8_t *)&txid,sizeof(txid),txidstr);
                            bitcoin_addr2rmd160(&addrtype,rmd160,coinaddr);
                            komodo_gateway_deposit(coinaddr,value,shortflag,base,fiatoshis,rmd160,txid,vout,height);
                        }
                    }
                }
            }
            printf("retstr.(%s)\n",retstr);
            free(retstr);
        }
    }
}
