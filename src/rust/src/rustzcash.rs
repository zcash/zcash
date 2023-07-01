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

use bellman::groth16::{self, Parameters, PreparedVerifyingKey};
use bls12_381::Bls12;
use libc::{c_uchar, size_t};
use rand_core::{OsRng, RngCore};
use std::path::PathBuf;
use std::slice;
use subtle::CtOption;

use zcash_primitives::{
    sapling::{keys::FullViewingKey, redjubjub, spend_sig, Diversifier},
    zip32::{self, sapling_address, sapling_derive_internal_fvk, sapling_find_address},
};

mod blake2b;
mod ed25519;
mod equihash;
mod metrics_ffi;
mod streams_ffi;
mod tracing_ffi;
mod zcashd_orchard;

mod bridge;

mod address_ffi;
mod builder_ffi;
mod bundlecache;
mod history_ffi;
mod incremental_merkle_tree;
mod init_ffi;
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

static mut SAPLING_SPEND_VK: Option<groth16::VerifyingKey<Bls12>> = None;
static mut SAPLING_OUTPUT_VK: Option<groth16::VerifyingKey<Bls12>> = None;
static mut SPROUT_GROTH16_VK: Option<PreparedVerifyingKey<Bls12>> = None;

static mut SAPLING_SPEND_PARAMS: Option<Parameters<Bls12>> = None;
static mut SAPLING_OUTPUT_PARAMS: Option<Parameters<Bls12>> = None;
static mut SPROUT_GROTH16_PARAMS_PATH: Option<PathBuf> = None;

static mut ORCHARD_PK: Option<orchard::circuit::ProvingKey> = None;
static mut ORCHARD_VK: Option<orchard::circuit::VerifyingKey> = None;

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

