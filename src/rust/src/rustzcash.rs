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
use blake2s_simd::Params as Blake2sParams;
use bls12_381::Bls12;
use group::{cofactor::CofactorGroup, GroupEncoding};
use incrementalmerkletree::Hashable;
use libc::{c_uchar, size_t};
use rand_core::{OsRng, RngCore};
use std::fs::File;
use std::io::BufReader;
use std::path::{Path, PathBuf};
use std::slice;
use std::sync::Once;
use subtle::CtOption;
use tracing::info;

#[cfg(not(target_os = "windows"))]
use std::ffi::OsStr;
#[cfg(not(target_os = "windows"))]
use std::os::unix::ffi::OsStrExt;

#[cfg(target_os = "windows")]
use std::ffi::OsString;
#[cfg(target_os = "windows")]
use std::os::windows::ffi::OsStringExt;

use zcash_note_encryption::{Domain, EphemeralKeyBytes};
use zcash_primitives::{
    consensus::Network,
    constants::{CRH_IVK_PERSONALIZATION, PROOF_GENERATION_KEY_GENERATOR, SPENDING_KEY_GENERATOR},
    merkle_tree::HashSer,
    sapling::{
        keys::FullViewingKey,
        merkle_hash,
        note::{ExtractedNoteCommitment, NoteCommitment},
        note_encryption::{PreparedIncomingViewingKey, SaplingDomain},
        redjubjub, spend_sig,
        value::NoteValue,
        Diversifier, Node, Note, NullifierDerivingKey, PaymentAddress, Rseed, SaplingIvk,
    },
    zip32::{self, sapling_address, sapling_derive_internal_fvk, sapling_find_address},
};
use zcash_proofs::{load_parameters, sprout};

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
mod streams;
mod transaction_ffi;
mod unified_keys_ffi;
mod wallet;
mod wallet_scanner;
mod zip339_ffi;

mod test_harness_ffi;

#[cfg(test)]
mod tests;

static PROOF_PARAMETERS_LOADED: Once = Once::new();
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

/// Reads an FsRepr from a [u8; 32]
/// and multiplies it by the given base.
fn fixed_scalar_mult(from: &[u8; 32], p_g: &jubjub::SubgroupPoint) -> jubjub::SubgroupPoint {
    // We only call this with `from` being a valid jubjub::Scalar.
    let f = jubjub::Scalar::from_bytes(from).unwrap();

    p_g * f
}

/// Loads the zk-SNARK parameters into memory and saves paths as necessary.
/// Only called once.
///
/// If `load_proving_keys` is `false`, the proving keys will not be loaded, making it
/// impossible to create proofs. This flag is for the Boost test suite, which never
/// creates shielded transactions, but exercises code that requires the verifying keys to
/// be present even if there are no shielded components to verify.
#[no_mangle]
pub extern "C" fn librustzcash_init_zksnark_params(
    #[cfg(not(target_os = "windows"))] spend_path: *const u8,
    #[cfg(target_os = "windows")] spend_path: *const u16,
    spend_path_len: usize,
    #[cfg(not(target_os = "windows"))] output_path: *const u8,
    #[cfg(target_os = "windows")] output_path: *const u16,
    output_path_len: usize,
    #[cfg(not(target_os = "windows"))] sprout_path: *const u8,
    #[cfg(target_os = "windows")] sprout_path: *const u16,
    sprout_path_len: usize,
    load_proving_keys: bool,
) {
    PROOF_PARAMETERS_LOADED.call_once(|| {
        #[cfg(not(target_os = "windows"))]
        let (spend_path, output_path, sprout_path) = {
            (
                OsStr::from_bytes(unsafe { slice::from_raw_parts(spend_path, spend_path_len) }),
                OsStr::from_bytes(unsafe { slice::from_raw_parts(output_path, output_path_len) }),
                if sprout_path.is_null() {
                    None
                } else {
                    Some(OsStr::from_bytes(unsafe {
                        slice::from_raw_parts(sprout_path, sprout_path_len)
                    }))
                },
            )
        };

        #[cfg(target_os = "windows")]
        let (spend_path, output_path, sprout_path) = {
            (
                OsString::from_wide(unsafe { slice::from_raw_parts(spend_path, spend_path_len) }),
                OsString::from_wide(unsafe { slice::from_raw_parts(output_path, output_path_len) }),
                if sprout_path.is_null() {
                    None
                } else {
                    Some(OsString::from_wide(unsafe {
                        slice::from_raw_parts(sprout_path, sprout_path_len)
                    }))
                },
            )
        };

        let (spend_path, output_path, sprout_path) = (
            Path::new(&spend_path),
            Path::new(&output_path),
            sprout_path.as_ref().map(Path::new),
        );

        // Load params
        let params = load_parameters(spend_path, output_path, sprout_path);
        let sapling_spend_params = params.spend_params;
        let sapling_output_params = params.output_params;

        // We need to clone these because we aren't necessarily storing the proving
        // parameters in memory.
        let sapling_spend_vk = sapling_spend_params.vk.clone();
        let sapling_output_vk = sapling_output_params.vk.clone();

        // Generate Orchard parameters.
        info!(target: "main", "Loading Orchard parameters");
        let orchard_pk = load_proving_keys.then(orchard::circuit::ProvingKey::build);
        let orchard_vk = orchard::circuit::VerifyingKey::build();

        // Caller is responsible for calling this function once, so
        // these global mutations are safe.
        unsafe {
            SAPLING_SPEND_PARAMS = load_proving_keys.then_some(sapling_spend_params);
            SAPLING_OUTPUT_PARAMS = load_proving_keys.then_some(sapling_output_params);
            SPROUT_GROTH16_PARAMS_PATH = sprout_path.map(|p| p.to_owned());

            SAPLING_SPEND_VK = Some(sapling_spend_vk);
            SAPLING_OUTPUT_VK = Some(sapling_output_vk);
            SPROUT_GROTH16_VK = params.sprout_vk;

            ORCHARD_PK = orchard_pk;
            ORCHARD_VK = Some(orchard_vk);
        }
    });
}

