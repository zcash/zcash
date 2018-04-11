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

#ifndef H_KOMODOCCDATA_H
#define H_KOMODOCCDATA_H


/*struct komodo_ccdatapair { int32_t notarization_height; uint32_t MoMoMoffset; };
struct komodo_ccdata
{
    uint32_t CCid;
    bits256 MoMoM,MoM;
    int32_t MoMoMdepth,numpairs,notarized_height,height,txi;
    struct ccdatapair *pairs;
    char symbol[65];
};
*/

int32_t komodo_rwccdata(int32_t rwflag,struct komodo_ccdata *ccdata)
{
    if ( rwflag == 0 )
    {
        
    }
    char str[65]; fprintf(stderr,"[%s] ccdata.%s id.%d notarized_ht.%d MoM.%s height.%d/t%d numpairs.%d\n",ASSETCHAINS_SYMBOL,ccdata->symbol,ccdata->CCid,ccdata->notarized_height,bits256_str(str,*(bits256 *)&ccdata->MoM),ccdata->height,ccdata->txi,ccdata->numpairs);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
    }
    else
    {
    }
}

#endif
