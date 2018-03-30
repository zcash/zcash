#ifndef KOMODO_CC_H
#define KOMODO_CC_H

#include "cryptoconditions/include/cryptoconditions.h"
#include "primitives/transaction.h"


/*
 * Check if CryptoConditions is enabled based on chain or cmd flag
 */
extern int32_t ASSETCHAINS_CC;
static bool IsCryptoConditionsEnabled() 
{
    return 0 != ASSETCHAINS_CC;
}


/*
 * Check if the server can accept the condition based on it's structure / types
 */
static bool IsAcceptableCryptoCondition(const CC *cond)
{
    int32_t typeMask = cc_typeMask(cond);

    // Require a signature to prevent transaction malleability
    if (0 == typeMask & (1 << CC_Secp256k1) ||
        0 == typeMask & (1 << CC_Ed25519)) return false;

    // Limit acceptable condition types
    // Prefix not enabled because no current use case, ambiguity on how to combine with secp256k1
    // RSA not enabled because no current use case, not implemented
    int enabledTypes = 1 << CC_Secp256k1 | 1 << CC_Threshold | 1 << CC_Eval | \
                       1 << CC_Preimage | 1 << CC_Ed25519;
    if (typeMask & ~enabledTypes) return false;
    
    return true;
}


#endif /* KOMODO_CC_H */
