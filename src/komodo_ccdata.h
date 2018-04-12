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

/*
struct komodo_ccdataMoM
{
    uint256 MoM;
    int32_t MoMdepth,notarized_height,height,txi;
};

struct komodo_ccdatapair { int32_t notarization_height; uint32_t MoMoMoffset; };

struct komodo_ccdataMoMoM
{
    uint256 MoMoM;
    int32_t MoMoMstarti,MoMoMendi,numpairs,len;
    struct komodo_ccdatapair *pairs;
};

struct komodo_ccdata
{
    struct komodo_ccdataMoM MoMdata;
    uint32_t CCid,len,inMoMoM;
    char symbol[65];
};
*/

struct komodo_ccdata *CC_data;

bits256 iguana_merkle(bits256 *tree,int32_t txn_count)
{
    int32_t i,n=0,prev; uint8_t serialized[sizeof(bits256) * 2];
    if ( txn_count == 1 )
        return(tree[0]);
    prev = 0;
    while ( txn_count > 1 )
    {
        if ( (txn_count & 1) != 0 )
            tree[prev + txn_count] = tree[prev + txn_count-1], txn_count++;
        n += txn_count;
        for (i=0; i<txn_count; i+=2)
        {
            iguana_rwbignum(1,serialized,sizeof(*tree),tree[prev + i].bytes);
            iguana_rwbignum(1,&serialized[sizeof(*tree)],sizeof(*tree),tree[prev + i + 1].bytes);
            tree[n + (i >> 1)] = bits256_doublesha256(0,serialized,sizeof(serialized));
        }
        prev = n;
        txn_count >>= 1;
    }
    return(tree[n]);
}

char *komodo_MoMoMdata(char *symbol,int32_t kmdheight,int32_t notarized_height)
{
    cJSON *retjson,*pairs,*item; struct komodo_ccdata *ccdata,*tmp; int32_t max,offset,starti,endi,kmdstarti=0; bits256 *tree,MoMoM;
    starti = endi = offset = max = 0;
    pairs = cJSON_CreateArray();
    portable_mutex_lock(&KOMODO_CC_mutex);
    DL_FOREACH_SAFE(CC_data,ccdata,tmp)
    {
        if ( ccdata->MoMdata.height < kmdheight )
        {
            if ( endi == 0 )
                endi = ccdata->MoMdata.height;
            if ( strcmp(ccdata->symbol,symbol) == 0 )
            {
                if (ccdata->MoMdata.notarized_height <= notarized_height )
                {
                    starti = ccdata->MoMdata.height + 1;
                    break;
                }
                item = cJSON_CreateArray();
                jaddinum(item,ccdata->MoMdata.notarized_height);
                jaddinum(item,offset);
                jaddi(pairs,item);
            }
            if ( offset >= max )
            {
                max += 100;
                tree = (bits256 *)realloc(tree,sizeof(*tree)*max);
            }
            memcpy(&tree[offset++],&ccdata->MoMdata.MoM,sizeof(bits256));
            starti = ccdata->MoMdata.height;
        }
    }
    portable_mutex_unlock(&KOMODO_CC_mutex);
    retjson = cJSON_CreateObject();
    jaddnum(retjson,(char *)"kmdstarti",starti);
    jaddnum(retjson,(char *)"kmdendi",endi);
    if ( starti != 0 && endi != 0 && endi >= starti )
    {
        if ( tree != 0 && offset > 0 )
        {
            MoMoM = iguana_merkle(tree,offset);
            jaddbits256(retjson,(char *)"MoMoM",MoMoM);
        }
    }
    if ( tree != 0 )
        free(tree);
    jadd(retjson,(char *)"offsets",pairs);
    fprintf(stderr,"%s\n",jprint(retjson,0));
    return(jprint(retjson,1));
}

int32_t komodo_rwccdata(char *thischain,int32_t rwflag,struct komodo_ccdata *ccdata,struct komodo_ccdataMoMoM *MoMoMdata)
{
    bits256 hash; int32_t i; struct komodo_ccdata *ptr;
    if ( rwflag == 0 )
    {
        
    }
    for (i=0; i<32; i++)
        hash.bytes[i] = ((uint8_t *)&ccdata->MoMdata.MoM)[31-i];
    char str[65]; fprintf(stderr,"[%s] ccdata.%s id.%d notarized_ht.%d MoM.%s height.%d/t%d\n",ASSETCHAINS_SYMBOL,ccdata->symbol,ccdata->CCid,ccdata->MoMdata.notarized_height,bits256_str(str,hash),ccdata->MoMdata.height,ccdata->MoMdata.txi);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        ptr = (struct komodo_ccdata *)calloc(1,sizeof(*ptr));
        *ptr = *ccdata;
        portable_mutex_lock(&KOMODO_CC_mutex);
        DL_PREPEND(CC_data,ptr);
        portable_mutex_unlock(&KOMODO_CC_mutex);
    }
    else
    {
    }
}

#endif
