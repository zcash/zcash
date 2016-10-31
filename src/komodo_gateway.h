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
    struct queueitem DL;
    uint256 txid;
    uint64_t komodoshis,fiatoshis;
    uint16_t vout;
    char symbol[4]; uint8_t rmd160[20],shortflag;
};

int32_t komodo_issued_opreturn(uint8_t *shortflagp,char *base,uint256 *txids,uint16_t *vouts,uint8_t *opretbuf,int32_t opretlen)
{
    int32_t i,n=0,j,len;
    if ( opretbuf[opretlen-5] == '-' )
        *shortflagp = 1;
    else *shortflagp = 0;
    for (i=0; i<4; i++)
        base[i] = opretbuf[opretlen-4+i];
    if ( strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0 ) // shortflag
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

void komodo_gateway_deposits(CMutableTransaction *txNew)
{
    struct pax_transaction *ptr; uint8_t *script,opret[10000],data[10000]; int32_t i,len=0,opretlen=0,numvouts=1;
    PENDING_KOMODO_TX = 0;
    while ( numvouts < 64 && (ptr= (struct pax_transaction *)queue_dequeue(&DepositsQ)) != 0 )
    {
        txNew->vout.resize(numvouts+1);
        txNew->vout[numvouts].nValue = ptr->fiatoshis;
        txNew->vout[numvouts].scriptPubKey.resize(25);
        script = (uint8_t *)&txNew->vout[numvouts].scriptPubKey[0];
        *script++ = 0x76;
        *script++ = 0xa9;
        *script++ = 20;
        memcpy(script,ptr->rmd160,20), script += 20;
        *script++ = 0x88;
        *script++ = 0xac;
        for (i=0; i<32; i++)
        {
            printf("%02x",((uint8_t *)&ptr->txid)[i]);
            data[len++] = ((uint8_t *)&ptr->txid)[i];
        }
        data[len++] = ptr->vout & 0xff;
        data[len++] = (ptr->vout >> 8) & 0xff;
        printf(" vout.%u DEPOSIT %.8f <- paxdeposit.%s\n",ptr->vout,(double)txNew->vout[numvouts].nValue/COIN,ASSETCHAINS_SYMBOL);
        PENDING_KOMODO_TX += ptr->fiatoshis;
        numvouts++;
        queue_enqueue((char *)"PENDINGS",&PendingsQ,&ptr->DL);
    }
    while ( (ptr= (struct pax_transaction *)queue_dequeue(&PendingsQ)) != 0 )
        queue_enqueue((char *)"DEPOSITS",&DepositsQ,&ptr->DL);
    if ( numvouts > 1 )
    {
        if ( ASSETCHAINS_SHORTFLAG != 0 )
            data[len++] = '-';
        for (i=0; ASSETCHAINS_SYMBOL[i]!=0; i++)
            data[len++] = ASSETCHAINS_SYMBOL[i];
        data[len++] = 0;
        opretlen = komodo_opreturnscript(opret,'I',data,len);
        txNew->vout.resize(numvouts+1);
        txNew->vout[numvouts].nValue = 0;
        txNew->vout[numvouts].scriptPubKey.resize(opretlen);
        script = (uint8_t *)&txNew->vout[numvouts].scriptPubKey[0];
        memcpy(script,opret,opretlen);
        printf("total numvouts.%d %.8f opretlen.%d\n",numvouts,dstr(PENDING_KOMODO_TX),opretlen);
    } else KOMODO_DEPOSIT = 0;
}

void komodo_gateway_deposit(char *coinaddr,uint64_t value,int32_t shortflag,char *symbol,uint64_t fiatoshis,uint8_t *rmd160,uint256 txid,uint16_t vout) // assetchain context
{
    struct pax_transaction *ptr;
    ptr = (struct pax_transaction *)calloc(1,sizeof(*ptr));
    ptr->komodoshis = value;
    ptr->fiatoshis = fiatoshis;
    memcpy(ptr->symbol,symbol,3);
    memcpy(ptr->rmd160,rmd160,20);
    ptr->shortflag = shortflag;
    ptr->txid = txid;
    ptr->vout = vout;
    KOMODO_DEPOSIT += fiatoshis;
    printf("ADD DEPOSIT %s %.8f -> %s TO QUEUE\n",symbol,dstr(fiatoshis),coinaddr);
    queue_enqueue((char *)"DEPOSITS",&DepositsQ,&ptr->DL);
}

int32_t komodo_gateway_depositremove(uint256 txid,uint16_t vout) // assetchain context
{
    int32_t iter,i,n=0; queue_t *Q; struct pax_transaction *ptr; struct queueitem *item;
    for (iter=0; iter<2; iter++)
    {
        Q = (iter == 0) ? &DepositsQ : &PendingsQ;
        portable_mutex_lock(&Q->mutex);
        if ( Q->list != 0 )
        {
            item = &ptr->DL;
            DL_FOREACH(Q->list,item)
            {
                ptr = (struct pax_transaction *)item;
                if ( memcmp(&ptr->txid,&txid,sizeof(txid)) == 0 && ptr->vout == vout )
                {
                    if ( KOMODO_DEPOSIT >= ptr->fiatoshis )
                        KOMODO_DEPOSIT -= ptr->fiatoshis;
                    else KOMODO_DEPOSIT = 0;
                    for (i=0; i<32; i++)
                        printf("%02x",((uint8_t *)&txid)[i]);
                    printf(" v%d DELETE %.8f DEPOSIT %s %.8f\n",vout,dstr(ptr->komodoshis),ptr->symbol,dstr(ptr->fiatoshis));
                    DL_DELETE(Q->list,&ptr->DL);
                    n++;
                    free(ptr);
                    break;
                }
            }
        }
        portable_mutex_unlock(&Q->mutex);
    }
    if ( queue_size(&DepositsQ) == 0 && queue_size(&PendingsQ) == 0 )
        KOMODO_DEPOSIT = PENDING_KOMODO_TX = 0;
    return(n);
}

int32_t komodo_check_deposit(const CBlock& block) // verify above block is valid pax pricing
{
    int32_t i,j,n,opretlen,num,iter,matchflag,offset=1; uint256 txids[64]; uint8_t shortflag; char base[16]; uint16_t vouts[64]; uint8_t *script; queue_t *Q; struct pax_transaction *ptr; struct queueitem *item;
    n = block.vtx[0].vout.size();
    script = (uint8_t *)block.vtx[0].vout[n-1].scriptPubKey.data();
    if ( n <= 2 || script[0] != 0x6a )
        return(0);
    offset += komodo_scriptitemlen(&opretlen,&script[offset]);
    //printf("checkdeposit n.%d [%02x] [%c] %d vs %d\n",n,script[0],script[offset],script[offset],'I');
    if ( script[offset] == 'I' && opretlen < block.vtx[0].vout[n-1].scriptPubKey.size() )
    {
        if ( (num= komodo_issued_opreturn(&shortflag,base,txids,vouts,&script[offset],opretlen)) > 0 )
        {
            for (i=1; i<n-1; i++)
            {
                for (iter=0; iter<2; iter++)
                {
                    Q = (iter == 0) ? &DepositsQ : &PendingsQ;
                    portable_mutex_lock(&Q->mutex);
                    ptr = 0;
                    if ( Q->list != 0 )
                    {
                        item = &ptr->DL;
                        matchflag = 0;
                        DL_FOREACH(Q->list,item)
                        {
                            ptr = (struct pax_transaction *)item;
                            if ( memcmp(&ptr->txid,&txids[i-1],sizeof(txids[i-1])) == 0 && ptr->vout == vouts[i-1] )
                            {
                                if ( ptr->fiatoshis == block.vtx[0].vout[i].nValue )
                                {
                                    /*for (j=0; j<32; j++)
                                        printf("%02x",((uint8_t *)&ptr->txid)[j]);
                                    printf(" v%d matched %.8f vout.%d ",ptr->vout,dstr(ptr->fiatoshis),i);
                                    hash = block.GetHash();
                                    for (j=0; j<32; j++)
                                        printf("%02x",((uint8_t *)&hash)[j]);
                                    printf(".blockhash\n");*/
                                    matchflag = 1;
                                } else printf("error finding %.8f vout.%d\n",dstr(ptr->fiatoshis),i);
                                break;
                            }
                        }
                    }
                    portable_mutex_unlock(&Q->mutex);
                }
                if ( matchflag == 0 )
                {
                    printf("couldnt find vout.[%d]\n",i);
                    return(-1);
                }
            }
        }
        //printf("opretlen.%d num.%d\n",opretlen,num);
    }
    return(0);
}

const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout)
{
    uint8_t rmd160[20],addrtype,shortflag,pubkey33[33]; int32_t i,j,n,len,tokomodo=0; char base[4],coinaddr[64],destaddr[64]; uint256 txids[64]; uint16_t vouts[64]; int64_t fiatoshis,checktoshis; const char *typestr = "unknown";
    tokomodo = (komodo_is_issuer() == 0);
    if ( opretbuf[0] == ((tokomodo == 0) ? 'D' : 'W') )
    {
        if ( opretlen == 34 )
        {
            memset(base,0,sizeof(base));
            PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&fiatoshis);
            if ( fiatoshis < 0 )
                fiatoshis = -fiatoshis;
            bitcoin_address(coinaddr,addrtype,rmd160,20);
            checktoshis = PAX_fiatdest(tokomodo,destaddr,pubkey33,coinaddr,height-1,base,fiatoshis);
            typestr = "deposit";
            if ( tokomodo == 0 && strncmp(ASSETCHAINS_SYMBOL,base,strlen(base)) == 0 )
            {
                for (i=0; i<opretlen; i++)
                    printf("%02x",opretbuf[i]);
                printf(" DEPOSIT %.8f %c%s -> %s ",dstr(fiatoshis),shortflag!=0?'-':'+',base,coinaddr);
                for (i=0; i<32; i++)
                    printf("%02x",((uint8_t *)&txid)[i]);
                printf(" <- txid.v%u ",vout);
                for (i=0; i<33; i++)
                    printf("%02x",pubkey33[i]);
                printf(" checkpubkey check %.8f v %.8f dest.(%s) height.%d\n",dstr(checktoshis),dstr(value),destaddr,height);
                if ( value >= checktoshis && shortflag == ASSETCHAINS_SHORTFLAG )
                {
                    komodo_gateway_deposit(coinaddr,value,shortflag,base,fiatoshis,rmd160,txid,vout);
                }
            }
            else
            {
                if ( value <= checktoshis )
                {
                    
                }
            }
        }
    }
    else if ( strncmp((char *)"KMD",(char *)&opretbuf[opretlen-4],3) != 0 )
    {
        if ( tokomodo == 0 && opretbuf[0] == 'I' )
        {
            if ( (n= komodo_issued_opreturn(&shortflag,base,txids,vouts,opretbuf,opretlen)) > 0 && shortflag == ASSETCHAINS_SHORTFLAG )
            {
                for (i=0; i<n; i++)
                {
                    for (j=0; j<32; j++)
                        printf("%02x",((uint8_t *)&txids[i])[j]);
                    printf(" issuedtxid v%d i.%d of n.%d opretlen.%d\n",vouts[i],i,n,opretlen);
                    if ( komodo_gateway_depositremove(txids[i],vouts[i]) == 0 )
                        printf("%s error removing deposit\n",ASSETCHAINS_SYMBOL);
                }
            }
        }
    }
    return(typestr);
}

