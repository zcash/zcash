#include <assert.h>
#include <cryptoconditions.h>

#include "primitives/transaction.h"
#include "komodo_cc.h"
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
    fprintf(stderr, "CC Eval %s %s: %s spending tx %s\n", lvl.data(), cond->method,
            eval->state.GetRejectReason().data(), tx.vin[nIn].prevout.hash.GetHex().data());
    if (eval->state.IsError()) fprintf(stderr, "Culprit: %s\n", EncodeHexTx(tx).data());
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
        if (!GetTx(txIn.prevout.hash, tx, hashBlock, false)) return false;
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
 * Get MoM from a notarisation tx hash
 */
bool Eval::GetNotarisationData(const uint256 notaryHash, NotarisationData &data) const
{
    CTransaction notarisationTx;
    uint256 notarisationBlock;
    if (!GetTx(notaryHash, notarisationTx, notarisationBlock, true)) return false;
    CBlockIndex block;
    if (!GetBlock(notarisationBlock, block)) return false;
    if (!CheckNotaryInputs(notarisationTx, block.nHeight, block.nTime)) return false;
    if (notarisationTx.vout.size() < 2) return false;
    if (!data.Parse(notarisationTx.vout[1].scriptPubKey)) return false;
    return true;
}
