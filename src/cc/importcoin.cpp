#include "cc/importcoin.h"
#include "coins.h"
#include "hash.h"
#include "script/cc.h"
#include "primitives/transaction.h"


/*
 * Generate ImportCoin transaction.
 *
 * Contains an empty OP_RETURN as first output; this is critical for preventing a double
 * import. If it doesn't contain this it's invalid. The empty OP_RETURN will hang around
 * in the UTXO set and the transaction will be detected as a duplicate.
 */
CTransaction MakeImportCoinTransaction(const MomoProof proof, const CTransaction burnTx, const std::vector<CTxOut> payouts)
{
    std::vector<uint8_t> payload =
        E_MARSHAL(ss << EVAL_IMPORTCOIN; ss << proof; ss << burnTx);
    CMutableTransaction mtx;
    mtx.vin.push_back(CTxIn(COutPoint(burnTx.GetHash(), 10e8), CScript() << payload));
    mtx.vout = payouts;
    return CTransaction(mtx);
}

CTxOut MakeBurnOutput(CAmount value, int targetChain, const std::vector<CTxOut> payouts)
{
    std::vector<uint8_t> opret = E_MARSHAL(ss << VARINT(targetChain); ss << SerializeHash(payouts));
    return CTxOut(value, CScript() << OP_RETURN << opret);
}


/*
 * CC Eval method for import coin.
 *
 * This method has to control *every* parameter of the ImportCoin transaction, so that the legal
 * importTx for a valid burnTx is 1:1. There can be no two legal importTx transactions for a burnTx
 * on another chain.
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
        NotarisationData data;
        if (!GetNotarisationData(proof.notarisationHeight, data, true))
            return Invalid("coudnt-load-momom");

        if (data.MoMoM != proof.branch.Exec(burnTx.GetHash()))
            return Invalid("momom-check-fail");
    }

    return Valid();
}


static bool UnmarshalImportTx(const CTransaction &importTx, MomoProof &proof, CTransaction &burnTx)
{
    CScript scriptSig = importTx.vin[0].scriptSig;
    auto pc = scriptSig.begin();
    opcodetype opcode;
    std::vector<uint8_t> evalScript;
    int code;
    bool out = false;
    if (scriptSig.GetOp(pc, opcode, evalScript))
        if (pc == scriptSig.end())
            out = E_UNMARSHAL(evalScript, ss >> VARINT(code); ss >> proof; ss >> burnTx);
    return code == EVAL_IMPORTCOIN && out;
}


/*
 * Required by main
 * TODO: test
 */
CAmount GetCoinImportValue(const CTransaction &tx)
{
    MomoProof proof;
    CTransaction burnTx;
    if (UnmarshalImportTx(tx, proof, burnTx)) {
        return burnTx.vout.size() ? burnTx.vout[0].nValue : 0;
    }
    return 0;
}


/*
 * CoinImport is different enough from normal script execution that it's not worth
 * making all the mods neccesary in the interpreter to do the dispatch correctly.
 */
bool VerifyCoinImport(const CScript& scriptSig, TransactionSignatureChecker& checker, CValidationState &state)
{
    auto pc = scriptSig.begin();
    opcodetype opcode;
    std::vector<uint8_t> evalScript;
    
    auto f = [&] () {
        if (!scriptSig.GetOp(pc, opcode, evalScript))
            return false;
        if (pc != scriptSig.end())
            return false;
        if (evalScript.size() == 0)
            return false;
        if (evalScript.begin()[0] != EVAL_IMPORTCOIN)
            return false;
        // Ok, all looks good so far...
        CC *cond = CCNewEval(evalScript);
        bool out = checker.CheckEvalCondition(cond);
        cc_free(cond);
        return out;
    };

    return f() ? true : state.Invalid(false, 0, "invalid-coin-import");
}


void AddImportTombstone(const CTransaction &importTx, CCoinsViewCache &inputs, int nHeight)
{
    uint256 burnHash = importTx.vin[0].prevout.hash;
    CCoinsModifier modifier = inputs.ModifyCoins(burnHash);
    modifier->nHeight = nHeight;
    modifier->nVersion = 1;
    modifier->vout.push_back(CTxOut(0, CScript() << OP_0));
}


void RemoveImportTombstone(const CTransaction &importTx, CCoinsViewCache &inputs)
{
    uint256 burnHash = importTx.vin[0].prevout.hash;
    inputs.ModifyCoins(burnHash)->Clear();
}


int ExistsImportTombstone(const CTransaction &importTx, const CCoinsViewCache &inputs)
{
    uint256 burnHash = importTx.vin[0].prevout.hash;
    return inputs.HaveCoins(burnHash);
}
