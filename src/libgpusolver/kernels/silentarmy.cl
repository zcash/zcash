#define PARAM_N				200
#define PARAM_K				9
#define PREFIX                          (PARAM_N / (PARAM_K + 1))
#define NR_INPUTS                       (1 << PREFIX)
// Approximate log base 2 of number of elements in hash tables
#define APX_NR_ELMS_LOG                 (PREFIX + 1)
// Number of rows and slots is affected by this. 20 offers the best performance
// but occasionally misses ~1% of solutions.
#define NR_ROWS_LOG                     20

// Make hash tables OVERHEAD times larger than necessary to store the average
// number of elements per row. The ideal value is as small as possible to
// reduce memory usage, but not too small or else elements are dropped from the
// hash tables.
//
// The actual number of elements per row is closer to the theoretical average
// (less variance) when NR_ROWS_LOG is small. So accordingly OVERHEAD can be
// smaller.
//
// Even (as opposed to odd) values of OVERHEAD sometimes significantly decrease
// performance as they cause VRAM channel conflicts.
#if NR_ROWS_LOG == 16
#define OVERHEAD                        3
#elif NR_ROWS_LOG == 18
#define OVERHEAD                        5
#elif NR_ROWS_LOG == 19
#define OVERHEAD                        9
#elif NR_ROWS_LOG == 20
#define OVERHEAD                        13
#endif

#define NR_ROWS                         (1 << NR_ROWS_LOG)
#define NR_SLOTS            ((1 << (APX_NR_ELMS_LOG - NR_ROWS_LOG)) * OVERHEAD)
// Length of 1 element (slot) in bytes
#define SLOT_LEN                        32
// Total size of hash table
#define HT_SIZE				(NR_ROWS * NR_SLOTS * SLOT_LEN)
// Length of Zcash block header and nonce
#define ZCASH_BLOCK_HEADER_LEN		140
#define ZCASH_NONCE_LEN			32
// Number of bytes Zcash needs out of Blake
#define ZCASH_HASH_LEN                  50
// Number of wavefronts per SIMD for the Blake kernel.
// Blake is ALU-bound (beside the atomic counter being incremented) so we need
// at least 2 wavefronts per SIMD to hide the 2-clock latency of integer
// instructions. 10 is the max supported by the hw.
#define BLAKE_WPS               	10
#define MAX_SOLS			2000

// Optional features
#undef ENABLE_DEBUG

/*
** Return the offset of Xi in bytes from the beginning of the slot.
*/
#define xi_offset_for_round(round)	(8 + ((round) / 2) * 4)

// An (uncompressed) solution stores (1 << PARAM_K) 32-bit values
#define SOL_SIZE			((1 << PARAM_K) * 4)
typedef struct	sols_s
{
    uint	nr;
    uint	likely_invalidss;
    uchar	valid[MAX_SOLS];
    uint	values[MAX_SOLS][(1 << PARAM_K)];
}		sols_t;

/*
** Assuming NR_ROWS_LOG == 16, the hash table slots have this layout (length in
** bytes in parens):
**
** round 0, table 0: cnt(4) i(4)                     pad(0)   Xi(23.0) pad(1)
** round 1, table 1: cnt(4) i(4)                     pad(0.5) Xi(20.5) pad(3)
** round 2, table 0: cnt(4) i(4) i(4)                pad(0)   Xi(18.0) pad(2)
** round 3, table 1: cnt(4) i(4) i(4)                pad(0.5) Xi(15.5) pad(4)
** round 4, table 0: cnt(4) i(4) i(4) i(4)           pad(0)   Xi(13.0) pad(3)
** round 5, table 1: cnt(4) i(4) i(4) i(4)           pad(0.5) Xi(10.5) pad(5)
** round 6, table 0: cnt(4) i(4) i(4) i(4) i(4)      pad(0)   Xi( 8.0) pad(4)
** round 7, table 1: cnt(4) i(4) i(4) i(4) i(4)      pad(0.5) Xi( 5.5) pad(6)
** round 8, table 0: cnt(4) i(4) i(4) i(4) i(4) i(4) pad(0)   Xi( 3.0) pad(5)
**
** - cnt is an atomic counter keeping track of the number of used slots.
**   it is used in the first slot only; subsequent slots replace it with
**   4 padding bytes
** - i encodes either the 21-bit input value (round 0) or a reference to two
**   inputs from the previous round
**
** If the first byte of Xi is 0xAB then:
** - on even rounds, 'A' is part of the colliding PREFIX, 'B' is part of Xi
** - on odd rounds, 'A' is padding, 'B' is part of the colliding PREFIX
**
** Formula for Xi length and pad length above:
** > for i in range(9):
** >   xi=(200-20*i-NR_ROWS_LOG)/8.; ci=8+4*((i)/2); print xi,32-ci-xi
**
** Note that the fractional .5-byte/4-bit padding following Xi for odd rounds
** is the 4 most significant bits of the last byte of Xi.
*/

