// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_BLAKE2B_H
#define ZCASH_RUST_INCLUDE_RUST_BLAKE2B_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct BLAKE2bState;
typedef struct BLAKE2bState BLAKE2bState;
#define BLAKE2bPersonalBytes 16U

/// Initializes a BLAKE2b state with no key and no salt.
///
/// `personalization` MUST be a pointer to a 16-byte array.
///
/// Please free this with `blake2b_free` when you are done.
BLAKE2bState* blake2b_init(
    size_t output_len,
    const unsigned char* personalization);

/// Clones the given BLAKE2b state.
///
/// Both states need to be separately freed with `blake2b_free` when you are
/// done.
BLAKE2bState* blake2b_clone(const BLAKE2bState* state);

/// Frees a BLAKE2b state returned by `blake2b_init`.
void blake2b_free(BLAKE2bState* state);

/// Adds input to the hash. You can call this any number of times.
void blake2b_update(
    BLAKE2bState* state,
    const unsigned char* input,
    size_t input_len);

/// Finalizes the `state` and stores the result in `output`.
///
/// `output_len` MUST be less than or equal to the value that was passed as the
/// first parameter to `blake2b_init`.
///
/// This method is idempotent, and calling it multiple times will give the same
/// result. It's also possible to call `blake2b_update` with more input in
/// between.
void blake2b_finalize(
    BLAKE2bState* state,
    unsigned char* output,
    size_t output_len);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_BLAKE2B_H
