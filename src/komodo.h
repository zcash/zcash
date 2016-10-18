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

#ifndef H_KOMODO_H
#define H_KOMODO_H

#include <stdint.h>
#include <stdio.h>

#define CRYPTO777_PUBSECPSTR "020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9"

int32_t IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,NOTARIZED_HEIGHT;
std::string NOTARY_PUBKEY;
uint256 NOTARIZED_HASH;

int32_t komodo_blockindexcheck(CBlockIndex *pindex,uint32_t *nBitsp)
{
    // 1 -> valid notary block, change nBits to KOMODO_MINDIFF_NBITS
    // -1 -> invalid, ie, prior to notarized block
    CBlock block; int32_t height; char *coinbasestr;
    if ( pindex == 0 )
        return(0);
    if ( ReadBlockFromDisk(block,pindex,1) == 0 )
        return(0);
    if ( block.vtx.size() > 0 )
    {
        height = pindex->nHeight;
        coinbasestr = (char *)block.vtx[0].vout[0].scriptPubKey.ToString().c_str();
        printf("komodo_blockindexcheck ht.%d (%s)\n",height,coinbasestr);
    }
    // compare against elected notary pubkeys as of height
    return(0);
}

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    char *scriptstr; int32_t i,height,txn_count,len;
    // update voting results and official (height, notaries[])
    if ( pindex != 0 )
    {
        height = pindex->nHeight;
        txn_count = block.vtx.size();
        for (i=0; i<txn_count; i++)
        {
            scriptstr = (char *)block.vtx[i].vout[0].scriptPubKey.ToString().c_str();
            if ( (len= strlen(scriptstr)) == 0 )
            {
                printf("komodo_connectblock: ht.%d NULL script??\n",height);
                if ( ReadBlockFromDisk(block,pindex,1) == 0 )
                {
                    printf("komodo_connectblock: ht.%d error reading block\n",height);
                    return;
                }
                txn_count = block.vtx.size();
                i = -1;
                printf("loaded block ht.%d\n",height);
                continue;
            }
            if ( len == 66 && strcmp(scriptstr,CRYPTO777_PUBSECPSTR,66) == 0 )
                printf("ht.%d txi.%d (%s)\n",height,i,scriptstr);
        }
    } else printf("komodo_connectblock: unexpected null pindex\n");
}

int32_t komodo_is_notaryblock(CBlockHeader& blockhdr)
{
    //uint32_t nBits = 0;
    //return(komodo_blockindexcheck(mapBlockIndex[blockhdr.GetHash()],&nBits));
    return(0);
}

int32_t komodo_blockhdrcheck(CBlockHeader& blockhdr,uint32_t *nBitsp)
{
    int32_t retval;
    if ( (retval= komodo_is_notaryblock(blockhdr)) > 0 )
        *nBitsp = KOMODO_MINDIFF_NBITS;
    return(retval);
}

int32_t komodo_blockcheck(CBlock& block,uint32_t *nBitsp)
{
    return(komodo_blockhdrcheck(block,nBitsp));
}

#endif
