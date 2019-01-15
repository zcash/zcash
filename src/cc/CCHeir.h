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


#ifndef CC_HEIR_H
#define CC_HEIR_H

#include "CCinclude.h"
#include "CCtokens.h"

//#define EVAL_HEIR 0xea

bool HeirValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

class CoinHelper;
class TokenHelper;

// CCcustom

// this would not link
//template <class Helper> std::string HeirFund(uint64_t txfee,int64_t funds, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, uint256 assetid);
//template <class Helper> UniValue HeirAdd(uint256 fundingtxid, uint64_t txfee, int64_t amount);
//template <class Helper> UniValue HeirClaim(uint256 fundingtxid, uint64_t txfee, int64_t nValue);

UniValue HeirFundCoinCaller(uint64_t txfee, int64_t funds, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, uint256 assetid);
UniValue HeirFundTokenCaller(uint64_t txfee, int64_t funds, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, uint256 assetid);
UniValue HeirClaimCoinCaller(uint256 fundingtxid, uint64_t txfee, int64_t amount);
UniValue HeirClaimTokenCaller(uint256 fundingtxid, uint64_t txfee, int64_t amount);
UniValue HeirAddCoinCaller(uint256 fundingtxid, uint64_t txfee, int64_t amount);
UniValue HeirAddTokenCaller(uint256 fundingtxid, uint64_t txfee, int64_t amount);

UniValue HeirInfo(uint256 fundingtxid);
UniValue HeirList();
//std::string Heir_MakeBadTx(uint256 fundingtxid, uint8_t funcId, int64_t amount, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTime, uint32_t errMask);

//bool HeirExactTokenAmounts(bool compareTotals, struct CCcontract_info *cpHeir, Eval* eval, uint256 assetid, const CTransaction &tx);

#endif
