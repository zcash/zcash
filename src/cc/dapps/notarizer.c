/******************************************************************************
 * Copyright © 2014-2020 The SuperNET Developers.                             *
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

#define DEXP2P_CHAIN ((char *)"DPOW")
#define DEXP2P_PUBKEYS ((char *)"pubkeys")

#include "dappinc.h"

#define NOTARIZATION_TIME 300   // minimum every 5 minutes
#define _NOTARIZATION_BLOCKS 10

struct dpowentry { bits256 ntzhash; uint32_t timestamp; int32_t height,ind; char payload[81]; } NN[64];

int32_t Num_authorized,NOTARIZATION_BLOCKS = _NOTARIZATION_BLOCKS;
cJSON *NOTARIZER_json;
char *Authorized[64][3];

// issue ./komodod -ac_name=DPOW -handle=xxx -dexp2p=2 -addnode=136.243.58.134 -pubkey=02/03... &
// add blocknotify=notarizer KMD "" %s
// add blocknotify=notarizer ASSETCHAIN "" %s
// add blocknotify=notarizer BTC "bitcoin-cli" %s
// add blocknotify=notarizer 3rdparty "3rdparty-cli" %s
// build notarizer and put in path: gcc cc/dapps/notarizer.c -lm -o notarizer; cp notarizer /usr/bin

void dpow_authorizedcreate(char *handle,char *secpstr)
{
    Authorized[Num_authorized][0] = clonestr(handle);
    Authorized[Num_authorized][1] = clonestr(secpstr);
    Num_authorized++;
}

int32_t dpow_authorizedupdate()
{
    cJSON *retjson,*item,*array; char *tagB,*pubkey,*retstr,*pstr; int32_t i,j,n,m,retval = 0;
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_list","0","0","handles","","","","")) != 0 )
    {
        if ( (array= jarray(&n,retjson,"matches")) != 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (pubkey= jstr(item,"senderpub")) != 0 && (tagB= jstr(item,"tagB")) != 0 && (pstr= jstr(item,"decrypted")) != 0 && strlen(pstr) == 66 )
                {
                    for (j=0; j<Num_authorized; j++)
                    {
                        if ( strcmp(tagB,Authorized[j][0]) == 0 && strcmp(pstr,Authorized[j][1]) == 0 && Authorized[j][2] == 0 )
                            Authorized[j][2] = clonestr(pubkey);
                    }
                }
            }
        }
        free_json(retjson);
        for (j=m=0; j<Num_authorized; j++)
        {
            if ( Authorized[j][2] == 0 )
            {
                fprintf(stderr,"%s ",Authorized[j][0]);
                m++;
            }
        }
        fprintf(stderr,"NOT registered %d\n",m);
    }
    return(retval);
}

static int _NN_sortcmp(const void *a,const void *b)
{
#define nn_a (*(struct dpowentry **)a)
#define nn_b (*(struct dpowentry **)b)
    int32_t i;
    if ( nn_a->height > nn_b->height )
        return(-1);
    else if ( nn_a->height < nn_b->height )
        return(1);
    else
    {
        if ( nn_a->timestamp > nn_b->timestamp )
            return(-1);
        else if ( nn_a->timestamp < nn_b->timestamp )
            return(1);
        else
        {
            for (i=0; i<4; i++)
            {
                if ( nn_a->ntzhash.ulongs[i] >  nn_b->ntzhash.ulongs[i] )
                    return(-1);
                else if (  nn_a->ntzhash.ulongs[i] <  nn_b->ntzhash.ulongs[i] )
                    return(1);
            }
        }
    }
    return(0);
#undef nn_a
#undef nn_b
}

int32_t dpow_roundproposal(char *coin)
{
    uint8_t buf[4]; int32_t i,n; char str[65];
    for (i=n=0; i<Num_authorized; i++)
    {
        NN[n].ind = i;
        if ( Authorized[i][0] != 0 && Authorized[i][1] != 0 && Authorized[i][2] != 0 && dpow_getmessage(NN[n].payload,sizeof(NN[n].payload),"rounds",coin,Authorized[i][2]) > 0 )
        {
            decode_hex(NN[n].ntzhash.bytes,32,NN[n].payload);
            decode_hex(buf,4,NN[n].payload + 32*2);
            NN[n].height = ((int32_t)buf[3] + ((int32_t)buf[2] << 8) + ((int32_t)buf[1] << 16) + ((int32_t)buf[0] << 24));
            decode_hex(buf,4,NN[n].payload + 32*2+8);
            NN[n].timestamp = ((int32_t)buf[3] + ((int32_t)buf[2] << 8) + ((int32_t)buf[1] << 16) + ((int32_t)buf[0] << 24));
            n++;
        }
    }
    if ( n >= 13 )
    {
        qsort(NN,n,sizeof(NN[n]),_NN_sortcmp);
        for (i=0; i<n; i++)
            fprintf(stderr,"%-2d h.%d t.%u %s %s\n",NN[i].ind,NN[i].height,NN[i].timestamp,bits256_str(str,NN[i].ntzhash),Authorized[NN[i].ind][0]);
        fprintf(stderr,"%s num.%d\n",coin,n);
    }
}

int32_t main(int32_t argc,char **argv)
{
    int32_t i,n,height,nextheight,priority=8; char *coin,*handle,*secpstr,*pubkeys,*kcli,*hashstr,*acname=(char *)""; cJSON *retjson,*item,*authorized; bits256 blockhash; long fsize; uint32_t heighttime; char checkstr[65],str[65],str2[65];
    srand((int32_t)time(NULL));
    if ( (pubkeys= filestr(&fsize,DEXP2P_PUBKEYS)) == 0 )
    {
        fprintf(stderr,"cant load %s file\n",DEXP2P_PUBKEYS);
        exit(-1);
    }
    if ( (NOTARIZER_json= cJSON_Parse(pubkeys)) == 0 )
    {
        fprintf(stderr,"cant parse notarize json file (%s)\n",pubkeys);
        exit(-1);
    }
    if ( (authorized= jarray(&n,NOTARIZER_json,"authorized")) != 0 )
    {
        for (i=0; i<n; i++)
        {
            item = jitem(authorized,i);
            handle = jfieldname(item);
            secpstr = jstr(item,handle);
            dpow_authorizedcreate(handle,secpstr);
        }
    }
    if ( argc == 4 )
    {
        if ( dpow_pubkey() < 0 )
        {
            fprintf(stderr,"couldnt set pubkey for dPoW\n");
            return(-1);
        }
        coin = (char *)argv[1];
        if ( argv[2][0] != 0 )
            REFCOIN_CLI = (char *)argv[2];
        else
        {
            if ( strcmp(coin,"KMD") != 0 )
                acname = coin;
        }
        hashstr = (char *)argv[3];
        height = get_coinheight(coin,acname,&blockhash);
        get_coinmerkleroot(coin,acname,blockhash,&heighttime);
        bits256_str(checkstr,blockhash);
        if ( strcmp(checkstr,hashstr) == 0 )
        {
            fprintf(stderr,"%s: (%s) %s height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,height);
            if ( (retjson= dpow_ntzdata(coin,priority,height,blockhash)) != 0 )
                free_json(retjson);
        } else fprintf(stderr,"coin.%s (%s) %s vs %s, height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,hashstr,height);
        if ( strcmp("BTC",coin) != 0 )
        {
            bits256 prevntzhash,ntzhash,checkhash,chainhash; int32_t h,prevntzheight=0,ntzheight=0; uint32_t nexttime=0,ntztime=0,t,prevntztime=0; char hexstr[81]; cJSON *retjson2;
            dpow_pubkeyregister(priority);
            dpow_authorizedupdate();
            prevntzhash = dpow_ntzhash(coin,&prevntzheight,&prevntztime);
            if ( (retjson= get_getinfo(coin,acname)) != 0 )
            {
                ntzheight = juint(retjson,"notarized");
                ntzhash = jbits256(retjson,"notarizedhash");
                if ( ntzheight > prevntzheight )
                {
                    get_coinmerkleroot(coin,acname,ntzhash,&ntztime);
                    fprintf(stderr,"NOTARIZATION %s.%d %s t.%u\n",coin,ntzheight,bits256_str(str,ntzhash),ntztime);
                    bits256_str(hexstr,ntzhash);
                    sprintf(&hexstr[64],"%08x",ntzheight);
                    sprintf(&hexstr[72],"%08x",ntztime);
                    hexstr[80] = 0;
                    if ( (retjson2= dpow_broadcast(priority,hexstr,coin,"notarizations",DPOW_pubkeystr,"","")) != 0 )
                        free_json(retjson2);
                }
                else if ( ntzheight == prevntzheight )
                {
                    if ( memcmp(&prevntzhash,&ntzhash,32) != 0 )
                        fprintf(stderr,"NTZ ERROR %s.%d %s != %s\n",coin,ntzheight,bits256_str(str,ntzhash),bits256_str(str2,prevntzhash));
                    else if ( height > ntzheight )
                    {
                        for (h=height; h>ntzheight && h>height-100; h--)
                        {
                            checkhash = dpow_blockhash(coin,h);             // from network
                            chainhash = get_coinblockhash(coin,acname,h);   // from blockchain
                            if ( memcmp(&checkhash,&chainhash,sizeof(checkhash)) != 0 )
                            {
                                fprintf(stderr,"%s.%d: chainhash.%s != %s, must have  been reorged\n",coin,h,bits256_str(str,chainhash),bits256_str(str2,checkhash));
                                if ( (retjson2= dpow_ntzdata(coin,priority,h,chainhash)) != 0 )
                                    free_json(retjson2);
                            }
                        }
                    }
                }
                free_json(retjson);
                if ( strcmp("KMD",coin) != 0 )
                    NOTARIZATION_BLOCKS = 1;
                nextheight = ntzheight + NOTARIZATION_BLOCKS - (ntzheight % NOTARIZATION_BLOCKS);
                if ( nextheight < height - NOTARIZATION_BLOCKS/2 )
                {
                    nexttime = get_heighttime(coin,acname,nextheight);
                    if ( (time(NULL) - nexttime) > 2*NOTARIZATION_TIME ) // find a more recent block
                    {
                        for (i=NOTARIZATION_BLOCKS; nextheight+i < height-NOTARIZATION_BLOCKS/2 - 1; i+=NOTARIZATION_BLOCKS)
                        {
                            t =  get_heighttime(coin,acname,nextheight+i);
                            if ( 0 && NOTARIZATION_BLOCKS == 1 )
                                fprintf(stderr,"%s nextheight.%d lag.%d\n",coin,nextheight+i,(int32_t)(time(NULL) - t));
                            if ( (time(NULL) - t) < 3*NOTARIZATION_TIME/2 )
                            {
                                nextheight += i;
                                nexttime = t;
                                break;
                            }
                        }
                    }
                    // check ongoing rounds and prevent too close ones
                    if ( time(NULL) > nexttime + NOTARIZATION_TIME && height > nextheight+NOTARIZATION_BLOCKS/2 && time(NULL) > prevntztime+NOTARIZATION_TIME && nexttime > prevntztime+NOTARIZATION_TIME )
                    {
                        //if ( nexttime - dpow_roundpending(coin,acname) > NOTARIZATION_TIME )
                        {
                            checkhash = dpow_blockhash(coin,nextheight);
                            chainhash = get_coinblockhash(coin,acname,nextheight);
                            if ( memcmp(&checkhash,&chainhash,sizeof(checkhash)) == 0 )
                            {
                                bits256_str(hexstr,chainhash);
                                sprintf(&hexstr[64],"%08x",nextheight);
                                sprintf(&hexstr[72],"%08x",nexttime);
                                hexstr[80] = 0;
                                if ( dpow_hasmessage(hexstr,coin,"rounds",DPOW_pubkeystr) == 0 )
                                {
                                    if ( (retjson2= dpow_broadcast(priority,hexstr,coin,(char *)"rounds",DPOW_pubkeystr,"","")) != 0 )
                                    {
                                        free_json(retjson2);
                                        fprintf(stderr,"start notarization for %s.%d when ht.%d prevntz.%d\n",coin,nextheight,height,ntzheight);
                                        if ( (retjson2= dpow_notarize(coin,nextheight)) != 0 )
                                            free_json(retjson2);
                                    }
                                }
                                dpow_roundproposal(coin);
                            } else fprintf(stderr,"%s.%d: chainhash.%s != %s, must have  been reorged\n",coin,nextheight,bits256_str(str,chainhash),bits256_str(str2,checkhash));
                        }
                    }
                }
            }
        }
    }
    return(0);
}

