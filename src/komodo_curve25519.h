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
#pragma once

#include <stdint.h>
#include <memory.h>
#include <string.h>
#ifdef _WIN32
#include <sodium.h>
#endif

#include "komodo_structs.h"

bits320 fmul(const bits320 in2,const bits320 in);
bits320 fexpand(bits256 basepoint);
bits256 fcontract(const bits320 input);
void cmult(bits320 *resultx,bits320 *resultz,bits256 secret,const bits320 q);
bits320 crecip(const bits320 z);
bits256 curve25519(bits256 mysecret,bits256 basepoint);

// Sum two numbers: output += in
static inline bits320 fsum(bits320 output,bits320 in)
{
    int32_t i;
    for (i=0; i<5; i++)
        output.ulongs[i] += in.ulongs[i];
    return(output);
}

static inline void fdifference_backwards(uint64_t *out,const uint64_t *in)
{
    static const uint64_t two54m152 = (((uint64_t)1) << 54) - 152;  // 152 is 19 << 3
    static const uint64_t two54m8 = (((uint64_t)1) << 54) - 8;
    int32_t i;
    out[0] = in[0] + two54m152 - out[0];
    for (i=1; i<5; i++)
        out[i] = in[i] + two54m8 - out[i];
}

void store_limb(uint8_t *out,uint64_t in);

// Take a little-endian, 32-byte number and expand it into polynomial form
bits320 fexpand(bits256 basepoint);

#if __amd64__
// donna: special gcc mode for 128-bit integers. It's implemented on 64-bit platforms only as far as I know.
typedef unsigned uint128_t __attribute__((mode(TI)));

// Multiply a number by a scalar: output = in * scalar
static inline bits320 fscalar_product(const bits320 in,const uint64_t scalar)
{
    int32_t i; uint128_t a = 0; bits320 output;
    a = ((uint128_t)in.ulongs[0]) * scalar;
    output.ulongs[0] = ((uint64_t)a) & 0x7ffffffffffffLL;
    for (i=1; i<5; i++)
    {
        a = ((uint128_t)in.ulongs[i]) * scalar + ((uint64_t) (a >> 51));
        output.ulongs[i] = ((uint64_t)a) & 0x7ffffffffffffLL;
    }
    output.ulongs[0] += (a >> 51) * 19;
    return(output);
}

// Multiply two numbers: output = in2 * in
// output must be distinct to both inputs. The inputs are reduced coefficient form, the output is not.
// Assumes that in[i] < 2**55 and likewise for in2. On return, output[i] < 2**52
bits320 fmul(const bits320 in2,const bits320 in);

bits320 fsquare_times(const bits320 in,uint64_t count);

static inline void fcontract_iter(uint128_t t[5],int32_t flag)
{
    int32_t i; uint64_t mask = 0x7ffffffffffffLL;
    for (i=0; i<4; i++)
        t[i+1] += t[i] >> 51, t[i] &= mask;
    if ( flag != 0 )
        t[0] += 19 * (t[4] >> 51); t[4] &= mask;
}

// donna: Take a fully reduced polynomial form number and contract it into a little-endian, 32-byte array
bits256 fcontract(const bits320 input);

bits256 curve25519(bits256 mysecret,bits256 basepoint);

#else
// from curve25519-donna.c
typedef uint8_t u8;
typedef int32_t s32;
typedef int64_t limb;

/* Multiply a number by a scalar: output = in * scalar */
static void fscalar_product32(limb *output, const limb *in, const limb scalar) {
    unsigned i;
    for (i = 0; i < 10; ++i) {
        output[i] = in[i] * scalar;
    }
}

