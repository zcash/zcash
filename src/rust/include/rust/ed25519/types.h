// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ED25519_TYPES_H
#define ZCASH_RUST_INCLUDE_RUST_ED25519_TYPES_H

#ifndef __cplusplus
#include <assert.h>
#include <stdalign.h>
#endif

#define ED25519_SIGNING_KEY_LEN 32U
#define ED25519_VERIFICATION_KEY_LEN 32U
#define ED25519_SIGNATURE_LEN 64U

/// An Ed25519 signing key.
///
/// This is also called a secret key by other implementations.
typedef struct Ed25519SigningKey {
    unsigned char bytes[ED25519_SIGNING_KEY_LEN];
} Ed25519SigningKey;
static_assert(
    sizeof(Ed25519SigningKey) == ED25519_SIGNING_KEY_LEN,
    "Ed25519SigningKey struct is not the same size as the underlying byte array");
static_assert(
    alignof(Ed25519SigningKey) == 1,
    "Ed25519SigningKey struct alignment is not 1");

/// An encoded Ed25519 verification key.
///
/// This is also called a public key by other implementations.
///
/// This type does not guarantee validity of the verification key.
typedef struct Ed25519VerificationKey {
    unsigned char bytes[ED25519_VERIFICATION_KEY_LEN];
} Ed25519VerificationKey;
static_assert(
    sizeof(Ed25519VerificationKey) == ED25519_VERIFICATION_KEY_LEN,
    "Ed25519VerificationKey struct is not the same size as the underlying byte array");
static_assert(
    alignof(Ed25519VerificationKey) == 1,
    "Ed25519VerificationKey struct alignment is not 1");

/// An Ed25519 signature.
typedef struct Ed25519Signature {
    unsigned char bytes[ED25519_SIGNATURE_LEN];
} Ed25519Signature;
static_assert(
    sizeof(Ed25519Signature) == ED25519_SIGNATURE_LEN,
    "Ed25519Signature struct is not the same size as the underlying byte array");
static_assert(
    alignof(Ed25519Signature) == 1,
    "Ed25519Signature struct alignment is not 1");

#endif // ZCASH_RUST_INCLUDE_RUST_ED25519_TYPES_H
