#include "primitives/transaction.h"
#include "komodo_cc.h"
#include "cc/eval.h"
#include <cryptoconditions.h>


/*
 * Test the validity of an Eval node
 */
bool EvalConditionValidity(const CC *cond, const CTransaction *txTo, int nIn)
{
    if (strcmp(cond->method, "testEval") == 0) {
        return cond->paramsBinLength == 8 &&
            memcmp(cond->paramsBin, "testEval", 8) == 0;
    }

    if (strcmp(cond->method, "ImportPayout") == 0) {
        return CheckImportPayout(cond, txTo, nIn);
    }

    /* Example of how you might call DisputePayout

    if (strcmp(ASSETCHAINS_SYMBOL, "PANGEA") == 0) {
        if (strcmp(cond->method, "DisputePoker") == 0) {
            return DisputePayout(PokerVM(), cond, txTo, nIn);
        }
    }

    */

    fprintf(stderr, "no defined behaviour for method: %s\n", cond->method);
    return 0;
}



bool GetPushData(const CScript &sig, std::vector<unsigned char> &data)
{
    opcodetype opcode;
    auto pc = sig.begin();
    if (sig.GetOp(pc, opcode, data)) return opcode > OP_0 && opcode <= OP_PUSHDATA4;
    return false;
}


bool GetOpReturnData(const CScript &sig, std::vector<unsigned char> &data)
{
    auto pc = sig.begin();
    opcodetype opcode;
    if (sig.GetOp2(pc, opcode, NULL))
        if (opcode == OP_RETURN)
            if (sig.GetOp(pc, opcode, data))
                return opcode > OP_0 && opcode <= OP_PUSHDATA4;
    return false;
}

