// Equihash solver
// Copyright (c) 2016 John Tromp, The Zcash developers

// Fix N, K, such that n = N/(k+1) is integer
// Fix M = 2^{n+1} hashes each of length N bits,
// H_0, ... , H_{M-1}, generated fom (n+1)-bit indices.
// Problem: find binary tree on 2^K distinct indices,
// for which the exclusive-or of leaf hashes is all 0s.
// Additionally, it should satisfy the Wagner conditions:
// for each height i subtree, the exclusive-or
// of its 2^i corresponding hashes starts with i*n 0 bits,
// and for i>0 the leftmost leaf of its left subtree
// is less than the leftmost leaf of its right subtree

// The algorithm below solves this by maintaining the trees
// in a graph of K layers, each split into buckets
// with buckets indexed by the first n-RESTBITS bits following
// the i*n 0s, each bucket having 4 * 2^RESTBITS slots,
// twice the number of subtrees expected to land there.

#include "pow/tromp/equi.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

typedef uint16_t u16;
typedef uint64_t u64;

#ifdef EQUIHASH_TROMP_ATOMIC
#include <atomic>
typedef std::atomic<u32> au32;
#else
typedef u32 au32;
#endif

#ifndef RESTBITS
#define RESTBITS	8
#endif

// 2_log of number of buckets
#define BUCKBITS (DIGITBITS-RESTBITS)

#ifndef SAVEMEM
#if RESTBITS == 4
// can't save memory in such small buckets
#define SAVEMEM 1
#elif RESTBITS >= 8
// take advantage of law of large numbers (sum of 2^8 random numbers)
// this reduces (200,9) memory to under 144MB, with negligible discarding
#define SAVEMEM 9/14
#endif
#endif

// number of buckets
static const u32 NBUCKETS = 1<<BUCKBITS;
// 2_log of number of slots per bucket
static const u32 SLOTBITS = RESTBITS+1+1;
static const u32 SLOTRANGE = 1<<SLOTBITS;
static const u32 SLOTMSB = 1<<(SLOTBITS-1);
// number of slots per bucket
static const u32 NSLOTS = SLOTRANGE * SAVEMEM;
// number of per-xhash slots
static const u32 XFULL = 16;
// SLOTBITS mask
static const u32 SLOTMASK = SLOTRANGE-1;
// number of possible values of xhash (rest of n) bits
static const u32 NRESTS = 1<<RESTBITS;
// number of blocks of hashes extracted from single 512 bit blake2b output
static const u32 NBLOCKS = (NHASHES+HASHESPERBLAKE-1)/HASHESPERBLAKE;
// nothing larger found in 100000 runs
static const u32 MAXSOLS = 8;

// tree node identifying its children as two different slots in
// a bucket on previous layer with the same rest bits (x-tra hash)
struct tree {
  u32 bid_s0_s1; // manual bitfields

  tree(const u32 idx) {
    bid_s0_s1 = idx;
  }
  tree(const u32 bid, const u32 s0, const u32 s1) {
#ifdef SLOTDIFF
    u32 ds10 = (s1 - s0) & SLOTMASK;
    if (ds10 & SLOTMSB) {
      bid_s0_s1 = (((bid << SLOTBITS) | s1) << (SLOTBITS-1)) | (SLOTMASK & ~ds10);
    } else {
      bid_s0_s1 = (((bid << SLOTBITS) | s0) << (SLOTBITS-1)) | (ds10 - 1);
    }
#else
    bid_s0_s1 = (((bid << SLOTBITS) | s0) << SLOTBITS) | s1;
#endif
  }
  u32 getindex() const {
    return bid_s0_s1;
  }
  u32 bucketid() const {
#ifdef SLOTDIFF
    return bid_s0_s1 >> (2 * SLOTBITS - 1);
#else
    return bid_s0_s1 >> (2 * SLOTBITS);
#endif
  }
  u32 slotid0() const {
#ifdef SLOTDIFF
    return (bid_s0_s1 >> (SLOTBITS-1)) & SLOTMASK;
#else
    return (bid_s0_s1 >> SLOTBITS) & SLOTMASK;
#endif
  }
  u32 slotid1() const {
#ifdef SLOTDIFF
    return (slotid0() + 1 + (bid_s0_s1 & (SLOTMASK>>1))) & SLOTMASK;
#else
    return bid_s0_s1 & SLOTMASK;
#endif
  }
};

