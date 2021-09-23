/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef CC_EVAL_H
#define CC_EVAL_H

#include <cryptoconditions.h>

#include "cc/utils.h"
#include "chain.h"
#include "streams.h"
#include "version.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"

#define KOMODO_FIRSTFUNGIBLEID 100

/*
 * Eval codes
 *
 * Add to below macro to generate new code.
 *
 * If at some point a new interpretation model is introduced,
 * there should be a code identifying it. For example,
 * a possible code is EVAL_BITCOIN_SCRIPT, where the entire binary
 * after the code is interpreted as a bitcoin script.
 * Verus EVAL_STAKEGUARD is 0x01
 */
#define FOREACH_EVAL(EVAL)             \
        EVAL(EVAL_IMPORTPAYOUT, 0xe1)  \
        EVAL(EVAL_IMPORTCOIN,   0xe2)  \
        EVAL(EVAL_ASSETS,   0xe3)  \
        EVAL(EVAL_FAUCET, 0xe4) \
        EVAL(EVAL_REWARDS, 0xe5) \
        EVAL(EVAL_DICE, 0xe6) \
        EVAL(EVAL_FSM, 0xe7) \
        EVAL(EVAL_AUCTION, 0xe8) \
        EVAL(EVAL_LOTTO, 0xe9) \
        EVAL(EVAL_HEIR, 0xea) \
        EVAL(EVAL_CHANNELS, 0xeb) \
        EVAL(EVAL_ORACLES, 0xec) \
        EVAL(EVAL_PRICES, 0xed) \
        EVAL(EVAL_PEGS, 0xee) \
        EVAL(EVAL_PAYMENTS, 0xf0) \
        EVAL(EVAL_GATEWAYS, 0xf1) \
		EVAL(EVAL_TOKENS, 0xf2) \
        EVAL(EVAL_IMPORTGATEWAY, 0xf3)  \


// evalcodes 0x10 to 0x7f are reserved for cclib dynamic CC
#define EVAL_FIRSTUSER 0x10
#define EVAL_LASTUSER 0x7f

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
    virtual bool CheckNotaryInputs(const CTransaction &tx, uint32_t height, uint32_t timestamp) const;
    virtual uint32_t GetAssetchainsCC() const;
    virtual std::string GetAssetchainsSymbol() const;
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
    int IsBackNotarisation = 0;
    uint256 blockHash      = uint256();
    uint32_t height        = 0;
    uint256 txHash         = uint256();
    char symbol[64];
    uint256 MoM            = uint256();
    uint16_t MoMDepth      = 0;
    uint16_t ccId          = 0;
    uint256 MoMoM          = uint256();
    uint32_t MoMoMDepth    = 0;

    NotarisationData(int IsBack=2) : IsBackNotarisation(IsBack) {
        symbol[0] = '\0';
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {

        bool IsBack = IsBackNotarisation;
        if (2 == IsBackNotarisation) IsBack = DetectBackNotarisation(s, ser_action);

        READWRITE(blockHash);
        READWRITE(height);
        if (IsBack)
            READWRITE(txHash);
        SerSymbol(s, ser_action);
        if (s.size() == 0) return;
        READWRITE(MoM);
        READWRITE(MoMDepth);
        READWRITE(ccId);
        if (s.size() == 0) return;
        if (IsBack) {
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
        size_t readlen = std::min(sizeof(symbol), s.size());
        char *nullPos = (char*) memchr(&s[0], 0, readlen);
        if (!nullPos)
            throw std::ios_base::failure("couldn't parse symbol");
        s.read(symbol, nullPos-&s[0]+1);
    }

    template <typename Stream>
    bool DetectBackNotarisation(Stream& s, CSerActionUnserialize act)
    {
        if (ASSETCHAINS_SYMBOL[0]) return 1;
        if (s.size() >= 72) {
            if (strcmp("BTC", &s[68]) == 0) return 1;
            if (strcmp("KMD", &s[68]) == 0) return 1;
        }
        return 0;
    }
    
    template <typename Stream>
    bool DetectBackNotarisation(Stream& s, CSerActionSerialize act)
    {
        return !txHash.IsNull();
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
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(VARINT(nIndex));
        READWRITE(branch);
    }
};


typedef std::pair<uint256,MerkleBranch> TxProof;


uint256 GetMerkleRoot(const std::vector<uint256>& vLeaves);
struct CCcontract_info *CCinit(struct CCcontract_info *cp,uint8_t evalcode);
bool ProcessCC(struct CCcontract_info *cp,Eval* eval, std::vector<uint8_t> paramsNull, const CTransaction &tx, unsigned int nIn);


#endif /* CC_EVAL_H */
