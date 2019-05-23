#include <stdint.h>

/*
#ifndef CPUCYCLES_H
#define CPUCYCLES_H

#ifdef DBENCH
#define DBENCH_START() uint64_t time = cpucycles_start()
#define DBENCH_STOP(t) t += cpucycles_stop() - time - timing_overhead
#else
#define DBENCH_START()
#define DBENCH_STOP(t)
#endif

#ifdef USE_RDPMC  // Needs echo 2 > /sys/devices/cpu/rdpmc
#ifdef SERIALIZE_RDC

static inline uint64_t cpucycles_start(void) {
    const uint32_t ecx = (1U << 30) + 1;
    uint64_t result;
    
    asm volatile("cpuid; movl %1,%%ecx; rdpmc; shlq $32,%%rdx; orq %%rdx,%%rax"
                 : "=&a" (result) : "r" (ecx) : "rbx", "rcx", "rdx");
    
    return result;
}

static inline uint64_t cpucycles_stop(void) {
    const uint32_t ecx = (1U << 30) + 1;
    uint64_t result, dummy;
    
    asm volatile("rdpmc; shlq $32,%%rdx; orq %%rdx,%%rax; movq %%rax,%0; cpuid"
                 : "=&r" (result), "=c" (dummy) : "c" (ecx) : "rax", "rbx", "rdx");
    
    return result;
}

#else

static inline uint64_t cpucycles_start(void) {
    const uint32_t ecx = (1U << 30) + 1;
    uint64_t result;
    
    asm volatile("rdpmc; shlq $32,%%rdx; orq %%rdx,%%rax"
                 : "=a" (result) : "c" (ecx) : "rdx");
    
    return result;
}

static inline uint64_t cpucycles_stop(void) {
    const uint32_t ecx = (1U << 30) + 1;
    uint64_t result;
    
    asm volatile("rdpmc; shlq $32,%%rdx; orq %%rdx,%%rax"
                 : "=a" (result) : "c" (ecx) : "rdx");
    
    return result;
}

#endif
#else
#ifdef SERIALIZE_RDC

static inline uint64_t cpucycles_start(void) {
    uint64_t result;
    
    asm volatile("cpuid; rdtsc; shlq $32,%%rdx; orq %%rdx,%%rax"
                 : "=a" (result) : : "%rbx", "%rcx", "%rdx");
    
    return result;
}

static inline uint64_t cpucycles_stop(void) {
    uint64_t result;
    
    asm volatile("rdtscp; shlq $32,%%rdx; orq %%rdx,%%rax; mov %%rax,%0; cpuid"
                 : "=r" (result) : : "%rax", "%rbx", "%rcx", "%rdx");
    
    return result;
}

#else

static inline uint64_t cpucycles_start(void) {
    uint64_t result;
    
    asm volatile("rdtsc; shlq $32,%%rdx; orq %%rdx,%%rax"
                 : "=a" (result) : : "%rdx");
    
    return result;
}

static inline uint64_t cpucycles_stop(void) {
    uint64_t result;
    
    asm volatile("rdtsc; shlq $32,%%rdx; orq %%rdx,%%rax"
                 : "=a" (result) : : "%rdx");
    
    return result;
}

#endif
#endif

int64_t cpucycles_overhead(void);

#endif*/

#ifndef FIPS202_H
#define FIPS202_H


#define SHAKE128_RATE 168
#define SHAKE256_RATE 136

void shake128_absorb(uint64_t *s,
                     const uint8_t *input,
                     int32_t inlen);

void shake128_squeezeblocks(uint8_t *output,
                            int32_t nblocks,
                            uint64_t *s);

void shake256_absorb(uint64_t *s,
                     const uint8_t *input,
                     int32_t inlen);

void shake256_squeezeblocks(uint8_t *output,
                            int32_t nblocks,
                            uint64_t *s);

void shake128(uint8_t *output,
              int32_t outlen,
              const uint8_t *input,
              int32_t inlen);

void shake256(uint8_t *output,
              int32_t outlen,
              const uint8_t *input,
              int32_t inlen);

#endif

#ifndef PARAMS_H
#define PARAMS_H

#ifndef MODE
#define MODE 3
#endif

#define SEEDBYTES 32U
#define CRHBYTES 48U
#define N 256U
#define Q 8380417U
#define QBITS 23U
#define ROOT_OF_UNITY 1753U
#define D 14U
#define GAMMA1 ((Q - 1U)/16U)
#define GAMMA2 (GAMMA1/2U)
#define ALPHA (2U*GAMMA2)

#if MODE == 0
#define K 3U
#define L 2U
#define ETA 7U
#define SETABITS 4U
#define BETA 375U
#define OMEGA 64U

#elif MODE == 1
#define K 4U
#define L 3U
#define ETA 6U
#define SETABITS 4U
#define BETA 325U
#define OMEGA 80U

#elif MODE == 2
#define K 5U
#define L 4U
#define ETA 5U
#define SETABITS 4U
#define BETA 275U
#define OMEGA 96U

