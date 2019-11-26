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


#ifndef CC_REWARDS_H
#define CC_REWARDS_H

#include "CCinclude.h"
#include <gmp.h>

#define EVAL_REWARDS 0xe5
#define REWARDSCC_MAXAPR (COIN * 25)

bool RewardsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);
UniValue RewardsInfo(uint256 rewardid);
UniValue RewardsList();

std::string RewardsCreateFunding(uint64_t txfee,char *planstr,int64_t funds,int64_t APR,int64_t minseconds,int64_t maxseconds,int64_t mindeposit);
std::string RewardsAddfunding(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t amount);
std::string RewardsLock(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t amount);
std::string RewardsUnlock(uint64_t txfee,char *planstr,uint256 fundingtxid,uint256 locktxid);

#endif
