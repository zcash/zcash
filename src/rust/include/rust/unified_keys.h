// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_UNIFIED_KEYS_H
#define ZCASH_RUST_INCLUDE_RUST_UNIFIED_KEYS_H

#ifdef __cplusplus
extern "C" {
#endif

//
// Unified full viewing keys
//

/**
 * Void pointer type representing a reference to a Rust-allocated
 * unified::Ufvk value
 */
struct UnifiedFullViewingKeyPtr;
typedef struct UnifiedFullViewingKeyPtr UnifiedFullViewingKeyPtr;

/**
 * Free the memory allocated for the given address::unified::Ufvk;
 */
void unified_full_viewing_key_free(UnifiedFullViewingKeyPtr* ptr);

/**
 * Clones the given Unified full viewing key and returns
 * a pointer to the newly created value. Both the original
 * one's memory and the newly allocated one need to be freed
 * independently.
 */
UnifiedFullViewingKeyPtr* unified_full_viewing_key_clone(
        const UnifiedFullViewingKeyPtr* ptr);

/**
 * Parses a unified full viewing key from the given string.
 *
 * - If the key does not parse correctly, or the network for which the
 *   key was encoded does not match the specified network the returned
 *   pointer will be null.
 */
UnifiedFullViewingKeyPtr* unified_full_viewing_key_parse(
    const char* network,
    const char* str);

/**
 * Serializes a unified full viewing key and returns its string representation.
 */
char* unified_full_viewing_key_serialize(
    const char* network,
    const UnifiedFullViewingKeyPtr* full_viewing_key);

/**
 * Reads the transparent component of a unified viewing key.
 *
 * `tkeyout` must be of length 65.
 *
 * Returns `true` if the UFVK contained a transparent component,
 * `false` otherwise. The bytes of the transparent key will be
 * copied to tkeyout
 */
bool unified_full_viewing_key_read_transparent(
    const UnifiedFullViewingKeyPtr* full_viewing_key,
    unsigned char *tkeyout);

/**
 * Reads the Sapling component of a unified viewing key.
 *
 * `skeyout` must be of length 128.
 *
 * Returns `true` if the UFVK contained a Sapling component,
 * false otherwise.
 */
bool unified_full_viewing_key_read_sapling(
    const UnifiedFullViewingKeyPtr* full_viewing_key,
    unsigned char *skeyout);

/**
 * Sets the Sapling component of a unified viewing key.
 *
 * `t_key` must be of length 65 and must be the concatenated
 * bytes of the serialized `(ChainCode, CPubKey)` pair.
 *
 * `sapling_key` must be of length 128 and must be the concatenated
 * bytes of the serialized `(SaplingFullViewingKey, DiversifierKey)`
 * pair.
 *
 * Returns the newly allocated UFVK  if the operation succeeds,
 * or the null pointer otherwise.
 */
UnifiedFullViewingKeyPtr* unified_full_viewing_key_from_components(
    const unsigned char *t_key,
    const unsigned char *sapling_key);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_UNIFIED_KEYS_H