void komodo_gateway_voutupdate(char *symbol,int32_t isspecial,int32_t height,int32_t txi,bits256 txid,int32_t vout,int32_t numvouts,uint64_t value,uint8_t *script,int32_t len)
{
    int32_t i,opretlen,offset = 0; uint256 zero,utxid; const char *typestr;
    typestr = "unknown";
    memcpy(&utxid,&txid,sizeof(utxid));
    if ( 0 )//txi != 0 || vout != 0 )
    {
        for (i=0; i<len; i++)
            printf("%02x",script[i]);
        printf(" <- %s VOUTUPDATE.%d txi.%d vout.%d %.8f scriptlen.%d OP_RETURN.%d (%s) len.%d\n",symbol,height,txi,vout,dstr(value),len,script[0] == 0x6a,typestr,opretlen);
    }
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
    else if ( numvouts > 13 )
        typestr = "ratify";
}

int32_t komodo_gateway_tx(char *symbol,int32_t height,int32_t txi,char *txidstr,uint32_t port)
{
    char *retstr,params[256],*hexstr; uint8_t script[10000]; cJSON *json,*result,*vouts,*item,*sobj; int32_t vout,n,len,isspecial,retval = -1; uint64_t value; bits256 txid;
    sprintf(params,"[\"%s\", 1]",txidstr);
    if ( (retstr= komodo_issuemethod((char *)"getrawtransaction",params,port)) != 0 )
    {
        if ( (json= cJSON_Parse(retstr)) != 0 )
        {
            if ( (result= jobj(json,(char *)"result")) != 0 && (vouts= jarray(&n,result,(char *)"vout")) != 0 )
            {
                retval = 0;
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
            } else printf("error getting txids.(%s)\n",retstr);
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
    if ( (retstr= komodo_issuemethod((char *)"getinfo",0,port)) != 0 )
    {
        if ( (infoobj= cJSON_Parse(retstr)) != 0 )
        {
            if ( (result= jobj(infoobj,(char *)"result")) != 0 && (kmdheight= jint(result,(char *)"blocks")) != 0 )
            {
                for (i=0; i<1000 && KMDHEIGHT<kmdheight; i++,KMDHEIGHT++)
                {
                    if ( (KMDHEIGHT % 100) == 0 )
                    {
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

#ifdef KOMODO_ISSUER
void komodo_gateway_issuer() // from "assetchain" connectblock()
{
    // check for redeems
}
#else

void komodo_gateway_redeemer() // from "KMD" connectblock()
{
    
}
#endif
