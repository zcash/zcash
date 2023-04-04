// Equihash solver
// Copyright (c) 2016-2023 John Tromp, The Zcash developers

#ifndef ZCASH_POW_TROMP_EQUI_H
#define ZCASH_POW_TROMP_EQUI_H

#ifdef __APPLE__
#include "pow/tromp/osx_barrier.h"
#endif
#include "compat/endian.h"

#include <stdint.h> // for types uint32_t,uint64_t
#include <string.h> // for functions memset
#include <stdlib.h> // for function qsort

#include <rust/blake2b.h>
#include <rust/constants.h>
#include <rust/cxx.h>

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


enum verify_code { POW_OK, POW_DUPLICATE, POW_OUT_OF_ORDER, POW_NONZERO_XOR };
extern const char *errstr[4];

void genhash(const rust::Box<blake2b::State>& ctx, u32 idx, uchar *hash);
int verifyrec(const rust::Box<blake2b::State>& ctx, u32 *indices, uchar *hash, int r);
int compu32(const void *pa, const void *pb);
bool duped(proof prf);

// verify Wagner conditions
int verify(u32 indices[PROOFSIZE], const rust::Box<blake2b::State> ctx);

bool equihash_solve(
  const rust::Box<blake2b::State>& curr_state,
  std::function<void()>& incrementRuns,
  std::function<bool(size_t s, const std::vector<uint32_t>&)>& checkSolution);

#endif // ZCASH_POW_TROMP_EQUI_H
