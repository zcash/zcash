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

#include "old/CCassets_v0.h"

#define ASSETS_GLOBALADDR_VIN  1
#define ASSETS_GLOBALADDR_VOUT 0
#define ASSETS_MARKER_AMOUNT 10000


// CCcustom
bool AssetsValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn);

// CCassetsCore
vscript_t EncodeAssetOpRet(uint8_t assetFuncId, uint256 assetid2, int64_t unit_price, std::vector<uint8_t> origpubkey);
uint8_t DecodeAssetTokenOpRet(const CScript &scriptPubKey, uint8_t &assetsEvalCode, uint256 &tokenid, uint256 &assetid2, int64_t &unit_price, std::vector<uint8_t> &origpubkey);
uint8_t SetAssetOrigpubkey(std::vector<uint8_t> &origpubkey_out, CAmount &unit_price, const CTransaction &tx);
int64_t IsAssetvout(struct CCcontract_info *cp, int64_t &remaining_units_out, std::vector<uint8_t> &origpubkey_out, const CTransaction& tx, int32_t v, uint256 refassetid);
bool ValidateBidRemainder(CAmount unit_price, int64_t remaining_nValue, int64_t orig_nValue, int64_t received_nValue, int64_t paid_units);
bool ValidateAskRemainder(CAmount unit_price, int64_t remaining_assetoshis, int64_t orig_assetoshis, int64_t received_assetoshis, int64_t paid_nValue);
bool ValidateSwapRemainder(int64_t remaining_units, int64_t remaining_nValue, int64_t orig_nValue, int64_t received_nValue, int64_t paidprice, int64_t totalprice);
bool SetBidFillamounts(CAmount unit_price, int64_t &received_nValue, int64_t orig_nValue, int64_t &paid_units, int64_t orig_units, CAmount paid_unit_price);
bool SetAskFillamounts(CAmount unit_price, int64_t fill_assetoshis, int64_t orig_assetoshis, int64_t paid_nValue);
bool SetSwapFillamounts(CAmount unit_price, int64_t &paid, int64_t orig_nValue, int64_t &received, int64_t totalprice); // not implemented
int64_t AssetValidateBuyvin(struct CCcontract_info *cp, Eval* eval, int64_t &unit_price, std::vector<uint8_t> &origpubkey_out, char *origCCaddr_out, char *origaddr_out, const CTransaction &tx, uint256 refassetid);
int64_t AssetValidateSellvin(struct CCcontract_info *cp, Eval* eval, int64_t &unit_price, std::vector<uint8_t> &origpubkey_out, char *origCCaddr_out, char *origaddr_out, const CTransaction &tx, uint256 assetid);
//bool AssetsCalcAmounts(struct CCcontract_info *cpAssets, int64_t &inputs, int64_t &outputs, Eval* eval, const CTransaction &tx, uint256 assetid);
//bool AssetsGetCCInputs(struct CCcontract_info *cpAssets, CAmount &tokensInputs, int64_t &assetsInputs, Eval* eval, const CTransaction &tx, uint256 assetid);
CAmount AssetsGetCCInputs(struct CCcontract_info *cp, const char *addr, const CTransaction &tx);
//uint256 AssetsGetPrevOrdertxid(const CTransaction &tx);
//CAmount AssetsGetUnitPrice(uint256 ordertxid);

// CCassetstx
//int64_t GetAssetBalance(CPubKey pk,uint256 tokenid); // --> GetTokenBalance()
//int64_t AddAssetInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 assetid, int64_t total, int32_t maxinputs);

UniValue AssetOrders(uint256 tokenid, CPubKey pubkey, uint8_t additionalEvalCode);
//UniValue AssetInfo(uint256 tokenid);
//UniValue AssetList();
//std::string CreateAsset(int64_t txfee,int64_t assetsupply,std::string name,std::string description);
//std::string AssetTransfer(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total);
//std::string AssetConvert(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total,int32_t evalcode);

UniValue CreateBuyOffer(const CPubKey &mypk, int64_t txfee, int64_t bidamount, uint256 assetid, int64_t numtokens);
UniValue CancelBuyOffer(const CPubKey &mypk, int64_t txfee, uint256 assetid, uint256 bidtxid);
UniValue FillBuyOffer(const CPubKey &mypk, int64_t txfee, uint256 assetid, uint256 bidtxid, int64_t fill_units, CAmount paid_unit_price);
UniValue CreateSell(const CPubKey &mypk, int64_t txfee, int64_t numtokens, uint256 assetid, int64_t askamount);
std::string CreateSwap(int64_t txfee, int64_t askamount, uint256 assetid, uint256 assetid2, int64_t pricetotal);
UniValue CancelSell(const CPubKey &mypk, int64_t txfee, uint256 assetid, uint256 asktxid);
UniValue FillSell(const CPubKey &mypk, int64_t txfee, uint256 assetid, uint256 assetid2, uint256 asktxid, int64_t fillamount, CAmount paid_unit_price);

const char ccassets_log[] = "ccassets";


#endif
