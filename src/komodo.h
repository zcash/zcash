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

int32_t IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,NOTARIZED_HEIGHT;
std::string NOTARY_PUBKEY;
uint256 NOTARIZED_HASH;

int32_t komodo_blockindexcheck(CBlockIndex *pindex,uint32_t *nBitsp)
{
    // 1 -> valid notary block, change nBits to KOMODO_MINDIFF_NBITS
    // -1 -> invalid, ie, prior to notarized block
    CBlock block; int32_t height; char *coinbasestr;
    if ( ReadBlockFromDisk(block,pindex) == 0 )
        return(-1);
    height = pindex->nHeight;
    coinbasestr = block.vtx[0].vout[0].scriptPubKey.c_str();
    printf("ht.%d (%s)\n",height,coinbasestr);
    // compare against elected notary pubkeys as of height
    return(0);
}

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    // update voting results and official (height, notaries[])
}

int32_t komodo_is_notaryblock(CBlockHeader& blockhdr)
{
    uint32_t nBits = 0;
    return(komodo_blockindexcheck(mapBlockIndex[blockhdr.GetHash()],&nBits));
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
