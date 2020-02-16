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
int32_t NOTARIZATION_BLOCKS = _NOTARIZATION_BLOCKS;

// issue ./komodod -ac_name=DPOW -handle=xxx -dexp2p=2 -addnode=136.243.58.134 -pubkey=02/03... &
// add blocknotify=notarizer KMD "" %s
// add blocknotify=notarizer ASSETCHAIN "" %s
// add blocknotify=notarizer BTC "bitcoin-cli" %s
// add blocknotify=notarizer 3rdparty "3rdparty-cli" %s
// build notarizer and put in path: gcc cc/dapps/notarizer.c -lm -o notarizer; cp notarizer /usr/bin

int32_t main(int32_t argc,char **argv)
{
    int32_t i,height,nextheight,priority=8; char *coin,*kcli,*hashstr,*acname=(char *)""; cJSON *retjson; bits256 blockhash; uint32_t heighttime; char checkstr[65],str[65],str2[65];
    srand((int32_t)time(NULL));
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
                        checkhash = dpow_blockhash(coin,nextheight);
                        chainhash = get_coinblockhash(coin,acname,nextheight);
                        if ( memcmp(&checkhash,&chainhash,sizeof(checkhash)) == 0 )
                        {
                            bits256_str(hexstr,chainhash);
                            sprintf(&hexstr[64],"%08x",nextheight);
                            sprintf(&hexstr[72],"%08x",nexttime);
                            hexstr[80] = 0;
                            if ( (retjson2= dpow_broadcast(priority,hexstr,coin,(char *)"rounds",DPOW_pubkeystr,"","")) != 0 )
                            {
                                free_json(retjson2);
                                fprintf(stderr,"start notarization for %s.%d when ht.%d prevntz.%d\n",coin,nextheight,height,ntzheight);
                                if ( (retjson2= dpow_notarize(coin,nextheight)) != 0 )
                                    free_json(retjson2);
                            }
                        } else fprintf(stderr,"%s.%d: chainhash.%s != %s, must have  been reorged\n",coin,nextheight,bits256_str(str,chainhash),bits256_str(str2,checkhash));
                    }
                }
            }
        }
    }
    return(0);
}

