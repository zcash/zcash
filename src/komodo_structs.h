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

#include "uthash.h"
#include "utlist.h"

struct pax_transaction
{
    UT_hash_handle hh;
    uint256 txid;
    uint64_t komodoshis,fiatoshis;
    int32_t marked,height,otherheight;
    uint16_t vout;
    char symbol[16],coinaddr[64]; uint8_t rmd160[20],shortflag;
};

struct nutxo_entry { UT_hash_handle hh; uint256 txhash; uint64_t voutmask; int32_t notaryid,height; };
struct knotary_entry { UT_hash_handle hh; uint8_t pubkey[33],notaryid; };
struct knotaries_entry { int32_t height,numnotaries; struct knotary_entry *Notaries; };
struct notarized_checkpoint { uint256 notarized_hash,notarized_desttxid; int32_t nHeight,notarized_height; };

struct komodo_state
{
    uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID;
    int32_t CURRENT_HEIGHT,NOTARIZED_HEIGHT;
    // gateway state
};
