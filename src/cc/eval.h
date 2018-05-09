#ifndef CC_EVAL_H
#define CC_EVAL_H

#include <cryptoconditions.h>

#include "cc/utils.h"
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
#define FOREACH_EVAL(EVAL)             \
        EVAL(EVAL_IMPORTPAYOUT, 0xe1)  \
        EVAL(EVAL_IMPORTCOIN,   0xe2)


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
     * Import coin from another chain with same symbol
     */
    bool ImportCoin(std::vector<uint8_t> params, const CTransaction &importTx, unsigned int nIn);

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
    virtual bool GetNotarisationData(int notarisationHeight, NotarisationData &data,
            bool verifyCanonical) const;
    virtual bool CheckNotaryInputs(const CTransaction &tx, uint32_t height, uint32_t timestamp) const;
    virtual uint32_t GetCurrentLedgerID() const;
};


extern Eval* EVAL_TEST;


/*
 * Get a pointer to an Eval to use
 */
typedef std::unique_ptr<Eval,void(*)(Eval*)> EvalRef_;
class EvalRef : public EvalRef_
{
public:
    EvalRef() : EvalRef_(
            EVAL_TEST ? EVAL_TEST : new Eval(),
            [](Eval* e){if (e!=EVAL_TEST) delete e;}) { }
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


extern char ASSETCHAINS_SYMBOL[65];


/*
 * Data from notarisation OP_RETURN from chain being notarised
 */
class NotarisationData
{
public:
    bool IsBackNotarisation = 0;
    uint256 blockHash;
    uint32_t height;
    uint256 txHash;  // Only get this guy in asset chains not in KMD
    char symbol[64] = "\0";
    uint256 MoM;
    uint32_t MoMDepth;
    uint32_t ccId;
    uint256 MoMoM;
    uint32_t MoMoMDepth;

    NotarisationData(bool IsBack=0) : IsBackNotarisation(IsBack) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(blockHash);
        READWRITE(height);
        if (IsBackNotarisation || (!ser_action.ForRead() && !txHash.IsNull()))
            READWRITE(txHash);
        SerSymbol(s, ser_action);
        READWRITE(MoM);
        READWRITE(MoMDepth);
        if (s.size() == 0) return;
        READWRITE(ccId);
        if (IsBackNotarisation) {
            READWRITE(MoMoM);
            READWRITE(MoMoMDepth);
        }
    }
    
    template <typename Stream>
    void SerSymbol(Stream& s, CSerActionSerialize act)
    {
        s.write(symbol, strlen(symbol)+1);
    }

    template <typename Stream>
    void SerSymbol(Stream& s, CSerActionUnserialize act)
    {
        char *nullPos = (char*) memchr(&s[0], 0, s.size());
        if (!nullPos)
            throw std::ios_base::failure("couldn't parse symbol");
        s.read(symbol, nullPos-&s[0]+1);
    }
};


bool ParseNotarisationOpReturn(const CTransaction &tx, NotarisationData &data);


/*
 * Eval code utilities.
 */
#define EVAL_GENERATE_DEF(L,I) const uint8_t L = I;
#define EVAL_GENERATE_STRING(L,I) if (c == I) return #L;

FOREACH_EVAL(EVAL_GENERATE_DEF);

std::string EvalToStr(EvalCode c);


/*
 * Merkle stuff
 */
uint256 SafeCheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex);


class MerkleBranch
{
public:
    int nIndex;
    std::vector<uint256> branch;

    MerkleBranch() {}
    MerkleBranch(int i, std::vector<uint256> b) : nIndex(i), branch(b) {}
    uint256 Exec(uint256 hash) const { return SafeCheckMerkleBranch(hash, branch, nIndex); }

    MerkleBranch& operator<<(MerkleBranch append)
    {
        nIndex += append.nIndex << branch.size();
        branch.insert(branch.end(), append.branch.begin(), append.branch.end());
        return *this;
    }

    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(nIndex));
        READWRITE(branch);
    }
};


uint256 GetMerkleRoot(const std::vector<uint256>& vLeaves);


#endif /* CC_EVAL_H */