/// Writes the "uncommitted" note value for empty leaves of the Merkle tree.
///
/// `result` must be a valid pointer to 32 bytes which will be written.
#[no_mangle]
pub extern "C" fn librustzcash_tree_uncommitted(result: *mut [c_uchar; 32]) {
    // Should be okay, caller is responsible for ensuring the pointer
    // is a valid pointer to 32 bytes that can be mutated.
    let result = unsafe { &mut *result };
    Node::empty_leaf()
        .write(&mut result[..])
        .expect("Sapling leaves are 32 bytes");
}

/// Computes a merkle tree hash for a given depth. The `depth` parameter should
/// not be larger than 62.
///
/// `a` and `b` each must be of length 32, and must each be scalars of BLS12-381.
///
/// The result of the merkle tree hash is placed in `result`, which must also be
/// of length 32.
#[no_mangle]
pub extern "C" fn librustzcash_merkle_hash(
    depth: size_t,
    a: *const [c_uchar; 32],
    b: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) {
    // Should be okay, because caller is responsible for ensuring
    // the pointers are valid pointers to 32 bytes.
    let tmp = merkle_hash(depth, unsafe { &*a }, unsafe { &*b });

    // Should be okay, caller is responsible for ensuring the pointer
    // is a valid pointer to 32 bytes that can be mutated.
    let result = unsafe { &mut *result };
    *result = tmp;
}

#[no_mangle] // ToScalar
pub extern "C" fn librustzcash_to_scalar(input: *const [c_uchar; 64], result: *mut [c_uchar; 32]) {
    // Should be okay, because caller is responsible for ensuring
    // the pointer is a valid pointer to 32 bytes, and that is the
    // size of the representation
    let scalar = jubjub::Scalar::from_bytes_wide(unsafe { &*input });

    let result = unsafe { &mut *result };

    *result = scalar.to_bytes();
}

#[no_mangle]
pub extern "C" fn librustzcash_ask_to_ak(ask: *const [c_uchar; 32], result: *mut [c_uchar; 32]) {
    let ask = unsafe { &*ask };
    let ak = fixed_scalar_mult(ask, &SPENDING_KEY_GENERATOR);

    let result = unsafe { &mut *result };

    *result = ak.to_bytes();
}

#[no_mangle]
pub extern "C" fn librustzcash_nsk_to_nk(nsk: *const [c_uchar; 32], result: *mut [c_uchar; 32]) {
    let nsk = unsafe { &*nsk };
    let nk = fixed_scalar_mult(nsk, &PROOF_GENERATION_KEY_GENERATOR);

    let result = unsafe { &mut *result };

    *result = nk.to_bytes();
}

