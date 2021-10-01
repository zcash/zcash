#ifndef ZCASH_RUST_INCLUDE_RUST_ZIP339_H
#define ZCASH_RUST_INCLUDE_RUST_ZIP339_H

#include "rust/types.h"

#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
  #include <assert.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

    // These must match src/rust/src/zip339_ffi.rs.
    // (They happen to also match the integer values correspond in the bip0039-rs crate with the
    // "all-languages" feature enabled, but that's not required.)
    enum Language
#ifdef __cplusplus
    : uint32_t
#endif
    {
        English = 0,
        SimplifiedChinese = 1,
        TraditionalChinese = 2,
        Czech = 3,
        French = 4,
        Italian = 5,
        Japanese = 6,
        Korean = 7,
        Portuguese = 8,
        Spanish = 9,
        SIZE_HACK = 0xFFFFFFFF // needed when compiling as C
    };

    static_assert(sizeof(Language) == 4);

    /// Creates a phrase with the given entropy, in the given `Language`. The phrase is represented as
    /// a pointer to a null-terminated C string that must not be written to, and must be freed by
    /// `zip339_free_phrase`.
    const char * zip339_entropy_to_phrase(Language language, const uint8_t *entropy, size_t entropy_len);

    /// Frees a phrase returned by `zip339_entropy_to_phrase`.
    void zip339_free_phrase(const char *phrase);

    /// Returns `true` if the given string is a valid mnemonic phrase in the given `Language`.
    bool zip339_validate_phrase(Language language, const char *phrase);

    /// Copies the seed for the given phrase into the 64-byte buffer `buf`.
    /// Returns true if successful, false on error. In case of error, `buf` is zeroed.
    bool zip339_phrase_to_seed(Language language, const char *phrase, uint8_t (*buf)[64]);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ZIP339_H