__constant ulong blake_iv[] =
{
    0x6a09e667f3bcc908, 0xbb67ae8584caa73b,
    0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
    0x510e527fade682d1, 0x9b05688c2b3e6c1f,
    0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
};

/*
** Reset counters in hash table.
*/
__kernel
void kernel_init_ht(__global char *ht)
{
    uint        tid = get_global_id(0);
    *(__global uint *)(ht + tid * NR_SLOTS * SLOT_LEN) = 0;
}

/*
** If xi0,xi1,xi2,xi3 are stored consecutively in little endian then they
** represent (hex notation, group of 5 hex digits are a group of PREFIX bits):
**   aa aa ab bb bb cc cc cd dd...  [round 0]
**         --------------------
**      ...ab bb bb cc cc cd dd...  [odd round]
**               --------------
**               ...cc cc cd dd...  [next even round]
**                        -----
** Bytes underlined are going to be stored in the slot. Preceding bytes
** (and possibly part of the underlined bytes, depending on NR_ROWS_LOG) are
** used to compute the row number.
**
** Round 0: xi0,xi1,xi2,xi3 is a 25-byte Xi (xi3: only the low byte matter)
** Round 1: xi0,xi1,xi2 is a 23-byte Xi (incl. the colliding PREFIX nibble)
** TODO: update lines below with padding nibbles
** Round 2: xi0,xi1,xi2 is a 20-byte Xi (xi2: only the low 4 bytes matter)
** Round 3: xi0,xi1,xi2 is a 17.5-byte Xi (xi2: only the low 1.5 bytes matter)
** Round 4: xi0,xi1 is a 15-byte Xi (xi1: only the low 7 bytes matter)
** Round 5: xi0,xi1 is a 12.5-byte Xi (xi1: only the low 4.5 bytes matter)
** Round 6: xi0,xi1 is a 10-byte Xi (xi1: only the low 2 bytes matter)
** Round 7: xi0 is a 7.5-byte Xi (xi0: only the low 7.5 bytes matter)
** Round 8: xi0 is a 5-byte Xi (xi0: only the low 5 bytes matter)
**
** Return 0 if successfully stored, or 1 if the row overflowed.
*/
uint ht_store(uint round, __global char *ht, uint i,
        ulong xi0, ulong xi1, ulong xi2, ulong xi3)
{
    uint		row;
    __global char       *p;
    uint                cnt;
#if NR_ROWS_LOG == 16
    if (!(round % 2))
	row = (xi0 & 0xffff);
    else
	// if we have in hex: "ab cd ef..." (little endian xi0) then this
	// formula computes the row as 0xdebc. it skips the 'a' nibble as it
	// is part of the PREFIX. The Xi will be stored starting with "ef...";
	// 'e' will be considered padding and 'f' is part of the current PREFIX
	row = ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
	    ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 18
    if (!(round % 2))
	row = (xi0 & 0xffff) | ((xi0 & 0xc00000) >> 6);
    else
	row = ((xi0 & 0xc0000) >> 2) |
	    ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
	    ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 19
    if (!(round % 2))
	row = (xi0 & 0xffff) | ((xi0 & 0xe00000) >> 5);
    else
	row = ((xi0 & 0xe0000) >> 1) |
	    ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
	    ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 20
    if (!(round % 2))
	row = (xi0 & 0xffff) | ((xi0 & 0xf00000) >> 4);
    else
	row = ((xi0 & 0xf0000) >> 0) |
	    ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
	    ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#else
#error "unsupported NR_ROWS_LOG"
#endif
    xi0 = (xi0 >> 16) | (xi1 << (64 - 16));
    xi1 = (xi1 >> 16) | (xi2 << (64 - 16));
    xi2 = (xi2 >> 16) | (xi3 << (64 - 16));
    p = ht + row * NR_SLOTS * SLOT_LEN;
    cnt = atomic_inc((__global uint *)p);
    if (cnt >= NR_SLOTS)
        return 1;
    p += cnt * SLOT_LEN + xi_offset_for_round(round);
    // store "i" (always 4 bytes before Xi)
    *(__global uint *)(p - 4) = i;
    if (round == 0 || round == 1)
      {
	// store 24 bytes
	*(__global ulong *)(p + 0) = xi0;
	*(__global ulong *)(p + 8) = xi1;
	*(__global ulong *)(p + 16) = xi2;
      }
    else if (round == 2)
      {
	// store 20 bytes
	*(__global ulong *)(p + 0) = xi0;
	*(__global ulong *)(p + 8) = xi1;
	*(__global uint *)(p + 16) = xi2;
      }
    else if (round == 3 || round == 4)
      {
	// store 16 bytes
	*(__global ulong *)(p + 0) = xi0;
	*(__global ulong *)(p + 8) = xi1;

      }
    else if (round == 5)
      {
	// store 12 bytes
	*(__global ulong *)(p + 0) = xi0;
	*(__global uint *)(p + 8) = xi1;
      }
    else if (round == 6 || round == 7)
      {
	// store 8 bytes
	*(__global ulong *)(p + 0) = xi0;
      }
    else if (round == 8)
      {
	// store 4 bytes
	*(__global uint *)(p + 0) = xi0;
      }
    return 0;
}

