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


/*
 CCassetstx has the functions that create the EVAL_ASSETS transactions. It is expected that rpc calls would call these functions. For EVAL_ASSETS, the rpc functions are in rpcwallet.cpp
 
 CCassetsCore has functions that are used in two contexts, both during rpc transaction create time and also during the blockchain validation. Using the identical functions is a good way to prevent them from being mismatched. The must match or the transaction will get rejected.
 */

#ifndef CC_TOKENS_H
#define CC_TOKENS_H

#include "CCinclude.h"

// CCcustom
bool TokensValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);
bool TokensExactAmounts(bool goDeeper, struct CCcontract_info *cpTokens, int64_t &inputs, int64_t &outputs, Eval* eval, const CTransaction &tx, uint256 tokenid);
std::string CreateToken(int64_t txfee, int64_t assetsupply, std::string name, std::string description, std::vector<uint8_t> nonfungibleData);
std::string TokenTransfer(int64_t txfee, uint256 assetid, std::vector<uint8_t> destpubkey, int64_t total);
int64_t HasBurnedTokensvouts(struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, uint256 reftokenid);
CPubKey GetTokenOriginatorPubKey(CScript scriptPubKey);
bool IsTokenMarkerVout(CTxOut vout);

int64_t GetTokenBalance(CPubKey pk, uint256 tokenid);
UniValue TokenInfo(uint256 tokenid);
UniValue TokenList();

#endif
