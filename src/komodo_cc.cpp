#include "cryptoconditions/include/cryptoconditions.h"
#include "komodo_cc.h"


bool IsCryptoConditionsEnabled()
{
    return 0 != ASSETCHAINS_CC;
}

// Limit acceptable condition types
// Prefix not enabled because no current use case, ambiguity on how to combine with secp256k1
// RSA not enabled because no current use case, not implemented
int CCEnabledTypes = 1 << CC_Secp256k1 | \
                     1 << CC_Threshold | \
                     1 << CC_Eval | \
                     1 << CC_Preimage | \
                     1 << CC_Ed25519;


int CCSigningNodes = 1 << CC_Ed25519 | 1 << CC_Secp256k1;


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
