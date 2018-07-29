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


#ifndef CC_DICE_H
#define CC_DICE_H

#include "CCinclude.h"

#define EVAL_DICE 0xe6

bool DiceValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx);

std::string DiceFund(uint64_t txfee,uint64_t funds);
std::string DiceCreateFunding(uint64_t txfee,int64_t funds,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t forfeitblocks);

#endif
