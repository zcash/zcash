/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../../komodo_cJSON.c"
#include "../../komodo_bitcoind.h"

#define issue_curl(url,cmdstr) bitcoind_RPC(0,(char *)"curl",(char *)(url),0,0,(char *)(cmdstr))

cJSON *url_json(char *url)
{
    char *jsonstr; cJSON *json = 0;
    if ( (jsonstr= issue_curl(url)) != 0 )
    {
        printf("(%s) -> (%s)\n",url,jsonstr);
        json = cJSON_Parse(jsonstr);
        free(jsonstr);
    }
    return(json);
}

int32_t main(int32_t argc,char **argv)
{
    cJSON *pjson,*bpi,*usd;
    printf("Powered by CoinDesk (%s)\n","https://www.coindesk.com/price/");
    if ( (pjson= url_json("http://api.coindesk.com/v1/bpi/currentprice.json","")) != 0 )
    {
        if ( (bpi= jobj(pjson,"bpi")) != 0 && (usd= jobj(bpi,"USD")) != 0 )
            printf("BTC/USD %.4f\n",jdouble(usd,"rate_float"));
        json_close(pjson);
    }
    return(0);
}
