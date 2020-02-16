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

// build subatomic and put in path: git pull; gcc cc/dapps/subatomic.c -lm -o subatomic; cp subatomic /usr/bin
// alice sends relcoin and gets basecoin

#define DEXP2P_CHAIN ((char *)"DEX")
#define DEXP2P_PUBKEYS ((char *)"subatomic")
#include "dappinc.h"
#include "subatomicinc.h"

// new inventory:
//   anonsend

// bob nodes:
//   mutex for bob instances
//   "deposits" messages and approved bobs
//   volume caps per coin and non-notarized exposure

// later:
//   sharded storage

int32_t main(int32_t argc,char **argv)
{
    char *fname = "subatomic.json";
    int32_t i,height; char *coin,*kcli,*subatomic,*hashstr,*acname=(char *)""; cJSON *retjson; bits256 blockhash; char checkstr[65],str[65],str2[65],tmpstr[32]; long fsize; struct msginfo M;
    memset(&M,0,sizeof(M));
    srand((int32_t)time(NULL));
    if ( (subatomic= filestr(&fsize,fname)) == 0 )
    {
        fprintf(stderr,"cant load %s file\n",fname);
        exit(-1);
    }
    if ( (SUBATOMIC_json= cJSON_Parse(subatomic)) == 0 )
    {
        fprintf(stderr,"cant parse subatomic.json file (%s)\n",subatomic);
        exit(-1);
    }
    free(subatomic);
    if ( argc >= 4 )
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
            {
                acname = coin;
            }
        }
        hashstr = (char *)argv[3];
        strcpy(M.rel.coin,subatomic_checkname(tmpstr,&M,1,coin));
        strcpy(M.rel.name,coin);
        if ( argc == 4 && strlen(hashstr) == 64 ) // for blocknotify usage, seems not needed
        {
            height = get_coinheight(coin,acname,&blockhash);
            bits256_str(checkstr,blockhash);
            if ( strcmp(checkstr,hashstr) == 0 )
            {
                fprintf(stderr,"%s: (%s) %s height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,height);
                if ( (retjson= dpow_ntzdata(coin,SUBATOMIC_PRIORITY,height,blockhash)) != 0 )
                    free_json(retjson);
            } else fprintf(stderr,"coin.%s (%s) %s vs %s, height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,hashstr,height);
            if ( strcmp("BTC",coin) != 0 )
            {
                bits256 prevntzhash,ntzhash; int32_t prevntzheight,ntzheight; uint32_t ntztime,prevntztime; char hexstr[81]; cJSON *retjson2;
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
                        if ( (retjson2= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,coin,"notarizations",DPOW_pubkeystr,"","")) != 0 )
                            free_json(retjson2);
                    }
                    else if ( ntzheight == prevntzheight && memcmp(&prevntzhash,&ntzhash,32) != 0 )
                        fprintf(stderr,"NTZ ERROR %s.%d %s != %s\n",coin,ntzheight,bits256_str(str,ntzhash),bits256_str(str2,prevntzhash));
                    free_json(retjson);
                }
            }
        }
        else if ( argc == 5 && atol(hashstr) > 10000 )
        {
            char checkstr[32]; uint64_t mult = 1;
            M.origid = (uint32_t)atol(hashstr);
            sprintf(checkstr,"%u",M.origid);
            if ( strcmp(checkstr,hashstr) == 0 ) // alice
            {
                M.rel.satoshis = (uint64_t)(atof(argv[4])*SATOSHIDEN+0.0000000049999);
                for (i=0; M.rel.name[i]!=0; i++)
                    if ( M.rel.name[i] == '.' )
                    {
                        mult = SATOSHIDEN;
                        break;
                    }
                if ( subatomic_getbalance(&M.rel) < M.rel.satoshis/mult )
                {
                    fprintf(stderr,"not enough balance %s %.8f for %.8f\n",M.rel.coin,dstr(subatomic_getbalance(&M.rel)),dstr(M.rel.satoshis/mult));
                    return(-1);
                }
                fprintf(stderr,"subatomic_channel_alice (%s/%s) %s %u with %.8f %llu\n",M.rel.name,M.rel.coin,hashstr,M.origid,atof(argv[4]),(long long)M.rel.satoshis);
                dpow_pubkeyregister(SUBATOMIC_PRIORITY);
                M.openrequestid = subatomic_alice_openrequest(&M);
                if ( M.openrequestid != 0 )
                    subatomic_loop(&M);
            } else fprintf(stderr,"checkstr mismatch %s %s != %s\n",coin,hashstr,checkstr);
        }
        else
        {
            M.bobflag = 1;
            strcpy(M.base.name,hashstr);
            strcpy(M.base.coin,subatomic_checkname(tmpstr,&M,0,hashstr));
            subatomic_loop(&M); // while ( 1 ) loop for each relcoin -> basecoin
        }
    }
    return(SUBATOMIC_retval);
}

