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

void komodo_gateway_deposits(CMutableTransaction& txNew)
{
    struct pax_transaction *ptr; uint8_t *script,opret[10000],data[10000]; int32_t i,len=0,opretlen=0,numvouts=1;
    PENDING_KOMODO_TX = 0;
    while ( (ptr= (struct pax_transaction *)queue_dequeue(&DepositsQ)) != 0 )
    {
        txNew.vout.resize(numvouts+1);
        txNew.vout[numvouts].nValue = ptr->fiatoshis;
        txNew.vout[numvouts].scriptPubKey.resize(25);
        script = (uint8_t *)&txNew.vout[numvouts].scriptPubKey[0];
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
        printf(" vout.%u DEPOSIT %.8f\n",ptr->vout,(double)KOMODO_DEPOSIT/COIN);
        PENDING_KOMODO_TX += ptr->fiatoshis;
        numvouts++;
        queue_enqueue((char *)"PENDINGS",&PendingsQ,&ptr->DL);
    }
    if ( numvouts > 1 )
    {
        opretlen = komodo_opreturnscript(opret,'I',data,len);
        txNew.vout.resize(numvouts+1);
        txNew.vout[numvouts].nValue = 0;
        txNew.vout[numvouts].scriptPubKey.resize(opretlen);
        script = (uint8_t *)&txNew.vout[numvouts].scriptPubKey[0];
        memcpy(script,opret,opretlen);
    }
    printf("total numvouts.%d %.8f\n",numvouts,dstr(PENDING_KOMODO_TX));
}

void komodo_gateway_deposit(uint64_t value,int32_t shortflag,char *symbol,uint64_t fiatoshis,uint8_t *rmd160,uint256 txid,uint16_t vout) // assetchain context
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
    queue_enqueue((char *)"DEPOSITS",&DepositsQ,&ptr->DL);
}

int32_t komodo_gateway_depositremove(uint256 txid,uint16_t vout) // assetchain context
{
    int32_t iter,n=0; queue_t *Q; struct pax_transaction *ptr; struct queueitem *item;
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
                    printf("DELETE %.8f DEPOSIT %s %.8f\n",dstr(ptr->komodoshis),ptr->symbol,dstr(ptr->fiatoshis));
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
        KOMODO_DEPOSIT = 0;
    return(n);
}

const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout)
{
    uint8_t rmd160[20],addrtype,shortflag,pubkey33[33]; int32_t i,j,len,tokomodo=0; char base[4],coinaddr[64],destaddr[64]; int64_t fiatoshis,checktoshis; const char *typestr = "unknown";
    //printf("komodo_opreturn[%c]: ht.%d %.8f opretlen.%d\n",opretbuf[0],height,dstr(value),opretlen);
#ifdef KOMODO_ISSUER
    tokomodo = 1;
#endif
    if ( opretbuf[0] == ((tokomodo != 0) ? 'D' : 'W') )
    {
        if ( opretlen == 34 )
        {
            memset(base,0,sizeof(base));
            PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&fiatoshis);
            if ( fiatoshis < 0 )
                fiatoshis = -fiatoshis;
            bitcoin_address(coinaddr,addrtype,rmd160,20);
            checktoshis = PAX_fiatdest(tokomodo,destaddr,pubkey33,coinaddr,height,base,fiatoshis);
            for (i=0; i<opretlen; i++)
                printf("%02x",opretbuf[i]);
            printf(" DEPOSIT %.8f %c%s -> %s ",dstr(fiatoshis),shortflag!=0?'-':'+',base,coinaddr);
            for (i=0; i<32; i++)
                printf("%02x",((uint8_t *)&txid)[i]);
            printf(" <- txid.v%u ",vout);
            for (i=0; i<33; i++)
                printf("%02x",pubkey33[i]);
            printf(" checkpubkey check %.8f v %.8f dest.(%s)\n",dstr(checktoshis),dstr(value),destaddr);
            typestr = "deposit";
#ifdef KOMODO_ISSUER
            if ( strncmp(KOMODO_SOURCE,base,strlen(base)) == 0 && value >= (9999*checktoshis)/10000 && shortflag == ASSETCHAINS_SHORTFLAG )
            {
                komodo_gateway_deposit(value,shortflag,base,fiatoshis,rmd160,txid,vout);
            }
#else
            if ( tokomodo != 0 && value <= (10000*checktoshis)/9999 )
            {
                
            }
#endif
        }
    }
    else if ( opretbuf[0] == 'I' )
    {
        uint256 issuedtxid; uint16_t issuedvout;
        opretbuf++, opretlen--;
        for (i=len=0; i<opretlen/34; i++)
        {
            for (j=0; j<32; j++)
            {
                ((uint8_t *)&issuedtxid)[j] = opretbuf[len++];
                printf("%02x",((uint8_t *)&issuedtxid)[j]);
            }
            issuedvout = opretbuf[len++];
            issuedvout = (vout << 8) | opretbuf[len++];
            printf(" issuedtxid v%d i.%d opretlen.%d\n",issuedvout,i,opretlen);
            if ( komodo_gateway_depositremove(issuedtxid,issuedvout) == 0 )
                printf("error removing deposit\n");
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
        if ( isspecial != 0 && len >= offset+32*2+4 && strcmp((char *)&script[offset+32*2+4],"KMD") == 0 )
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
                                printf("call voutupdate vout.%d\n",vout);
                                komodo_gateway_voutupdate(symbol,isspecial,height,txi,txid,vout,n,value,script,len);
                            }
                        }
                    }
                }
            }
            free_json(json);
        }
        free(retstr);
    }
    return(retval);
}

int32_t komodo_gateway_block(char *symbol,int32_t height,uint16_t port)
{
    char *retstr,*retstr2,params[128],*txidstr; int32_t i,n,retval = -1; cJSON *json,*tx,*result,*result2;
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
                            else printf("error i.%d vs n.%d\n",i,n);
                        }
                        free_json(json);
                    }
                    free(retstr2);
                }
            } else printf("strlen.%ld (%s)\n",strlen(txidstr),txidstr);
            free_json(result);
        }
        free(retstr);
    }
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
                    //printf("KMDHEIGHT %d\n",KMDHEIGHT);
                    if ( (KMDHEIGHT % 100) == 0 )
                    {
                        fprintf(stderr,"%s.%d ",symbol,KMDHEIGHT);
                        memset(&zero,0,sizeof(zero));
                        komodo_stateupdate(KMDHEIGHT,0,0,0,zero,0,0,0,0,KMDHEIGHT,0,0,0,0);
                    }
                    if ( komodo_gateway_block(symbol,KMDHEIGHT,port) < 0 )
                        break;
                    usleep(10000);
                }
            }
            free_json(infoobj);
        }
        free(retstr);
    }
    else
    {
        //printf("error from %s\n",symbol);
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
