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

/// Returns non-fungible data of token if this is a NFT
/// @param tokenid id of token
/// @param vopretNonfungible non-fungible token data. The first byte is the evalcode of the contract that validates the NFT-data
void GetNonfungibleData(uint256 tokenid, vscript_t &vopretNonfungible);

// CCcustom
bool TokensValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);
bool TokensExactAmounts(bool goDeeper, struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, std::string &errorStr);
std::string CreateTokenLocal(int64_t txfee, int64_t tokensupply, std::string name, std::string description, vscript_t nonfungibleData);
UniValue CreateTokenExt(const CPubKey &remotepk, int64_t txfee, int64_t tokensupply, std::string name, std::string description, vscript_t nonfungibleData, uint8_t markerEvalCode, bool addTxInMemory);
std::string TokenTransfer(int64_t txfee, uint256 assetid, CPubKey destpk, int64_t total);
UniValue TokenTransferExt(const CPubKey &remotepk, int64_t txfee, uint256 tokenid, const char *tokenaddr, std::vector<std::pair<CC*, uint8_t*>> probeconds, std::vector<CPubKey> destpubkeys, int64_t total, bool useMempool = false);
//UniValue TokenTransferSpk(const CPubKey &remotepk, int64_t txfee, uint256 tokenid, const char *tokenaddr, std::vector<std::pair<CC*, uint8_t*>> probeconds, const CScript &spk, int64_t total, const std::vector<CPubKey> &voutPubkeys, bool useMempool = false);
CAmount HasBurnedTokensvouts(const CTransaction& tx, uint256 reftokenid);
CPubKey GetTokenOriginatorPubKey(CScript scriptPubKey);
bool IsTokenMarkerVout(CTxOut vout);

int64_t GetTokenBalance(CPubKey pk, uint256 tokenid, bool usemempool = false);
UniValue TokenInfo(uint256 tokenid);
UniValue TokenList();

UniValue TokenBeginTransferTx(CMutableTransaction &mtx, struct CCcontract_info *cp, const CPubKey &remotepk, CAmount txfee);
UniValue TokenAddTransferVout(CMutableTransaction &mtx, struct CCcontract_info *cp, const CPubKey &remotepk, uint256 tokenid, const char *tokenaddr, std::vector<CPubKey> destpubkeys, const std::pair<CC*, uint8_t*> &probecond, CAmount amount, bool useMempool);
UniValue TokenFinalizeTransferTx(CMutableTransaction &mtx, struct CCcontract_info *cp, const CPubKey &remotepk, CAmount txfee, const CScript &opret);

/// Adds token inputs to transaction object. If tokenid is a non-fungible token then the function will set additionalTokensEvalcode2 variable in the cp object to the eval code from NFT data to spend NFT outputs properly
/// @param cp CCcontract_info structure
/// @param mtx mutable transaction object
/// @param pk pubkey for whose token inputs to add
/// @param tokenid id of token which inputs to add
/// @param total amount to add (if total==0 no inputs are added and all available amount is returned)
/// @param maxinputs maximum number of inputs to add. If 0 then CC_MAXVINS define is used
//int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, const CPubKey &pk, uint256 tokenid, int64_t total, int32_t maxinputs, bool useMempool = false);

/// @private overload used in kogs
//int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, char *tokenaddr, uint256 tokenid, int64_t total, int32_t maxinputs, vscript_t &vopretNonfungible);

/// An overload that also returns NFT data in vopretNonfungible parameter
/// the rest parameters are the same as in the first AddTokenCCInputs overload
/// @see AddTokenCCInputs
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, const char *tokenaddr, uint256 tokenid, int64_t total, int32_t maxinputs, bool useMempool = false);

/// @private overload used in old cc
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, const CPubKey &pk, uint256 tokenid, int64_t total, int32_t maxinputs);

/// @private overload used in old assets
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, const CPubKey &pk, uint256 tokenid, int64_t total, int32_t maxinputs, vscript_t &vopretNonfungible);

