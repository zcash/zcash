#include "primitives/transaction.h"
#include "streams.h"
#include "chain.h"
#include "main.h"
#include "cc/eval.h"
#include "cryptoconditions/include/cryptoconditions.h"


bool GetSpends(uint256 hash, std::vector<boost::optional<CTransaction>> &spends)
{
    // NOT IMPLEMENTED
    return false;
}


class DisputeHeader
{
public:
    int waitBlocks;
    std::vector<unsigned char> vmHeader;
    
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(waitBlocks));
        READWRITE(vmHeader);
    }
};


/*
 * Crypto-Condition EVAL method that resolves a dispute of a session
 *
 * IN: vm - AppVM virtual machine to verify states
 * IN: cond - CC EVAL node
 * IN: disputeTx - transaction attempting to resolve dispute
 * IN: nIn  - index of input of dispute tx
 *
 * disputeTx: attempt to resolve a dispute
 *
 *   in  0:      Spends Session TX first output, reveals DisputeHeader
 *   out 0:      OP_RETURN hash of payouts
 */
bool DisputePayout(AppVM &vm, const CC *cond, const CTransaction *disputeTx, int nIn)
{
    // TODO: Error messages!
    if (disputeTx->vout.size() < 2) return 0;

    // get payouts hash
    std::vector<unsigned char> vPayoutHash;
    uint256 payoutHash;
    if (!GetOpReturnData(disputeTx->vout[0].scriptPubKey, vPayoutHash)) return 0;
    memcpy(payoutHash.begin(), vPayoutHash.data(), 32);

    // load dispute header
    DisputeHeader disputeHeader;
    std::vector<unsigned char> headerData(cond->paramsBin,
                                          cond->paramsBin+cond->paramsBinLength);
    CDataStream(headerData, SER_DISK, PROTOCOL_VERSION) >> disputeHeader;
    // TODO: exception? end of stream?

    // ensure that enough time has passed
    CTransaction sessionTx;
    uint256 sessionBlockHash;
    if (!GetTransaction(disputeTx->vin[0].prevout.hash, sessionTx, sessionBlockHash, false))
        return false;  // wth? TODO: log   TODO: MUST be upsteam of disputeTx, how to ensure?
                       // below does this by looking up block in blockindex
                       // what if GetTransaction returns from mempool, maybe theres no block?
    CBlockIndex* sessionBlockIndex = mapBlockIndex[sessionBlockHash];
    if (chainActive.Height() < sessionBlockIndex->nHeight + disputeHeader.waitBlocks)
        return false;  // Not yet

    // get spends
    std::vector<boost::optional<CTransaction>> spends;
    if (!GetSpends(disputeTx->vin[0].prevout.hash, spends)) return 0;

    // verify result from VM
    int maxLength = -1;
    uint256 bestPayout;
    for (int i=1; i<spends.size(); i++)
    {
        if (!spends[i]) continue;
        std::vector<unsigned char> vmBody;
        if (!GetOpReturnData(spends[i]->vout[0].scriptPubKey, vmBody)) continue;
        auto out = vm.evaluate(disputeHeader.vmHeader, vmBody);
        uint256 resultHash = SerializeHash(out.second);
        if (out.first > maxLength) {
            maxLength = out.first;
            bestPayout = resultHash;
        }
        // The below means that if for any reason there is a draw, 
        else if (out.first == maxLength) {
            if (bestPayout != payoutHash) {
                fprintf(stderr, "WARNING: VM has multiple solutions of same length\n");
                bestPayout = resultHash;
            }
        }
    }

    return bestPayout == payoutHash;
}
