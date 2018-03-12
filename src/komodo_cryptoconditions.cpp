
#include "replacementpool.h"
#include "komodo_cryptoconditions.h"
#include "cryptoconditions/include/cryptoconditions.h"


bool ASSETCHAINS_CC_TEST = false;


/*
 * Evaluate the validity of an Eval node
 */
bool EvalConditionValidity(const CC *cond, const CTransaction *txTo)
{
    if (ASSETCHAINS_CC_TEST) {
        if (strcmp(cond->method, "testEval") == 0) {
            return cond->paramsBinLength == 8 &&
                memcmp(cond->paramsBin, "testEval", 8) == 0;
        }
        if (strcmp(cond->method, "testReplace") == 0) {
            std::vector<unsigned char> data;
            auto out = txTo->vout[txTo->vout.size()-1];  // Last output is data
            return GetOpReturnData(out.scriptPubKey, data) && data.size() == 2;
        }
    }
    fprintf(stderr, "no defined behaviour for method: %s\n", cond->method);
    return 0;
}


/*
 * Evaluate the priority of an eval node.
 *
 * This method should set the ->priority and ->replacementWindow (in blocks)
 * of the provided replacement pool item. Priority is a 64 bit unsigned int and
 * 0 is invalid.
 *
 * This method does not need to validate, that is done separately. Actually,
 * this method will nearly always be called with the same condition and transaction
 * in sequence after EvalConditionValidity. If performance became an issue, a very
 * small LRU cache could be used to cache a result.
 */
bool EvalConditionPriority(const CC *cond, CTxReplacementPoolItem *rep)
{
    if (ASSETCHAINS_CC_TEST) {
        if (strcmp((const char*)cond->method, "testReplace") == 0) {
            std::vector<unsigned char> data;
            auto out = rep->tx.vout[rep->tx.vout.size()-1];  // Last output is data
            if (GetOpReturnData(out.scriptPubKey, data)) {
                rep->replacementWindow = (int) data[0];
                rep->priority = (uint64_t) data[1];
                return true;
            }
        }
    }
    return false;
}


int visitConditionPriority(CC *cond, struct CCVisitor visitor)
{
    auto rep = (CTxReplacementPoolItem*)visitor.context;
    // visitor protocol is 1 for continue, 0 for stop
    return !(cc_typeId(cond) == CC_Eval && EvalConditionPriority(cond, rep));
}


/*
 * Try to get replacement parameters from a transaction in &rep.
 */
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
