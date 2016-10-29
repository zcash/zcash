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
