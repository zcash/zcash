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

void komodo_gateway_iteration(char *symbol)
{
    char *retstr; int32_t i,num; cJSON *item,*listobj; static uint32_t r,n,counter=0;
    if ( r == 0 )
        r = rand();
    if ( (counter++ % 10) == (r % 10) )
    {
        printf("%s calling getinfo %d\n",symbol,n);
        if ( (retstr= komodo_issuemethod((char *)"getinfo",0,7771)) != 0 )
        {
            //printf("GETINFO from.%s (%s)\n",ASSETCHAINS_SYMBOL,retstr);
            free(retstr);
            if ( (retstr= komodo_issuemethod((char *)"listtransactions",0,7771)) != 0 )
            {
                //printf("LIST.(%s)\n",retstr);
                if ( (listobj= cJSON_Parse(retstr)) != 0 )
                {
                    if ( (num= cJSON_GetArraySize(listobj)) > 0 )
                    {
                        for (i=0; i<num; i++)
                        {
                            item = jitem(listobj,i);
                            printf("%d.(%s)\n",i,jprint(item,0));
                        }
                    }
                    free_json(listobj);
                }
                free(retstr);
            }
            n++;
        } else printf("error from %s\n",symbol);
    }
}

void komodo_gateway_issuer() // "assetchain"
{
    
}

void komodo_gateway_redeemer() // "KMD"
{
    
}