union hashunit {
  u32 word;
  uchar bytes[sizeof(u32)];
};

#define WORDS(bits)	((bits + 31) / 32)
#define HASHWORDS0 WORDS(WN - DIGITBITS + RESTBITS)
#define HASHWORDS1 WORDS(WN - 2*DIGITBITS + RESTBITS)

struct slot0 {
  tree attr;
  hashunit hash[HASHWORDS0];
};

struct slot1 {
  tree attr;
  hashunit hash[HASHWORDS1];
};

// a bucket is NSLOTS treenodes
typedef slot0 bucket0[NSLOTS];
typedef slot1 bucket1[NSLOTS];
// the N-bit hash consists of K+1 n-bit "digits"
// each of which corresponds to a layer of NBUCKETS buckets
typedef bucket0 digit0[NBUCKETS];
typedef bucket1 digit1[NBUCKETS];

// size (in bytes) of hash in round 0 <= r < WK
u32 hashsize(const u32 r) {
  const u32 hashbits = WN - (r+1) * DIGITBITS + RESTBITS;
  return (hashbits + 7) / 8;
}

u32 hashwords(u32 bytes) {
  return (bytes + 3) / 4;
}

// manages hash and tree data
struct htalloc {
  u32 *heap0;
  u32 *heap1;
  bucket0 *trees0[(WK+1)/2];
  bucket1 *trees1[WK/2];
  u32 alloced;
  htalloc() {
    alloced = 0;
  }
  void alloctrees() {
// optimize xenoncat's fixed memory layout, avoiding any waste
// digit  trees  hashes  trees hashes
// 0      0 A A A A A A   . . . . . .
// 1      0 A A A A A A   1 B B B B B
// 2      0 2 C C C C C   1 B B B B B
// 3      0 2 C C C C C   1 3 D D D D
// 4      0 2 4 E E E E   1 3 D D D D
// 5      0 2 4 E E E E   1 3 5 F F F
// 6      0 2 4 6 . G G   1 3 5 F F F
// 7      0 2 4 6 . G G   1 3 5 7 H H
// 8      0 2 4 6 8 . I   1 3 5 7 H H
    assert(DIGITBITS >= 16); // ensures hashes shorten by 1 unit every 2 digits
    heap0 = (u32 *)alloc(1, sizeof(digit0));
    heap1 = (u32 *)alloc(1, sizeof(digit1));
    for (int r=0; r<WK; r++)
      if ((r&1) == 0)
        trees0[r/2]  = (bucket0 *)(heap0 + r/2);
      else
        trees1[r/2]  = (bucket1 *)(heap1 + r/2);
  }
  void dealloctrees() {
    free(heap0);
    free(heap1);
  }
  void *alloc(const u32 n, const u32 sz) {
    void *mem  = calloc(n, sz);
    assert(mem);
    alloced += n * sz;
    return mem;
  }
};

typedef au32 bsizes[NBUCKETS];

u32 min(const u32 a, const u32 b) {
  return a < b ? a : b;
}