#elif MODE == 3
#define K 6U
#define L 5U
#define ETA 3U
#define SETABITS 3U
#define BETA 175U
#define OMEGA 120U

#endif

#define POL_SIZE_PACKED ((N*QBITS)/8)
#define POLT1_SIZE_PACKED ((N*(QBITS - D))/8)
#define POLT0_SIZE_PACKED ((N*D)/8)
#define POLETA_SIZE_PACKED ((N*SETABITS)/8)
#define POLZ_SIZE_PACKED ((N*(QBITS - 3))/8)
#define POLW1_SIZE_PACKED ((N*4)/8)
#define POLVECK_SIZE_PACKED (K*POL_SIZE_PACKED)
#define POLVECL_SIZE_PACKED (L*POL_SIZE_PACKED)

#define CRYPTO_PUBLICKEYBYTES (SEEDBYTES + K*POLT1_SIZE_PACKED)
#define CRYPTO_SECRETKEYBYTES (2*SEEDBYTES + (L + K)*POLETA_SIZE_PACKED + CRHBYTES + K*POLT0_SIZE_PACKED)
#define CRYPTO_BYTES (L*POLZ_SIZE_PACKED + (OMEGA + K) + (N/8 + 8))

#endif
#ifndef POLY_H
#define POLY_H

//#include <stdint.h>
//#include "params.h"
//#include "fips202.h"

typedef struct {
    uint32_t coeffs[N];
} poly __attribute__((aligned(32)));

void poly_reduce(poly *a);
void poly_csubq(poly *a);
void poly_freeze(poly *a);

void poly_add(poly *c, const poly *a, const poly *b);
void poly_sub(poly *c, const poly *a, const poly *b);
void poly_neg(poly *a);
void poly_shiftl(poly *a, uint32_t k);

void poly_ntt(poly *a);
void poly_invntt_montgomery(poly *a);
void poly_pointwise_invmontgomery(poly *c, const poly *a, const poly *b);

void poly_power2round(poly *a1, poly *a0, const poly *a);
void poly_decompose(poly *a1, poly *a0, const poly *a);
uint32_t poly_make_hint(poly *h, const poly *a, const poly *b);
void poly_use_hint(poly *a, const poly *b, const poly *h);

int  poly_chknorm(const poly *a, uint32_t B);
void poly_uniform(poly *a, const uint8_t *buf);
void poly_uniform_eta(poly *a,
                      const uint8_t seed[SEEDBYTES],
                      uint8_t nonce);
void poly_uniform_gamma1m1(poly *a,
                           const uint8_t seed[SEEDBYTES + CRHBYTES],
                           uint16_t nonce);

void polyeta_pack(uint8_t *r, const poly *a);
void polyeta_unpack(poly *r, const uint8_t *a);

void polyt1_pack(uint8_t *r, const poly *a);
void polyt1_unpack(poly *r, const uint8_t *a);

void polyt0_pack(uint8_t *r, const poly *a);
void polyt0_unpack(poly *r, const uint8_t *a);

void polyz_pack(uint8_t *r, const poly *a);
void polyz_unpack(poly *r, const uint8_t *a);

void polyw1_pack(uint8_t *r, const poly *a);
#endif
#ifndef POLYVEC_H
#define POLYVEC_H

//#include <stdint.h>
//#include "params.h"
//#include "poly.h"

/* Vectors of polynomials of length L */
typedef struct {
    poly vec[L];
} polyvecl;

void polyvecl_freeze(polyvecl *v);

void polyvecl_add(polyvecl *w, const polyvecl *u, const polyvecl *v);

void polyvecl_ntt(polyvecl *v);
void polyvecl_pointwise_acc_invmontgomery(poly *w,
                                          const polyvecl *u,
                                          const polyvecl *v);

int polyvecl_chknorm(const polyvecl *v, uint32_t B);



/* Vectors of polynomials of length K */
typedef struct {
    poly vec[K];
} polyveck;

void polyveck_reduce(polyveck *v);
void polyveck_csubq(polyveck *v);
void polyveck_freeze(polyveck *v);

void polyveck_add(polyveck *w, const polyveck *u, const polyveck *v);
void polyveck_sub(polyveck *w, const polyveck *u, const polyveck *v);
void polyveck_shiftl(polyveck *v, uint32_t k);

void polyveck_ntt(polyveck *v);
void polyveck_invntt_montgomery(polyveck *v);

int polyveck_chknorm(const polyveck *v, uint32_t B);

void polyveck_power2round(polyveck *v1, polyveck *v0, const polyveck *v);
void polyveck_decompose(polyveck *v1, polyveck *v0, const polyveck *v);
uint32_t polyveck_make_hint(polyveck *h,
                                const polyveck *u,
                                const polyveck *v);
void polyveck_use_hint(polyveck *w, const polyveck *v, const polyveck *h);

#endif

#ifndef NTT_H
#define NTT_H

//#include <stdint.h>
//#include "params.h"

void ntt(uint32_t p[N]);
void invntt_frominvmont(uint32_t p[N]);

