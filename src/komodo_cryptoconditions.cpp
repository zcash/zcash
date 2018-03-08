
#include "replacementpool.h"
#include "komodo_cryptoconditions.h"
#include "cryptoconditions/include/cryptoconditions.h"
#include "script/interpreter.h"
#include "coins.h"


#define REPLACEMENT_WINDOW_BLOCKS 2


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


bool EvalConditionBool(const CC *cond, const CTransaction *txTo)
{
    if (strcmp(cond->method, "testEval") == 0) {
        return cond->paramsBinLength == 8 &&
            memcmp(cond->paramsBin, "testEval", 8) == 0;
    }
    if (strcmp(cond->method, "testReplace") == 0) {
        return true;
    }
    fprintf(stderr, "no defined behaviour for method: %s\n", cond->method);
    return 0;
}


bool GetConditionPriority(const CC *cond, CTxReplacementPoolItem *rep)
{
    if (strcmp((const char*)cond->method, "testReplace") == 0) {
        std::vector<unsigned char> data;
        if (GetOpReturnData(rep->tx.vout[0].scriptPubKey, data)) {
            rep->priority = (uint64_t) data[0];
            rep->replacementWindow = REPLACEMENT_WINDOW_BLOCKS;
            return true;
        }
    }
    return false;
}


bool TransactionSignatureChecker::CheckEvalCondition(const CC *cond) const
{
    return EvalConditionBool(cond, txTo);
}


extern "C"
{
    int visitConditionPriority(CC *cond, struct CCVisitor visitor);
}


int visitConditionPriority(CC *cond, struct CCVisitor visitor)
{
    if (cc_typeId(cond) == CC_Eval) {
        if (GetConditionPriority(cond, (CTxReplacementPoolItem*)visitor.context)) {
            return 0; // stop
        }
    }
    return 1; // continue
}


bool SetReplacementParams(CTxReplacementPoolItem &rep)
{
    // first, see if we have a cryptocondition
    const CScript &sig = rep.tx.vin[0].scriptSig;
    if (!sig.IsPushOnly()) return false;
    CScript::const_iterator pc = sig.begin();
    std::vector<unsigned char> data;
    opcodetype opcode;
    if (!sig.GetOp(pc, opcode, data)) return false;
    CC *cond = cc_readFulfillmentBinary((unsigned char*)data.data(), data.size());
    if (!cond) return false;

    // now, see if it has a replacement node
    CC *replacementNode = 0;
    CCVisitor visitor = {&visitConditionPriority, (const unsigned char*)"", 0, &rep};
    bool out = cc_visit(cond, visitor);
    cc_free(cond);
    return !out;
}
