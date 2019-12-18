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

#include "gmp_i64.h"

void mpz_set_si64( mpz_t rop, int64_t op )
{
	int isneg = op < 0 ? 1 : 0;
    int64_t op_abs = op < 0 ? -op : op;
    mpz_import(rop, 1, 1, sizeof(op_abs), 0, 0, &op_abs);
    if (isneg)
    	mpz_neg(rop, rop);
}

int64_t mpz_get_si64( mpz_t op )
{
    uint64_t u = 0LL; /* if op is zero nothing will be written into u */
    uint64_t u_abs;

    mpz_export(&u, NULL, 1, sizeof(u), 0, 0, op);
    u_abs = u < 0 ? -u : u;
    if (mpz_sgn(op) < 0)
		return -(int64_t)u_abs;
    else
        return (int64_t)u_abs;
}

void mpz_set_ui64( mpz_t rop, uint64_t op )
{
    mpz_import(rop, 1, 1, sizeof(op), 0, 0, &op);
}

uint64_t mpz_get_ui64( mpz_t op )
{
    uint64_t u = 0LL; /* if op is zero nothing will be written into u */
    mpz_export(&u, NULL, 1, sizeof(u), 0, 0, op);
    return u;
}