/* Multiply two numbers: output = in2 * in
 *
 * output must be distinct to both inputs. The inputs are reduced coefficient
 * form, the output is not.
 *
 * output[x] <= 14 * the largest product of the input limbs.
 static void fproduct(limb *output, const limb *in2, const limb *in) {
 output[0] =       ((limb) ((s32) in2[0])) * ((s32) in[0]);
 output[1] =       ((limb) ((s32) in2[0])) * ((s32) in[1]) +
 ((limb) ((s32) in2[1])) * ((s32) in[0]);
 output[2] =  2 *  ((limb) ((s32) in2[1])) * ((s32) in[1]) +
 ((limb) ((s32) in2[0])) * ((s32) in[2]) +
 ((limb) ((s32) in2[2])) * ((s32) in[0]);
 output[3] =       ((limb) ((s32) in2[1])) * ((s32) in[2]) +
 ((limb) ((s32) in2[2])) * ((s32) in[1]) +
 ((limb) ((s32) in2[0])) * ((s32) in[3]) +
 ((limb) ((s32) in2[3])) * ((s32) in[0]);
 output[4] =       ((limb) ((s32) in2[2])) * ((s32) in[2]) +
 2 * (((limb) ((s32) in2[1])) * ((s32) in[3]) +
 ((limb) ((s32) in2[3])) * ((s32) in[1])) +
 ((limb) ((s32) in2[0])) * ((s32) in[4]) +
 ((limb) ((s32) in2[4])) * ((s32) in[0]);
 output[5] =       ((limb) ((s32) in2[2])) * ((s32) in[3]) +
 ((limb) ((s32) in2[3])) * ((s32) in[2]) +
 ((limb) ((s32) in2[1])) * ((s32) in[4]) +
 ((limb) ((s32) in2[4])) * ((s32) in[1]) +
 ((limb) ((s32) in2[0])) * ((s32) in[5]) +
 ((limb) ((s32) in2[5])) * ((s32) in[0]);
 output[6] =  2 * (((limb) ((s32) in2[3])) * ((s32) in[3]) +
 ((limb) ((s32) in2[1])) * ((s32) in[5]) +
 ((limb) ((s32) in2[5])) * ((s32) in[1])) +
 ((limb) ((s32) in2[2])) * ((s32) in[4]) +
 ((limb) ((s32) in2[4])) * ((s32) in[2]) +
 ((limb) ((s32) in2[0])) * ((s32) in[6]) +
 ((limb) ((s32) in2[6])) * ((s32) in[0]);
 output[7] =       ((limb) ((s32) in2[3])) * ((s32) in[4]) +
 ((limb) ((s32) in2[4])) * ((s32) in[3]) +
 ((limb) ((s32) in2[2])) * ((s32) in[5]) +
 ((limb) ((s32) in2[5])) * ((s32) in[2]) +
 ((limb) ((s32) in2[1])) * ((s32) in[6]) +
 ((limb) ((s32) in2[6])) * ((s32) in[1]) +
 ((limb) ((s32) in2[0])) * ((s32) in[7]) +
 ((limb) ((s32) in2[7])) * ((s32) in[0]);
 output[8] =       ((limb) ((s32) in2[4])) * ((s32) in[4]) +
 2 * (((limb) ((s32) in2[3])) * ((s32) in[5]) +
 ((limb) ((s32) in2[5])) * ((s32) in[3]) +
 ((limb) ((s32) in2[1])) * ((s32) in[7]) +
 ((limb) ((s32) in2[7])) * ((s32) in[1])) +
 ((limb) ((s32) in2[2])) * ((s32) in[6]) +
 ((limb) ((s32) in2[6])) * ((s32) in[2]) +
 ((limb) ((s32) in2[0])) * ((s32) in[8]) +
 ((limb) ((s32) in2[8])) * ((s32) in[0]);
 output[9] =       ((limb) ((s32) in2[4])) * ((s32) in[5]) +
 ((limb) ((s32) in2[5])) * ((s32) in[4]) +
 ((limb) ((s32) in2[3])) * ((s32) in[6]) +
 ((limb) ((s32) in2[6])) * ((s32) in[3]) +
 ((limb) ((s32) in2[2])) * ((s32) in[7]) +
 ((limb) ((s32) in2[7])) * ((s32) in[2]) +
 ((limb) ((s32) in2[1])) * ((s32) in[8]) +
 ((limb) ((s32) in2[8])) * ((s32) in[1]) +
 ((limb) ((s32) in2[0])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[0]);
 output[10] = 2 * (((limb) ((s32) in2[5])) * ((s32) in[5]) +
 ((limb) ((s32) in2[3])) * ((s32) in[7]) +
 ((limb) ((s32) in2[7])) * ((s32) in[3]) +
 ((limb) ((s32) in2[1])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[1])) +
 ((limb) ((s32) in2[4])) * ((s32) in[6]) +
 ((limb) ((s32) in2[6])) * ((s32) in[4]) +
 ((limb) ((s32) in2[2])) * ((s32) in[8]) +
 ((limb) ((s32) in2[8])) * ((s32) in[2]);
 output[11] =      ((limb) ((s32) in2[5])) * ((s32) in[6]) +
 ((limb) ((s32) in2[6])) * ((s32) in[5]) +
 ((limb) ((s32) in2[4])) * ((s32) in[7]) +
 ((limb) ((s32) in2[7])) * ((s32) in[4]) +
 ((limb) ((s32) in2[3])) * ((s32) in[8]) +
 ((limb) ((s32) in2[8])) * ((s32) in[3]) +
 ((limb) ((s32) in2[2])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[2]);
 output[12] =      ((limb) ((s32) in2[6])) * ((s32) in[6]) +
 2 * (((limb) ((s32) in2[5])) * ((s32) in[7]) +
 ((limb) ((s32) in2[7])) * ((s32) in[5]) +
 ((limb) ((s32) in2[3])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[3])) +
 ((limb) ((s32) in2[4])) * ((s32) in[8]) +
 ((limb) ((s32) in2[8])) * ((s32) in[4]);
 output[13] =      ((limb) ((s32) in2[6])) * ((s32) in[7]) +
 ((limb) ((s32) in2[7])) * ((s32) in[6]) +
 ((limb) ((s32) in2[5])) * ((s32) in[8]) +
 ((limb) ((s32) in2[8])) * ((s32) in[5]) +
 ((limb) ((s32) in2[4])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[4]);
 output[14] = 2 * (((limb) ((s32) in2[7])) * ((s32) in[7]) +
 ((limb) ((s32) in2[5])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[5])) +
 ((limb) ((s32) in2[6])) * ((s32) in[8]) +
 ((limb) ((s32) in2[8])) * ((s32) in[6]);
 output[15] =      ((limb) ((s32) in2[7])) * ((s32) in[8]) +
 ((limb) ((s32) in2[8])) * ((s32) in[7]) +
 ((limb) ((s32) in2[6])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[6]);
 output[16] =      ((limb) ((s32) in2[8])) * ((s32) in[8]) +
 2 * (((limb) ((s32) in2[7])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[7]));
 output[17] =      ((limb) ((s32) in2[8])) * ((s32) in[9]) +
 ((limb) ((s32) in2[9])) * ((s32) in[8]);
 output[18] = 2 *  ((limb) ((s32) in2[9])) * ((s32) in[9]);
 }*/

