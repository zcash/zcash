// Equihash solver
// Copyright (c) 2016-2016 John Tromp

#include "blake/blake2.h"
#ifdef __APPLE__
#include "osx_barrier.h"
#include <machine/endian.h>
#include <libkern/OSByteOrder.h>
#define htole32(x) OSSwapHostToLittleInt32(x)
#else
#include <endian.h>
#endif
#include <stdint.h> // for types uint32_t,uint64_t
#include <string.h> // for functions memset
#include <stdlib.h> // for function qsort

typedef uint32_t u32;
typedef unsigned char uchar;

// algorithm parameters, prefixed with W to reduce include file conflicts

#ifndef WN
#define WN	200
#endif

#ifndef WK
#define WK	9
#endif

#define NDIGITS		(WK+1)
#define DIGITBITS	(WN/(NDIGITS))

static const u32 PROOFSIZE = 1<<WK;
static const u32 BASE = 1<<DIGITBITS;
static const u32 NHASHES = 2*BASE;
static const u32 HASHESPERBLAKE = 512/WN;
static const u32 HASHOUT = HASHESPERBLAKE*WN/8;

typedef u32 proof[PROOFSIZE];

void setheader(blake2b_state *ctx, const char *header, const u32 headerlen, u32 nce) {
  uint32_t le_N = htole32(WN);
  uint32_t le_K = htole32(WK);
  uchar personal[] = "ZcashPoW01230123";
  memcpy(personal+8,  &le_N, 4);
  memcpy(personal+12, &le_K, 4);
  blake2b_param P[1];
  P->digest_length = HASHOUT;
  P->key_length    = 0;
  P->fanout        = 1;
  P->depth         = 1;
  P->leaf_length   = 0;
  P->node_offset   = 0;
  P->node_depth    = 0;
  P->inner_length  = 0;
  memset(P->reserved, 0, sizeof(P->reserved));
  memset(P->salt,     0, sizeof(P->salt));
  memcpy(P->personal, (const uint8_t *)personal, 16);
  blake2b_init_param(ctx, P);
  blake2b_update(ctx, (const uchar *)header, headerlen);
  uchar nonce[32];
  memset(nonce, 0, 32);
  uint32_t le_nonce = htole32(nce);
  memcpy(nonce,  &le_nonce, 4);
  blake2b_update(ctx, nonce, 32);
}

enum verify_code { POW_OK, POW_DUPLICATE, POW_OUT_OF_ORDER, POW_NONZERO_XOR };
const char *errstr[] = { "OK", "duplicate index", "indices out of order", "nonzero xor" };

void genhash(blake2b_state *ctx, u32 idx, uchar *hash) {
  blake2b_state state = *ctx;
  u32 leb = htole32(idx / HASHESPERBLAKE);
  blake2b_update(&state, (uchar *)&leb, sizeof(u32));
  uchar blakehash[HASHOUT];
  blake2b_final(&state, blakehash, HASHOUT);
  memcpy(hash, blakehash + (idx % HASHESPERBLAKE) * WN/8, WN/8);
}

int verifyrec(blake2b_state *ctx, u32 *indices, uchar *hash, int r) {
  if (r == 0) {
    genhash(ctx, *indices, hash);
    return POW_OK;
  }
  u32 *indices1 = indices + (1 << (r-1));
  if (*indices >= *indices1)
    return POW_OUT_OF_ORDER;
  uchar hash0[WN/8], hash1[WN/8];
  int vrf0 = verifyrec(ctx, indices,  hash0, r-1);
  if (vrf0 != POW_OK)
    return vrf0;
  int vrf1 = verifyrec(ctx, indices1, hash1, r-1);
  if (vrf1 != POW_OK)
    return vrf1;
  for (int i=0; i < WN/8; i++)
    hash[i] = hash0[i] ^ hash1[i];
  int i, b = r * DIGITBITS;
  for (i = 0; i < b/8; i++)
    if (hash[i])
      return POW_NONZERO_XOR;
  if ((b%8) && hash[i] >> (8-(b%8)))
    return POW_NONZERO_XOR;
  return POW_OK;
}

int compu32(const void *pa, const void *pb) {
  u32 a = *(u32 *)pa, b = *(u32 *)pb;
  return a<b ? -1 : a==b ? 0 : +1;
}

bool duped(proof prf) {
  proof sortprf;
  memcpy(sortprf, prf, sizeof(proof));
  qsort(sortprf, PROOFSIZE, sizeof(u32), &compu32);
  for (u32 i=1; i<PROOFSIZE; i++)
    if (sortprf[i] <= sortprf[i-1])
      return true;
  return false;
}

// verify Wagner conditions
int verify(u32 indices[PROOFSIZE], const char *header, const u32 headerlen, const u32 nonce) {
  if (duped(indices))
    return POW_DUPLICATE;
  blake2b_state ctx;
  setheader(&ctx, header, headerlen, nonce);
  uchar hash[WN/8];
  return verifyrec(&ctx, indices, hash, WK);
}
