#include <assert.h>
#include <cryptoconditions.h>

#include "primitives/transaction.h"
#include "komodo_cc.h"
#include "cc/eval.h"
#include "main.h"
#include "chain.h"


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
    fprintf(stderr, "CC Eval %s %s: %s in tx %s\n", lvl.data(), cond->method,
            eval->state.GetRejectReason().data(), tx.GetHash().GetHex().data());
    return false;
}


/*
 * Test the validity of an Eval node
 */
bool Eval::Dispatch(const CC *cond, const CTransaction &txTo, unsigned int nIn)
{
    if (strcmp(cond->method, "TestEval") == 0) {
        bool valid = cond->paramsBinLength == 8 && memcmp(cond->paramsBin, "TestEval", 8) == 0;
        return valid ? Valid() : Invalid("testing");
    }

    if (strcmp(cond->method, "ImportPayout") == 0) {
        return ImportPayout(cond, txTo, nIn);
    }

    /* Example of how you might call DisputePayout
    if (strcmp(ASSETCHAINS_SYMBOL, "PANGEA") == 0) {
        if (strcmp(cond->method, "DisputePoker") == 0) {
            return DisputePayout(PokerVM(), cond, txTo, nIn);
        }
    }
    */

    return Invalid("no-such-method");
}


bool Eval::GetSpends(uint256 hash, std::vector<CTransaction> &spends) const
{
    // NOT IMPLEMENTED
    return false;
}


bool Eval::GetTx(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock, bool fAllowSlow) const
{
    return GetTransaction(hash, txOut, hashBlock, fAllowSlow);
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
    return false;
}


extern int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);


bool Eval::CheckNotaryInputs(const CTransaction &tx, uint32_t height, uint32_t timestamp) const
{
    if (tx.vin.size() < 11) return false;

    uint8_t notaries[64][33];
    uint8_t seenNotaries[64];
    int nNotaries = komodo_notaries(notaries, height, timestamp);
    char pk[33];

    BOOST_FOREACH(const CTxIn &txIn, tx.vin)
    {
        // Get notary pubkey
        CTransaction tx;
        uint256 hashBlock;
        if (!GetTx(txIn.prevout.hash, tx, hashBlock, false)) return false;
        if (tx.vout.size() < txIn.prevout.n) return false;
        const unsigned char *script = tx.vout[txIn.prevout.n].scriptPubKey.data();
        if (script[0] != 33) return false;
        memcpy(pk, script+1, 33);
        return true;

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
}


/*
 * Get MoM from a notarisation tx hash
 */
bool Eval::GetMoM(const uint256 notaryHash, uint256 &mom) const
{
    CTransaction notarisationTx;
    uint256 notarisationBlock;
    if (!GetTx(notaryHash, notarisationTx, notarisationBlock, true)) return 0;
    CBlockIndex block;
    if (!GetBlock(notarisationBlock, block)) return 0;
    if (!CheckNotaryInputs(notarisationTx, block.nHeight, block.nTime)) {
        return false;
    }
    if (!notarisationTx.vout.size() < 1) return 0;
    std::vector<unsigned char> opret;
    if (!GetOpReturnData(notarisationTx.vout[0].scriptPubKey, opret)) return 0;
    if (opret.size() < 36) return 0;  // In reality it is more than 36, but at the moment I 
                                      // only know where it is relative to the end, and this
                                      // is enough to prevent a memory fault. In the case that
                                      // the assumption about the presence of a MoM at this
                                      // offset fails, we will return random other data that is
                                      // not more likely to generate a false positive.
    memcpy(mom.begin(), opret.data()+opret.size()-36, 32);
    return 1;
}
