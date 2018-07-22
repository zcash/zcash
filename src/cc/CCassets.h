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

#ifndef CC_ASSETS_H
#define CC_ASSETS_H

#include "CCinclude.h"

extern const char *AssetsCCaddr;
extern char AssetsCChexstr[67];
// CCcustom
bool IsAssetsInput(CScript const& scriptSig);

// CCassetsCore
CTxOut MakeAssetsVout(CAmount nValue,CPubKey pk);
CScript EncodeAssetCreateOpRet(uint8_t funcid,std::vector<uint8_t> origpubkey,std::string name,std::string description);
CScript EncodeAssetOpRet(uint8_t funcid,uint256 assetid,uint256 assetid2,uint64_t price,std::vector<uint8_t> origpubkey);
uint8_t DecodeAssetOpRet(const CScript &scriptPubKey,uint256 &assetid,uint256 &assetid2,uint64_t &price,std::vector<uint8_t> &origpubkey);
bool SetAssetOrigpubkey(std::vector<uint8_t> &origpubkey,uint64_t &price,CTransaction &tx);
uint64_t IsAssetvout(uint64_t &price,std::vector<uint8_t> &origpubkey,CTransaction& tx,int32_t v,uint256 refassetid);
bool ValidateAssetRemainder(uint64_t remaining_price,uint64_t remaining_nValue,uint64_t orig_nValue,uint64_t received,uint64_t paid,uint64_t totalprice);
bool SetAssetFillamounts(uint64_t &paid,uint64_t &remaining_price,uint64_t orig_nValue,uint64_t &received,uint64_t totalprice);
uint64_t AssetValidateBuyvin(Eval* eval,uint64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *CCaddr,char *origaddr,CTransaction &tx,uint256 refassetid);
uint64_t AssetValidateSellvin(Eval* eval,uint64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *CCaddr,char *origaddr,CTransaction &tx,uint256 assetid);
bool ConstrainAssetVout(CTxOut vout,int32_t CCflag,char *cmpaddr,uint64_t nValue);
bool AssetExactAmounts(Eval* eval,CTransaction &tx,uint256 assetid);

// CCassetstx
uint64_t GetAssetBalance(CPubKey pk,uint256 tokenid);
uint64_t AddAssetInputs(CMutableTransaction &mtx,CPubKey pk,uint256 assetid,uint64_t total,int32_t maxinputs);
UniValue AssetOrders(uint256 tokenid);
std::string CreateAsset(uint64_t txfee,uint64_t assetsupply,std::string name,std::string description);
std::string AssetTransfer(uint64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,uint64_t total);
std::string CreateBuyOffer(uint64_t txfee,uint64_t bidamount,uint256 assetid,uint64_t pricetotal);
std::string CancelBuyOffer(uint64_t txfee,uint256 assetid,uint256 bidtxid);
std::string FillBuyOffer(uint64_t txfee,uint256 assetid,uint256 bidtxid,uint64_t fillamount);
std::string CreateSell(uint64_t txfee,uint64_t askamount,uint256 assetid,uint256 assetid2,uint64_t pricetotal);
std::string CancelSell(uint64_t txfee,uint256 assetid,uint256 asktxid);
std::string FillSell(uint64_t txfee,uint256 assetid,uint256 assetid2,uint256 asktxid,uint64_t fillamount);

#endif