/* Reduce a long form to a short form by taking the input mod 2^255 - 19.
 *
 * On entry: |output[i]| < 14*2^54
 * On exit: |output[0..8]| < 280*2^54 */
static void freduce_degree(limb *output) {
    /* Each of these shifts and adds ends up multiplying the value by 19.
     *
     * For output[0..8], the absolute entry value is < 14*2^54 and we add, at
     * most, 19*14*2^54 thus, on exit, |output[0..8]| < 280*2^54. */
    output[8] += output[18] << 4;
    output[8] += output[18] << 1;
    output[8] += output[18];
    output[7] += output[17] << 4;
    output[7] += output[17] << 1;
    output[7] += output[17];
    output[6] += output[16] << 4;
    output[6] += output[16] << 1;
    output[6] += output[16];
    output[5] += output[15] << 4;
    output[5] += output[15] << 1;
    output[5] += output[15];
    output[4] += output[14] << 4;
    output[4] += output[14] << 1;
    output[4] += output[14];
    output[3] += output[13] << 4;
    output[3] += output[13] << 1;
    output[3] += output[13];
    output[2] += output[12] << 4;
    output[2] += output[12] << 1;
    output[2] += output[12];
    output[1] += output[11] << 4;
    output[1] += output[11] << 1;
    output[1] += output[11];
    output[0] += output[10] << 4;
    output[0] += output[10] << 1;
    output[0] += output[10];
}

#if (-1 & 3) != 3
#error "This code only works on a two's complement system"
#endif

/* return v / 2^26, using only shifts and adds.
 *
 * On entry: v can take any value. */
static inline limb
div_by_2_26(const limb v)
{
    /* High word of v; no shift needed. */
    const uint32_t highword = (uint32_t) (((uint64_t) v) >> 32);
    /* Set to all 1s if v was negative; else set to 0s. */
    const int32_t sign = ((int32_t) highword) >> 31;
    /* Set to 0x3ffffff if v was negative; else set to 0. */
    const int32_t roundoff = ((uint32_t) sign) >> 6;
    /* Should return v / (1<<26) */
    return (v + roundoff) >> 26;
}

