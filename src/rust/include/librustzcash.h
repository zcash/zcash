#ifndef ZCASH_RUST_INCLUDE_LIBRUSTZCASH_H
#define ZCASH_RUST_INCLUDE_LIBRUSTZCASH_H

#include "rust/types.h"

#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
  #include <assert.h>
  #include <stdalign.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /// Fills the provided buffer with random bytes. This is intended to
    /// be a cryptographically secure RNG; it uses Rust's `OsRng`, which
    /// is implemented in terms of the `getrandom` crate. The first call
    /// to this function may block until sufficient randomness is available.
    void librustzcash_getrandom(
        unsigned char *buf,
        size_t buf_len
    );
#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_LIBRUSTZCASH_H
