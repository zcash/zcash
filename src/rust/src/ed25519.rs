// Copyright (c) 2020-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

use ed25519_zebra::{Signature, SigningKey, VerificationKey};
use rand_core::OsRng;
use std::convert::TryFrom;

#[cxx::bridge(namespace = "ed25519")]
mod ffi {
    struct SigningKey {
        bytes: [u8; 32],
    }

    struct VerificationKey {
        bytes: [u8; 32],
    }

    struct Signature {
        bytes: [u8; 64],
    }

    extern "Rust" {
        fn generate_keypair(sk: &mut SigningKey, vk: &mut VerificationKey);
        fn sign(sk: &SigningKey, msg: &[u8], signature: &mut Signature);
        fn verify(vk: &VerificationKey, signature: &Signature, msg: &[u8]) -> bool;
    }
}

/// Generates a new Ed25519 keypair.
fn generate_keypair(sk: &mut ffi::SigningKey, vk: &mut ffi::VerificationKey) {
    let signing_key = SigningKey::new(OsRng);

    sk.bytes = signing_key.into();
    vk.bytes = VerificationKey::from(&signing_key).into();
}

/// Creates a signature on msg using the given signing key.
fn sign(sk: &ffi::SigningKey, msg: &[u8], signature: &mut ffi::Signature) {
    let sk = SigningKey::from(sk.bytes);
    signature.bytes = sk.sign(msg).into();
}

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
fn verify(vk: &ffi::VerificationKey, signature: &ffi::Signature, msg: &[u8]) -> bool {
    let vk = match VerificationKey::try_from(vk.bytes) {
        Ok(vk) => vk,
        Err(_) => return false,
    };

    let signature = Signature::from(signature.bytes);

    vk.verify(&signature, msg).is_ok()
}
