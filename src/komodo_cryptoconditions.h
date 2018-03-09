#ifndef KOMODO_CRYPTOCONDITIONS_H
#define KOMODO_CRYPTOCONDITIONS_H

#include "replacementpool.h"
#include "cryptoconditions/include/cryptoconditions.h"
#include "script/script.h"


extern int32_t ASSETCHAINS_CC;

static bool IsCryptoConditionsEnabled() {
    return 0 != ASSETCHAINS_CC;
}

extern CTxReplacementPool replacementPool;

bool EvalConditionValidity(const CC *cond, const CTransaction *tx);

bool SetReplacementParams(CTxReplacementPoolItem &rep);

bool GetOpReturnData(const CScript &sig, std::vector<unsigned char> &data);

#endif /* KOMODO_CRYPTOCONDITIONS_H */