/* return v / (2^25), using only shifts and adds.
 *
 * On entry: v can take any value. */
static inline limb
div_by_2_25(const limb v)
{
    /* High word of v; no shift needed*/
    const uint32_t highword = (uint32_t) (((uint64_t) v) >> 32);
    /* Set to all 1s if v was negative; else set to 0s. */
    const int32_t sign = ((int32_t) highword) >> 31;
    /* Set to 0x1ffffff if v was negative; else set to 0. */
    const int32_t roundoff = ((uint32_t) sign) >> 7;
    /* Should return v / (1<<25) */
    return (v + roundoff) >> 25;
}

/* Reduce all coefficients of the short form input so that |x| < 2^26.
 *
 * On entry: |output[i]| < 280*2^54 */
static void freduce_coefficients(limb *output) {
    unsigned i;

    output[10] = 0;

    for (i = 0; i < 10; i += 2) {
        limb over = div_by_2_26(output[i]);
        /* The entry condition (that |output[i]| < 280*2^54) means that over is, at
         * most, 280*2^28 in the first iteration of this loop. This is added to the
         * next limb and we can approximate the resulting bound of that limb by
         * 281*2^54. */
        output[i] -= over << 26;
        output[i+1] += over;

        /* For the first iteration, |output[i+1]| < 281*2^54, thus |over| <
         * 281*2^29. When this is added to the next limb, the resulting bound can
         * be approximated as 281*2^54.
         *
         * For subsequent iterations of the loop, 281*2^54 remains a conservative
         * bound and no overflow occurs. */
        over = div_by_2_25(output[i+1]);
        output[i+1] -= over << 25;
        output[i+2] += over;
    }
    /* Now |output[10]| < 281*2^29 and all other coefficients are reduced. */
    output[0] += output[10] << 4;
    output[0] += output[10] << 1;
    output[0] += output[10];

    output[10] = 0;

    /* Now output[1..9] are reduced, and |output[0]| < 2^26 + 19*281*2^29
     * So |over| will be no more than 2^16. */
    {
        limb over = div_by_2_26(output[0]);
        output[0] -= over << 26;
        output[1] += over;
    }

    /* Now output[0,2..9] are reduced, and |output[1]| < 2^25 + 2^16 < 2^26. The
     * bound on |output[1]| is sufficient to meet our needs. */
}

/* A helpful wrapper around fproduct: output = in * in2.
 *
 * On entry: |in[i]| < 2^27 and |in2[i]| < 2^27.
 *
 * output must be distinct to both inputs. The output is reduced degree
 * (indeed, one need only provide storage for 10 limbs) and |output[i]| < 2^26.
 static void fmul32(limb *output, const limb *in, const limb *in2)
 {
 limb t[19];
 fproduct(t, in, in2);
 //|t[i]| < 14*2^54
 freduce_degree(t);
 freduce_coefficients(t);
 // |t[i]| < 2^26
 memcpy(output, t, sizeof(limb) * 10);
 }*/

/* Square a number: output = in**2
 *
 * output must be distinct from the input. The inputs are reduced coefficient
 * form, the output is not.
 *
 * output[x] <= 14 * the largest product of the input limbs. */
