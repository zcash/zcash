#include "cc/eval.h"
#include "cc/utils.h"
#include "importcoin.h"
#include "primitives/transaction.h"


/*
 * CC Eval method for import coin.
 *
 * This method should control every parameter of the ImportCoin transaction, since it has no signature
 * to protect it from malleability.
 */
bool Eval::ImportCoin(const std::vector<uint8_t> params, const CTransaction &importTx, unsigned int nIn)
{
    if (importTx.vout.size() < 2)
        return Invalid("too-few-vouts");

    // params
    TxProof proof;
    CTransaction burnTx;
    std::vector<CTxOut> payouts;

    if (!UnmarshalImportTx(importTx, proof, burnTx, payouts))
        return Invalid("invalid-params");
    
    // Control all aspects of this transaction
    // It should not be at all malleable
    if (MakeImportCoinTransaction(proof, burnTx, payouts).GetHash() != importTx.GetHash())
        return Invalid("non-canonical");

    // burn params
    uint32_t targetCcid;
    std::string targetSymbol;
    uint256 payoutsHash;

    if (!UnmarshalBurnTx(burnTx, targetSymbol, &targetCcid, payoutsHash))
        return Invalid("invalid-burn-tx");

    if (targetCcid != GetAssetchainsCC() || targetSymbol != GetAssetchainsSymbol())
        return Invalid("importcoin-wrong-chain");

    // check burn amount
    {
        uint64_t burnAmount = burnTx.vout[0].nValue;
        if (burnAmount == 0)
            return Invalid("invalid-burn-amount");
        uint64_t totalOut = 0;
        for (int i=0; i<importTx.vout.size(); i++)
            totalOut += importTx.vout[i].nValue;
        if (totalOut > burnAmount)
            return Invalid("payout-too-high");
    }

    // Check burntx shows correct outputs hash
    if (payoutsHash != SerializeHash(payouts))
        return Invalid("wrong-payouts");

    // Check proof confirms existance of burnTx
    {
        uint256 momom, target;
        if (!GetProofRoot(proof.first, momom))
            return Invalid("coudnt-load-momom");

        target = proof.second.Exec(burnTx.GetHash());
        if (momom != proof.second.Exec(burnTx.GetHash()))
            return Invalid("momom-check-fail");
    }

    return Valid();
}


