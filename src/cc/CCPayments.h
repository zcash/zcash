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


#ifndef CC_PAYMENTS_H
#define CC_PAYMENTS_H

#include "CCinclude.h"
#include <gmp.h>
#include <key_io.h>

#define PAYMENTS_TXFEE 10000
#define PAYMENTS_MERGEOFSET 60 // 1H extra. 
extern std::vector <std::pair<CAmount, CTxDestination>> vAddressSnapshot;
extern int32_t lastSnapShotHeight;

bool PaymentsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

// CCcustom
UniValue PaymentsRelease(struct CCcontract_info *cp,char *jsonstr);
UniValue PaymentsFund(struct CCcontract_info *cp,char *jsonstr);
UniValue PaymentsMerge(struct CCcontract_info *cp,char *jsonstr);
UniValue PaymentsTxidopret(struct CCcontract_info *cp,char *jsonstr);
UniValue PaymentsCreate(struct CCcontract_info *cp,char *jsonstr);
UniValue PaymentsAirdrop(struct CCcontract_info *cp,char *jsonstr);
UniValue PaymentsAirdropTokens(struct CCcontract_info *cp,char *jsonstr);
UniValue PaymentsInfo(struct CCcontract_info *cp,char *jsonstr);
UniValue PaymentsList(struct CCcontract_info *cp,char *jsonstr);

#endif
 