/// Checks if a transaction vout is true token vout, for this check pubkeys and eval code in token opreturn are used to recreate vout and compare it with the checked vout.
/// Verifies that the transaction total token inputs value equals to total token outputs (that is, token balance is not changed in this transaction)
/// @param goDeeper also recursively checks the previous token transactions (or the creation transaction) and ensures token balance is not changed for them too
/// @param checkPubkeys always true
/// @param cp CCcontract_info structure initialized for EVAL_TOKENS eval code
/// @param eval could be NULL, if not NULL then the eval parameter is used to report validation error
/// @param tx transaction object to check
/// @param v vout number (starting from 0)
/// @param reftokenid id of the token. The vout is checked if it has this tokenid
/// @returns true if vout is true token with the reftokenid id
int64_t IsTokensvout(bool goDeeper, bool checkPubkeys, struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, int32_t v, uint256 reftokenid);

/// Creates a token cryptocondition that allows to spend it by one key
/// The resulting cc will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk pubkey to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeTokensCCcond1(uint8_t evalcode, CPubKey pk);

/// Overloaded function that creates a token cryptocondition that allows to spend it by one key
/// The resulting cc will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param evalcode2 yet another cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk pubkey to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeTokensCCcond1(uint8_t evalcode, uint8_t evalcode2, CPubKey pk);

/// Creates new 1of2 token cryptocondition that allows to spend it by either of two keys
/// Resulting vout will have three eval codes (EVAL_TOKENS, evalcode and evalcode2 parameter values).
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeTokensCCcond1of2(uint8_t evalcode, CPubKey pk1, CPubKey pk2);

/// Creates new 1of2 token cryptocondition that allows to spend it by either of two keys
/// The resulting cc will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param evalcode2 yet another cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeTokensCCcond1of2(uint8_t evalcode, uint8_t evalcode2, CPubKey pk1, CPubKey pk2);

/// Creates a token transaction output with a cryptocondition that allows to spend it by one key. 
/// The resulting vout will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk pubkey to spend the cc
/// @returns vout object
/// @see CCinit
/// @see CCcontract_info
CTxOut MakeTokensCC1vout(uint8_t evalcode, CAmount nValue, CPubKey pk, std::vector<std::vector<unsigned char>>* vData = nullptr);

/// Another MakeTokensCC1vout overloaded function that creates a token transaction output with a cryptocondition with two eval codes that allows to spend it by one key. 
/// Resulting vout will have three eval codes (EVAL_TOKENS, evalcode and evalcode2 parameter values).
/// The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param evalcode2 yet another cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk pubkey to spend the cc
/// @returns vout object
/// @see CCinit
/// @see CCcontract_info
CTxOut MakeTokensCC1vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk, std::vector<std::vector<unsigned char>>* vData = nullptr);

/// MakeTokensCC1of2vout creates a token transaction output with a 1of2 cryptocondition that allows to spend it by either of two keys. 
/// The resulting vout will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns vout object
/// @see CCinit
/// @see CCcontract_info
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, CAmount nValue, CPubKey pk1, CPubKey pk2, std::vector<std::vector<unsigned char>>* vData = nullptr);

/// Another overload of MakeTokensCC1of2vout creates a token transaction output with a 1of2 cryptocondition with two eval codes that allows to spend it by either of two keys. 
/// The resulting vout will have three eval codes (EVAL_TOKENS, evalcode and evalcode2 parameter values).
/// The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param evalcode2 yet another cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns vout object
/// @see CCinit
/// @see CCcontract_info
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk1, CPubKey pk2, std::vector<std::vector<unsigned char>>* vData = nullptr);

inline bool IsTokenCreateFuncid(uint8_t funcid) { return funcid == 'c'; }
inline bool IsTokenTransferFuncid(uint8_t funcid) { return funcid == 't'; }
bool MyGetCCopretV2(const CScript &scriptPubKey, CScript &opret);

bool IsEqualVouts(const CTxOut &v1, const CTxOut &v2);

bool TokensIsVer1Active(const Eval *eval);

const char cctokens_log[] = "cctokens";

#endif