static void fsquare_inner(limb *output, const limb *in) {
    output[0] =       ((limb) ((s32) in[0])) * ((s32) in[0]);
    output[1] =  2 *  ((limb) ((s32) in[0])) * ((s32) in[1]);
    output[2] =  2 * (((limb) ((s32) in[1])) * ((s32) in[1]) +
                      ((limb) ((s32) in[0])) * ((s32) in[2]));
    output[3] =  2 * (((limb) ((s32) in[1])) * ((s32) in[2]) +
                      ((limb) ((s32) in[0])) * ((s32) in[3]));
    output[4] =       ((limb) ((s32) in[2])) * ((s32) in[2]) +
    4 *  ((limb) ((s32) in[1])) * ((s32) in[3]) +
    2 *  ((limb) ((s32) in[0])) * ((s32) in[4]);
    output[5] =  2 * (((limb) ((s32) in[2])) * ((s32) in[3]) +
                      ((limb) ((s32) in[1])) * ((s32) in[4]) +
                      ((limb) ((s32) in[0])) * ((s32) in[5]));
    output[6] =  2 * (((limb) ((s32) in[3])) * ((s32) in[3]) +
                      ((limb) ((s32) in[2])) * ((s32) in[4]) +
                      ((limb) ((s32) in[0])) * ((s32) in[6]) +
                      2 *  ((limb) ((s32) in[1])) * ((s32) in[5]));
    output[7] =  2 * (((limb) ((s32) in[3])) * ((s32) in[4]) +
                      ((limb) ((s32) in[2])) * ((s32) in[5]) +
                      ((limb) ((s32) in[1])) * ((s32) in[6]) +
                      ((limb) ((s32) in[0])) * ((s32) in[7]));
    output[8] =       ((limb) ((s32) in[4])) * ((s32) in[4]) +
    2 * (((limb) ((s32) in[2])) * ((s32) in[6]) +
         ((limb) ((s32) in[0])) * ((s32) in[8]) +
         2 * (((limb) ((s32) in[1])) * ((s32) in[7]) +
              ((limb) ((s32) in[3])) * ((s32) in[5])));
    output[9] =  2 * (((limb) ((s32) in[4])) * ((s32) in[5]) +
                      ((limb) ((s32) in[3])) * ((s32) in[6]) +
                      ((limb) ((s32) in[2])) * ((s32) in[7]) +
                      ((limb) ((s32) in[1])) * ((s32) in[8]) +
                      ((limb) ((s32) in[0])) * ((s32) in[9]));
    output[10] = 2 * (((limb) ((s32) in[5])) * ((s32) in[5]) +
                      ((limb) ((s32) in[4])) * ((s32) in[6]) +
                      ((limb) ((s32) in[2])) * ((s32) in[8]) +
                      2 * (((limb) ((s32) in[3])) * ((s32) in[7]) +
                           ((limb) ((s32) in[1])) * ((s32) in[9])));
    output[11] = 2 * (((limb) ((s32) in[5])) * ((s32) in[6]) +
                      ((limb) ((s32) in[4])) * ((s32) in[7]) +
                      ((limb) ((s32) in[3])) * ((s32) in[8]) +
                      ((limb) ((s32) in[2])) * ((s32) in[9]));
    output[12] =      ((limb) ((s32) in[6])) * ((s32) in[6]) +
    2 * (((limb) ((s32) in[4])) * ((s32) in[8]) +
         2 * (((limb) ((s32) in[5])) * ((s32) in[7]) +
              ((limb) ((s32) in[3])) * ((s32) in[9])));
    output[13] = 2 * (((limb) ((s32) in[6])) * ((s32) in[7]) +
                      ((limb) ((s32) in[5])) * ((s32) in[8]) +
                      ((limb) ((s32) in[4])) * ((s32) in[9]));
    output[14] = 2 * (((limb) ((s32) in[7])) * ((s32) in[7]) +
                      ((limb) ((s32) in[6])) * ((s32) in[8]) +
                      2 *  ((limb) ((s32) in[5])) * ((s32) in[9]));
    output[15] = 2 * (((limb) ((s32) in[7])) * ((s32) in[8]) +
                      ((limb) ((s32) in[6])) * ((s32) in[9]));
    output[16] =      ((limb) ((s32) in[8])) * ((s32) in[8]) +
    4 *  ((limb) ((s32) in[7])) * ((s32) in[9]);
    output[17] = 2 *  ((limb) ((s32) in[8])) * ((s32) in[9]);
    output[18] = 2 *  ((limb) ((s32) in[9])) * ((s32) in[9]);
}

/* fsquare sets output = in^2.
 *
 * On entry: The |in| argument is in reduced coefficients form and |in[i]| <
 * 2^27.
 *
 * On exit: The |output| argument is in reduced coefficients form (indeed, one
 * need only provide storage for 10 limbs) and |out[i]| < 2^26. */
static void
fsquare32(limb *output, const limb *in) {
    limb t[19];
    fsquare_inner(t, in);
    /* |t[i]| < 14*2^54 because the largest product of two limbs will be <
     * 2^(27+27) and fsquare_inner adds together, at most, 14 of those
     * products. */
    freduce_degree(t);
    freduce_coefficients(t);
    /* |t[i]| < 2^26 */
    memcpy(output, t, sizeof(limb) * 10);
}

#if (-32 >> 1) != -16
#error "This code only works when >> does sign-extension on negative numbers"
#endif

