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


#ifndef CC_IMPORTGATEWAY_H
#define CC_IMPORTGATEWAY_H

#include "CCinclude.h"

// CCcustom
bool ImportGatewayValidate(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx, uint32_t nIn);
bool ImportGatewayExactAmounts(bool goDeeper, struct CCcontract_info *cpTokens, int64_t &inputs, int64_t &outputs, Eval* eval, const CTransaction &tx, uint256 tokenid);
std::string ImportGatewayBind(uint64_t txfee,std::string coin,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> pubkeys,uint8_t p1,uint8_t p2,uint8_t p3,uint8_t p4);
std::string ImportGatewayDeposit(uint64_t txfee,uint256 bindtxid,int32_t height,std::string refcoin,uint256 burntxid,int32_t burnvout,std::string rawburntx,std::vector<uint8_t>proof,CPubKey destpub,int64_t amount);
std::string ImportGatewayWithdraw(uint64_t txfee,uint256 bindtxid,std::string refcoin,CPubKey withdrawpub,int64_t amount);
std::string ImportGatewayPartialSign(uint64_t txfee,uint256 lasttxid,std::string refcoin, std::string hex);
std::string ImportGatewayCompleteSigning(uint64_t txfee,uint256 lasttxid,std::string refcoin,std::string hex);
std::string ImportGatewayMarkDone(uint64_t txfee,uint256 completetxid,std::string refcoin);
UniValue ImportGatewayPendingDeposits(uint256 bindtxid,std::string refcoin);
UniValue ImportGatewayPendingWithdraws(uint256 bindtxid,std::string refcoin);
UniValue ImportGatewayProcessedWithdraws(uint256 bindtxid,std::string refcoin);
UniValue ImportGatewayExternalAddress(uint256 bindtxid,CPubKey pubkey);
UniValue ImportGatewayDumpPrivKey(uint256 bindtxid,CKey key);
UniValue ImportGatewayList();
UniValue ImportGatewayInfo(uint256 bindtxid);
#endif