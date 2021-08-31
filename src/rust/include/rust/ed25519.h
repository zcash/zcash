// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ED25519_H
#define ZCASH_RUST_INCLUDE_RUST_ED25519_H

#include "ed25519/types.h"

#include <stddef.h>

#ifndef __cplusplus
#include <assert.h>
#include <stdalign.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Generates a new Ed25519 keypair.
void ed25519_generate_keypair(
    Ed25519SigningKey* sk,
    Ed25519VerificationKey* vk);

/// Creates a signature on msg using the given signing key.
bool ed25519_sign(
    const Ed25519SigningKey* sk,
    const unsigned char* msg,
    size_t msglen,
    Ed25519Signature* signature);

/// Verifies a purported `signature` on the given `msg`.
///
/// # Zcash-specific consensus properties
///
/// Ed25519 checks are described in §5.4.5 of the Zcash protocol specification
/// and in ZIP 215. The verification criteria for an (encoded) `signature`
/// (`R_bytes`, `s_bytes`) with (encoded) verification key `A_bytes` are:
///
/// - `A_bytes` and `R_bytes` MUST be encodings of points `A` and `R`
///   respectively on the twisted Edwards form of Curve25519, and non-canonical
///   encodings MUST be accepted;
/// - `s_bytes` MUST represent an integer `s` less than `l`, the order of the
///   prime-order subgroup of Curve25519;
/// - the verification equation `[8][s]B = [8]R + [8][k]A` MUST be satisfied;
/// - the alternate verification equation `[s]B = R + [k]A`, allowed by RFC 8032,
///   MUST NOT be used.
bool ed25519_verify(
    const Ed25519VerificationKey* vk,
    const Ed25519Signature* signature,
    const unsigned char* msg,
    size_t msglen);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ED25519_H