#[no_mangle]
pub extern "C" fn librustzcash_crh_ivk(
    ak: *const [c_uchar; 32],
    nk: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) {
    let ak = unsafe { &*ak };
    let nk = unsafe { &*nk };

    let mut h = Blake2sParams::new()
        .hash_length(32)
        .personal(CRH_IVK_PERSONALIZATION)
        .to_state();
    h.update(ak);
    h.update(nk);
    let mut h = h.finalize().as_ref().to_vec();

    // Drop the last five bits, so it can be interpreted as a scalar.
    h[31] &= 0b0000_0111;

    let result = unsafe { &mut *result };

    result.copy_from_slice(&h);
}

#[no_mangle]
pub extern "C" fn librustzcash_check_diversifier(diversifier: *const [c_uchar; 11]) -> bool {
    let diversifier = Diversifier(unsafe { *diversifier });
    diversifier.g_d().is_some()
}

#[no_mangle]
pub extern "C" fn librustzcash_ivk_to_pkd(
    ivk: *const [c_uchar; 32],
    diversifier: *const [c_uchar; 11],
    result: *mut [c_uchar; 32],
) -> bool {
    let ivk = de_ct(jubjub::Scalar::from_bytes(unsafe { &*ivk }));
    let diversifier = Diversifier(unsafe { *diversifier });
    if let (Some(ivk), Some(g_d)) = (ivk, diversifier.g_d()) {
        let pk_d = g_d * ivk;

        let result = unsafe { &mut *result };

        *result = pk_d.to_bytes();

        true
    } else {
        false
    }
}

/// Test generation of commitment randomness
#[test]
fn test_gen_r() {
    let mut r1 = [0u8; 32];
    let mut r2 = [0u8; 32];

    // Verify different r values are generated
    librustzcash_sapling_generate_r(&mut r1);
    librustzcash_sapling_generate_r(&mut r2);
    assert_ne!(r1, r2);

    // Verify r values are valid in the field
    let _ = jubjub::Scalar::from_bytes(&r1).unwrap();
    let _ = jubjub::Scalar::from_bytes(&r2).unwrap();
}

/// Generate uniformly random scalar in Jubjub. The result is of length 32.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_generate_r(result: *mut [c_uchar; 32]) {
    // create random 64 byte buffer
    let mut rng = OsRng;
    let mut buffer = [0u8; 64];
    rng.fill_bytes(&mut buffer);

    // reduce to uniform value
    let r = jubjub::Scalar::from_bytes_wide(&buffer);
    let result = unsafe { &mut *result };
    *result = r.to_bytes();
}

// Private utility function to get Note from C parameters
fn priv_get_note(
    diversifier: *const [c_uchar; 11],
    pk_d: *const [c_uchar; 32],
    value: u64,
    rcm: *const [c_uchar; 32],
) -> Result<Note, ()> {
    let recipient_bytes = {
        let mut tmp = [0; 43];
        tmp[..11].copy_from_slice(unsafe { &*diversifier });
        tmp[11..].copy_from_slice(unsafe { &*pk_d });
        tmp
    };
    let recipient = PaymentAddress::from_bytes(&recipient_bytes).ok_or(())?;

    // Deserialize randomness
    // If this is after ZIP 212, the caller has calculated rcm, and we don't need to call
    // Note::derive_esk, so we just pretend the note was using this rcm all along.
    let rseed = Rseed::BeforeZip212(de_ct(jubjub::Scalar::from_bytes(unsafe { &*rcm })).ok_or(())?);

    Ok(Note::from_parts(
        recipient,
        NoteValue::from_raw(value),
        rseed,
    ))
}