/* s32_eq returns 0xffffffff iff a == b and zero otherwise. */
static s32 s32_eq(s32 a, s32 b) {
    a = ~(a ^ b);
    a &= a << 16;
    a &= a << 8;
    a &= a << 4;
    a &= a << 2;
    a &= a << 1;
    return a >> 31;
}

/* s32_gte returns 0xffffffff if a >= b and zero otherwise, where a and b are
 * both non-negative. */
static s32 s32_gte(s32 a, s32 b) {
    a -= b;
    /* a >= 0 iff a >= b. */
    return ~(a >> 31);
}

/* Take a fully reduced polynomial form number and contract it into a
 * little-endian, 32-byte array.
 *
 * On entry: |input_limbs[i]| < 2^26 */
static void fcontract32(u8 *output, limb *input_limbs)
{
    int i;
    int j;
    s32 input[10];
    s32 mask;

    /* |input_limbs[i]| < 2^26, so it's valid to convert to an s32. */
    for (i = 0; i < 10; i++)
        input[i] = (s32)input_limbs[i];

    for (j = 0; j < 2; ++j) {
        for (i = 0; i < 9; ++i) {
            if ((i & 1) == 1) {
                /* This calculation is a time-invariant way to make input[i]
                 * non-negative by borrowing from the next-larger limb. */
                const s32 mask = input[i] >> 31;
                const s32 carry = -((input[i] & mask) >> 25);
                input[i] = input[i] + (carry << 25);
                input[i+1] = input[i+1] - carry;
            } else {
                const s32 mask = input[i] >> 31;
                const s32 carry = -((input[i] & mask) >> 26);
                input[i] = input[i] + (carry << 26);
                input[i+1] = input[i+1] - carry;
            }
        }

        /* There's no greater limb for input[9] to borrow from, but we can multiply
         * by 19 and borrow from input[0], which is valid mod 2^255-19. */
        {
            const s32 mask = input[9] >> 31;
            const s32 carry = -((input[9] & mask) >> 25);
            input[9] = input[9] + (carry << 25);
            input[0] = input[0] - (carry * 19);
        }

        /* After the first iteration, input[1..9] are non-negative and fit within
         * 25 or 26 bits, depending on position. However, input[0] may be
         * negative. */
    }

    /* The first borrow-propagation pass above ended with every limb
     except (possibly) input[0] non-negative.

     If input[0] was negative after the first pass, then it was because of a
     carry from input[9]. On entry, input[9] < 2^26 so the carry was, at most,
     one, since (2**26-1) >> 25 = 1. Thus input[0] >= -19.

     In the second pass, each limb is decreased by at most one. Thus the second
     borrow-propagation pass could only have wrapped around to decrease
     input[0] again if the first pass left input[0] negative *and* input[1]
     through input[9] were all zero.  In that case, input[1] is now 2^25 - 1,
     and this last borrow-propagation step will leave input[1] non-negative. */
    {
        const s32 mask = input[0] >> 31;
        const s32 carry = -((input[0] & mask) >> 26);
        input[0] = input[0] + (carry << 26);
        input[1] = input[1] - carry;
    }

    /* All input[i] are now non-negative. However, there might be values between
     * 2^25 and 2^26 in a limb which is, nominally, 25 bits wide. */
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 9; i++) {
            if ((i & 1) == 1) {
                const s32 carry = input[i] >> 25;
                input[i] &= 0x1ffffff;
                input[i+1] += carry;
            } else {
                const s32 carry = input[i] >> 26;
                input[i] &= 0x3ffffff;
                input[i+1] += carry;
            }
        }

        {
            const s32 carry = input[9] >> 25;
            input[9] &= 0x1ffffff;
            input[0] += 19*carry;
        }
    }

    /* If the first carry-chain pass, just above, ended up with a carry from
     * input[9], and that caused input[0] to be out-of-bounds, then input[0] was
     * < 2^26 + 2*19, because the carry was, at most, two.
     *
     * If the second pass carried from input[9] again then input[0] is < 2*19 and
     * the input[9] -> input[0] carry didn't push input[0] out of bounds. */

    /* It still remains the case that input might be between 2^255-19 and 2^255.
     * In this case, input[1..9] must take their maximum value and input[0] must
     * be >= (2^255-19) & 0x3ffffff, which is 0x3ffffed. */
    mask = s32_gte(input[0], 0x3ffffed);
    for (i = 1; i < 10; i++) {
        if ((i & 1) == 1) {
            mask &= s32_eq(input[i], 0x1ffffff);
        } else {
            mask &= s32_eq(input[i], 0x3ffffff);
        }
    }

    /* mask is either 0xffffffff (if input >= 2^255-19) and zero otherwise. Thus
     * this conditionally subtracts 2^255-19. */
    input[0] -= mask & 0x3ffffed;

    for (i = 1; i < 10; i++) {
        if ((i & 1) == 1) {
            input[i] -= mask & 0x1ffffff;
        } else {
            input[i] -= mask & 0x3ffffff;
        }
    }

    input[1] <<= 2;
    input[2] <<= 3;
    input[3] <<= 5;
    input[4] <<= 6;
    input[6] <<= 1;
    input[7] <<= 3;
    input[8] <<= 4;
    input[9] <<= 6;
