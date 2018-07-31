/** @file
 *****************************************************************************
 Assembly code snippets for F[p] finite field arithmetic, used by fp.tcc .
 Specific to x86-64, and used only if USE_ASM is defined.
 On other architectures or without USE_ASM, fp.tcc uses a portable
 C++ implementation instead.
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FP_AUX_TCC_
#define FP_AUX_TCC_

namespace libsnark {

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* addq is faster than adcq, even if preceded by clc */
#define ADD_FIRSTADD                            \
    "movq    (%[B]), %%rax           \n\t"      \
    "addq    %%rax, (%[A])           \n\t"

#define ADD_NEXTADD(ofs)                                \
    "movq    " STR(ofs) "(%[B]), %%rax          \n\t"   \
    "adcq    %%rax, " STR(ofs) "(%[A])          \n\t"

#define ADD_CMP(ofs)                                  \
    "movq    " STR(ofs) "(%[mod]), %%rax   \n\t"      \
    "cmpq    %%rax, " STR(ofs) "(%[A])     \n\t"      \
    "jb      done%=              \n\t"                \
    "ja      subtract%=          \n\t"

#define ADD_FIRSTSUB                            \
    "movq    (%[mod]), %%rax     \n\t"          \
    "subq    %%rax, (%[A])       \n\t"

#define ADD_FIRSTSUB                            \
    "movq    (%[mod]), %%rax     \n\t"          \
    "subq    %%rax, (%[A])       \n\t"

#define ADD_NEXTSUB(ofs)                                \
    "movq    " STR(ofs) "(%[mod]), %%rax    \n\t"       \
    "sbbq    %%rax, " STR(ofs) "(%[A])      \n\t"

#define SUB_FIRSTSUB                            \
    "movq    (%[B]), %%rax\n\t"                 \
    "subq    %%rax, (%[A])\n\t"

#define SUB_NEXTSUB(ofs)                        \
    "movq    " STR(ofs) "(%[B]), %%rax\n\t"     \
    "sbbq    %%rax, " STR(ofs) "(%[A])\n\t"

#define SUB_FIRSTADD                            \
    "movq    (%[mod]), %%rax\n\t"               \
    "addq    %%rax, (%[A])\n\t"

#define SUB_NEXTADD(ofs)                        \
    "movq    " STR(ofs) "(%[mod]), %%rax\n\t"   \
    "adcq    %%rax, " STR(ofs) "(%[A])\n\t"

#define MONT_CMP(ofs)                                 \
    "movq    " STR(ofs) "(%[M]), %%rax   \n\t"        \
    "cmpq    %%rax, " STR(ofs) "(%[tmp])   \n\t"      \
    "jb      done%=              \n\t"                \
    "ja      subtract%=          \n\t"

#define MONT_FIRSTSUB                           \
    "movq    (%[M]), %%rax     \n\t"            \
    "subq    %%rax, (%[tmp])     \n\t"

#define MONT_NEXTSUB(ofs)                               \
    "movq    " STR(ofs) "(%[M]), %%rax    \n\t"         \
    "sbbq    %%rax, " STR(ofs) "(%[tmp])   \n\t"

/*
  The x86-64 Montgomery multiplication here is similar
  to Algorithm 2 (CIOS method) in http://eprint.iacr.org/2012/140.pdf
  and the PowerPC pseudocode of gmp-ecm library (c) Paul Zimmermann and Alexander Kruppa
  (see comments on top of powerpc64/mulredc.m4).
*/

#define MONT_PRECOMPUTE                                                 \
    "xorq    %[cy], %[cy]            \n\t"                              \
    "movq    0(%[A]), %%rax          \n\t"                              \
    "mulq    0(%[B])                 \n\t"                              \
    "movq    %%rax, %[T0]            \n\t"                              \
    "movq    %%rdx, %[T1]            # T1:T0 <- A[0] * B[0] \n\t"       \
    "mulq    %[inv]                  \n\t"                              \
    "movq    %%rax, %[u]             # u <- T0 * inv \n\t"              \
    "mulq    0(%[M])                 \n\t"                              \
    "addq    %[T0], %%rax            \n\t"                              \
    "adcq    %%rdx, %[T1]            \n\t"                              \
    "adcq    $0, %[cy]               # cy:T1 <- (M[0]*u + T1 * b + T0) / b\n\t"

