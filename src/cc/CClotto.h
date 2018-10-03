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


#ifndef CC_LOTTO_H
#define CC_LOTTO_H

#include "CCinclude.h"

#define EVAL_LOTTO 0xe9

bool LottoValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

UniValue LottoInfo(uint256 lottoid);
UniValue LottoList();
std::string LottoTicket(uint64_t txfee,int64_t numtickets);
std::string LottoWinner(uint64_t txfee);

#endif
