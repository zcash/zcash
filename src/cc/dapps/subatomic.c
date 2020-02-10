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
#define DEXP2P_PUBKEYS ((char *)"subatomic")
#include "dappinc.h"

// add blocknotify=notarizer KMD "" %s
// add blocknotify=notarizer ASSETCHAIN "" %s
// add blocknotify=notarizer BTC "bitcoin-cli" %s
// add blocknotify=notarizer 3rdparty "3rdparty-cli" %s
// build notarizer and put in path: gcc cc/dapps/subatomic.c -lm -o subatomic; cp subatomic /usr/bin

int32_t main(int32_t argc,char **argv)
{
    int32_t i,height,priority=8; char *coin,*kcli,*hashstr,*acname=(char *)""; cJSON *retjson; bits256 blockhash; char checkstr[65],str[65],str2[65];
    srand((int32_t)time(NULL));
    if ( argc == 4 )
    {
        if ( dpow_pubkey() < 0 )
        {
            fprintf(stderr,"couldnt set pubkey for DEX\n");
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
        if ( strlen(hashstr) == 64 )
        {
            height = get_coinheight(coin,acname,&blockhash);
            bits256_str(checkstr,blockhash);
            if ( strcmp(checkstr,hashstr) == 0 )
            {
                fprintf(stderr,"%s: (%s) %s height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,height);
                if ( (retjson= dpow_ntzdata(coin,priority,height,blockhash)) != 0 )
                    free_json(retjson);
            } else fprintf(stderr,"coin.%s (%s) %s vs %s, height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,hashstr,height);
            if ( strcmp("BTC",coin) != 0 )
            {
                bits256 prevntzhash,ntzhash; int32_t prevntzheight,ntzheight; uint32_t ntztime,prevntztime; char hexstr[81]; cJSON *retjson2;
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
                        if ( (retjson2= dpow_broadcast(priority,hexstr,coin,"notarizations")) != 0 )
                            free_json(retjson2);
                    }
                    else if ( ntzheight == prevntzheight && memcmp(&prevntzhash,&ntzhash,32) != 0 )
                        fprintf(stderr,"NTZ ERROR %s.%d %s != %s\n",coin,ntzheight,bits256_str(str,ntzhash),bits256_str(str2,prevntzhash));
                    free_json(retjson);
                }
            }
        }
        else if ( atol(hashstr) > 10000 )
        {
            char checkstr[32];
            sprintf(checkstr,"%ld",atol(hashstr));
            if ( strcmp(checkstr,hashstr) == 0 )
            {
                fprintf(stderr,"fill %s %s %ld\n",coin,hashstr,atol(hashstr));
                
               // DEX_get hashstr
                // channel open request
                // wait for confirm and approval -> channel open and send opened message
                // wait for open confirmed message -> send payment and message
                // wait for recv and confirm trade completed
                //{"id":nnnnn,"base":"PIRATE","send":"zs15qj8w57g4mfc0zfp2ke9apfa8mjj3r3zvm3v2d902586x93avv4fmp58p4kp0d79dsknwxywmf6","baseamount":1,"rel":"KMD","recv":"RBNXombNrcoqQem2u95zDwyFpxv7iVtagW","relamount":0.1"}

                // send message to inbox
                // wait for response
                //{"approval:"<hash>","id":nnnnn,"base":"PIRATE","send":"zs15qj8w57g4mfc0zfp2ke9apfa8mjj3r3zvm3v2d902586x93avv4fmp58p4kp0d79dsknwxywmf6","baseamount":1,"rel":"KMD","recv":"RBNXombNrcoqQem2u95zDwyFpxv7iVtagW","relamount":0.1"}

                // send payment and txid to inbox
                // wait for return payment
            } else fprintf(stderr,"checkstr mismatch %s %s != %s\n",coin,hashstr,checkstr);
        }
        else
        {
            fprintf(stderr,"start receive %s -> %s loop\n",coin,hashstr);
            while ( 1 )
            {
                // check inbox for ordermatch or payment sent or trade complete messages
                
                // checkinbox and respond (if already paid or too big then reject, else send exact amount to send)
                //{"approval":"<hash>","id":nnnnn,"base":"PIRATE","send":"zs15qj8w57g4mfc0zfp2ke9apfa8mjj3r3zvm3v2d902586x93avv4fmp58p4kp0d79dsknwxywmf6","baseamount":1,"rel":"KMD","recv":"RBNXombNrcoqQem2u95zDwyFpxv7iVtagW","relamount":0.1"}
                
                // wait for payment and txid in inbox, mark to own inbox
                //{"txid":"<txid>","approval:"<hash>","id":nnnnn,"base":"PIRATE","send":"zs15qj8w57g4mfc0zfp2ke9apfa8mjj3r3zvm3v2d902586x93avv4fmp58p4kp0d79dsknwxywmf6","baseamount":1,"rel":"KMD","recv":"RBNXombNrcoqQem2u95zDwyFpxv7iVtagW","relamount":0.1"}

                // mark as paid
                // send return payment
                sleep(1);
            }
        }
    }
    return(0);
}

