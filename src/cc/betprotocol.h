#ifndef BETPROTOCOL_H
#define BETPROTOCOL_H

#include "pubkey.h"
#include "primitives/transaction.h"
#include "cryptoconditions/include/cryptoconditions.h"


#define ExecMerkle CBlock::CheckMerkleBranch


class MoMProof
{
public:
    int nIndex;
    std::vector<uint256> branch;
    uint256 notarisationHash;

    MoMProof() {}
    MoMProof(int i, std::vector<uint256> b, uint256 n) : notarisationHash(n), nIndex(i), branch(b) {}

    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(nIndex));
        READWRITE(branch);
        READWRITE(notarisationHash);
    }
};


class DisputeHeader
{
public:
    int waitBlocks;
    std::vector<unsigned char> vmParams;

    DisputeHeader() {}
    DisputeHeader(int w, std::vector<unsigned char> vmp) : waitBlocks(w), vmParams(vmp) {}
    
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(waitBlocks));
        READWRITE(vmParams);
    }
};


class BetProtocol
{
protected:
    char* disputeFunc = (char*) "DisputeBet";
    std::vector<CC*> playerConditions();
public:
    CAmount MINFEE = 1;
    std::vector<CPubKey> players;
    DisputeHeader disputeHeader;

    // Utility
    BetProtocol(std::vector<CPubKey> ps, DisputeHeader dh) : players(ps), disputeHeader(dh) {}
    std::vector<CC*> PlayerConditions();

    // on PANGEA
    CC* MakeDisputeCond();
    CMutableTransaction MakeSessionTx();
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


CC* CCNewSecp256k1(CPubKey k);


#endif /* BETPROTOCOL_H */
