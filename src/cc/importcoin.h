#ifndef CC_IMPORTCOIN_H
#define CC_IMPORTCOIN_H

#include "cc/eval.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include <cryptoconditions.h>


class MomoProof
{
public:
    MerkleBranch branch;
    int notarisationHeight;
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(branch);
        READWRITE(notarisationHeight);
    }
};

CAmount GetCoinImportValue(const CTransaction &tx);

CTransaction MakeImportCoinTransaction(const MomoProof proof,
        const CTransaction burnTx, const std::vector<CTxOut> payouts);

CTxOut MakeBurnOutput(CAmount value, int targetChain, const std::vector<CTxOut> payouts);

bool VerifyCoinImport(const CScript& scriptSig,
    TransactionSignatureChecker& checker, CValidationState &state);

#endif /* CC_IMPORTCOIN_H */
