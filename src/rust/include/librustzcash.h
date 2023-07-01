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

    /// Computes the signature for each Spend description, given the key
    /// `ask`, the re-randomization `ar`, the 32-byte sighash `sighash`,
    /// and an output `result` buffer of 64-bytes for the signature.
    ///
    /// This function will fail if the provided `ask` or `ar` are invalid.
    bool librustzcash_sapling_spend_sig(
        const unsigned char *ask,
        const unsigned char *ar,
        const unsigned char *sighash,
        unsigned char *result
    );

    /// Derive the master ExtendedSpendingKey from a seed.
    void librustzcash_zip32_sapling_xsk_master(
        const unsigned char *seed,
        size_t seedlen,
        unsigned char *xsk_master
    );

    /// Derive a child ExtendedSpendingKey from a parent.
    void librustzcash_zip32_sapling_xsk_derive(
        const unsigned char *xsk_parent,
        uint32_t i,
        unsigned char *xsk_i
    );

    /// Derive a internal ExtendedSpendingKey from an external key
    void librustzcash_zip32_sapling_xsk_derive_internal(
        const unsigned char *xsk_external,
        unsigned char *xsk_internal
    );

    /// Derive a child ExtendedFullViewingKey from a parent.
    bool librustzcash_zip32_sapling_xfvk_derive(
        const unsigned char *xfvk_parent,
        uint32_t i,
        unsigned char *xfvk_i
    );

    /**
     * Derive the Sapling internal FVK corresponding to the given
     * Sapling external FVK.
     */
    void librustzcash_zip32_sapling_derive_internal_fvk(
        const unsigned char *fvk,
        const unsigned char *dk,
        unsigned char *fvk_ret,
        unsigned char *dk_ret
    );

    /**
     * Derive a PaymentAddress from a (SaplingFullViewingKey, DiversifierKey)
     * pair.  Returns 'false' if no valid address can be derived for the
     * specified diversifier index.
     *
     * Arguments:
     * - fvk: [c_uchar; 96] the serialized form of a Sapling full viewing key
     * - dk: [c_uchar; 32] the byte representation of a Sapling diversifier key
     * - j: [c_uchar; 11] the 88-bit diversifier index, encoded in little-endian
     *   order
     * - addr_ret: [c_uchar; 43] array to which the returned address will be
     *   written, if the specified diversifier index `j` produces a valid
     *   address.
     */
    bool librustzcash_zip32_sapling_address(
        const unsigned char *fvk,
        const unsigned char *dk,
        const unsigned char *j,
        unsigned char *addr_ret
    );

    /**
     * Derive a PaymentAddress from a (SaplingFullViewingKey, DiversifierKey)
     * pair by searching the space of valid diversifiers starting at
     * diversifier index `j`. This will always return a valid address along
     * with the diversifier index that produced the address unless no addresses
     * can be derived at any diversifier index >= `j`, in which case this
     * function will return `false`.
     *
     * Arguments:
     * - fvk: [c_uchar; 96] the serialized form of a Sapling full viewing key
     * - dk: [c_uchar; 32] the byte representation of a Sapling diversifier key
     * - j: [c_uchar; 11] the 88-bit diversifier index at which to start
     *   searching, encoded in little-endian order
     * - j_ret: [c_uchar; 11] array that will store the diversifier index at
     *   which the returned address was found
     * - addr_ret: [c_uchar; 43] array to which the returned address will be
     *   written
     */
    bool librustzcash_zip32_find_sapling_address(
        const unsigned char *fvk,
        const unsigned char *dk,
        const unsigned char *j,
        unsigned char *j_ret,
        unsigned char *addr_ret
    );

    /**
     * Decrypts a Sapling diversifier using the specified diversifier key
     * to obtain the diversifier index `j` at which the diversifier was
     * derived.
     *
     * Arguments:
     * - dk: [c_uchar; 32] the byte representation of a Sapling diversifier key
     * - addr: [c_uchar; 11] the bytes of the diversifier
     * - j_ret: [c_uchar; 11] array that will store the resulting diversifier index
     */
    void librustzcash_sapling_diversifier_index(
        const unsigned char *dk,
        const unsigned char *d,
        unsigned char *j_ret
    );

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
