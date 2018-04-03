#include <cryptoconditions.h>

#include "primitives/transaction.h"


class DisputeHeader
{
public:
    int waitBlocks;
    std::vector<unsigned char> vmParams;
    
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(waitBlocks));
        READWRITE(vmParams);
    }
};


static CScript CCPubKey(const CC *cond)
{
    unsigned char buf[1000];
    size_t len = cc_conditionBinary(cond, buf);
    return CScript() << std::vector<unsigned char>(buf, buf+len) << OP_CHECKCRYPTOCONDITION;
}


static CScript CCSig(const CC *cond)
{
    unsigned char buf[1000];
    size_t len = cc_fulfillmentBinary(cond, buf, 1000);
    auto ffill = std::vector<unsigned char>(buf, buf+len);
    ffill.push_back(SIGHASH_ALL);
    return CScript() << ffill;
}


static unsigned char* CopyPubKey(CPubKey pkIn)
{
    auto *pk = malloc(33);
    memcpy(pk, pkIn.begin(), 33);  // TODO: compressed?
    return pk;
}


class PokerProtocol
{
public:
    CAmount MINFEE = 1;
    std::vector<CPubKey> players;
    DisputeHeader disputeHeader;

    // on PANGEA
    CC* MakeDisputeCond();
    CMutableTransaction MakeSessionTx(CTxIn dealerInput);
    CMutableTransaction MakeDisputeTx(uint256 signedSessionTxHash);
    CMutableTransaction MakePostEvidenceTx(uint256 signedSessionTxHash,
            CPubKey playerKey, std::vector<unsigned char> state);

    // on KMD
    CC* MakePayoutCond();
    CMutableTransaction MakeStakeTx(CAmount totalPayout, std::vector<CTxIn> playerInputs,
            uint256 signedSessionTx);
    CMutableTransaction MakeAgreePayoutTx(std::vector<CTxOut> payouts,
            CC *signedPayoutCond, uint256 signedStakeTxHash);
    CMutableTransaction MakeImportPayoutTx(std::vector<CTxOut> payouts,
            CC *signedPayoutCond, CTransaction signedDisputeTx);
}


CC* PokerProtocol::MakeDisputeCond()
{
    CC *disputePoker = cond->subconditions[0] = cc_new(CC_Eval);
    char err[1000];
    std::vector<unsigned char> headerData;
    disputeHeader >> CDataStream(headerData, SER_DISK, PROTOCOL_VERSION);
    disputePoker->paramsBin = malloc(headerData.size());
    memcpy(disputePoker->paramsBin, headerData.data());
    disputePoker.paramsBinLength = headerData.size();

    CC *spendSig = cond->subconditions[1] = cc_new(CC_Threshold);
    spendSig->threshold = 1;
    spendSig->size = players.size() + 1;
    spendSig->subconditions = malloc(spendSig->size, sizeof(CC*));

    for (int i=0; i<players.size()+1; i++) {
        CC *sub = spendSig->subconditions[i] = cc_new(CC_Secp256k1);
        sub->publicKey = CopyPubKey(players[i]);
    }
    
    CC *cond = cc_new(CC_Threshold);
    cond->threshold = 2;
    cond->size = 2;
    cond->subconditions = calloc(2, sizeof(CC*));
    cond->subconditions[0] = disputePoker;
    cond->subconditions[1] = spendSig;
    return cond;
}


CMutableTransaction PokerProtocol::MakeSessionTx(CTxIn dealerInput)
{
    CMutableTransaction mtx;
    mtx.vin.push_back(dealerInput); 

    CC *disputeCond = MakeDisputeCond(players, disputeHeader);
    mtx.vout.push_back(CTxOut(MINFEE, CCPubKey(disputeCond)));
    cc_free(disputeCond);

    for (int i=0; i<players.size(); i++) {
        CC *cond = cc_new(CC_Secp256k1);
        cond->publicKey = CopyPubKey(players[i]);
        mtx.vout.push_back(CTxOut(MINFEE, CCPubKey(cond)));
        cc_free(cond);
    }
    return mtx;
}


