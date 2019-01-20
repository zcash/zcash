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


#ifndef CC_FAUCET_H
#define CC_FAUCET_H

#include "CCinclude.h"

#define EVAL_FAUCET 0xe4
#define FAUCETSIZE (COIN / 10)

bool FaucetValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

// CCcustom
std::string FaucetFund(uint64_t txfee,int64_t funds);
std::string FaucetGet(uint64_t txfee);
UniValue FaucetInfo();

#endif
