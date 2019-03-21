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


/*
 CCassetstx has the functions that create the EVAL_ASSETS transactions. It is expected that rpc calls would call these functions. For EVAL_ASSETS, the rpc functions are in rpcwallet.cpp
 
 CCassetsCore has functions that are used in two contexts, both during rpc transaction create time and also during the blockchain validation. Using the identical functions is a good way to prevent them from being mismatched. The must match or the transaction will get rejected.
 */

#ifndef CC_ASSETS_H
#define CC_ASSETS_H

#include "CCinclude.h"

// CCcustom
bool AssetsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

// CCassetsCore
vscript_t EncodeAssetOpRet(uint8_t assetFuncId, uint256 assetid2, int64_t price, std::vector<uint8_t> origpubkey);
uint8_t DecodeAssetTokenOpRet(const CScript &scriptPubKey, uint8_t &assetsEvalCode, uint256 &tokenid, uint256 &assetid2, int64_t &price, std::vector<uint8_t> &origpubkey);
bool SetAssetOrigpubkey(std::vector<uint8_t> &origpubkey,int64_t &price,const CTransaction &tx);
int64_t IsAssetvout(struct CCcontract_info *cp, int64_t &price, std::vector<uint8_t> &origpubkey, const CTransaction& tx, int32_t v, uint256 refassetid);
bool ValidateBidRemainder(int64_t remaining_price,int64_t remaining_nValue,int64_t orig_nValue,int64_t received_nValue,int64_t paidprice,int64_t totalprice);
bool ValidateAskRemainder(int64_t remaining_price,int64_t remaining_nValue,int64_t orig_nValue,int64_t received_nValue,int64_t paidprice,int64_t totalprice);
bool ValidateSwapRemainder(int64_t remaining_price,int64_t remaining_nValue,int64_t orig_nValue,int64_t received_nValue,int64_t paidprice,int64_t totalprice);
bool SetBidFillamounts(int64_t &paid,int64_t &remaining_price,int64_t orig_nValue,int64_t &received,int64_t totalprice);
bool SetAskFillamounts(int64_t &paid,int64_t &remaining_price,int64_t orig_nValue,int64_t &received,int64_t totalprice);
bool SetSwapFillamounts(int64_t &paid,int64_t &remaining_price,int64_t orig_nValue,int64_t &received,int64_t totalprice);
int64_t AssetValidateBuyvin(struct CCcontract_info *cp,Eval* eval,int64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *CCaddr,char *origaddr,const CTransaction &tx,uint256 refassetid);
int64_t AssetValidateSellvin(struct CCcontract_info *cp,Eval* eval,int64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *CCaddr,char *origaddr,const CTransaction &tx,uint256 assetid);
bool AssetCalcAmounts(struct CCcontract_info *cpAssets, int64_t &inputs, int64_t &outputs, Eval* eval, const CTransaction &tx, uint256 assetid);

// CCassetstx
//int64_t GetAssetBalance(CPubKey pk,uint256 tokenid); // --> GetTokenBalance()
int64_t AddAssetInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 assetid, int64_t total, int32_t maxinputs);

UniValue AssetOrders(uint256 tokenid, CPubKey pubkey, uint8_t additionalEvalCode);
//UniValue AssetInfo(uint256 tokenid);
//UniValue AssetList();
//std::string CreateAsset(int64_t txfee,int64_t assetsupply,std::string name,std::string description);
//std::string AssetTransfer(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total);
//std::string AssetConvert(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total,int32_t evalcode);

std::string CreateBuyOffer(int64_t txfee,int64_t bidamount,uint256 assetid,int64_t pricetotal);
std::string CancelBuyOffer(int64_t txfee,uint256 assetid,uint256 bidtxid);
std::string FillBuyOffer(int64_t txfee,uint256 assetid,uint256 bidtxid,int64_t fillamount);
std::string CreateSell(int64_t txfee,int64_t askamount,uint256 assetid,int64_t pricetotal);
std::string CreateSwap(int64_t txfee,int64_t askamount,uint256 assetid,uint256 assetid2,int64_t pricetotal);
std::string CancelSell(int64_t txfee,uint256 assetid,uint256 asktxid);
std::string FillSell(int64_t txfee,uint256 assetid,uint256 assetid2,uint256 asktxid,int64_t fillamount);

#endif
