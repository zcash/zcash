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

    fprintf(stderr, "no defined behaviour for method: %s\n", cond->method);
    return 0;
}