struct equi {
  crypto_generichash_blake2b_state blake_ctx;
  htalloc hta;
  bsizes *nslots; // PUT IN BUCKET STRUCT
  proof *sols;
  au32 nsols;
  u32 nthreads;
  u32 xfull;
  u32 hfull;
  u32 bfull;
  pthread_barrier_t barry;
  equi(const u32 n_threads) {
    assert(sizeof(hashunit) == 4);
    nthreads = n_threads;
    const int err = pthread_barrier_init(&barry, NULL, nthreads);
    assert(!err);
    hta.alloctrees();
    nslots = (bsizes *)hta.alloc(2 * NBUCKETS, sizeof(au32));
    sols   =  (proof *)hta.alloc(MAXSOLS, sizeof(proof));
  }
  ~equi() {
    hta.dealloctrees();
    free(nslots);
    free(sols);
  }
  void setstate(const crypto_generichash_blake2b_state *ctx) {
    blake_ctx = *ctx;
    memset(nslots, 0, NBUCKETS * sizeof(au32)); // only nslots[0] needs zeroing
    nsols = 0;
  }
  u32 getslot(const u32 r, const u32 bucketi) {
#ifdef EQUIHASH_TROMP_ATOMIC
    return std::atomic_fetch_add_explicit(&nslots[r&1][bucketi], 1U, std::memory_order_relaxed);
#else
    return nslots[r&1][bucketi]++;
#endif
  }
  u32 getnslots(const u32 r, const u32 bid) { // SHOULD BE METHOD IN BUCKET STRUCT
    au32 &nslot = nslots[r&1][bid];
    const u32 n = min(nslot, NSLOTS);
    nslot = 0;
    return n;
  }
  void orderindices(u32 *indices, u32 size) {
    if (indices[0] > indices[size]) {
      for (u32 i=0; i < size; i++) {
        const u32 tmp = indices[i];
        indices[i] = indices[size+i];
        indices[size+i] = tmp;
      }
    }
  }
  void listindices0(u32 r, const tree t, u32 *indices) {
    if (r == 0) {
      *indices = t.getindex();
      return;
    }
    const bucket1 &buck = hta.trees1[--r/2][t.bucketid()];
    const u32 size = 1 << r;
    u32 *indices1 = indices + size;
    listindices1(r, buck[t.slotid0()].attr, indices);
    listindices1(r, buck[t.slotid1()].attr, indices1);
    orderindices(indices, size);
  }
  void listindices1(u32 r, const tree t, u32 *indices) {
    const bucket0 &buck = hta.trees0[--r/2][t.bucketid()];
    const u32 size = 1 << r;
    u32 *indices1 = indices + size;
    listindices0(r, buck[t.slotid0()].attr, indices);
    listindices0(r, buck[t.slotid1()].attr, indices1);
    orderindices(indices, size);
  }
  void candidate(const tree t) {
    proof prf;
    listindices1(WK, t, prf); // assume WK odd
    qsort(prf, PROOFSIZE, sizeof(u32), &compu32);
    for (u32 i=1; i<PROOFSIZE; i++)
      if (prf[i] <= prf[i-1])
        return;
#ifdef EQUIHASH_TROMP_ATOMIC
    u32 soli = std::atomic_fetch_add_explicit(&nsols, 1U, std::memory_order_relaxed);
#else
    u32 soli = nsols++;
#endif
    if (soli < MAXSOLS)
      listindices1(WK, t, sols[soli]); // assume WK odd
  }
  void showbsizes(u32 r) {
#if defined(HIST) || defined(SPARK) || defined(LOGSPARK)
    u32 binsizes[65];
    memset(binsizes, 0, 65 * sizeof(u32));
    for (u32 bucketid = 0; bucketid < NBUCKETS; bucketid++) {
      u32 bsize = min(nslots[r&1][bucketid], NSLOTS) >> (SLOTBITS-6);
      binsizes[bsize]++;
    }
    for (u32 i=0; i < 65; i++) {
#ifdef HIST
//      printf(" %d:%d", i, binsizes[i]);
#else
#ifdef SPARK
      u32 sparks = binsizes[i] / SPARKSCALE;
#else
      u32 sparks = 0;
      for (u32 bs = binsizes[i]; bs; bs >>= 1) sparks++;
      sparks = sparks * 7 / SPARKSCALE;
#endif
//      printf("\342\226%c", '\201' + sparks);
#endif
    }
//    printf("\n");
#endif
  }

  struct htlayout {
    htalloc hta;
    u32 prevhashunits;
    u32 nexthashunits;
    u32 dunits;
    u32 prevbo;
    u32 nextbo;
  
    htlayout(equi *eq, u32 r): hta(eq->hta), prevhashunits(0), dunits(0) {
      u32 nexthashbytes = hashsize(r);
      nexthashunits = hashwords(nexthashbytes);
      prevbo = 0;
      nextbo = nexthashunits * sizeof(hashunit) - nexthashbytes; // 0-3
      if (r) {
        u32 prevhashbytes = hashsize(r-1);
        prevhashunits = hashwords(prevhashbytes);
        prevbo = prevhashunits * sizeof(hashunit) - prevhashbytes; // 0-3
        dunits = prevhashunits - nexthashunits;
      }
    }
    u32 getxhash0(const slot0* pslot) const {
#if WN == 200 && RESTBITS == 4
      return pslot->hash->bytes[prevbo] >> 4;
#elif WN == 200 && RESTBITS == 8
      return (pslot->hash->bytes[prevbo] & 0xf) << 4 | pslot->hash->bytes[prevbo+1] >> 4;
#elif WN == 200 && RESTBITS == 9
      return (pslot->hash->bytes[prevbo] & 0x1f) << 4 | pslot->hash->bytes[prevbo+1] >> 4;
#elif WN == 144 && RESTBITS == 4
      return pslot->hash->bytes[prevbo] & 0xf;
#else
#error non implemented
#endif
    }
    u32 getxhash1(const slot1* pslot) const {
#if WN == 200 && RESTBITS == 4
      return pslot->hash->bytes[prevbo] & 0xf;
#elif WN == 200 && RESTBITS == 8
      return pslot->hash->bytes[prevbo];
#elif WN == 200 && RESTBITS == 9
      return (pslot->hash->bytes[prevbo]&1) << 8 | pslot->hash->bytes[prevbo+1];
#elif WN == 144 && RESTBITS == 4
      return pslot->hash->bytes[prevbo] & 0xf;
#else
#error non implemented
#endif
    }
    bool equal(const hashunit *hash0, const hashunit *hash1) const {
      return hash0[prevhashunits-1].word == hash1[prevhashunits-1].word;
    }
  };

