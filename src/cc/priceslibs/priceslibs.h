/******************************************************************************
* Copyright © 2014-2019 The SuperNET Developers.                             *
*                                                                            *
* See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
* the top-level directory of this distribution for the individual copyright  *
* holder information and the developer policies on copyright and licensing.  *
*                                                                            *
* Unless otherwise agreed in a custom licensing agreement, no part of the    *
* SuperNET software, including this file may be copied, modified, propagated *
* or distributed except according to the terms contained in the LICENSE file *
*                                                                            *
* Removal or modification of this copyright notice is prohibited.            *
*                                                                            *
******************************************************************************/

#ifndef __PRICESLIBS__
#define __PRICESLIBS__

#include <stdint.h>

#define PF_CUSTOMJSONPARSERFUNCNAME "pricesJsonParser"
#define PF_CUSTOMVALIDATORFUNCNAME "pricesValidator"
#define PF_CUSTOMCLAMPERFUNCNAME "pricesClamper"

extern "C" {
    typedef int (*CustomJsonParser)(const char *sjson /*in*/, const char *symbol /*in*/, const char *customdata, uint32_t multiplier /*in*/, uint32_t *value /*out*/);
    int pricesJsonParser(const char *sjson /*in*/, const char *symbol /*in*/, const char *customdata, uint32_t multiplier /*in*/, uint32_t *value /*out*/);
    typedef int (*CustomValidator)(int32_t height, uint32_t prices[], uint32_t prevprices[], int32_t beginpos, int32_t endpos);
    typedef void (*CustomClamper)(int32_t height, uint32_t prices[], uint32_t prevprices[], int32_t beginpos, int32_t endpos, int64_t tolerance);

}

#endif // #ifndef __PRICESLIBS__