#define mix(va, vb, vc, vd, x, y) \
    va = (va + vb + x); \
    vd = rotate((vd ^ va), (ulong)64 - 32); \
    vc = (vc + vd); \
    vb = rotate((vb ^ vc), (ulong)64 - 24); \
    va = (va + vb + y); \
    vd = rotate((vd ^ va), (ulong)64 - 16); \
    vc = (vc + vd); \
    vb = rotate((vb ^ vc), (ulong)64 - 63);

/*
** Execute round 0 (blake).
**
** Note: making the work group size less than or equal to the wavefront size
** allows the OpenCL compiler to remove the barrier() calls, see "2.2 Local
** Memory (LDS) Optimization 2-10" in:
** http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/opencl-optimization-guide/
*/
__kernel __attribute__((reqd_work_group_size(64, 1, 1)))
void kernel_round0(__global ulong *blake_state, __global char *ht,
        __global uint *debug)
{
    uint                tid = get_global_id(0);
    ulong               v[16];
    uint                inputs_per_thread = NR_INPUTS / get_global_size(0);
    uint                input = tid * inputs_per_thread;
    uint                input_end = (tid + 1) * inputs_per_thread;
    uint                dropped = 0;
    while (input < input_end)
      {
        // shift "i" to occupy the high 32 bits of the second ulong word in the
        // message block
        ulong word1 = (ulong)input << 32;
        // init vector v
        v[0] = blake_state[0];
        v[1] = blake_state[1];
        v[2] = blake_state[2];
        v[3] = blake_state[3];
        v[4] = blake_state[4];
        v[5] = blake_state[5];
        v[6] = blake_state[6];
        v[7] = blake_state[7];
        v[8] =  blake_iv[0];
        v[9] =  blake_iv[1];
        v[10] = blake_iv[2];
        v[11] = blake_iv[3];
        v[12] = blake_iv[4];
        v[13] = blake_iv[5];
        v[14] = blake_iv[6];
        v[15] = blake_iv[7];
        // mix in length of data
        v[12] ^= ZCASH_BLOCK_HEADER_LEN + 4 /* length of "i" */;
        // last block
        v[14] ^= -1;

        // round 1 
        mix(v[0], v[4], v[8],  v[12], 0, word1);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 2 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], word1, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 3 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, word1);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 4 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, word1);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 5 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, word1);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 6 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], word1, 0);
        // round 7 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], word1, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 8 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, word1);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 9 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], word1, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 10 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], word1, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 11 
        mix(v[0], v[4], v[8],  v[12], 0, word1);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], 0, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);
        // round 12 
        mix(v[0], v[4], v[8],  v[12], 0, 0);
        mix(v[1], v[5], v[9],  v[13], 0, 0);
        mix(v[2], v[6], v[10], v[14], 0, 0);
        mix(v[3], v[7], v[11], v[15], 0, 0);
        mix(v[0], v[5], v[10], v[15], word1, 0);
        mix(v[1], v[6], v[11], v[12], 0, 0);
        mix(v[2], v[7], v[8],  v[13], 0, 0);
        mix(v[3], v[4], v[9],  v[14], 0, 0);

        // compress v into the blake state; this produces the 50-byte hash
        // (two Xi values)
        ulong h[7];
        h[0] = blake_state[0] ^ v[0] ^ v[8];
        h[1] = blake_state[1] ^ v[1] ^ v[9];
        h[2] = blake_state[2] ^ v[2] ^ v[10];
        h[3] = blake_state[3] ^ v[3] ^ v[11];
        h[4] = blake_state[4] ^ v[4] ^ v[12];
        h[5] = blake_state[5] ^ v[5] ^ v[13];
        h[6] = (blake_state[6] ^ v[6] ^ v[14]) & 0xffff;

        // store the two Xi values in the hash table
