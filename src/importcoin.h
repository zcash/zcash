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

enum ProofKind : uint8_t {
    PROOF_NONE = 0x00,
    PROOF_MERKLEBRANCH = 0x11,
    PROOF_NOTARYTXIDS = 0x12,
    PROOF_MERKLEBLOCK = 0x13
};

class ImportProof {

private:
    uint8_t proofKind;
    TxProof proofBranch;
    std::vector<uint256> notaryTxids;
    std::vector<uint8_t> proofBlock;

public:
    ImportProof() { proofKind = PROOF_NONE; }
    ImportProof(const TxProof &_proofBranch) {
        proofKind = PROOF_MERKLEBRANCH; proofBranch = _proofBranch;
    }
    ImportProof(const std::vector<uint256> &_notaryTxids) {
        proofKind = PROOF_NOTARYTXIDS; notaryTxids = _notaryTxids;
    }
    ImportProof(const std::vector<uint8_t> &_proofBlock) {
        proofKind = PROOF_MERKLEBLOCK; proofBlock = _proofBlock;
    }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(proofKind);
        if (proofKind == PROOF_MERKLEBRANCH)
            READWRITE(proofBranch);
        else if (proofKind == PROOF_NOTARYTXIDS)
            READWRITE(notaryTxids);
        else if (proofKind == PROOF_MERKLEBLOCK)
            READWRITE(proofBlock);
        else
            proofKind = PROOF_NONE;  // if we have read some trash
    }

    bool IsMerkleBranch(TxProof &_proofBranch) const {
        if (proofKind == PROOF_MERKLEBRANCH) {
            _proofBranch = proofBranch;
            return true;
        }
        else
            return false;
    }
    bool IsNotaryTxids(std::vector<uint256> &_notaryTxids) const {
        if (proofKind == PROOF_NOTARYTXIDS) {
            _notaryTxids = notaryTxids;
            return true;
        }
        else
            return false;
    }
    bool IsMerkleBlock(std::vector<uint8_t> &_proofBlock) const {
        if (proofKind == PROOF_MERKLEBLOCK) {
            _proofBlock = proofBlock;
            return true;
        }
        else
            return false;
    }
};



CAmount GetCoinImportValue(const CTransaction &tx);

CTransaction MakeImportCoinTransaction(const ImportProof proof, const CTransaction burnTx, const std::vector<CTxOut> payouts, uint32_t nExpiryHeightOverride = 0);
CTransaction MakePegsImportCoinTransaction(const ImportProof proof, const CTransaction burnTx, const std::vector<CTxOut> payouts, uint32_t nExpiryHeightOverride = 0);

CTxOut MakeBurnOutput(CAmount value, uint32_t targetCCid, const std::string targetSymbol, const std::vector<CTxOut> payouts, const std::vector<uint8_t> rawproof);
CTxOut MakeBurnOutput(CAmount value, uint32_t targetCCid, std::string targetSymbol, const std::vector<CTxOut> payouts,std::vector<uint8_t> rawproof,
                        uint256 bindtxid,std::vector<CPubKey> publishers,std::vector<uint256>txids,uint256 burntxid,int32_t height,int32_t burnvout,std::string rawburntx,CPubKey destpub, int64_t amount);
CTxOut MakeBurnOutput(CAmount value, uint32_t targetCCid, std::string targetSymbol, const std::vector<CTxOut> payouts,std::vector<uint8_t> rawproof,std::string srcaddr,
                        std::string receipt);
CTxOut MakeBurnOutput(CAmount value,uint32_t targetCCid,std::string targetSymbol,const std::vector<CTxOut> payouts,std::vector<uint8_t> rawproof,uint256 pegstxid,
                        uint256 tokenid,CPubKey srcpub,int64_t amount,std::pair<int64_t,int64_t> account);

bool UnmarshalBurnTx(const CTransaction burnTx, std::string &targetSymbol, uint32_t *targetCCid, uint256 &payoutsHash,std::vector<uint8_t> &rawproof);
bool UnmarshalBurnTx(const CTransaction burnTx, std::string &srcaddr, std::string &receipt);
bool UnmarshalBurnTx(const CTransaction burnTx,uint256 &bindtxid,std::vector<CPubKey> &publishers,std::vector<uint256> &txids,uint256& burntxid,int32_t &height,int32_t &burnvout,std::string &rawburntx,CPubKey &destpub, int64_t &amount);
bool UnmarshalBurnTx(const CTransaction burnTx,uint256 &pegstxid,uint256 &tokenid,CPubKey &srcpub,int64_t &amount,std::pair<int64_t,int64_t> &account);
bool UnmarshalImportTx(const CTransaction importTx, ImportProof &proof, CTransaction &burnTx,std::vector<CTxOut> &payouts);

bool VerifyCoinImport(const CScript& scriptSig, TransactionSignatureChecker& checker, CValidationState &state);

void AddImportTombstone(const CTransaction &importTx, CCoinsViewCache &inputs, int nHeight);
void RemoveImportTombstone(const CTransaction &importTx, CCoinsViewCache &inputs);
int ExistsImportTombstone(const CTransaction &importTx, const CCoinsViewCache &inputs);

bool CheckVinPubKey(const CTransaction &sourcetx, int32_t i, uint8_t pubkey33[33]);

CMutableTransaction MakeSelfImportSourceTx(CTxDestination &dest, int64_t amount);
int32_t GetSelfimportProof(const CMutableTransaction sourceMtx, CMutableTransaction &templateMtx, ImportProof &proofNull);

#endif /* IMPORTCOIN_H */
