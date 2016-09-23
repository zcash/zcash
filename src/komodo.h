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

int32_t IS_KOMODO_NOTARY;

int32_t komodo_is_notaryblock(void *block)
{
    return(0);
}

int32_t komodo_checkmsg(void *bitcoinpeer,uint8_t *data,int32_t datalen)
{
    fprintf(stderr,"KOMODO.[%d] message from peer.%p\n",datalen,bitcoinpeer);
    return(0);
}

int32_t komodo_blockcheck(void *block,uint32_t *nBitsp)
{
    //fprintf(stderr,"check block %p\n",block);
    // 1 -> valid notary block, change nBits to KOMODO_MINDIFF_NBITS
    // -1 -> invalid, ie, prior to notarized block
    return(0); // normal PoW block
}

#endif