#define MONT_FIRSTITER(j)                                               \
    "xorq    %[T0], %[T0]            \n\t"                              \
    "movq    0(%[A]), %%rax          \n\t"                              \
    "mulq    " STR((j*8)) "(%[B])                 \n\t"                 \
    "addq    %[T1], %%rax            \n\t"                              \
    "movq    %%rax, " STR(((j-1)*8)) "(%[tmp])        \n\t"             \
    "adcq    $0, %%rdx               \n\t"                              \
    "movq    %%rdx, %[T1]            # now T1:tmp[j-1] <-- X[0] * Y[j] + T1\n\t" \
    "movq    " STR((j*8)) "(%[M]), %%rax          \n\t"                 \
    "mulq    %[u]                    \n\t"                              \
    "addq    %%rax, " STR(((j-1)*8)) "(%[tmp])        \n\t"             \
    "adcq    %[cy], %%rdx            \n\t"                              \
    "adcq    $0, %[T0]               \n\t"                              \
    "xorq    %[cy], %[cy]            \n\t"                              \
    "addq    %%rdx, %[T1]            \n\t"                              \
    "adcq    %[T0], %[cy]            # cy:T1:tmp[j-1] <---- (X[0] * Y[j] + T1) + (M[j] * u + cy * b) \n\t"

#define MONT_ITERFIRST(i)                            \
    "xorq    %[cy], %[cy]            \n\t"              \
    "movq    " STR((i*8)) "(%[A]), %%rax          \n\t" \
    "mulq    0(%[B])                 \n\t"              \
    "addq    0(%[tmp]), %%rax        \n\t"              \
    "adcq    8(%[tmp]), %%rdx        \n\t"              \
    "adcq    $0, %[cy]               \n\t"              \
    "movq    %%rax, %[T0]            \n\t"                              \
    "movq    %%rdx, %[T1]            # cy:T1:T0 <- A[i] * B[0] + tmp[1] * b + tmp[0]\n\t" \
    "mulq    %[inv]                  \n\t"                              \
    "movq    %%rax, %[u]             # u <- T0 * inv\n\t"               \
    "mulq    0(%[M])                 \n\t"                              \
    "addq    %[T0], %%rax            \n\t"                              \
    "adcq    %%rdx, %[T1]            \n\t"                              \
    "adcq    $0, %[cy]               # cy:T1 <- (M[0]*u + cy * b * b + T1 * b + T0) / b\n\t"

#define MONT_ITERITER(i, j)                                             \
    "xorq    %[T0], %[T0]            \n\t"                              \
    "movq    " STR((i*8)) "(%[A]), %%rax          \n\t"                 \
    "mulq    " STR((j*8)) "(%[B])                 \n\t"                 \
    "addq    %[T1], %%rax            \n\t"                              \
    "movq    %%rax, " STR(((j-1)*8)) "(%[tmp])        \n\t"                              \
    "adcq    $0, %%rdx               \n\t"                              \
    "movq    %%rdx, %[T1]            # now T1:tmp[j-1] <-- X[i] * Y[j] + T1 \n\t" \
    "movq    " STR((j*8)) "(%[M]), %%rax          \n\t"                 \
    "mulq    %[u]                    \n\t"                              \
    "addq    %%rax, " STR(((j-1)*8)) "(%[tmp])        \n\t"             \
    "adcq    %[cy], %%rdx            \n\t"                              \
    "adcq    $0, %[T0]               \n\t"                              \
    "xorq    %[cy], %[cy]            \n\t"                              \
    "addq    %%rdx, %[T1]            \n\t"                              \
    "adcq    %[T0], %[cy]            # cy:T1:tmp[j-1] <-- (X[i] * Y[j] + T1) + M[j] * u + cy * b \n\t" \
    "addq    " STR(((j+1)*8)) "(%[tmp]), %[T1]       \n\t" \
    "adcq    $0, %[cy]               # cy:T1:tmp[j-1] <-- (X[i] * Y[j] + T1) + M[j] * u + (tmp[j+1] + cy) * b \n\t"

#define MONT_FINALIZE(j)                  \
    "movq    %[T1], " STR((j*8)) "(%[tmp])       \n\t"          \
    "movq    %[cy], " STR(((j+1)*8)) "(%[tmp])       \n\t"

