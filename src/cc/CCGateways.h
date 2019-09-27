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


#ifndef CC_GATEWAYS_H
#define CC_GATEWAYS_H

#include "CCinclude.h"

bool GatewaysValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);
UniValue GatewaysBind(const CPubKey& pk, uint64_t txfee,std::string coin,uint256 tokenid,int64_t totalsupply,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> pubkeys,uint8_t p1,uint8_t p2,uint8_t p3,uint8_t p4);
UniValue GatewaysDeposit(const CPubKey& pk, uint64_t txfee,uint256 bindtxid,int32_t height,std::string refcoin,uint256 cointxid,int32_t claimvout,std::string deposithex,std::vector<uint8_t>proof,CPubKey destpub,int64_t amount);
UniValue GatewaysClaim(const CPubKey& pk, uint64_t txfee,uint256 bindtxid,std::string refcoin,uint256 deposittxid,CPubKey destpub,int64_t amount);
UniValue GatewaysWithdraw(const CPubKey& pk, uint64_t txfee,uint256 bindtxid,std::string refcoin,CPubKey withdrawpub,int64_t amount);
UniValue GatewaysPartialSign(const CPubKey& pk, uint64_t txfee,uint256 txidaddr,std::string refcoin,std::string hex);
UniValue GatewaysCompleteSigning(const CPubKey& pk, uint64_t txfee,uint256 txidaddr,std::string refcoin,std::string hex);
UniValue GatewaysMarkDone(const CPubKey& pk, uint64_t txfee,uint256 withdrawtxid,std::string refcoin);
UniValue GatewaysPendingDeposits(const CPubKey& pk, uint256 bindtxid,std::string refcoin);
UniValue GatewaysPendingWithdraws(const CPubKey& pk, uint256 bindtxid,std::string refcoin);
UniValue GatewaysProcessedWithdraws(const CPubKey& pk, uint256 bindtxid,std::string refcoin);

// CCcustom
UniValue GatewaysInfo(uint256 bindtxid);
UniValue GatewaysExternalAddress(uint256 bindtxid,CPubKey pubkey);
UniValue GatewaysDumpPrivKey(uint256 bindtxid,CKey privkey);
UniValue GatewaysList();

#endif