#if ZCASH_HASH_LEN == 50
        dropped += ht_store(0, ht, input * 2,
                h[0],
                h[1],
                h[2],
                h[3]);
        dropped += ht_store(0, ht, input * 2 + 1,
                (h[3] >> 8) | (h[4] << (64 - 8)),
                (h[4] >> 8) | (h[5] << (64 - 8)),
                (h[5] >> 8) | (h[6] << (64 - 8)),
                (h[6] >> 8));
#else
#error "unsupported ZCASH_HASH_LEN"
#endif

        input++;
      }
#ifdef ENABLE_DEBUG
    debug[tid * 2] = 0;
    debug[tid * 2 + 1] = dropped;
#endif
}

#if NR_ROWS_LOG <= 16 && NR_SLOTS <= (1 << 8)

#define ENCODE_INPUTS(row, slot0, slot1) \
    ((row << 16) | ((slot1 & 0xff) << 8) | (slot0 & 0xff))
#define DECODE_ROW(REF)		(REF >> 16)
#define DECODE_SLOT1(REF)	((REF >> 8) & 0xff)
#define DECODE_SLOT0(REF)	(REF & 0xff)

#elif NR_ROWS_LOG == 18 && NR_SLOTS <= (1 << 7)

#define ENCODE_INPUTS(row, slot0, slot1) \
    ((row << 14) | ((slot1 & 0x7f) << 7) | (slot0 & 0x7f))
#define DECODE_ROW(REF)		(REF >> 14)
#define DECODE_SLOT1(REF)	((REF >> 7) & 0x7f)
#define DECODE_SLOT0(REF)	(REF & 0x7f)

#elif NR_ROWS_LOG == 19 && NR_SLOTS <= (1 << 6)

#define ENCODE_INPUTS(row, slot0, slot1) \
    ((row << 13) | ((slot1 & 0x3f) << 6) | (slot0 & 0x3f)) /* 1 spare bit */
#define DECODE_ROW(REF)		(REF >> 13)
#define DECODE_SLOT1(REF)	((REF >> 6) & 0x3f)
#define DECODE_SLOT0(REF)	(REF & 0x3f)

#elif NR_ROWS_LOG == 20 && NR_SLOTS <= (1 << 6)

#define ENCODE_INPUTS(row, slot0, slot1) \
    ((row << 12) | ((slot1 & 0x3f) << 6) | (slot0 & 0x3f))
#define DECODE_ROW(REF)		(REF >> 12)
#define DECODE_SLOT1(REF)	((REF >> 6) & 0x3f)
#define DECODE_SLOT0(REF)	(REF & 0x3f)

#else
#error "unsupported NR_ROWS_LOG"
#endif

/*
** XOR a pair of Xi values and store them in the hash table.
**
** Return 0 if successfully stored, or 1 if the row overflowed.
*/
uint xor_and_store(uint round, __global char *ht_dst, uint row,
	uint slot_a, uint slot_b, __global ulong *a, __global ulong *b)
{
    ulong	xi0, xi1, xi2;
#if NR_ROWS_LOG >= 16 && NR_ROWS_LOG <= 20
    // Note: for NR_ROWS_LOG == 20, for odd rounds, we could optimize by not
    // storing the byte containing bits from the previous PREFIX block for
    if (round == 1 || round == 2)
      {
	// Note: round N xors bytes from round N-1
	// xor 24 bytes
	xi0 = *(a++) ^ *(b++);
	xi1 = *(a++) ^ *(b++);
	xi2 = *a ^ *b;
      }
    else if (round == 3)
      {
	// xor 20 bytes
	xi0 = *a++ ^ *b++;
	xi1 = *a++ ^ *b++;
	xi2 = *(__global uint *)a ^ *(__global uint *)b;
      }
    else if (round == 4 || round == 5)
      {
	// xor 16 bytes
	xi0 = *a++ ^ *b++;
	xi1 = *a ^ *b;
	xi2 = 0;
      }
    else if (round == 6)
      {
	// xor 12 bytes
	xi0 = *a++ ^ *b++;
	xi1 = *(__global uint *)a ^ *(__global uint *)b;
	xi2 = 0;
      }
    else if (round == 7 || round == 8)
      {
	// xor 8 bytes
	xi0 = *a ^ *b;
	xi1 = 0;
	xi2 = 0;
      }
    // invalid solutions (which start happenning in round 5) have duplicate
    // inputs and xor to zero, so discard them
    if (!xi0 && !xi1)
	return 0;
#else
#error "unsupported NR_ROWS_LOG"
#endif
    return ht_store(round, ht_dst, ENCODE_INPUTS(row, slot_a, slot_b),
	    xi0, xi1, xi2, 0);
}

