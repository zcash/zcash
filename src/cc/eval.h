#ifndef CC_EVAL_H
#define CC_EVAL_H

#include <cryptoconditions.h>

#include "chain.h"
#include "streams.h"
#include "version.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"


/*
 * Eval codes
 *
 * Add to below macro to generate new code.
 *
 * If at some point a new interpretation model is introduced,
 * there should be a code identifying it. For example,
 * a possible code is EVAL_BITCOIN_SCRIPT, where the entire binary
 * after the code is interpreted as a bitcoin script.
 */
#define FOREACH_EVAL(EVAL) \
        EVAL(EVAL_IMPORTPAYOUT, 0xe1)

typedef uint8_t EvalCode;


class AppVM;
class NotarisationData;


class Eval
{
public:
    CValidationState state;

    bool Invalid(std::string s) { return state.Invalid(false, 0, s); }
    bool Error(std::string s) { return state.Error(s); }
    bool Valid() { return true; }

    /*
     * Test validity of a CC_Eval node
     */
    virtual bool Dispatch(const CC *cond, const CTransaction &tx, unsigned int nIn);

    /*
     * Dispute a payout using a VM
     */
    bool DisputePayout(AppVM &vm, std::vector<uint8_t> params, const CTransaction &disputeTx, unsigned int nIn);

    /*
     * Test an ImportPayout CC Eval condition
     */
    bool ImportPayout(std::vector<uint8_t> params, const CTransaction &importTx, unsigned int nIn);

    /*
     * IO functions
     */
    virtual bool GetTxUnconfirmed(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock) const;
    virtual bool GetTxConfirmed(const uint256 &hash, CTransaction &txOut, CBlockIndex &block) const;
    virtual unsigned int GetCurrentHeight() const;
    virtual bool GetSpendsConfirmed(uint256 hash, std::vector<CTransaction> &spends) const;
    virtual bool GetBlock(uint256 hash, CBlockIndex& blockIdx) const;
    virtual int32_t GetNotaries(uint8_t pubkeys[64][33], int32_t height, uint32_t timestamp) const;
    virtual bool GetNotarisationData(uint256 notarisationHash, NotarisationData &data) const;
    virtual bool CheckNotaryInputs(const CTransaction &tx, uint32_t height, uint32_t timestamp) const;
};


bool RunCCEval(const CC *cond, const CTransaction &tx, unsigned int nIn);


/*
 * Virtual machine to use in the case of on-chain app evaluation
 */
class AppVM
{ 
public:
    /*
     * in:  header   - paramters agreed upon by all players
     * in:  body     - gamestate
     * out: length   - length of game (longest wins)
     * out: payments - vector of CTxOut, always deterministically sorted.
     */
    virtual std::pair<int,std::vector<CTxOut>>
        evaluate(std::vector<unsigned char> header, std::vector<unsigned char> body) = 0;
};


/*
 * Data from notarisation OP_RETURN
 */
class NotarisationData {
public:
    uint256 blockHash;
    uint32_t height;
    uint256 txHash;  // Only get this guy in asset chains not in KMD
    char symbol[64];
    uint256 MoM;
    uint32_t MoMDepth;

    bool Parse(CScript scriptPubKey);
};


/*
 * Eval code utilities.
 */
#define EVAL_GENERATE_DEF(L,I) const uint8_t L = I;
#define EVAL_GENERATE_STRING(L,I) if (c == I) return #L;

FOREACH_EVAL(EVAL_GENERATE_DEF);

std::string EvalToStr(EvalCode c);


/*
 * Serialisation boilerplate
 */
#define E_MARSHAL(body) SerializeF([&] (CDataStream &ss) {body;})
template <class T>
std::vector<uint8_t> SerializeF(const T f)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    f(ss);
    return std::vector<unsigned char>(ss.begin(), ss.end());
}

#define E_UNMARSHAL(params, body) DeserializeF(params, [&] (CDataStream &ss) {body;})
template <class T>
bool DeserializeF(const std::vector<unsigned char> vIn, T f)
{
    CDataStream ss(vIn, SER_NETWORK, PROTOCOL_VERSION);
    try {
         f(ss);
        if (ss.eof()) return true;
    } catch(...) {}
    return false;
}


#endif /* CC_EVAL_H */
