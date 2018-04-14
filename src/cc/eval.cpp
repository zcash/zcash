#include <assert.h>
#include <cryptoconditions.h>

#include "primitives/transaction.h"
#include "script/cc.h"
#include "cc/eval.h"
#include "main.h"
#include "chain.h"
#include "core_io.h"


Eval* EVAL_TEST = 0;


bool RunCCEval(const CC *cond, const CTransaction &tx, unsigned int nIn)
{
    Eval eval_;
    Eval *eval = EVAL_TEST;
    if (!eval) eval = &eval_;

    bool out = eval->Dispatch(cond, tx, nIn);
    assert(eval->state.IsValid() == out);

    if (eval->state.IsValid()) return true;

    std::string lvl = eval->state.IsInvalid() ? "Invalid" : "Error!";
    fprintf(stderr, "CC Eval %s %s: %s spending tx %s\n",
            EvalToStr(cond->code[0]).data(),
            lvl.data(),
            eval->state.GetRejectReason().data(),
            tx.vin[nIn].prevout.hash.GetHex().data());
    if (eval->state.IsError()) fprintf(stderr, "Culprit: %s\n", EncodeHexTx(tx).data());
    return false;
}


/*
 * Test the validity of an Eval node
 */
bool Eval::Dispatch(const CC *cond, const CTransaction &txTo, unsigned int nIn)
{
    if (cond->codeLength == 0)
        return Invalid("empty-eval");

    uint8_t ecode = cond->code[0];
    std::vector<uint8_t> vparams(cond->code+1, cond->code+cond->codeLength);

    if (ecode == EVAL_IMPORTPAYOUT) {
        return ImportPayout(vparams, txTo, nIn);
    }

    return Invalid("invalid-code");
}


bool Eval::GetSpendsConfirmed(uint256 hash, std::vector<CTransaction> &spends) const
{
    // NOT IMPLEMENTED
    return false;
}


bool Eval::GetTxUnconfirmed(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock) const
{
    bool fAllowSlow = false; // Don't allow slow
    return GetTransaction(hash, txOut, hashBlock, fAllowSlow);
}


bool Eval::GetTxConfirmed(const uint256 &hash, CTransaction &txOut, CBlockIndex &block) const
{
    uint256 hashBlock;
    if (!GetTxUnconfirmed(hash, txOut, hashBlock))
        return false;
    if (hashBlock.IsNull() || !GetBlock(hashBlock, block))
        return false;
    return true;
}


unsigned int Eval::GetCurrentHeight() const
{
    return chainActive.Height();
}


bool Eval::GetBlock(uint256 hash, CBlockIndex& blockIdx) const
{
    auto r = mapBlockIndex.find(hash);
    if (r != mapBlockIndex.end()) {
        blockIdx = *r->second;
        return true;
    }
    fprintf(stderr, "CC Eval Error: Can't get block from index\n");
    return false;
}


extern int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);


int32_t Eval::GetNotaries(uint8_t pubkeys[64][33], int32_t height, uint32_t timestamp) const
{
    return komodo_notaries(pubkeys, height, timestamp);
}


bool Eval::CheckNotaryInputs(const CTransaction &tx, uint32_t height, uint32_t timestamp) const
{
    if (tx.vin.size() < 11) return false;

    uint8_t seenNotaries[64] = {0};
    uint8_t notaries[64][33];
    int nNotaries = GetNotaries(notaries, height, timestamp);

    BOOST_FOREACH(const CTxIn &txIn, tx.vin)
    {
        // Get notary pubkey
        CTransaction tx;
        uint256 hashBlock;
        if (!GetTxUnconfirmed(txIn.prevout.hash, tx, hashBlock)) return false;
        if (tx.vout.size() < txIn.prevout.n) return false;
        CScript spk = tx.vout[txIn.prevout.n].scriptPubKey;
        if (spk.size() != 35) return false;
        const unsigned char *pk = spk.data();
        if (pk++[0] != 33) return false;
        if (pk[33] != OP_CHECKSIG) return false;

        // Check it's a notary
        for (int i=0; i<nNotaries; i++) {
            if (!seenNotaries[i]) {
                if (memcmp(pk, notaries[i], 33) == 0) {
                    seenNotaries[i] = 1;
                    goto found;
                }
            }
        }
        return false;
        found:;
    }

    return true;
}


/*
 * Get MoM from a notarisation tx hash
 */
bool Eval::GetNotarisationData(const uint256 notaryHash, NotarisationData &data) const
{
    CTransaction notarisationTx;
    CBlockIndex block;
    if (!GetTxConfirmed(notaryHash, notarisationTx, block)) return false;
    if (!CheckNotaryInputs(notarisationTx, block.nHeight, block.nTime)) return false;
    if (notarisationTx.vout.size() < 2) return false;
    if (!data.Parse(notarisationTx.vout[1].scriptPubKey)) return false;
    return true;
}


/*
 * Notarisation data, ie, OP_RETURN payload in notarisation transactions
 */
extern char ASSETCHAINS_SYMBOL[16];

bool NotarisationData::Parse(const CScript scriptPK)
{
    *this = NotarisationData();

    std::vector<unsigned char> vdata;
    if (!GetOpReturnData(scriptPK, vdata)) return false;

    CDataStream ss(vdata, SER_NETWORK, PROTOCOL_VERSION);

    try {
        ss >> blockHash;
        ss >> height;
        if (ASSETCHAINS_SYMBOL[0])
            ss >> txHash;

        char *nullPos = (char*) memchr(&ss[0], 0, ss.size());
        if (!nullPos) return false;
        ss.read(symbol, nullPos-&ss[0]+1);

        if (ss.size() < 36) return false;
        ss >> MoM;
        ss >> MoMDepth;
    } catch (...) {
        return false;
    }
    return true;
}


/*
 * Misc
 */

std::string EvalToStr(EvalCode c)
{
    FOREACH_EVAL(EVAL_GENERATE_STRING);
    char s[10];
    sprintf(s, "0x%x", c);
    return std::string(s);
}
