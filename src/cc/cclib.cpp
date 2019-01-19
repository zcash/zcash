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
//#include <cryptoconditions.h>
#include "cryptoconditions/include/cryptoconditions.h"
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

char *CClib_name() { return((char *)"stub"); }

bool CClib_Dispatch(const CC *cond,Eval *eval,std::vector<uint8_t> paramsNull,const CTransaction &txTo,unsigned int nIn)
{
    uint8_t evalcode = cond->code[0];
    if ( evalcode >= EVAL_FIRSTUSER && evalcode <= EVAL_LASTUSER )
    {
        
    }
    return eval->Invalid("cclib CC must have evalcode between 16 and 127");
}
