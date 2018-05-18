#ifndef IMPORTCOIN_H
#define IMPORTCOIN_H

#include "cc/eval.h"
#include "coins.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include <cryptoconditions.h>


CAmount GetCoinImportValue(const CTransaction &tx);

CTransaction MakeImportCoinTransaction(const TxProof proof,
        const CTransaction burnTx, const std::vector<CTxOut> payouts);

CTxOut MakeBurnOutput(CAmount value, int targetChain, const std::vector<CTxOut> payouts);

bool VerifyCoinImport(const CScript& scriptSig,
    TransactionSignatureChecker& checker, CValidationState &state);


void AddImportTombstone(const CTransaction &importTx, CCoinsViewCache &inputs, int nHeight);
void RemoveImportTombstone(const CTransaction &importTx, CCoinsViewCache &inputs);
int ExistsImportTombstone(const CTransaction &importTx, const CCoinsViewCache &inputs);

#endif /* IMPORTCOIN_H */
