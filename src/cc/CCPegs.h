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


#ifndef CC_PEGS_H
#define CC_PEGS_H

#include "CCinclude.h"

bool PegsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

// CCcustom
UniValue PegsCreate(const CPubKey& pk,uint64_t txfee,int64_t amount,std::vector<uint256> bindtxids);
UniValue PegsFund(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid, int64_t amount);
UniValue PegsGet(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid, int64_t amount);
UniValue PegsRedeem(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid);
UniValue PegsLiquidate(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid, uint256 liquidatetxid);
UniValue PegsExchange(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid, int64_t amount);
UniValue PegsAccountHistory(const CPubKey& pk,uint256 pegstxid);
UniValue PegsAccountInfo(const CPubKey& pk,uint256 pegstxid);
UniValue PegsWorstAccounts(uint256 pegstxid);
UniValue PegsInfo(uint256 pegstxid);

#endif
