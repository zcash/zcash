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

#include <stdint.h>
#include <gmp.h>

#if defined (__cplusplus)
extern "C" {
#endif

    // a small extension to gmp lib for setting/getting int64_t/uint64_t
    void mpz_set_si64(mpz_t rop, int64_t op);
    int64_t mpz_get_si64(mpz_t op);
    void mpz_set_ui64(mpz_t rop, uint64_t op);
    uint64_t mpz_get_ui64(mpz_t op);

#if defined (__cplusplus)
}
#endif
