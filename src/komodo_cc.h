#ifndef KOMODO_CC_H
#define KOMODO_CC_H

#include "cryptoconditions/include/cryptoconditions.h"


extern int32_t ASSETCHAINS_CC;
bool IsCryptoConditionsEnabled();

// Limit acceptable condition types
// Prefix not enabled because no current use case, ambiguity on how to combine with secp256k1
// RSA not enabled because no current use case, not implemented
const int CCEnabledTypes = 1 << CC_Secp256k1 | \
                           1 << CC_Threshold | \
                           1 << CC_Eval | \
                           1 << CC_Preimage | \
                           1 << CC_Ed25519;

const int CCSigningNodes = 1 << CC_Ed25519 | 1 << CC_Secp256k1;


/*
 * Check if the server can accept the condition based on it's structure / types
 */
bool IsSupportedCryptoCondition(const CC *cond);


/*
 * Check if crypto condition is signed. Can only accept signed conditions.
 */
bool IsSignedCryptoCondition(const CC *cond);


#endif /* KOMODO_CC_H */