  struct collisiondata {
#ifdef XBITMAP
#if NSLOTS > 64
#error cant use XBITMAP with more than 64 slots
#endif
    u64 xhashmap[NRESTS];
    u64 xmap;
#else
#if RESTBITS <= 6
    typedef uchar xslot;
#else
    typedef u16 xslot;
#endif
    xslot nxhashslots[NRESTS];
    xslot xhashslots[NRESTS][XFULL];
    xslot *xx;
    u32 n0;
    u32 n1;
#endif
    u32 s0;

    void clear() {
#ifdef XBITMAP
      memset(xhashmap, 0, NRESTS * sizeof(u64));
#else
      memset(nxhashslots, 0, NRESTS * sizeof(xslot));
#endif
    }
    bool addslot(u32 s1, u32 xh) {
#ifdef XBITMAP
      xmap = xhashmap[xh];
      xhashmap[xh] |= (u64)1 << s1;
      s0 = -1;
      return true;
#else
      n1 = (u32)nxhashslots[xh]++;
      if (n1 >= XFULL)
        return false;
      xx = xhashslots[xh];
      xx[n1] = s1;
      n0 = 0;
      return true;
#endif
    }
    bool nextcollision() const {
#ifdef XBITMAP
      return xmap != 0;
#else
      return n0 < n1;
#endif
    }
    u32 slot() {
#ifdef XBITMAP
      const u32 ffs = __builtin_ffsll(xmap);
      s0 += ffs; xmap >>= ffs;
      return s0;
#else
      return (u32)xx[n0++];
#endif
    }
  };

  void digit0(const u32 id) {
    uchar hash[HASHOUT];
    crypto_generichash_blake2b_state state;
    htlayout htl(this, 0);
    const u32 hashbytes = hashsize(0);
    for (u32 block = id; block < NBLOCKS; block += nthreads) {
      state = blake_ctx;
      u32 leb = htole32(block);
      crypto_generichash_blake2b_update(&state, (uchar *)&leb, sizeof(u32));
      crypto_generichash_blake2b_final(&state, hash, HASHOUT);
      for (u32 i = 0; i<HASHESPERBLAKE; i++) {
        const uchar *ph = hash + i * WN/8;
#if BUCKBITS == 16 && RESTBITS == 4
        const u32 bucketid = ((u32)ph[0] << 8) | ph[1];
#elif BUCKBITS == 12 && RESTBITS == 8
        const u32 bucketid = ((u32)ph[0] << 4) | ph[1] >> 4;
#elif BUCKBITS == 11 && RESTBITS == 9
        const u32 bucketid = ((u32)ph[0] << 3) | ph[1] >> 5;
#elif BUCKBITS == 20 && RESTBITS == 4
        const u32 bucketid = ((((u32)ph[0] << 8) | ph[1]) << 4) | ph[2] >> 4;
#elif BUCKBITS == 12 && RESTBITS == 4
        const u32 bucketid = ((u32)ph[0] << 4) | ph[1] >> 4;
        const u32 xhash = ph[1] & 0xf;
#else
#error not implemented
#endif
        const u32 slot = getslot(0, bucketid);
        if (slot >= NSLOTS) {
          bfull++;
          continue;
        }
        slot0 &s = hta.trees0[0][bucketid][slot];
        s.attr = tree(block * HASHESPERBLAKE + i);
        memcpy(s.hash->bytes+htl.nextbo, ph+WN/8-hashbytes, hashbytes);
      }
    }
  }
  