/*
** Execute one Equihash round. Read from ht_src, XOR colliding pairs of Xi,
** store them in ht_dst.
*/
void equihash_round(uint round, __global char *ht_src, __global char *ht_dst,
	__global uint *debug)
{
    uint                tid = get_global_id(0);
    uint		tlid = get_local_id(0);
    __global char       *p;
    uint                cnt;
    uchar		first_words[NR_SLOTS];
    uchar		mask;
    uint                i, j;
    // NR_SLOTS is already oversized (by a factor of OVERHEAD), but we want to
    // make it twice larger
    ushort		collisions[NR_SLOTS * 2];
    uint                nr_coll = 0;
    uint                n;
    uint                dropped_coll, dropped_stor;
    __global ulong      *a, *b;
    uint		xi_offset;
    // read first words of Xi from the previous (round - 1) hash table
    xi_offset = xi_offset_for_round(round - 1);
    // the mask is also computed to read data from the previous round
#if NR_ROWS_LOG == 16
    mask = ((!(round % 2)) ? 0x0f : 0xf0);
#elif NR_ROWS_LOG == 18
    mask = ((!(round % 2)) ? 0x03 : 0x30);
#elif NR_ROWS_LOG == 19
    mask = ((!(round % 2)) ? 0x01 : 0x10);
#elif NR_ROWS_LOG == 20
    mask = 0; /* we can vastly simplify the code below */
#else
#error "unsupported NR_ROWS_LOG"
#endif
    p = (ht_src + tid * NR_SLOTS * SLOT_LEN);
    cnt = *(__global uint *)p;
    cnt = min(cnt, (uint)NR_SLOTS); // handle possible overflow in prev. round
    p += xi_offset;
    for (i = 0; i < cnt; i++, p += SLOT_LEN)
        first_words[i] = *(__global uchar *)p;
    // find collisions
    nr_coll = 0;
    dropped_coll = 0;
    for (i = 0; i < cnt; i++)
        for (j = i + 1; j < cnt; j++)
            if ((first_words[i] & mask) ==
		    (first_words[j] & mask))
              {
                // collision!
                if (nr_coll >= sizeof (collisions) / sizeof (*collisions))
                    dropped_coll++;
                else
#if NR_SLOTS <= (1 << 8)
                    // note: this assumes slots can be encoded in 8 bits
                    collisions[nr_coll++] =
			((ushort)j << 8) | ((ushort)i & 0xff);
#else
#error "unsupported NR_SLOTS"
#endif
              }
    // drop the entire 0xAB byte (see description at top of this file)
    uint adj = (!(round % 2)) ? 1 : 0;
    // XOR colliding pairs of Xi
    dropped_stor = 0;
    for (n = 0; n < nr_coll; n++)
      {
        i = collisions[n] & 0xff;
        j = collisions[n] >> 8;
        a = (__global ulong *)
            (ht_src + tid * NR_SLOTS * SLOT_LEN + i * SLOT_LEN + xi_offset
	     + adj);
        b = (__global ulong *)
            (ht_src + tid * NR_SLOTS * SLOT_LEN + j * SLOT_LEN + xi_offset
	     + adj);
	dropped_stor += xor_and_store(round, ht_dst, tid, i, j, a, b);
      }
#ifdef ENABLE_DEBUG
    debug[tid * 2] = dropped_coll;
    debug[tid * 2 + 1] = dropped_stor;
#endif
}

