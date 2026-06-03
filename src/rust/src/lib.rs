//! FFI between the C++ zcashd codebase and the Rust Zcash crates.
//!
//! This is internal to zcashd and is not an officially-supported API.

// Catch documentation errors caused by code changes.
#![deny(broken_intra_doc_links)]
// Clippy has a default-deny lint to prevent dereferencing raw pointer arguments
// in a non-unsafe function. However, declaring a function as unsafe has the
// side-effect that the entire function body is treated as an unsafe {} block,
// and rustc will not enforce full safety checks on the parts of the function
// that would otherwise be safe.
//
// The functions in this crate are all for FFI usage, so it's obvious to the
// caller (which is only ever zcashd) that the arguments must satisfy the
// necessary assumptions. We therefore ignore this lint to retain the benefit of
// explicitly annotating the parts of each function that must themselves satisfy
// assumptions of underlying code.
//
// See https://github.com/rust-lang/rfcs/pull/2585 for more background.
#![allow(clippy::not_unsafe_ptr_arg_deref)]

use ::sapling::circuit::{
    OutputParameters, OutputVerifyingKey, SpendParameters, SpendVerifyingKey,
};
use bellman::groth16::PreparedVerifyingKey;
use bls12_381::Bls12;
use std::path::PathBuf;
use std::sync::{LazyLock, OnceLock};
use subtle::CtOption;

mod blake2b;
mod ed25519;
mod equihash;
mod metrics_ffi;
mod random;
mod streams_ffi;
mod tracing_ffi;
mod zcashd_orchard;

mod bridge;

mod address_ffi;
mod builder_ffi;
mod bundlecache;
mod history;
mod incremental_merkle_tree;
mod init;
mod merkle_frontier;
mod note_encryption;
mod orchard_bundle;
mod orchard_ffi;
mod orchard_keys_ffi;
mod params;
mod sapling;
mod sprout;
mod streams;
mod transaction_ffi;
mod unified_keys_ffi;
mod wallet;
mod wallet_scanner;
mod zip339_ffi;

mod test_harness_ffi;

#[cfg(test)]
mod tests;

static SAPLING_SPEND_VK: OnceLock<SpendVerifyingKey> = OnceLock::new();
static SAPLING_OUTPUT_VK: OnceLock<OutputVerifyingKey> = OnceLock::new();
static SPROUT_GROTH16_VK: OnceLock<PreparedVerifyingKey<Bls12>> = OnceLock::new();

static SAPLING_SPEND_PARAMS: OnceLock<SpendParameters> = OnceLock::new();
static SAPLING_OUTPUT_PARAMS: OnceLock<OutputParameters> = OnceLock::new();
static SPROUT_GROTH16_PARAMS_PATH: OnceLock<PathBuf> = OnceLock::new();

static ORCHARD_PK: OnceLock<orchard::circuit::ProvingKey> = OnceLock::new();
// The pre-NU6.2 (insecure) proving key. Only used to reproduce historical Orchard proofs when
// building transactions below the NU6.2 activation height — which production never does, since
// Orchard is soft-fork-disabled before NU6.2; this exists so that tests can construct pre-NU6.2
// chain history. Built lazily, so it costs nothing unless such a proof is actually created.
static ORCHARD_PK_INSECURE: LazyLock<orchard::circuit::ProvingKey> = LazyLock::new(|| {
    orchard::circuit::ProvingKey::build_for_version(
        orchard::circuit::OrchardCircuitVersion::InsecurePreNu6_2,
    )
});

// The Orchard circuit was changed in NU6.2 to fix the variable-base scalar multiplication
// gadget, which changes the verifying key. Pre-NU6.2 proofs verify only under the historical
// (insecure) verifying key, and NU6.2-onward proofs only under the fixed one, so a node that
// validates both historical and new blocks needs both.
static ORCHARD_VK_INSECURE: LazyLock<orchard::circuit::VerifyingKey> = LazyLock::new(|| {
    orchard::circuit::VerifyingKey::build_for_version(
        orchard::circuit::OrchardCircuitVersion::InsecurePreNu6_2,
    )
});
static ORCHARD_VK_FIXED: LazyLock<orchard::circuit::VerifyingKey> =
    LazyLock::new(orchard::circuit::VerifyingKey::build);

/// Converts CtOption<t> into Option<T>
fn de_ct<T>(ct: CtOption<T>) -> Option<T> {
    if ct.is_some().into() {
        Some(ct.unwrap())
    } else {
        None
    }
}

const GROTH_PROOF_SIZE: usize = 48 // π_A
    + 96 // π_B
    + 48; // π_C
