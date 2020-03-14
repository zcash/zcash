#ifndef CC_KOGS_H
#define CC_KOGS_H

#include "CCinclude.h"

bool KogsValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn);

#endif
