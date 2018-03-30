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


#endif /* CC_EVAL_H */
