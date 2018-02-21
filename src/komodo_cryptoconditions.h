#ifndef KOMODO_CRYPTOCONDITIONS_H
#define KOMODO_CRYPTOCONDITIONS_H

#include "cryptoconditions/include/cryptoconditions.h"

extern int32_t ASSETCHAINS_CC;

static bool IsCryptoConditionsEnabled() {
    return 0 != ASSETCHAINS_CC;
}

#endif /* KOMODO_CRYPTOCONDITIONS_H */
