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
    if (importTx.vout.size() == 0) return Invalid("no-vouts");

    // params
    MomoProof proof;
    CTransaction burnTx;
    if (!E_UNMARSHAL(params, ss >> proof; ss >> burnTx))
        return Invalid("invalid-params");
    
    // Control all aspects of this transaction
    // It must not be at all malleable
    if (MakeImportCoinTransaction(proof, burnTx, importTx.vout).GetHash() != importTx.GetHash())
        return Invalid("non-canonical");

    // burn params
    uint32_t chain; // todo
    uint256 payoutsHash;
    std::vector<uint8_t> burnOpret;
    if (burnTx.vout.size() == 0) return Invalid("invalid-burn-outputs");
    GetOpReturnData(burnTx.vout[0].scriptPubKey, burnOpret);
    if (!E_UNMARSHAL(burnOpret, ss >> VARINT(chain); ss >> payoutsHash))
        return Invalid("invalid-burn-params");

    // check chain
    if (chain != GetCurrentLedgerID())
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
    if (payoutsHash != SerializeHash(importTx.vout))
        return Invalid("wrong-payouts");

    // Check proof confirms existance of burnTx
    {
        NotarisationData data(1);
        if (!GetNotarisationData(proof.notarisationHeight, data, true))
            return Invalid("coudnt-load-momom");

        if (data.MoMoM != proof.branch.Exec(burnTx.GetHash()))
            return Invalid("momom-check-fail");
    }

    return Valid();
}


