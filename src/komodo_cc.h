#ifndef KOMODO_CC_H
#define KOMODO_CC_H

#include "cryptoconditions/include/cryptoconditions.h"


extern int32_t ASSETCHAINS_CC;
bool IsCryptoConditionsEnabled();


/*
 * Check if the server can accept the condition based on it's structure / types
 */
bool IsSupportedCryptoCondition(const CC *cond);


/*
 * Check if crypto condition is signed. Can only accept signed conditions.
 */
bool IsSignedCryptoCondition(const CC *cond);


#endif /* KOMODO_CC_H */
