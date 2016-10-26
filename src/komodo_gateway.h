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

// savestate with important tx
// convert paxdeposit into new coins in the next block
// paxdeposit equivalent in reverse makes opreturn and KMD does the same in reverse
// need to save most processed block in other chain(s)

void komodo_gateway_voutupdate(int32_t height,int32_t txi,int32_t vout,int32_t numvouts,uint64_t value,uint8_t *script,int32_t len)
{
    const char *typestr = "unknown";
    if ( script[0] == 0x6a )
    {
        if ( len >= 32*2+4 && strcmp((char *)&script[2+32*2+4],"KMD") == 0 )
            typestr = "notarized";
        if ( script[2] == 'P' )
            typestr = "pricefeed";
        else if ( script[2] == 'D' )
            typestr = "deposit";
    }
    else if ( numvouts > 13 )
        typestr = "ratify";
    printf("VOUTUPDATE.%d txi.%d vout.%d %.8f scriptlen.%d OP_RETURN.%d (%s)\n",height,txi,vout,dstr(value),len,script[0] == 0x6a,typestr);
}

void komodo_gateway_tx(int32_t height,int32_t txi,char *txidstr,uint32_t port)
{
    char *retstr,params[256],*hexstr; uint8_t script[10000]; cJSON *json,*result,*vouts,*item,*sobj; int32_t vout,n,len,isspecial; uint64_t value;
    sprintf(params,"[\"%s\", 1]",txidstr);
    if ( (retstr= komodo_issuemethod((char *)"getrawtransaction",params,port)) != 0 )
    {
        if ( (json= cJSON_Parse(retstr)) != 0 )
        {
            if ( (result= jobj(json,(char *)"result")) != 0 && (vouts= jarray(&n,result,(char *)"vout")) != 0 )
            {
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
                            if ( vout == 0 && memcmp(&hexstr[2],CRYPTO777_PUBSECPSTR,66) == 0 && len == 35 )
                                isspecial = 1;
                            if ( isspecial != 0 && len <= sizeof(script) )
                            {
                                decode_hex(script,len,hexstr);
                                komodo_gateway_voutupdate(height,txi,vout,n,value,script,len);
                            }
                        }
                    }
                }
            }
            free_json(json);
        }
        free(retstr);
    }
}

void komodo_gateway_block(int32_t height,uint16_t port)
{
    char *retstr,*retstr2,params[128],*txidstr; int32_t i,n; cJSON *json,*tx,*result,*result2;
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
                                komodo_gateway_tx(height,i,jstri(tx,i),port);
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
}

void komodo_gateway_iteration(char *symbol)
{
    char *retstr; int32_t i,kmdheight; cJSON *infoobj,*result; uint16_t port = 7771;
    if ( (retstr= komodo_issuemethod((char *)"getinfo",0,port)) != 0 )
    {
        if ( (infoobj= cJSON_Parse(retstr)) != 0 )
        {
            if ( (result= jobj(infoobj,(char *)"result")) != 0 && (kmdheight= jint(result,(char *)"blocks")) != 0 )
            {
                for (i=0; i<100 && KMDHEIGHT<kmdheight; i++,KMDHEIGHT++)
                {
                    if ( (KMDHEIGHT % 1000) == 0 )
                        fprintf(stderr,"%s.%d ",symbol,KMDHEIGHT);
                    komodo_gateway_block(KMDHEIGHT,port);
                    usleep(1000);
                }
            }
            free_json(infoobj);
        }
        free(retstr);
    } else printf("error from %s\n",symbol);
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
