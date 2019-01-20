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


#ifndef CC_AUCTION_H
#define CC_AUCTION_H

#include "CCinclude.h"

#define EVAL_AUCTION 0xe8

bool AuctionValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

std::string AuctionPost(uint64_t txfee,uint256 itemhash,int64_t minbid,char *title,char *description);
std::string AuctionBid(uint64_t txfee,uint256 itemhash,int64_t amount);
std::string AuctionDeliver(uint64_t txfee,uint256 itemhash,uint256 bidtxid);

#endif
