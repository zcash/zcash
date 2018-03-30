#ifndef CC_EVAL_H
#define CC_EVAL_H

#include "cryptoconditions/include/cryptoconditions.h"
#include "primitives/transaction.h"

/*
 * Test validity of a CC_Eval node
 */
bool EvalConditionValidity(const CC *cond, const CTransaction *tx, int nIn);

/*
 * Test an ImportPayout CC Eval condition
 */
bool CheckImportPayout(const CC *cond, const CTransaction *payoutTx, int nIn);

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
 * Test a DisputePayout CC Eval condition, using a provided AppVM
 */
bool DisputePayout(AppVM &vm, const CC *cond, const CTransaction *disputeTx, int nIn);

/*
 * Get PUSHDATA from a script
 */
bool GetPushData(const CScript &sig, std::vector<unsigned char> &data);

/*
 * Get OP_RETURN data from a script
 */
bool GetOpReturnData(const CScript &sig, std::vector<unsigned char> &data);


#endif /* CC_EVAL_H */
