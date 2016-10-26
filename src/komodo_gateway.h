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

// convert paxdeposit into new coins in the next block
// paxdeposit equivalent in reverse makes opreturn and KMD does the same in reverse
// need to save most processed block in other chain(s)

const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen)
{
    uint8_t rmd160[20],addrtype,shortflag,pubkey33[33]; int32_t i; char base[4],coinaddr[64],destaddr[64]; int64_t fiatoshis,checktoshis; const char *typestr = "unknown";
    if ( opretlen == 72 )
        return("notarized");
    printf("komodo_opreturn[%c]: ht.%d %.8f opretlen.%d\n",opretbuf[0],height,dstr(value),opretlen);
    if ( opretbuf[0] == 'D' )
    {
        if ( opretlen == 34 )
        {
            PAX_pubkey(0,&opretbuf[1],&addrtype,rmd160,base,&shortflag,&fiatoshis);
            if ( fiatoshis < 0 )
                fiatoshis = -fiatoshis;
            bitcoin_address(coinaddr,addrtype,rmd160,20);
            checktoshis = PAX_fiatdest(destaddr,pubkey33,coinaddr,height,base,fiatoshis);
            printf("DEPOSIT %.8f %c%s -> %s\n",dstr(fiatoshis),shortflag!=0?'-':'+',base,coinaddr);
            // verify price value for fiatoshis of base
            for (i=0; i<33; i++)
                printf("%02x",pubkey33[i]);
            printf(" checkpubkey check %.8f v %.8f dest.(%s)\n",dstr(checktoshis),dstr(value),destaddr);
            typestr = "deposit";
        }
    }
    return(typestr);
}

void komodo_gateway_voutupdate(char *symbol,int32_t isspecial,int32_t height,int32_t txi,int32_t vout,int32_t numvouts,uint64_t value,uint8_t *script,int32_t len)
{
    int32_t i,opretlen,offset = 0; uint256 zero; const char *typestr;
    typestr = "unknown";
    if ( script[offset++] == 0x6a )
    {
        offset += komodo_scriptitemlen(&opretlen,&script[offset]);
        if ( isspecial != 0 && len >= offset+32*2+4 && strcmp((char *)&script[offset+32*2+4],"KMD") == 0 )
            typestr = "notarized";
        else if ( txi == 0 && (script[offset] == 'P' || opretlen == 149) )
        {
            typestr = "pricefeed";
            komodo_paxpricefeed(height,&scriptbuf[len + scriptboffset] == 'P'],opretlen);
            printf("height.%d pricefeed len.%d\n",height,opretlen);
        }
        else if ( isspecial != 0 )
        {
            komodo_stateupdate(0,0,0,0,zero,0,0,0,0,0,value,&script[offset],opretlen);
            for (i=0; i<len; i++)
                printf("%02x",script[i]);
            printf(" <- %s VOUTUPDATE.%d txi.%d vout.%d %.8f scriptlen.%d OP_RETURN.%d (%s)\n",symbol,height,txi,vout,dstr(value),len,script[0] == 0x6a,typestr);
        }
    }
    else if ( numvouts > 13 )
        typestr = "ratify";
}

int32_t komodo_gateway_tx(char *symbol,int32_t height,int32_t txi,char *txidstr,uint32_t port)
{
    char *retstr,params[256],*hexstr; uint8_t script[10000]; cJSON *json,*result,*vouts,*item,*sobj; int32_t vout,n,len,isspecial,retval = -1; uint64_t value;
    sprintf(params,"[\"%s\", 1]",txidstr);
    if ( (retstr= komodo_issuemethod((char *)"getrawtransaction",params,port)) != 0 )
    {
        if ( (json= cJSON_Parse(retstr)) != 0 )
        {
            if ( (result= jobj(json,(char *)"result")) != 0 && (vouts= jarray(&n,result,(char *)"vout")) != 0 )
            {
                retval = 0;
                isspecial = 0;
                for (vout=0; vout<n; vout++)
                {
                    item = jitem(vouts,vout);
                    value = SATOSHIDEN * jdouble(item,(char *)"value");
                    if ( (sobj= jobj(item,(char *)"scriptPubKey")) != 0 )
                    {
                        if ( (hexstr= jstr(sobj,(char *)"hex")) != 0 )
                        {
                            len = (int32_t)strlen(hexstr) >> 1;
                            if ( len != 74 )
                                printf("ht.%d txi.%d vout.%d/%d %s script (%d %d)\n",height,txi,vout,n,hexstr,memcmp(&hexstr[2],CRYPTO777_PUBSECPSTR,66),memcmp(&hexstr[6],CRYPTO777_RMD160STR,40));
                            if ( vout == 0 && ((memcmp(&hexstr[2],CRYPTO777_PUBSECPSTR,66) == 0 && len == 35) || (memcmp(&hexstr[6],CRYPTO777_RMD160STR,40) == 0 && len == 25)) )
                                isspecial = 1;
                            else if ( len <= sizeof(script) )
                            {
                                decode_hex(script,len,hexstr);
                                komodo_gateway_voutupdate(symbol,isspecial,height,txi,vout,n,value,script,len);
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
                    if ( (KMDHEIGHT % 100) == 0 )
                    {
                        fprintf(stderr,"%s.%d ",symbol,KMDHEIGHT);
                        memset(&zero,0,sizeof(zero));
                        komodo_stateupdate(0,0,0,0,zero,0,0,0,0,KMDHEIGHT,0,0,0);
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
