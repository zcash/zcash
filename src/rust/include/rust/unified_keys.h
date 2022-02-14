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
 *
 * The returned string's memory must be freed by the caller using
 * `zcash_address_string_free`.
 */
char* unified_full_viewing_key_serialize(
    const char* network,
    const UnifiedFullViewingKeyPtr* full_viewing_key);

/**
 * Reads the transparent component of a unified full viewing key.
 *
 * `tkeyout` must be of length 65.
 *
 * Returns `true` if the UFVK contained a transparent component, `false`
 * otherwise. If this returns `true`, the transparent key will be copied to
 * `tkeyout` as the byte representation of the `(ChainCode, CPubKey)` pair.
 * If it returns `false` then `tkeyout` will be unchanged.
 */
bool unified_full_viewing_key_read_transparent(
    const UnifiedFullViewingKeyPtr* full_viewing_key,
    unsigned char* tkeyout);

/**
 * Reads the Sapling component of a unified full viewing key.
 *
 * `skeyout` must be of length 128.
 *
 * Returns `true` if the UFVK contained a Sapling component,
 * `false` otherwise. The bytes of the `(ak, nk, ovk, dk)` fields
 * of the viewing key, in the encoding given by `EncodeExtFVKParts`
 * defined in ZIP 32, will be copied to `skeyout` if `true` is returned.
 * If `false` is returned then `skeyout` will be unchanged.
 */
bool unified_full_viewing_key_read_sapling(
    const UnifiedFullViewingKeyPtr* full_viewing_key,
    unsigned char* skeyout);

/**
 * Constructs a unified full viewing key from the binary encodings
 * of its constituent parts.
 *
 * If `t_key` is not `null`, it must be of length 65 and must be the concatenated
 * bytes of the serialized `(ChainCode, CPubKey)` pair.
 *
 * If `sapling_key` is not `null`, it must be of length 128 and must be the concatenated
 * bytes of the `(ak, nk, ovk, dk)` fields in the encoding given by
 * `EncodeExtFVKParts` defined in ZIP 32.
 *
 * Returns a pointer to newly allocated UFVK if the operation succeeds,
 * or the null pointer otherwise. The pointer returned by this function
 * must be freed by the caller with `unified_full_viewing_key_free`.
 */
UnifiedFullViewingKeyPtr* unified_full_viewing_key_from_components(
    const unsigned char* t_key,
    const unsigned char* sapling_key);

/**
 * Derive the internal and external OVKs for the binary encoding
 * of a transparent FVK (the concatenated bytes of the serialized
 * `(ChainCode, CPubKey)` pair.)
 *
 * Returns `true` if `t_key` was successfully deserialized,
 * in which case `internal_ovk_ret` and `external_ovk_ret` (which
 * should both point to 32-byte arrays) will have been updated
 * with the appropriate key bytes; otherwise, this procedure
 * returns `false` and the return values are unmodified.
 */
bool transparent_key_ovks(
    const unsigned char* t_key,
    unsigned char* internal_ovk_ret,
    unsigned char* external_ovk_ret);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_UNIFIED_KEYS_H

