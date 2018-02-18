#ifndef KOMODO_CRYPTOCONDITIONS_H
#define KOMODO_CRYPTOCONDITIONS_H

#include "cryptoconditions/include/cryptoconditions.h"

extern int32_t ASSETCHAINS_CC;

static bool IsCryptoConditionsEnabled() {
    return 0 != ASSETCHAINS_CC;
}

/*
 * Method stub for aux conditions. Unimplemented, thus fails if an aux condition is encountered.
 */
static int komodoCCAux(CC *cond, void *context) {
    return 0;
}

#endif /* KOMODO_CRYPTOCONDITIONS_H */
