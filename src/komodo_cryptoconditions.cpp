#include "komodo_cryptoconditions.h"
#include "cryptoconditions/include/cryptoconditions.h"


/*
 * Evaluate the validity of an Eval node
 */
bool EvalConditionValidity(const CC *cond, const CTransaction *txTo)
{
    if (strcmp(cond->method, "testEval") == 0) {
        return cond->paramsBinLength == 8 &&
            memcmp(cond->paramsBin, "testEval", 8) == 0;
    }
    fprintf(stderr, "no defined behaviour for method: %s\n", cond->method);
    return 0;
}


bool GetOpReturnData(const CScript &sig, std::vector<unsigned char> &data)
{
    auto pc = sig.begin();
    opcodetype opcode;
    if (sig.GetOp(pc, opcode))
        if (opcode == OP_RETURN)
            if (sig.GetOp(pc, opcode, data))
                return opcode > OP_0 && opcode <= OP_PUSHDATA4;
    return false;
}