/// Compute a Sapling nullifier.
///
/// The `diversifier` parameter must be 11 bytes in length.
/// The `pk_d`, `r`, `ak` and `nk` parameters must be of length 32.
/// The result is also of length 32 and placed in `result`.
/// Returns false if `diversifier` or `pk_d` is not valid.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_compute_nf(
    diversifier: *const [c_uchar; 11],
    pk_d: *const [c_uchar; 32],
    value: u64,
    rcm: *const [c_uchar; 32],
    nk: *const [c_uchar; 32],
    position: u64,
    result: *mut [c_uchar; 32],
) -> bool {
    // ZIP 216: Nullifier derivation is not consensus-critical
    // (nullifiers are revealed, not calculated by consensus).
    // In any case, ZIP 216 is now enabled retroactively.
    let note = match priv_get_note(diversifier, pk_d, value, rcm) {
        Ok(p) => p,
        Err(_) => return false,
    };

    let nk = match de_ct(jubjub::ExtendedPoint::from_bytes(unsafe { &*nk })) {
        Some(p) => p,
        None => return false,
    };

    let nk = match de_ct(nk.into_subgroup()) {
        Some(nk) => NullifierDerivingKey(nk),
        None => return false,
    };

    let nf = note.nf(&nk, position);
    let result = unsafe { &mut *result };
    result.copy_from_slice(&nf.0);

    true
}

/// Compute a Sapling commitment.
///
/// The `diversifier` parameter must be 11 bytes in length.
/// The `pk_d` and `r` parameters must be of length 32.
/// The result is also of length 32 and placed in `result`.
/// Returns false if `diversifier` or `pk_d` is not valid.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_compute_cmu(
    diversifier: *const [c_uchar; 11],
    pk_d: *const [c_uchar; 32],
    value: u64,
    rcm: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) -> bool {
    let get_cm = || -> Result<NoteCommitment, ()> {
        let diversifier = Diversifier(unsafe { *diversifier });
        let g_d = diversifier.g_d().ok_or(())?;

        let pk_d = de_ct(jubjub::ExtendedPoint::from_bytes(unsafe { &*pk_d })).ok_or(())?;
        let pk_d = de_ct(pk_d.into_subgroup()).ok_or(())?;

        let rcm = de_ct(jubjub::Scalar::from_bytes(unsafe { &*rcm })).ok_or(())?;

        Ok(NoteCommitment::temporary_zcashd_derive(
            g_d.to_bytes(),
            pk_d.to_bytes(),
            NoteValue::from_raw(value),
            rcm,
        ))
    };

    let cmu = match get_cm() {
        Ok(cm) => ExtractedNoteCommitment::from(cm),
        Err(()) => return false,
    };

    let result = unsafe { &mut *result };
    *result = cmu.to_bytes();

    true
}

/// Computes KDF^Sapling(KA^Agree(sk, P), ephemeral_key).
///
/// `p` and `sk` must point to 32-byte buffers. If `p` does not represent a compressed
/// Jubjub point or `sk` does not represent a canonical Jubjub scalar, this function
/// returns `false`. Otherwise, it writes the result to the 32-byte `result` buffer and
/// returns `true`.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_ka_derive_symmetric_key(
    p: *const [c_uchar; 32],
    sk: *const [c_uchar; 32],
    ephemeral_key: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) -> bool {
    // Deserialize p (representing either epk or pk_d; we can handle them identically).
    let epk = match SaplingDomain::<Network>::epk(&EphemeralKeyBytes(unsafe { *p })) {
        Some(p) => SaplingDomain::<Network>::prepare_epk(p),
        None => return false,
    };

    // Deserialize sk (either ivk or esk; we can handle them identically).
    let ivk = match de_ct(jubjub::Scalar::from_bytes(unsafe { &*sk })) {
        Some(p) => PreparedIncomingViewingKey::new(&SaplingIvk(p)),
        None => return false,
    };

    // Compute key agreement
    let secret = SaplingDomain::<Network>::ka_agree_dec(&ivk, &epk);

    // Produce result
    let result = unsafe { &mut *result };
    result.clone_from_slice(
        SaplingDomain::<Network>::kdf(secret, &EphemeralKeyBytes(unsafe { *ephemeral_key }))
            .as_bytes(),
    );

    true
}

/// Compute g_d = GH(diversifier) and returns false if the diversifier is
/// invalid. Computes \[esk\] g_d and writes the result to the 32-byte `result`
/// buffer. Returns false if `esk` is not a valid scalar.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_ka_derivepublic(
    diversifier: *const [c_uchar; 11],
    esk: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) -> bool {
    let diversifier = Diversifier(unsafe { *diversifier });

    // Compute g_d from the diversifier
    let g_d = match diversifier.g_d() {
        Some(g) => g,
        None => return false,
    };

    // Deserialize esk
    let esk = match de_ct(jubjub::Scalar::from_bytes(unsafe { &*esk })) {
        Some(p) => p,
        None => return false,
    };

    let p = g_d * esk;

    let result = unsafe { &mut *result };
    *result = p.to_bytes();

    true
}