#define F(i, s) \
output[s+0] |=  input[i] & 0xff; \
output[s+1]  = (input[i] >> 8) & 0xff; \
output[s+2]  = (input[i] >> 16) & 0xff; \
output[s+3]  = (input[i] >> 24) & 0xff;
    output[0] = 0;
    output[16] = 0;
    F(0,0);
    F(1,3);
    F(2,6);
    F(3,9);
    F(4,12);
    F(5,16);
    F(6,19);
    F(7,22);
    F(8,25);
    F(9,28);
#undef F
}

bits320 bits320_limbs(limb limbs[10])
{
    bits320 output; int32_t i;
    for (i=0; i<10; i++)
        output.uints[i] = limbs[i];
    return(output);
}

static inline bits320 fscalar_product(const bits320 in,const uint64_t scalar)
{
    limb output[10],input[10]; int32_t i;
    for (i=0; i<10; i++)
        input[i] = in.uints[i];
    fscalar_product32(output,input,scalar);
    return(bits320_limbs(output));
}

static inline bits320 fsquare_times(const bits320 in,uint64_t count)
{
    limb output[10],input[10]; int32_t i;
    for (i=0; i<10; i++)
        input[i] = in.uints[i];
    for (i=0; i<count; i++)
    {
        fsquare32(output,input);
        memcpy(input,output,sizeof(input));
    }
    return(bits320_limbs(output));
}

bits256 fmul_donna(bits256 a,bits256 b);
bits256 crecip_donna(bits256 a);

bits256 fcontract(const bits320 in)
{
    bits256 contracted; limb input[10]; int32_t i;
    for (i=0; i<10; i++)
        input[i] = in.uints[i];
    fcontract32(contracted.bytes,input);
    return(contracted);
}

bits320 fmul(const bits320 in,const bits320 in2)
{
    /*limb output[11],input[10],input2[10]; int32_t i;
     for (i=0; i<10; i++)
     {
     input[i] = in.uints[i];
     input2[i] = in2.uints[i];
     }
     fmul32(output,input,input2);
     return(bits320_limbs(output));*/
    bits256 mulval;
    mulval = fmul_donna(fcontract(in),fcontract(in2));
    return(fexpand(mulval));
}

bits256 curve25519(bits256 mysecret,bits256 theirpublic)
{
    int32_t curve25519_donna(uint8_t *mypublic,const uint8_t *secret,const uint8_t *basepoint);
    bits256 rawkey;
    mysecret.bytes[0] &= 0xf8, mysecret.bytes[31] &= 0x7f, mysecret.bytes[31] |= 0x40;
    curve25519_donna(&rawkey.bytes[0],&mysecret.bytes[0],&theirpublic.bytes[0]);
    return(rawkey);
}

#endif


