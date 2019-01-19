/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
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

#include <assert.h>
#include <cryptoconditions.h>

#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/cc.h"
#include "cc/eval.h"
#include "cc/utils.h"
#include "cc/CCinclude.h"
#include "main.h"
#include "chain.h"
#include "core_io.h"
#include "crosschain.h"

#define MYCCLIBNAME ((char *)"stub")

char *CClib_name() { return(MYCCLIBNAME); }

bool CClib_Dispatch(const CC *cond,Eval *eval,std::vector<uint8_t> paramsNull,const CTransaction &txTo,unsigned int nIn)
{
    uint8_t evalcode; int32_t height,from_mempool;
    if ( ASSETCHAINS_CCLIB != MYCCLIBNAME )
    {
        fprintf(stderr,"-ac_cclib=%s vs myname %s\n",ASSETCHAINS_CCLIB.c_str(),MYCCLIBNAME);
        return eval->Invalid("-ac_cclib name mismatches myname");
    }
    height = KOMODO_CONNECTING;
    if ( KOMODO_CONNECTING < 0 ) // always comes back with > 0 for final confirmation
        return(true);
    if ( ASSETCHAINS_CC == 0 || (height & ~(1<<30)) < KOMODO_CCACTIVATE )
        return eval->Invalid("CC are disabled or not active yet");
    if ( (KOMODO_CONNECTING & (1<<30)) != 0 )
    {
        from_mempool = 1;
        height &= ((1<<30) - 1);
    }
    evalcode = cond->code[0];
    if ( evalcode >= EVAL_FIRSTUSER && evalcode <= EVAL_LASTUSER )
    {
        return(true);
    }
    return eval->Invalid("cclib CC must have evalcode between 16 and 127");
}
