#include "primitives/transaction.h"
#include "streams.h"
#include "chain.h"
#include "main.h"
#include "cc/eval.h"
#include "cryptoconditions/include/cryptoconditions.h"


class MomProof
{
public:
    uint256 notaryHash;
    int nPos; // Position of imported tx in MoM
    std::vector<uint256> branch;

    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(notaryHash);
        READWRITE(VARINT(nPos));
        READWRITE(branch);
    }
};


extern int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);

bool DerefNotaryPubkey(const COutPoint &prevout, char *pk33)
{
    CTransaction tx;
    uint256 hashBlock;
    if (!GetTransaction(prevout.hash, tx, hashBlock, false)) return false;
    if (tx.vout.size() < prevout.n) return false;
    const unsigned char *script = tx.vout[prevout.n].scriptPubKey.data();
    if (script[0] != 33) return false;
    memcpy(pk33, script+1, 33);
    return true;
}

bool CheckNotaryInputs(const CTransaction &tx, uint32_t height, uint32_t timestamp)
{
    if (tx.vin.size() < 11) return false;

    uint8_t notaries[64][33];
    uint8_t seenNotaries[64];
    int nNotaries = komodo_notaries(notaries, height, timestamp);
    char *pk;

    BOOST_FOREACH(const CTxIn &txIn, tx.vin)
    {
        if (!DerefNotaryPubkey(txIn.prevout, pk)) return false;

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
bool GetMoM(const uint256 notaryHash, uint256 &mom)
{
    CTransaction notarisationTx;
    uint256 notarisationBlock;
    if (!GetTransaction(notaryHash, notarisationTx, notarisationBlock, true)) return 0;
    CBlockIndex* blockindex = mapBlockIndex[notarisationBlock];
    if (!CheckNotaryInputs(notarisationTx, blockindex->nHeight, blockindex->nTime)) {
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


uint256 ExecMerkle(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    return CBlock::CheckMerkleBranch(hash, vMerkleBranch, nIndex);
}


/*
 * Crypto-Condition EVAL method that verifies a payout against a transaction
 * notarised on another chain.
 *
 * IN: cond - CC EVAL node
 * IN: payoutTx - Payout transaction on value chain (KMD)
 * IN: nIn  - index of input of stake
 *
 * payoutTx: Spends stakeTx with payouts from asset chain
 *
 *   in  0:      Spends Stake TX and contains ImportPayout CC
 *   out 0:      OP_RETURN MomProof
 *   out 1:      OP_RETURN serialized disputeTx from other chain
 *   out 2-:     arbitrary payouts
 *
 * disputeTx: Spends sessionTx.0 (opener on asset chain)
 *
 *   in 0:       spends sessionTx.0
 *   in 1-:      anything
 *   out 0:      OP_RETURN hash of payouts
 *   out 1-:     anything
 */
bool CheckImportPayout(const CC *cond, const CTransaction *payoutTx, int nIn)
{
    // TODO: Error messages!
    if (payoutTx->vin.size() != 1) return 0;
    if (payoutTx->vout.size() < 2) return 0;

    // Get hash of payouts
    std::vector<CTxOut> payouts(payoutTx->vout.begin() + 2, payoutTx->vout.end());
    uint256 payoutsHash = SerializeHash(payouts);
    std::vector<unsigned char> vPayoutsHash(payoutsHash.begin(), payoutsHash.end());

    // load disputeTx from vout[1]
    CTransaction disputeTx;
    {
        std::vector<unsigned char> exportData;
        if (!GetOpReturnData(payoutTx->vout[1].scriptPubKey, exportData)) return 0;
        CDataStream(exportData, SER_DISK, PROTOCOL_VERSION) >> disputeTx;
        // TODO: end of stream? exception?
    }

    // Check disputeTx.0 is vPayoutsHash
    std::vector<unsigned char> exportPayoutsHash;
    if (!GetOpReturnData(disputeTx.vout[0].scriptPubKey, exportPayoutsHash)) return 0;
    if (exportPayoutsHash != vPayoutsHash) return 0; 

    // Check disputeTx spends sessionTx.0
    // condition ImportPayout params is session ID from other chain
    {
        if (cond->paramsBinLength != 32) return 0;
        COutPoint prevout = disputeTx.vin[0].prevout;
        if (memcmp(prevout.hash.begin(), cond->paramsBin, 32) != 0 ||
                   prevout.n != 0) return 0;
    }

    // Check disputeTx solves momproof from vout[0]
    {
        std::vector<unsigned char> vchMomProof;
        if (!GetOpReturnData(payoutTx->vout[0].scriptPubKey, vchMomProof)) return 0;
        
        MomProof momProof;
        CDataStream(vchMomProof, SER_DISK, PROTOCOL_VERSION) >> momProof;
        
        uint256 mom;
        if (!GetMoM(momProof.notaryHash, mom)) return 0;

        uint256 proofResult = ExecMerkle(disputeTx.GetHash(), momProof.branch, momProof.nPos);
        if (proofResult != mom) return 0;
    }

    return 1;
}