CMutableTransaction PokerProtocol::MakeDisputeTx(uint256 signedSessionTxHash, CC *signedDisputeCond, uint256 vmResultHash)
{
    CMutableTransaction mtx;

    CC *disputeCond = MakeDisputeCond();
    mtx.vin.push_back(CTxIn(signedSessionTxHash, 0, CCSig(signedDisputeCond)));

    std::vector<unsigned char> result(vmResultHash.begin(), vmResultHash.begin()+32);
    mtx.vout.push_back(CTxOut(0, CScript() << OP_RETURN << result));
    return mtx;
}


CMutableTransaction PokerProtocol::MakePostEvidenceTx(uint256 signedSessionTxHash,
        int playerIdx, std::vector<unsigned char> state)
{
    CMutableTransaction mtx;

    CC *cond = cc_new(CC_Secp256k1);
    cond->publicKey = CopyPubKey(players[i]);
    mtx.vin.push_back(CTxIn(signedSessionTxHash, playerIdx+1, CCSig(cond)));

    mtx.vout.push_back(CTxOut(0, CScript() << OP_RETURN << state));

    return mtx;
}

CC* CCNewThreshold(int t, std::vector<CC*> v)
{
    CC *cond = cc_new(CC_Threshold);
    cond->threshold = t;
    cond->size = v.size();
    cond->subconditions = calloc(1, sizeof(CC*));
    memcpy(cond->subconditions, v.data(), v.size() * sizeof(CC*));
    return cond;
}


CCNewSecp256k1(CPubKey &k)
{
    cond = cc_new(CC_Secp256k1);
    cond->publicKey = CopyPubKey(players[i]);
    return cond;
}


CCNewEval(char *method, unsigned char* paramsBin, size_t len)
{
    CC *cond = cc_new(CC_Eval);
    strcpy(cond->method, method);
    cond->paramsBin = malloc(32);
    memcpy(cond->paramsBin, bin, len);
    cond->paramsBinLength = len;
    return cond;
}


CC* MakePayoutCond(uint256 signedSessionTx)
{
    CC* agree;
    {
        // TODO: 2/3 majority
        std::vector<CC*> subs;
        for (int i=0; i<players.size(); i++)
            subs[i] = CCNewSecp256k1(players[i]);
        agree = CCNewThreshold(players.size(), subs);
    }

    CC *import;
    {
        CC *importEval = CCNewEval("ImportPayout", signedSessionTx.begin(), 32);

        std::vector<CC*> subs;
        for (int i=0; i<players.size(); i++)
            subs[i] = CCNewSecp256k1(players[i]);
        CC *oneof = CCNewThreshold(1, subs);

        import = CCNewThreshold(2, {oneof, importEval});
    }

    return CCNewThreshold(1, {agree, import});
}


CMutableTransaction PokerProtocol::MakeStakeTx(CAmount totalPayout, std::vector<CTxIn> playerInputs,
        uint256 signedSessionTx)
{
    CMutableTransaction mtx;
    mtx.vin = playerInputs;

    CC *payoutCond = MakePayoutCond(signedSessionTx);push
    mtx.vout.push_back(CTxOut(totalPayout, CCPubKey(payoutCond)));
    cc_free(payoutCond);

    return mtx;
}


CMutableTransaction PokerProtocol::MakeAgreePayoutTx(std::vector<CTxOut> payouts,
        CC *signedPayoutCond, uint256 signedStakeTxHash)
{
    CMutableTransaction mtx;
    mtx.vin.push_back(CTxIn(signedStakeTxHash, 0, CCSig(signedPayoutCond)));
    mtx.vouts = payouts;
    return mtx;
}


CMutableTransaction PokerProtocol::MakeImportPayoutTx(std::vector<CTxOut> payouts,
        CC *signedPayoutCond, CTransaction signedDisputeTx, uint256 signedStakeTxHash)
{
    std::vector<unsigned char> vDisputeTx;
    signedDisputeTx >> CDataStream(vDisputeTx, SER_DISK, PROTOCOL_VERSION);
    CMutableTransaction mtx;
    mtx.vin.push_back(CTxInput(signedStakeTxHash, 0, CCSig(signedPayoutCond)));
    mtx.vout = payouts;
    CMutableTransaction.vout.insert(0, CTxOutput(0, CScript() << OP_RETURN << "PROOF HERE"));
    CMutableTransaction.vout.insert(1, CTxOutput(0, CScript() << OP_RETURN << vDisputeTx));
}
