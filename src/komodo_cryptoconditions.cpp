
#include "cryptoconditions/include/cryptoconditions.h"
#include "script/interpreter.h"


int CryptoConditionChecker::CheckAuxCondition(const CC *cond) const {
    if (0 == strcmp((const char*)cond->method, "equals")) {
        return (cond->conditionAuxLength == cond->fulfillmentAuxLength) &&
               (0 == memcmp(cond->conditionAux, cond->fulfillmentAux, cond->conditionAuxLength));
    }
    printf("no defined behaviour for method:%s\n", cond->method);
    return 0;
}
