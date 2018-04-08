#ifndef BETPROTOCOL_H
#define BETPROTOCOL_H

#include "cc/eval.h"
#include "pubkey.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "cryptoconditions/include/cryptoconditions.h"


class MoMProof
{
public:
    int nIndex;
    std::vector<uint256> branch;
    uint256 notarisationHash;

    MoMProof() {}
    MoMProof(int i, std::vector<uint256> b, uint256 n) : notarisationHash(n), nIndex(i), branch(b) {}
    uint256 Exec(uint256 hash) const { return CBlock::CheckMerkleBranch(hash, branch, nIndex); }

    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(nIndex));
        READWRITE(branch);
        READWRITE(notarisationHash);
    }
};


class BetProtocol
{
protected:
    std::vector<CC*> playerConditions();
public:
    EvalCode disputeCode;
    std::vector<CPubKey> players;
    std::vector<unsigned char> vmParams;
    uint32_t waitBlocks;

    // Utility
    BetProtocol(EvalCode dc, std::vector<CPubKey> ps, uint32_t wb, std::vector<uint8_t> vmp)
        : disputeCode(dc), waitBlocks(wb), vmParams(vmp), players(ps) {}
    std::vector<CC*> PlayerConditions();

    // on PANGEA
    CC* MakeDisputeCond();
    CMutableTransaction MakeSessionTx(CAmount spendFee);
    CMutableTransaction MakeDisputeTx(uint256 signedSessionTxHash, uint256 vmResultHash);
    CMutableTransaction MakePostEvidenceTx(uint256 signedSessionTxHash,
            int playerIndex, std::vector<unsigned char> state);

    // on KMD
    CC* MakePayoutCond(uint256 signedSessionTxHash);
    CMutableTransaction MakeStakeTx(CAmount totalPayout, uint256 signedSessionTx);
    CMutableTransaction MakeAgreePayoutTx(std::vector<CTxOut> payouts, uint256 signedStakeTxHash);
    CMutableTransaction MakeImportPayoutTx(std::vector<CTxOut> payouts,
            CTransaction signedDisputeTx, uint256 signedStakeTxHash, MoMProof momProof);
};



bool GetOpReturnHash(CScript script, uint256 &hash);


#endif /* BETPROTOCOL_H */