/*
  Comba multiplication and squaring routines are based on the
  public-domain tomsfastmath library by Tom St Denis
  <http://www.libtom.org/>
  <https://github.com/libtom/tomsfastmath/blob/master/src/sqr/fp_sqr_comba.c
  <https://github.com/libtom/tomsfastmath/blob/master/src/mul/fp_mul_comba.c>

  Compared to the above, we save 5-20% of cycles by using careful register
  renaming to implement Comba forward operation.
 */

#define COMBA_3_BY_3_MUL(c0_, c1_, c2_, res_, A_, B_)    \
    asm volatile (                              \
        "movq  0(%[A]), %%rax             \n\t" \
        "mulq  0(%[B])                    \n\t" \
        "movq  %%rax, 0(%[res])           \n\t" \
        "movq  %%rdx, %[c0]               \n\t" \
                                                \
        "xorq  %[c1], %[c1]               \n\t" \
        "movq  0(%[A]), %%rax             \n\t" \
        "mulq  8(%[B])                    \n\t" \
        "addq  %%rax, %[c0]               \n\t" \
        "adcq  %%rdx, %[c1]               \n\t" \
                                                \
        "xorq  %[c2], %[c2]               \n\t" \
        "movq  8(%[A]), %%rax             \n\t" \
        "mulq  0(%[B])                    \n\t" \
        "addq  %%rax, %[c0]               \n\t" \
        "movq  %[c0], 8(%[res])           \n\t" \
        "adcq  %%rdx, %[c1]               \n\t" \
        "adcq  $0, %[c2]                  \n\t" \
                                                \
        "// register renaming (c1, c2, c0)\n\t" \
        "xorq  %[c0], %[c0]               \n\t" \
        "movq  0(%[A]), %%rax             \n\t" \
        "mulq  16(%[B])                   \n\t" \
        "addq  %%rax, %[c1]               \n\t" \
        "adcq  %%rdx, %[c2]               \n\t" \
        "adcq  $0, %[c0]                  \n\t" \
                                                \
        "movq  8(%[A]), %%rax             \n\t" \
        "mulq  8(%[B])                    \n\t" \
        "addq  %%rax, %[c1]               \n\t" \
        "adcq  %%rdx, %[c2]               \n\t" \
        "adcq  $0, %[c0]                  \n\t" \
                                                \
        "movq  16(%[A]), %%rax            \n\t" \
        "mulq  0(%[B])                    \n\t" \
        "addq  %%rax, %[c1]               \n\t" \
        "movq  %[c1], 16(%[res])          \n\t" \
        "adcq  %%rdx, %[c2]               \n\t" \
        "adcq  $0, %[c0]                  \n\t" \
                                                \
        "// register renaming (c2, c0, c1)\n\t" \
        "xorq  %[c1], %[c1]               \n\t" \
        "movq  8(%[A]), %%rax             \n\t" \
        "mulq  16(%[B])                   \n\t" \
        "addq  %%rax, %[c2]               \n\t" \
        "adcq  %%rdx, %[c0]               \n\t" \
        "adcq  $0, %[c1]                  \n\t" \
                                                \
        "movq  16(%[A]), %%rax            \n\t" \
        "mulq  8(%[B])                    \n\t" \
        "addq  %%rax, %[c2]               \n\t" \
        "movq  %[c2], 24(%[res])          \n\t" \
        "adcq  %%rdx, %[c0]               \n\t" \
        "adcq  $0, %[c1]                  \n\t" \
                                                \
        "// register renaming (c0, c1, c2)\n\t" \
        "xorq  %[c2], %[c2]               \n\t" \
        "movq  16(%[A]), %%rax            \n\t" \
        "mulq  16(%[B])                   \n\t" \
        "addq  %%rax, %[c0]               \n\t" \
        "movq  %[c0], 32(%[res])          \n\t" \
        "adcq  %%rdx, %[c1]               \n\t" \
        "movq  %[c1], 40(%[res])          \n\t" \
        : [c0] "=&r" (c0_), [c1] "=&r" (c1_), [c2] "=&r" (c2_) \
        : [res] "r" (res_), [A] "r" (A_), [B] "r" (B_)     \
        : "%rax", "%rdx", "cc", "memory")

