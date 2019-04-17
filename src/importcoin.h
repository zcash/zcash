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

#ifndef IMPORTCOIN_H
#define IMPORTCOIN_H

#include "cc/eval.h"
#include "coins.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include <cryptoconditions.h>


CAmount GetCoinImportValue(const CTransaction &tx);

CTransaction MakeImportCoinTransaction(const TxProof proof,
        const CTransaction burnTx, const std::vector<CTxOut> payouts, uint32_t nExpiryHeightOverride = 0);

CTxOut MakeBurnOutput(CAmount value, uint32_t targetCCid, std::string targetSymbol, const std::vector<CTxOut> payouts,std::vector<uint8_t> rawproof);
CTxOut MakeBurnOutput(CAmount value, uint32_t targetCCid, std::string targetSymbol, const std::vector<CTxOut> payouts,std::vector<uint8_t> rawproof,
                        uint256 bindtxid,std::vector<CPubKey> publishers,std::vector<uint256>txids,int32_t height,int32_t burnvout,std::string rawburntx,CPubKey destpub);
CTxOut MakeBurnOutput(CAmount value, uint32_t targetCCid, std::string targetSymbol, const std::vector<CTxOut> payouts,std::vector<uint8_t> rawproof,std::string srcaddr,
                        std::string receipt);

bool UnmarshalBurnTx(const CTransaction burnTx, std::string &targetSymbol, uint32_t *targetCCid, uint256 &payoutsHash,std::vector<uint8_t> &rawproof);
bool UnmarshalBurnTx(const CTransaction burnTx, std::string &srcaddr, std::string &receipt);
bool UnmarshalBurnTx(const CTransaction burnTx,uint256 &bindtxid,std::vector<CPubKey> &publishers,std::vector<uint256> &txids,int32_t &height,int32_t &burnvout,std::string &rawburntx,CPubKey &destpub);
bool UnmarshalImportTx(const CTransaction importTx, TxProof &proof, CTransaction &burnTx,std::vector<CTxOut> &payouts);

bool VerifyCoinImport(const CScript& scriptSig, TransactionSignatureChecker& checker, CValidationState &state);

void AddImportTombstone(const CTransaction &importTx, CCoinsViewCache &inputs, int nHeight);
void RemoveImportTombstone(const CTransaction &importTx, CCoinsViewCache &inputs);
int ExistsImportTombstone(const CTransaction &importTx, const CCoinsViewCache &inputs);

#endif /* IMPORTCOIN_H */
