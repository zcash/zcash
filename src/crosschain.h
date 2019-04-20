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

#ifndef CROSSCHAIN_H
#define CROSSCHAIN_H

#include "cc/eval.h"

const int CROSSCHAIN_KOMODO = 1;
const int CROSSCHAIN_TXSCL = 2;
const int CROSSCHAIN_STAKED = 3;

typedef struct CrosschainAuthority {
    uint8_t notaries[64][33];
    int8_t size;
    int8_t requiredSigs;
} CrosschainAuthority;

int GetSymbolAuthority(const char* symbol);
bool CheckTxAuthority(const CTransaction &tx, CrosschainAuthority auth);

/* On assetchain */
TxProof GetAssetchainProof(uint256 hash,CTransaction burnTx);

/* On KMD */
uint256 CalculateProofRoot(const char* symbol, uint32_t targetCCid, int kmdHeight,
        std::vector<uint256> &moms, uint256 &destNotarisationTxid);
TxProof GetCrossChainProof(const uint256 txid, const char* targetSymbol, uint32_t targetCCid,
        const TxProof assetChainProof,int32_t offset);
void CompleteImportTransaction(CTransaction &importTx,int32_t offset);

/* On assetchain */
bool CheckMoMoM(uint256 kmdNotarisationHash, uint256 momom);
bool CheckNotariesApproval(uint256 burntxid, const std::vector<uint256> & notaryTxids);

#endif /* CROSSCHAIN_H */
