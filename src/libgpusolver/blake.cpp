#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "blake.h"

static const uint32_t   blake2b_block_len = 128;
static const uint32_t   blake2b_rounds = 12;
static const uint64_t   blake2b_iv[8] =
{
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
};
static const uint8_t    blake2b_sigma[12][16] =
{
      {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
      { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
      { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
      {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
      {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
      {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
      { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
      { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
      {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
      { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
      {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
      { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
};

/*
** Init the state according to Zcash parameters.
*/
void zcash_blake2b_init(blake2b_state_t *st, uint8_t hash_len,
	uint32_t n, uint32_t k)
{
    assert(n > k);
    assert(hash_len <= 64);
    st->h[0] = blake2b_iv[0] ^ (0x01010000 | hash_len);
    for (uint32_t i = 1; i <= 5; i++)
        st->h[i] = blake2b_iv[i];
    st->h[6] = blake2b_iv[6] ^ *(uint64_t *)"ZcashPoW";
    st->h[7] = blake2b_iv[7] ^ (((uint64_t)k << 32) | n);
    st->bytes = 0;
}

static uint64_t rotr64(uint64_t a, uint8_t bits)
{
    return (a >> bits) | (a << (64 - bits));
}

static void mix(uint64_t *va, uint64_t *vb, uint64_t *vc, uint64_t *vd,
        uint64_t x, uint64_t y)
{
    *va = (*va + *vb + x);
    *vd = rotr64(*vd ^ *va, 32);
    *vc = (*vc + *vd);
    *vb = rotr64(*vb ^ *vc, 24);
    *va = (*va + *vb + y);
    *vd = rotr64(*vd ^ *va, 16);
    *vc = (*vc + *vd);
    *vb = rotr64(*vb ^ *vc, 63);
}

/*
** Process either a full message block or the final partial block.
** Note that v[13] is not XOR'd because st->bytes is assumed to never overflow.
**
** _msg         pointer to message (must be zero-padded to 128 bytes if final block)
** msg_len      must be 128 (<= 128 allowed only for final partial block)
** is_final     indicate if this is the final block
*/
void zcash_blake2b_update(blake2b_state_t *st, const uint8_t *_msg,
        uint32_t msg_len, uint32_t is_final)
{
    const uint64_t      *m = (const uint64_t *)_msg;
    uint64_t            v[16];
    assert(msg_len <= 128);
    assert(st->bytes <= UINT64_MAX - msg_len);
    memcpy(v + 0, st->h, 8 * sizeof (*v));
    memcpy(v + 8, blake2b_iv, 8 * sizeof (*v));
    v[12] ^= (st->bytes += msg_len);
    v[14] ^= is_final ? -1 : 0;
    for (uint32_t round = 0; round < blake2b_rounds; round++)
      {
        const uint8_t   *s = blake2b_sigma[round];
        mix(v + 0, v + 4, v + 8,  v + 12, m[s[0]],  m[s[1]]);
        mix(v + 1, v + 5, v + 9,  v + 13, m[s[2]],  m[s[3]]);
        mix(v + 2, v + 6, v + 10, v + 14, m[s[4]],  m[s[5]]);
        mix(v + 3, v + 7, v + 11, v + 15, m[s[6]],  m[s[7]]);
        mix(v + 0, v + 5, v + 10, v + 15, m[s[8]],  m[s[9]]);
        mix(v + 1, v + 6, v + 11, v + 12, m[s[10]], m[s[11]]);
        mix(v + 2, v + 7, v + 8,  v + 13, m[s[12]], m[s[13]]);
        mix(v + 3, v + 4, v + 9,  v + 14, m[s[14]], m[s[15]]);
      }
    for (uint32_t i = 0; i < 8; i++)
        st->h[i] ^= v[i] ^ v[i + 8];
}

void zcash_blake2b_final(blake2b_state_t *st, uint8_t *out, uint8_t outlen)
{
    assert(outlen <= 64);
    memcpy(out, st->h, outlen);
}