#define COMBA_3_BY_3_SQR(c0_, c1_, c2_, res_, A_)    \
    asm volatile (                              \
        "xorq  %[c1], %[c1]               \n\t" \
        "xorq  %[c2], %[c2]               \n\t" \
        "movq  0(%[A]), %%rax             \n\t" \
        "mulq  %%rax                      \n\t" \
        "movq  %%rax, 0(%[res])           \n\t" \
        "movq  %%rdx, %[c0]               \n\t" \
                                                \
        "movq  0(%[A]), %%rax             \n\t" \
        "mulq  8(%[A])                    \n\t" \
        "addq  %%rax, %[c0]               \n\t" \
        "adcq  %%rdx, %[c1]               \n\t" \
        "addq  %%rax, %[c0]               \n\t" \
        "movq  %[c0], 8(%[res])           \n\t" \
        "adcq  %%rdx, %[c1]               \n\t" \
        "adcq  $0, %[c2]                  \n\t" \
                                                \
        "// register renaming (c1, c2, c0)\n\t" \
        "movq  0(%[A]), %%rax             \n\t" \
        "xorq  %[c0], %[c0]               \n\t" \
        "mulq  16(%[A])                   \n\t" \
        "addq  %%rax, %[c1]               \n\t" \
        "adcq  %%rdx, %[c2]               \n\t" \
        "adcq  $0, %[c0]                  \n\t" \
        "addq  %%rax, %[c1]               \n\t" \
        "adcq  %%rdx, %[c2]               \n\t" \
        "adcq  $0, %[c0]                  \n\t" \
                                                \
        "movq  8(%[A]), %%rax             \n\t" \
        "mulq  %%rax                      \n\t" \
        "addq  %%rax, %[c1]               \n\t" \
        "movq  %[c1], 16(%[res])          \n\t" \
        "adcq  %%rdx, %[c2]               \n\t" \
        "adcq  $0, %[c0]                  \n\t" \
                                                \
        "// register renaming (c2, c0, c1)\n\t" \
        "movq  8(%[A]), %%rax             \n\t" \
        "xorq  %[c1], %[c1]               \n\t" \
        "mulq  16(%[A])                   \n\t" \
        "addq  %%rax, %[c2]               \n\t" \
        "adcq  %%rdx, %[c0]               \n\t" \
        "adcq  $0, %[c1]                  \n\t" \
        "addq  %%rax, %[c2]               \n\t" \
        "movq  %[c2], 24(%[res])          \n\t" \
        "adcq  %%rdx, %[c0]               \n\t" \
        "adcq  $0, %[c1]                  \n\t" \
                                                \
        "// register renaming (c0, c1, c2)\n\t" \
        "movq  16(%[A]), %%rax            \n\t" \
        "mulq  %%rax                      \n\t" \
        "addq  %%rax, %[c0]               \n\t" \
        "movq  %[c0], 32(%[res])          \n\t" \
        "adcq  %%rdx, %[c1]               \n\t" \
        "movq  %[c1], 40(%[res])          \n\t" \
                                                \
        : [c0] "=&r" (c0_), [c1] "=&r" (c1_), [c2] "=&r" (c2_) \
        : [res] "r" (res_), [A] "r" (A_) \
        : "%rax", "%rdx", "cc", "memory")

/*
  The Montgomery reduction here is based on Algorithm 14.32 in
  Handbook of Applied Cryptography
  <http://cacr.uwaterloo.ca/hac/about/chap14.pdf>.
 */