#endif
#ifndef PACKING_H
#define PACKING_H

//#include "params.h"
//#include "polyvec.h"

void pack_pk(uint8_t pk[CRYPTO_PUBLICKEYBYTES],
             const uint8_t rho[SEEDBYTES], const polyveck *t1);
void pack_sk(uint8_t sk[CRYPTO_SECRETKEYBYTES],
             const uint8_t rho[SEEDBYTES],
             const uint8_t key[SEEDBYTES],
             const uint8_t tr[CRHBYTES],
             const polyvecl *s1,
             const polyveck *s2,
             const polyveck *t0);
void pack_sig(uint8_t sig[CRYPTO_BYTES],
              const polyvecl *z, const polyveck *h, const poly *c);

void unpack_pk(uint8_t rho[SEEDBYTES], polyveck *t1,
               const uint8_t pk[CRYPTO_PUBLICKEYBYTES]);
void unpack_sk(uint8_t rho[SEEDBYTES],
               uint8_t key[SEEDBYTES],
               uint8_t tr[CRHBYTES],
               polyvecl *s1,
               polyveck *s2,
               polyveck *t0,
               const uint8_t sk[CRYPTO_SECRETKEYBYTES]);
int unpack_sig(polyvecl *z, polyveck *h, poly *c,
                const uint8_t sig[CRYPTO_BYTES]);

#endif
#ifndef REDUCE_H
#define REDUCE_H

//#include <stdint.h>

#define MONT 4193792U // 2^32 % Q
#define QINV 4236238847U // -q^(-1) mod 2^32

/* a <= Q*2^32 => r < 2*Q */
uint32_t montgomery_reduce(uint64_t a);

/* r < 2*Q */
uint32_t reduce32(uint32_t a);

/* a < 2*Q => r < Q */
uint32_t csubq(uint32_t a);

/* r < Q */
uint32_t freeze(uint32_t a);

#endif
#ifndef ROUNDING_H
#define ROUNDING_H

//#include <stdint.h>

uint32_t power2round(const uint32_t a, uint32_t *a0);
uint32_t decompose(uint32_t a, uint32_t *a0);
uint32_t make_hint(const uint32_t a, const uint32_t b);
uint32_t use_hint(const uint32_t a, const uint32_t hint);

#endif
#ifndef SIGN_H
#define SIGN_H

//#include "params.h"
//#include "poly.h"
//#include "polyvec.h"

void expand_mat(polyvecl mat[K], const uint8_t rho[SEEDBYTES]);
void challenge(poly *c, const uint8_t mu[CRHBYTES],
               const polyveck *w1);

int crypto_sign_keypair(uint8_t *pk, uint8_t *sk);

int crypto_sign(uint8_t *sm, int32_t *smlen,
                const uint8_t *msg, int32_t len,
                const uint8_t *sk);

int crypto_sign_open(uint8_t *m, int32_t *mlen,
                     const uint8_t *sm, int32_t smlen,
                     const uint8_t *pk);

#endif

#ifndef API_H
#define API_H

#ifndef MODE
#define MODE 3
#endif

#if MODE == 0
#if CRYPTO_PUBLICKEYBYTES -896U
CRYPTO_PUBLICKEYBYTES size error
#endif
#if CRYPTO_SECRETKEYBYTES -2096U
CRYPTO_SECRETKEYBYTES size error
#endif
#if CRYPTO_BYTES -1387U
CRYPTO_BYTES size error
#endif

#elif MODE == 1
#if CRYPTO_PUBLICKEYBYTES -1184U
CRYPTO_PUBLICKEYBYTES size error
#endif
#if CRYPTO_SECRETKEYBYTES -2800U
CRYPTO_SECRETKEYBYTES size error
#endif
#if CRYPTO_BYTES -2044U
CRYPTO_BYTES size error
#endif

#elif MODE == 2
#if CRYPTO_PUBLICKEYBYTES -1472U
CRYPTO_PUBLICKEYBYTES size error
#endif
#if CRYPTO_SECRETKEYBYTES -3504U
CRYPTO_SECRETKEYBYTES size error
#endif
#if CRYPTO_BYTES -2701U
CRYPTO_BYTES size error
#endif

#elif MODE == 3
#if CRYPTO_PUBLICKEYBYTES -1760U
CRYPTO_PUBLICKEYBYTES size error
#endif
#if CRYPTO_SECRETKEYBYTES -3856U
CRYPTO_SECRETKEYBYTES size error
#endif
#if CRYPTO_BYTES -3366U
CRYPTO_BYTES size error
#endif

#endif

#define CRYPTO_ALGNAME "Dilithium"

int crypto_sign_keypair(uint8_t *pk, uint8_t *sk);

int crypto_sign(uint8_t *sm, int32_t *smlen,
                const uint8_t *msg, int32_t len,
                const uint8_t *sk);

int crypto_sign_open(uint8_t *m, int32_t *mlen,
                     const uint8_t *sm, int32_t smlen,
                     const uint8_t *pk);

#endif