/*
** This defines kernel_round1, kernel_round2, ..., kernel_round8.
*/
#define KERNEL_ROUND(N) \
__kernel __attribute__((reqd_work_group_size(64, 1, 1))) \
void kernel_round ## N(__global char *ht_src, __global char *ht_dst, \
	__global uint *debug) \
{ \
    equihash_round(N, ht_src, ht_dst, debug); \
}
KERNEL_ROUND(1)
KERNEL_ROUND(2)
KERNEL_ROUND(3)
KERNEL_ROUND(4)
KERNEL_ROUND(5)
KERNEL_ROUND(6)
KERNEL_ROUND(7)
KERNEL_ROUND(8)

uint expand_ref(__global char *ht, uint xi_offset, uint row, uint slot)
{
    return *(__global uint *)(ht + row * NR_SLOTS * SLOT_LEN +
	    slot * SLOT_LEN + xi_offset - 4);
}

void expand_refs(__global uint *ins, uint nr_inputs, __global char **htabs,
	uint round)
{
    __global char	*ht = htabs[round % 2];
    uint		i = nr_inputs - 1;
    uint		j = nr_inputs * 2 - 1;
    uint		xi_offset = xi_offset_for_round(round);
    do
      {
	ins[j] = expand_ref(ht, xi_offset,
		DECODE_ROW(ins[i]), DECODE_SLOT1(ins[i]));
	ins[j - 1] = expand_ref(ht, xi_offset,
		DECODE_ROW(ins[i]), DECODE_SLOT0(ins[i]));
	if (!i)
	    break ;
	i--;
	j -= 2;
      }
    while (1);
}

/*
** Verify if a potential solution is in fact valid.
*/
void potential_sol(__global char **htabs, __global sols_t *sols,
	uint ref0, uint ref1)
{
    uint	sol_i;
    uint	nr_values;
    sol_i = atomic_inc(&sols->nr);
    if (sol_i >= MAX_SOLS)
	return ;
    sols->valid[sol_i] = 0;
    nr_values = 0;
    sols->values[sol_i][nr_values++] = ref0;
    sols->values[sol_i][nr_values++] = ref1;
    uint round = PARAM_K - 1;
    do
      {
	round--;
	expand_refs(&(sols->values[sol_i][0]), nr_values, htabs, round);
	nr_values *= 2;
      }
    while (round > 0);
    sols->valid[sol_i] = 1;
}

/*
** Scan the hash tables to find Equihash solutions.
*/
__kernel
void kernel_sols(__global char *ht0, __global char *ht1, __global sols_t *sols)
{
    uint		tid = get_global_id(0);
    __global char	*htabs[2] = { ht0, ht1 };
    uint		ht_i = (PARAM_K - 1) % 2; // table filled at last round
    uint		cnt;
    uint		xi_offset = xi_offset_for_round(PARAM_K - 1);
    uint		i, j;
    __global char	*a, *b;
    uint		ref_i, ref_j;
    // it's ok for the collisions array to be so small, as if it fills up
    // the potential solutions are likely invalid (many duplicate inputs)
    ulong		collisions[5];
    uint		coll;
#if NR_ROWS_LOG >= 16 && NR_ROWS_LOG <= 20
    // in the final hash table, we are looking for a match on both the bits
    // part of the previous PREFIX colliding bits, and the last PREFIX bits.
    uint		mask = 0xffffff;
#else
#error "unsupported NR_ROWS_LOG"
#endif
    if (tid == 0)
	sols->nr = sols->likely_invalidss = 0;
    mem_fence(CLK_GLOBAL_MEM_FENCE); // for tid 0 initializing struct above
    a = htabs[ht_i] + tid * NR_SLOTS * SLOT_LEN;
    cnt = *(__global uint *)a;
    cnt = min(cnt, (uint)NR_SLOTS); // handle possible overflow in last round
    coll = 0;
    a += xi_offset;
    for (i = 0; i < cnt; i++, a += SLOT_LEN)
	for (j = i + 1, b = a + SLOT_LEN; j < cnt; j++, b += SLOT_LEN)
	    if (((*(__global uint *)a) & mask) ==
		    ((*(__global uint *)b) & mask))
	      {
		ref_i = *(__global uint *)(a - 4);
		ref_j = *(__global uint *)(b - 4);
		if (coll < sizeof (collisions) / sizeof (*collisions))
		    collisions[coll++] = ((ulong)ref_i << 32) | ref_j;
		else
		    atomic_inc(&sols->likely_invalidss);
	      }
    if (!coll)
	return ;
    for (i = 0; i < coll; i++)
	potential_sol(htabs, sols, collisions[i] >> 32,
		collisions[i] & 0xffffffff);
}
