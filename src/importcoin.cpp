#include "crosschain.h"
#include "importcoin.h"
#include "cc/utils.h"
#include "coins.h"
#include "hash.h"
#include "script/cc.h"
#include "primitives/transaction.h"


CTransaction MakeImportCoinTransaction(const TxProof proof, const CTransaction burnTx, const std::vector<CTxOut> payouts)
{
    std::vector<uint8_t> payload = E_MARSHAL(ss << EVAL_IMPORTCOIN);
    CMutableTransaction mtx;
    mtx.vin.push_back(CTxIn(COutPoint(burnTx.GetHash(), 10e8), CScript() << payload));
    mtx.vout = payouts;
    auto importData = E_MARSHAL(ss << proof; ss << burnTx);
    mtx.vout.insert(mtx.vout.begin(), CTxOut(0, CScript() << OP_RETURN << importData));
    return CTransaction(mtx);
}


CTxOut MakeBurnOutput(CAmount value, uint32_t targetCCid, std::string targetSymbol, const std::vector<CTxOut> payouts)
{
    std::vector<uint8_t> opret = E_MARSHAL(ss << VARINT(targetCCid);
                                           ss << targetSymbol;
                                           ss << SerializeHash(payouts));
    return CTxOut(value, CScript() << OP_RETURN << opret);
}


bool UnmarshalImportTx(const CTransaction &importTx, TxProof &proof, CTransaction &burnTx,
        std::vector<CTxOut> &payouts)
{
    std::vector<uint8_t> vData;
    GetOpReturnData(importTx.vout[0].scriptPubKey, vData);
    if (importTx.vout.size() < 1) return false;
    payouts = std::vector<CTxOut>(importTx.vout.begin()+1, importTx.vout.end());
    return importTx.vin.size() == 1 &&
           importTx.vin[0].scriptSig == (CScript() << E_MARSHAL(ss << EVAL_IMPORTCOIN)) &&
           E_UNMARSHAL(vData, ss >> proof; ss >> burnTx);
}


bool UnmarshalBurnTx(const CTransaction &burnTx, std::string &targetSymbol, uint32_t *targetCCid, uint256 &payoutsHash)
{
    std::vector<uint8_t> burnOpret;
    if (burnTx.vout.size() == 0) return false;
    GetOpReturnData(burnTx.vout.back().scriptPubKey, burnOpret);
    return E_UNMARSHAL(burnOpret, ss >> VARINT(*targetCCid);
                                  ss >> targetSymbol; 
                                  ss >> payoutsHash);
}


/*
 * Required by main
 */
CAmount GetCoinImportValue(const CTransaction &tx)
{
    TxProof proof;
    CTransaction burnTx;
    std::vector<CTxOut> payouts;
    if (UnmarshalImportTx(tx, proof, burnTx, payouts)) {
        return burnTx.vout.size() ? burnTx.vout.back().nValue : 0;
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
