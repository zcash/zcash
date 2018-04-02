#include "cryptoconditions/include/cryptoconditions.h"
#include "komodo_cc.h"


bool IsCryptoConditionsEnabled()
{
    return 0 != ASSETCHAINS_CC;
}


bool IsSupportedCryptoCondition(const CC *cond)
{
    int mask = cc_typeMask(cond);

    if (mask & ~CCEnabledTypes) return false;

    // Also require that the condition have at least one signable node
    if (!(mask & CCSigningNodes)) return false;

    return true;
}


bool IsSignedCryptoCondition(const CC *cond)
{
    if (!cc_isFulfilled(cond)) return false;
    if (1 << cc_typeId(cond) & CCSigningNodes) return true;
    if (cc_typeId(cond) == CC_Threshold)
        for (int i=0; i<cond->size; i++)
            if (IsSignedCryptoCondition(cond->subconditions[i])) return true;
    return false;
}