#define REDUCE_6_LIMB_PRODUCT(k_, tmp1_, tmp2_, tmp3_, inv_, res_, mod_) \
    __asm__ volatile                               \
        ("///////////////////////////////////\n\t" \
         "movq   0(%[res]), %%rax            \n\t" \
         "mulq   %[modprime]                 \n\t" \
         "movq   %%rax, %[k]                 \n\t" \
                                                   \
         "movq   (%[mod]), %%rax             \n\t" \
         "mulq   %[k]                        \n\t" \
         "movq   %%rax, %[tmp1]              \n\t" \
         "movq   %%rdx, %[tmp2]              \n\t" \
                                                   \
         "xorq   %[tmp3], %[tmp3]            \n\t" \
         "movq   8(%[mod]), %%rax            \n\t" \
         "mulq   %[k]                        \n\t" \
         "addq   %[tmp1], 0(%[res])          \n\t" \
         "adcq   %%rax, %[tmp2]              \n\t" \
         "adcq   %%rdx, %[tmp3]              \n\t" \
                                                   \
         "xorq   %[tmp1], %[tmp1]            \n\t" \
         "movq   16(%[mod]), %%rax           \n\t" \
         "mulq   %[k]                        \n\t" \
         "addq   %[tmp2], 8(%[res])          \n\t" \
         "adcq   %%rax, %[tmp3]              \n\t" \
         "adcq   %%rdx, %[tmp1]              \n\t" \
                                                   \
         "addq   %[tmp3], 16(%[res])         \n\t" \
         "adcq   %[tmp1], 24(%[res])         \n\t" \
         "adcq   $0, 32(%[res])              \n\t" \
         "adcq   $0, 40(%[res])              \n\t" \
                                                   \
         "///////////////////////////////////\n\t" \
         "movq   8(%[res]), %%rax            \n\t" \
         "mulq   %[modprime]                 \n\t" \
         "movq   %%rax, %[k]                 \n\t" \
                                                   \
         "movq   (%[mod]), %%rax             \n\t" \
         "mulq   %[k]                        \n\t" \
         "movq   %%rax, %[tmp1]              \n\t" \
         "movq   %%rdx, %[tmp2]              \n\t" \
                                                   \
         "xorq   %[tmp3], %[tmp3]            \n\t" \
         "movq   8(%[mod]), %%rax            \n\t" \
         "mulq   %[k]                        \n\t" \
         "addq   %[tmp1], 8(%[res])          \n\t" \
         "adcq   %%rax, %[tmp2]              \n\t" \
         "adcq   %%rdx, %[tmp3]              \n\t" \
                                                   \
         "xorq   %[tmp1], %[tmp1]            \n\t" \
         "movq   16(%[mod]), %%rax           \n\t" \
         "mulq   %[k]                        \n\t" \
         "addq   %[tmp2], 16(%[res])         \n\t" \
         "adcq   %%rax, %[tmp3]              \n\t" \
         "adcq   %%rdx, %[tmp1]              \n\t" \
                                                   \
         "addq   %[tmp3], 24(%[res])         \n\t" \
         "adcq   %[tmp1], 32(%[res])         \n\t" \
         "adcq   $0, 40(%[res])              \n\t" \
                                                   \
         "///////////////////////////////////\n\t" \
         "movq   16(%[res]), %%rax           \n\t" \
         "mulq   %[modprime]                 \n\t" \
         "movq   %%rax, %[k]                 \n\t" \
                                                   \
         "movq   (%[mod]), %%rax             \n\t" \
         "mulq   %[k]                        \n\t" \
         "movq   %%rax, %[tmp1]              \n\t" \
         "movq   %%rdx, %[tmp2]              \n\t" \
                                                   \
         "xorq   %[tmp3], %[tmp3]            \n\t" \
         "movq   8(%[mod]), %%rax            \n\t" \
         "mulq   %[k]                        \n\t" \
         "addq   %[tmp1], 16(%[res])         \n\t" \
         "adcq   %%rax, %[tmp2]              \n\t" \
         "adcq   %%rdx, %[tmp3]              \n\t" \
                                                   \
         "xorq   %[tmp1], %[tmp1]            \n\t" \
         "movq   16(%[mod]), %%rax           \n\t" \
         "mulq   %[k]                        \n\t" \
         "addq   %[tmp2], 24(%[res])         \n\t" \
         "adcq   %%rax, %[tmp3]              \n\t" \
         "adcq   %%rdx, %[tmp1]              \n\t" \
                                                   \
         "addq   %[tmp3], 32(%[res])         \n\t" \
         "adcq   %[tmp1], 40(%[res])         \n\t" \
         : [k] "=&r" (k_), [tmp1] "=&r" (tmp1_), [tmp2] "=&r" (tmp2_), [tmp3] "=&r" (tmp3_) \
         : [modprime] "r" (inv_), [res] "r" (res_), [mod] "r" (mod_) \
         : "%rax", "%rdx", "cc", "memory")

} // libsnark
#endif // FP_AUX_TCC_
