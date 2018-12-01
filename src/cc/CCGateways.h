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


#ifndef CC_GATEWAYS_H
#define CC_GATEWAYS_H

#include "CCinclude.h"
#include "../merkleblock.h"

bool GatewaysValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);
std::string GatewaysBind(uint64_t txfee,std::string coin,uint256 tokenid,int64_t totalsupply,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> pubkeys);
std::string GatewaysDeposit(uint64_t txfee,uint256 bindtxid,int32_t height,std::string refcoin,uint256 cointxid,int32_t claimvout,std::string deposithex,std::vector<uint8_t>proof,CPubKey destpub,int64_t amount);
std::string GatewaysClaim(uint64_t txfee,uint256 bindtxid,std::string refcoin,uint256 deposittxid,CPubKey destpub,int64_t amount);
std::string GatewaysWithdraw(uint64_t txfee,uint256 bindtxid,std::string refcoin,CPubKey withdrawpub,int64_t amount);
std::string GatewaysPartialSign(uint64_t txfee,uint256 txidaddr,std::string refcoin,std::string hex);
std::string GatewaysCompleteSigning(uint64_t txfee,uint256 txidaddr,std::string refcoin,std::string hex);
std::string GatewaysMarkDone(uint64_t txfee,uint256 withdrawtxid,std::string refcoin);
UniValue GatewaysPendingWithdraws(uint256 bindtxid,std::string refcoin);
UniValue GatewaysProcessedWithdraws(uint256 bindtxid,std::string refcoin);
UniValue GatewaysMultisig(char *txidaddr);


// CCcustom
UniValue GatewaysInfo(uint256 bindtxid);
UniValue GatewaysList();

#endif