/// Computes the signature for each Spend description, given the key `ask`, the
/// re-randomization `ar`, the 32-byte sighash `sighash`, and an output `result`
/// buffer of 64-bytes for the signature.
///
/// This function will fail if the provided `ask` or `ar` are invalid.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_spend_sig(
    ask: *const [c_uchar; 32],
    ar: *const [c_uchar; 32],
    sighash: *const [c_uchar; 32],
    result: *mut [c_uchar; 64],
) -> bool {
    // The caller provides the re-randomization of `ak`.
    let ar = match de_ct(jubjub::Scalar::from_bytes(unsafe { &*ar })) {
        Some(p) => p,
        None => return false,
    };

    // The caller provides `ask`, the spend authorizing key.
    let ask = match redjubjub::PrivateKey::read(&(unsafe { &*ask })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Initialize secure RNG
    let mut rng = OsRng;

    // Do the signing
    let sig = spend_sig(ask, ar, unsafe { &*sighash }, &mut rng);

    // Write out the signature
    sig.write(&mut (unsafe { &mut *result })[..])
        .expect("result should be 64 bytes");

    true
}

/// Derive the master ExtendedSpendingKey from a seed.
#[no_mangle]
pub extern "C" fn librustzcash_zip32_sapling_xsk_master(
    seed: *const c_uchar,
    seedlen: size_t,
    xsk_master: *mut [c_uchar; 169],
) {
    let seed = unsafe { std::slice::from_raw_parts(seed, seedlen) };

    let xsk = zip32::ExtendedSpendingKey::master(seed);

    xsk.write(&mut (unsafe { &mut *xsk_master })[..])
        .expect("should be able to serialize an ExtendedSpendingKey");
}

/// Derive a child ExtendedSpendingKey from a parent.
#[no_mangle]
pub extern "C" fn librustzcash_zip32_sapling_xsk_derive(
    xsk_parent: *const [c_uchar; 169],
    i: u32,
    xsk_i: *mut [c_uchar; 169],
) {
    let xsk_parent = zip32::ExtendedSpendingKey::read(&unsafe { *xsk_parent }[..])
        .expect("valid ExtendedSpendingKey");
    let i = zip32::ChildIndex::from_index(i);

    let xsk = xsk_parent.derive_child(i);

    xsk.write(&mut (unsafe { &mut *xsk_i })[..])
        .expect("should be able to serialize an ExtendedSpendingKey");
}

/// Derive the Sapling internal spending key from the external extended
/// spending key
#[no_mangle]
pub extern "C" fn librustzcash_zip32_sapling_xsk_derive_internal(
    xsk_external: *const [c_uchar; 169],
    xsk_internal_ret: *mut [c_uchar; 169],
) {
    let xsk_external = zip32::ExtendedSpendingKey::read(&unsafe { *xsk_external }[..])
        .expect("valid ExtendedSpendingKey");

    let xsk_internal = xsk_external.derive_internal();

    xsk_internal
        .write(&mut (unsafe { &mut *xsk_internal_ret })[..])
        .expect("should be able to serialize an ExtendedSpendingKey");
}

/// Derive a child ExtendedFullViewingKey from a parent.
#[no_mangle]
pub extern "C" fn librustzcash_zip32_sapling_xfvk_derive(
    xfvk_parent: *const [c_uchar; 169],
    i: u32,
    xfvk_i: *mut [c_uchar; 169],
) -> bool {
    let xfvk_parent = zip32::ExtendedFullViewingKey::read(&unsafe { *xfvk_parent }[..])
        .expect("valid ExtendedFullViewingKey");
    let i = zip32::ChildIndex::from_index(i);

    let xfvk = match xfvk_parent.derive_child(i) {
        Ok(xfvk) => xfvk,
        Err(_) => return false,
    };

    xfvk.write(&mut (unsafe { &mut *xfvk_i })[..])
        .expect("should be able to serialize an ExtendedFullViewingKey");

    true
}

/// Derive the Sapling internal full viewing key from the corresponding external full viewing key
#[no_mangle]
pub extern "C" fn librustzcash_zip32_sapling_derive_internal_fvk(
    fvk: *const [c_uchar; 96],
    dk: *const [c_uchar; 32],
    fvk_ret: *mut [c_uchar; 96],
    dk_ret: *mut [c_uchar; 32],
) {
    let fvk = FullViewingKey::read(&unsafe { *fvk }[..]).expect("valid Sapling FullViewingKey");
    let dk = zip32::sapling::DiversifierKey::from_bytes(unsafe { *dk });

    let (fvk_internal, dk_internal) = sapling_derive_internal_fvk(&fvk, &dk);
    let fvk_ret = unsafe { &mut *fvk_ret };
    let dk_ret = unsafe { &mut *dk_ret };

    fvk_ret.copy_from_slice(&fvk_internal.to_bytes());
    dk_ret.copy_from_slice(dk_internal.as_bytes());
}

/// Derive a PaymentAddress from an ExtendedFullViewingKey.
#[no_mangle]
pub extern "C" fn librustzcash_zip32_sapling_address(
    fvk: *const [c_uchar; 96],
    dk: *const [c_uchar; 32],
    j: *const [c_uchar; 11],
    addr_ret: *mut [c_uchar; 43],
) -> bool {
    let fvk = FullViewingKey::read(&unsafe { *fvk }[..]).expect("valid Sapling FullViewingKey");
    let dk = zip32::sapling::DiversifierKey::from_bytes(unsafe { *dk });
    let j = zip32::DiversifierIndex(unsafe { *j });

    match sapling_address(&fvk, &dk, j) {
        Some(addr) => {
            let addr_ret = unsafe { &mut *addr_ret };
            addr_ret.copy_from_slice(&addr.to_bytes());

            true
        }
        None => false,
    }
}

/// Derive a PaymentAddress from an ExtendedFullViewingKey.
#[no_mangle]
pub extern "C" fn librustzcash_zip32_find_sapling_address(
    fvk: *const [c_uchar; 96],
    dk: *const [c_uchar; 32],
    j: *const [c_uchar; 11],
    j_ret: *mut [c_uchar; 11],
    addr_ret: *mut [c_uchar; 43],
) -> bool {
    let fvk = FullViewingKey::read(&unsafe { *fvk }[..]).expect("valid Sapling FullViewingKey");
    let dk = zip32::sapling::DiversifierKey::from_bytes(unsafe { *dk });
    let j = zip32::DiversifierIndex(unsafe { *j });

    match sapling_find_address(&fvk, &dk, j) {
        Some((j, addr)) => {
            let j_ret = unsafe { &mut *j_ret };
            let addr_ret = unsafe { &mut *addr_ret };

            j_ret.copy_from_slice(&j.0);
            addr_ret.copy_from_slice(&addr.to_bytes());

            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn librustzcash_sapling_diversifier_index(
    dk: *const [c_uchar; 32],
    d: *const [c_uchar; 11],
    j_ret: *mut [c_uchar; 11],
) {
    let dk = zip32::sapling::DiversifierKey::from_bytes(unsafe { *dk });
    let diversifier = Diversifier(unsafe { *d });
    let j_ret = unsafe { &mut *j_ret };

    let j = dk.diversifier_index(&diversifier);
    j_ret.copy_from_slice(&j.0);
}

#[no_mangle]
pub extern "C" fn librustzcash_getrandom(buf: *mut u8, buf_len: usize) {
    let buf = unsafe { slice::from_raw_parts_mut(buf, buf_len) };
    OsRng.fill_bytes(buf);
}
