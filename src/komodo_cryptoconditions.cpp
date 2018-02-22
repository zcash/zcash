
#include "cryptoconditions/include/cryptoconditions.h"
#include "script/interpreter.h"


int TransactionSignatureChecker::CheckAuxCondition(const CC *cond) const {

    // Check that condition is equal to fulfillment
    if (0 == strcmp((const char*)cond->method, "equals")) {
        return (cond->conditionAuxLength == cond->fulfillmentAuxLength) &&
               (0 == memcmp(cond->conditionAux, cond->fulfillmentAux, cond->conditionAuxLength));
    }

    // Check that pubKeyScript specified in fulfillment is OP_RETURN
    if (0 == strcmp((const char*)cond->method, "inputIsReturn")) {
        if (cond->fulfillmentAuxLength != 1) return 0;
        int n = (int) cond->fulfillmentAux[0];
        if (n >= txTo->vout.size()) return 0;
        uint8_t *ptr = (uint8_t *)txTo->vout[n].scriptPubKey.data();
        return ptr[0] == OP_RETURN;
    }
    printf("no defined behaviour for method:%s\n", cond->method);
    return 0;
}