  void digitodd(const u32 r, const u32 id) {
    htlayout htl(this, r);
    collisiondata cd;
    for (u32 bucketid=id; bucketid < NBUCKETS; bucketid += nthreads) {
      cd.clear();
      slot0 *buck = htl.hta.trees0[(r-1)/2][bucketid]; // optimize by updating previous buck?!
      u32 bsize = getnslots(r-1, bucketid);       // optimize by putting bucketsize with block?!
      for (u32 s1 = 0; s1 < bsize; s1++) {
        const slot0 *pslot1 = buck + s1;          // optimize by updating previous pslot1?!
        if (!cd.addslot(s1, htl.getxhash0(pslot1))) {
          xfull++;
          continue;
        }
        for (; cd.nextcollision(); ) {
          const u32 s0 = cd.slot();
          const slot0 *pslot0 = buck + s0;
          if (htl.equal(pslot0->hash, pslot1->hash)) {
            hfull++;
            continue;
          }
          u32 xorbucketid;
          const uchar *bytes0 = pslot0->hash->bytes, *bytes1 = pslot1->hash->bytes;
#if WN == 200 && BUCKBITS == 12 && RESTBITS == 8
          xorbucketid = (((u32)(bytes0[htl.prevbo+1] ^ bytes1[htl.prevbo+1]) & 0xf) << 8)
                             | (bytes0[htl.prevbo+2] ^ bytes1[htl.prevbo+2]);
#elif WN == 200 && BUCKBITS == 11 && RESTBITS == 9
          xorbucketid = (((u32)(bytes0[htl.prevbo+1] ^ bytes1[htl.prevbo+1]) & 0xf) << 7)
                             | (bytes0[htl.prevbo+2] ^ bytes1[htl.prevbo+2]) >> 1;
#elif WN == 144 && BUCKBITS == 20 && RESTBITS == 4
          xorbucketid = ((((u32)(bytes0[htl.prevbo+1] ^ bytes1[htl.prevbo+1]) << 8)
                              | (bytes0[htl.prevbo+2] ^ bytes1[htl.prevbo+2])) << 4)
                              | (bytes0[htl.prevbo+3] ^ bytes1[htl.prevbo+3]) >> 4;
#elif WN == 96 && BUCKBITS == 12 && RESTBITS == 4
          xorbucketid = ((u32)(bytes0[htl.prevbo+1] ^ bytes1[htl.prevbo+1]) << 4)
                            | (bytes0[htl.prevbo+2] ^ bytes1[htl.prevbo+2]) >> 4;
#else
#error not implemented
#endif
          const u32 xorslot = getslot(r, xorbucketid);
          if (xorslot >= NSLOTS) {
            bfull++;
            continue;
          }
          slot1 &xs = htl.hta.trees1[r/2][xorbucketid][xorslot];
          xs.attr = tree(bucketid, s0, s1);
          for (u32 i=htl.dunits; i < htl.prevhashunits; i++)
            xs.hash[i-htl.dunits].word = pslot0->hash[i].word ^ pslot1->hash[i].word;
        }
      }
    }
  }
  
