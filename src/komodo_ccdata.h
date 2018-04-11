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
    uint256 MoMoM,MoM;
    int32_t MoMoMstart,MoMoMend,MoMoMdepth,numpairs,notarized_height,height,txi,len,MoMdepth;
    struct komodo_ccdatapair *pairs;
    char symbol[65];
};
*/

int32_t komodo_rwccdata(char *thischain,int32_t rwflag,struct komodo_ccdata *ccdata,struct komodo_ccdataMoMoM *MoMoMdata)
{
    bits256 hash; int32_t i;
    if ( rwflag == 0 )
    {
        
    }
    for (i=0; i<32; i++)
        hash.bytes[i] = ((uint8_t *)&ccdata->MoMdata.MoM)[31-i];
    char str[65]; fprintf(stderr,"[%s] ccdata.%s id.%d notarized_ht.%d MoM.%s height.%d/t%d\n",ASSETCHAINS_SYMBOL,ccdata->symbol,ccdata->CCid,ccdata->MoMdata.notarized_height,bits256_str(str,hash),ccdata->MoMdata.height,ccdata->MoMdata.txi);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        // find/create entry for CCid
        // if KMD, for all CCids, get range and calc MoMoM for RPC retrieval
        // given MoM height for CCid chain, find the offset and MoMoM
    }
    else
    {
    }
}

#endif