const GROTH_PROOF_SIZE: usize = 48 // π_A
    + 96 // π_B
    + 48; // π_C

/// Sprout JoinSplit proof generation.
#[no_mangle]
pub extern "C" fn librustzcash_sprout_prove(
    proof_out: *mut [c_uchar; GROTH_PROOF_SIZE],

    phi: *const [c_uchar; 32],
    rt: *const [c_uchar; 32],
    h_sig: *const [c_uchar; 32],

    // First input
    in_sk1: *const [c_uchar; 32],
    in_value1: u64,
    in_rho1: *const [c_uchar; 32],
    in_r1: *const [c_uchar; 32],
    in_auth1: *const [c_uchar; sprout::WITNESS_PATH_SIZE],

    // Second input
    in_sk2: *const [c_uchar; 32],
    in_value2: u64,
    in_rho2: *const [c_uchar; 32],
    in_r2: *const [c_uchar; 32],
    in_auth2: *const [c_uchar; sprout::WITNESS_PATH_SIZE],

    // First output
    out_pk1: *const [c_uchar; 32],
    out_value1: u64,
    out_r1: *const [c_uchar; 32],

    // Second output
    out_pk2: *const [c_uchar; 32],
    out_value2: u64,
    out_r2: *const [c_uchar; 32],

    // Public value
    vpub_old: u64,
    vpub_new: u64,
) {
    // Load parameters from disk
    let sprout_fs =
        File::open(unsafe { &SPROUT_GROTH16_PARAMS_PATH }.as_ref().expect(
            "Parameters not loaded: SPROUT_GROTH16_PARAMS_PATH should have been initialized",
        ))
        .expect("couldn't load Sprout groth16 parameters file");

    let mut sprout_fs = BufReader::with_capacity(1024 * 1024, sprout_fs);

    let params = Parameters::read(&mut sprout_fs, false)
        .expect("couldn't deserialize Sprout JoinSplit parameters file");

    drop(sprout_fs);

    let proof = sprout::create_proof(
        unsafe { *phi },
        unsafe { *rt },
        unsafe { *h_sig },
        unsafe { *in_sk1 },
        in_value1,
        unsafe { *in_rho1 },
        unsafe { *in_r1 },
        unsafe { &*in_auth1 },
        unsafe { *in_sk2 },
        in_value2,
        unsafe { *in_rho2 },
        unsafe { *in_r2 },
        unsafe { &*in_auth2 },
        unsafe { *out_pk1 },
        out_value1,
        unsafe { *out_r1 },
        unsafe { *out_pk2 },
        out_value2,
        unsafe { *out_r2 },
        vpub_old,
        vpub_new,
        &params,
    );

    proof
        .write(&mut (unsafe { &mut *proof_out })[..])
        .expect("should be able to serialize a proof");
}

/// Sprout JoinSplit proof verification.
#[no_mangle]
pub extern "C" fn librustzcash_sprout_verify(
    proof: *const [c_uchar; GROTH_PROOF_SIZE],
    rt: *const [c_uchar; 32],
    h_sig: *const [c_uchar; 32],
    mac1: *const [c_uchar; 32],
    mac2: *const [c_uchar; 32],
    nf1: *const [c_uchar; 32],
    nf2: *const [c_uchar; 32],
    cm1: *const [c_uchar; 32],
    cm2: *const [c_uchar; 32],
    vpub_old: u64,
    vpub_new: u64,
) -> bool {
    sprout::verify_proof(
        unsafe { &*proof },
        unsafe { &*rt },
        unsafe { &*h_sig },
        unsafe { &*mac1 },
        unsafe { &*mac2 },
        unsafe { &*nf1 },
        unsafe { &*nf2 },
        unsafe { &*cm1 },
        unsafe { &*cm2 },
        vpub_old,
        vpub_new,
        unsafe { SPROUT_GROTH16_VK.as_ref() }
            .expect("Parameters not loaded: SPROUT_GROTH16_VK should have been initialized"),
    )
}

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