  void digiteven(const u32 r, const u32 id) {
    htlayout htl(this, r);
    collisiondata cd;
    for (u32 bucketid=id; bucketid < NBUCKETS; bucketid += nthreads) {
      cd.clear();
      slot1 *buck = htl.hta.trees1[(r-1)/2][bucketid]; // OPTIMIZE BY UPDATING PREVIOUS
      u32 bsize = getnslots(r-1, bucketid);
      for (u32 s1 = 0; s1 < bsize; s1++) {
        const slot1 *pslot1 = buck + s1;          // OPTIMIZE BY UPDATING PREVIOUS
        if (!cd.addslot(s1, htl.getxhash1(pslot1))) {
          xfull++;
          continue;
        }
        for (; cd.nextcollision(); ) {
          const u32 s0 = cd.slot();
          const slot1 *pslot0 = buck + s0;
          if (htl.equal(pslot0->hash, pslot1->hash)) {
            hfull++;
            continue;
          }
          u32 xorbucketid;
          const uchar *bytes0 = pslot0->hash->bytes, *bytes1 = pslot1->hash->bytes;
#if WN == 200 && BUCKBITS == 12 && RESTBITS == 8
          xorbucketid = ((u32)(bytes0[htl.prevbo+1] ^ bytes1[htl.prevbo+1]) << 4)
                            | (bytes0[htl.prevbo+2] ^ bytes1[htl.prevbo+2]) >> 4;
#elif WN == 200 && BUCKBITS == 11 && RESTBITS == 9
          xorbucketid = ((u32)(bytes0[htl.prevbo+2] ^ bytes1[htl.prevbo+2]) << 3)
                            | (bytes0[htl.prevbo+3] ^ bytes1[htl.prevbo+3]) >> 5;
#elif WN == 144 && BUCKBITS == 20 && RESTBITS == 4
          xorbucketid = ((((u32)(bytes0[htl.prevbo+1] ^ bytes1[htl.prevbo+1]) << 8)
                              | (bytes0[htl.prevbo+2] ^ bytes1[htl.prevbo+2])) << 4)
                              | (bytes0[htl.prevbo+3] ^ bytes1[htl.prevbo+3]) >> 4;
#elif WN == 96 && BUCKBITS == 12 && RESTBITS == 4
          xorbucketid = ((u32)(bytes0[htl.prevbo+1] ^ bytes1[htl.prevbo+1]) << 4)
                            | (bytes0[htl.prevbo+2] ^ bytes1[htl.prevbo+2]) >> 4;
#else
#error not implemented
#endif
          const u32 xorslot = getslot(r, xorbucketid);
          if (xorslot >= NSLOTS) {
            bfull++;
            continue;
          }
          slot0 &xs = htl.hta.trees0[r/2][xorbucketid][xorslot];
          xs.attr = tree(bucketid, s0, s1);
          for (u32 i=htl.dunits; i < htl.prevhashunits; i++)
            xs.hash[i-htl.dunits].word = pslot0->hash[i].word ^ pslot1->hash[i].word;
        }
      }
    }
  }
  
  void digitK(const u32 id) {
    collisiondata cd;
    htlayout htl(this, WK);
u32 nc = 0;
    for (u32 bucketid = id; bucketid < NBUCKETS; bucketid += nthreads) {
      cd.clear();
      slot0 *buck = htl.hta.trees0[(WK-1)/2][bucketid];
      u32 bsize = getnslots(WK-1, bucketid);
      for (u32 s1 = 0; s1 < bsize; s1++) {
        const slot0 *pslot1 = buck + s1;
        if (!cd.addslot(s1, htl.getxhash0(pslot1))) // assume WK odd
          continue;
        for (; cd.nextcollision(); ) {
          const u32 s0 = cd.slot();
          if (htl.equal(buck[s0].hash, pslot1->hash))
nc++,       candidate(tree(bucketid, s0, s1));
        }
      }
    }
//printf(" %d candidates ", nc);
  }
};

typedef struct {
  u32 id;
  pthread_t thread;
  equi *eq;
} thread_ctx;

void barrier(pthread_barrier_t *barry) {
  const int rc = pthread_barrier_wait(barry);
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
//    printf("Could not wait on barrier\n");
    pthread_exit(NULL);
  }
}

void *worker(void *vp) {
  thread_ctx *tp = (thread_ctx *)vp;
  equi *eq = tp->eq;

  if (tp->id == 0)
//    printf("Digit 0\n");
  barrier(&eq->barry);
  eq->digit0(tp->id);
  barrier(&eq->barry);
  if (tp->id == 0) {
    eq->xfull = eq->bfull = eq->hfull = 0;
    eq->showbsizes(0);
  }
  barrier(&eq->barry);
  for (u32 r = 1; r < WK; r++) {
    if (tp->id == 0)
//      printf("Digit %d", r);
    barrier(&eq->barry);
    r&1 ? eq->digitodd(r, tp->id) : eq->digiteven(r, tp->id);
    barrier(&eq->barry);
    if (tp->id == 0) {
//      printf(" x%d b%d h%d\n", eq->xfull, eq->bfull, eq->hfull);
      eq->xfull = eq->bfull = eq->hfull = 0;
      eq->showbsizes(r);
    }
    barrier(&eq->barry);
  }
  if (tp->id == 0)
//    printf("Digit %d\n", WK);
  eq->digitK(tp->id);
  barrier(&eq->barry);
  pthread_exit(NULL);
  return 0;
}