// Input: Q, Q', Q-Q' -> Output: 2Q, Q+Q'
// x2 z2: long form && x3 z3: long form
// x z: short form, destroyed && xprime zprime: short form, destroyed
// qmqp: short form, preserved
static inline void
fmonty(bits320 *x2, bits320 *z2, // output 2Q
       bits320 *x3, bits320 *z3, // output Q + Q'
       bits320 *x, bits320 *z,   // input Q
       bits320 *xprime, bits320 *zprime, // input Q'
       const bits320 qmqp) // input Q - Q'
{
    bits320 origx,origxprime,zzz,xx,zz,xxprime,zzprime;
    origx = *x;
    *x = fsum(*x,*z), fdifference_backwards(z->ulongs,origx.ulongs);  // does x - z
    origxprime = *xprime;
    *xprime = fsum(*xprime,*zprime), fdifference_backwards(zprime->ulongs,origxprime.ulongs);
    xxprime = fmul(*xprime,*z), zzprime = fmul(*x,*zprime);
    origxprime = xxprime;
    xxprime = fsum(xxprime,zzprime), fdifference_backwards(zzprime.ulongs,origxprime.ulongs);
    *x3 = fsquare_times(xxprime,1), *z3 = fmul(fsquare_times(zzprime,1),qmqp);
    xx = fsquare_times(*x,1), zz = fsquare_times(*z,1);
    *x2 = fmul(xx,zz);
    fdifference_backwards(zz.ulongs,xx.ulongs);  // does zz = xx - zz
    zzz = fscalar_product(zz,121665);
    *z2 = fmul(zz,fsum(zzz,xx));
}

// -----------------------------------------------------------------------------
// Maybe swap the contents of two limb arrays (@a and @b), each @len elements
// long. Perform the swap iff @swap is non-zero.
// This function performs the swap without leaking any side-channel information.
// -----------------------------------------------------------------------------
static inline void swap_conditional(bits320 *a,bits320 *b,uint64_t iswap)
{
    int32_t i; const uint64_t swap = -iswap;
    for (i=0; i<5; ++i)
    {
        const uint64_t x = swap & (a->ulongs[i] ^ b->ulongs[i]);
        a->ulongs[i] ^= x, b->ulongs[i] ^= x;
    }
}

// Calculates nQ where Q is the x-coordinate of a point on the curve
// resultx/resultz: the x coordinate of the resulting curve point (short form)
// n: a little endian, 32-byte number
// q: a point of the curve (short form)
void cmult(bits320 *resultx,bits320 *resultz,bits256 secret,const bits320 q);

// Shamelessly copied from donna's code that copied djb's code, changed a little
inline bits320 crecip(const bits320 z)
{
    bits320 a,t0,b,c;
    /* 2 */ a = fsquare_times(z, 1); // a = 2
    /* 8 */ t0 = fsquare_times(a, 2);
    /* 9 */ b = fmul(t0, z); // b = 9
    /* 11 */ a = fmul(b, a); // a = 11
    /* 22 */ t0 = fsquare_times(a, 1);
    /* 2^5 - 2^0 = 31 */ b = fmul(t0, b);
    /* 2^10 - 2^5 */ t0 = fsquare_times(b, 5);
    /* 2^10 - 2^0 */ b = fmul(t0, b);
    /* 2^20 - 2^10 */ t0 = fsquare_times(b, 10);
    /* 2^20 - 2^0 */ c = fmul(t0, b);
    /* 2^40 - 2^20 */ t0 = fsquare_times(c, 20);
    /* 2^40 - 2^0 */ t0 = fmul(t0, c);
    /* 2^50 - 2^10 */ t0 = fsquare_times(t0, 10);
    /* 2^50 - 2^0 */ b = fmul(t0, b);
    /* 2^100 - 2^50 */ t0 = fsquare_times(b, 50);
    /* 2^100 - 2^0 */ c = fmul(t0, b);
    /* 2^200 - 2^100 */ t0 = fsquare_times(c, 100);
    /* 2^200 - 2^0 */ t0 = fmul(t0, c);
    /* 2^250 - 2^50 */ t0 = fsquare_times(t0, 50);
    /* 2^250 - 2^0 */ t0 = fmul(t0, b);
    /* 2^255 - 2^5 */ t0 = fsquare_times(t0, 5);
    /* 2^255 - 21 */ return(fmul(t0, a));
}

bits256 rand256(int32_t privkeyflag);

bits256 curve25519_basepoint9();

bits256 curve25519_keypair(bits256 *pubkeyp);

bits256 curve25519_shared(bits256 privkey,bits256 otherpub);

int32_t curve25519_donna(uint8_t *mypublic,const uint8_t *secret,const uint8_t *basepoint);

uint64_t conv_NXTpassword(unsigned char *mysecret,unsigned char *mypublic,uint8_t *pass,int32_t passlen);

uint256 komodo_kvprivkey(uint256 *pubkeyp,char *passphrase);

uint256 komodo_kvsig(uint8_t *buf,int32_t len,uint256 _privkey);

int32_t komodo_kvsigverify(uint8_t *buf,int32_t len,uint256 _pubkey,uint256 sig);
