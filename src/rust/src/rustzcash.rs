//! FFI between the C++ zcashd codebase and the Rust Zcash crates.
//!
//! This is internal to zcashd and is not an officially-supported API.

// Catch documentation errors caused by code changes.
#![deny(intra_doc_link_resolution_failure)]
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

use bellman::groth16::{Parameters, PreparedVerifyingKey, Proof};
use blake2s_simd::Params as Blake2sParams;
use ff::{PrimeField, PrimeFieldRepr};
use lazy_static;
use libc::{c_char, c_uchar, size_t};
use pairing::bls12_381::{Bls12, Fr, FrRepr};
use rand_core::{OsRng, RngCore};
use std::ffi::CStr;
use std::fs::File;
use std::io::BufReader;
use std::path::{Path, PathBuf};
use std::slice;

#[cfg(not(target_os = "windows"))]
use std::ffi::OsStr;
#[cfg(not(target_os = "windows"))]
use std::os::unix::ffi::OsStrExt;

#[cfg(target_os = "windows")]
use std::ffi::OsString;
#[cfg(target_os = "windows")]
use std::os::windows::ffi::OsStringExt;

use zcash_primitives::{
    block::equihash,
    constants::CRH_IVK_PERSONALIZATION,
    jubjub::{
        edwards,
        fs::{Fs, FsRepr},
        FixedGenerators, JubjubEngine, JubjubParams, PrimeOrder, ToUniform, Unknown,
    },
    merkle_tree::MerklePath,
    note_encryption::sapling_ka_agree,
    primitives::{Diversifier, Note, PaymentAddress, ProofGenerationKey, ViewingKey},
    redjubjub::{self, Signature},
    sapling::{merkle_hash, spend_sig},
    transaction::components::Amount,
    zip32, JUBJUB,
};
use zcash_proofs::{
    circuit::sapling::TREE_DEPTH as SAPLING_TREE_DEPTH,
    load_parameters,
    sapling::{SaplingProvingContext, SaplingVerificationContext},
    sprout,
};

use zcash_history::{Entry as MMREntry, NodeData as MMRNodeData, Tree as MMRTree};

#[cfg(test)]
mod tests;

static mut SAPLING_SPEND_VK: Option<PreparedVerifyingKey<Bls12>> = None;
static mut SAPLING_OUTPUT_VK: Option<PreparedVerifyingKey<Bls12>> = None;
static mut SPROUT_GROTH16_VK: Option<PreparedVerifyingKey<Bls12>> = None;

static mut SAPLING_SPEND_PARAMS: Option<Parameters<Bls12>> = None;
static mut SAPLING_OUTPUT_PARAMS: Option<Parameters<Bls12>> = None;
static mut SPROUT_GROTH16_PARAMS_PATH: Option<PathBuf> = None;

/// Reads an FrRepr from a [u8; 32].
fn read_fr(from: &[u8; 32]) -> FrRepr {
    let mut f = FrRepr::default();
    f.read_le(&from[..]).expect("length is 32 bytes");
    f
}

/// Reads an FsRepr from a [u8; 32].
fn read_fs(from: &[u8; 32]) -> FsRepr {
    let mut f = <<Bls12 as JubjubEngine>::Fs as PrimeField>::Repr::default();
    f.read_le(&from[..]).expect("length is 32 bytes");
    f
}

/// Reads an FsRepr from a [u8; 32]
/// and multiplies it by the given base.
fn fixed_scalar_mult(from: &[u8; 32], p_g: FixedGenerators) -> edwards::Point<Bls12, PrimeOrder> {
    let f = read_fs(from);

    JUBJUB.generator(p_g).mul(f, &JUBJUB)
}

/// Loads the zk-SNARK parameters into memory and saves paths as necessary.
/// Only called once.
#[cfg(not(target_os = "windows"))]
#[no_mangle]
pub extern "C" fn librustzcash_init_zksnark_params(
    spend_path: *const u8,
    spend_path_len: usize,
    spend_hash: *const c_char,
    output_path: *const u8,
    output_path_len: usize,
    output_hash: *const c_char,
    sprout_path: *const u8,
    sprout_path_len: usize,
    sprout_hash: *const c_char,
) {
    let spend_path = Path::new(OsStr::from_bytes(unsafe {
        slice::from_raw_parts(spend_path, spend_path_len)
    }));
    let output_path = Path::new(OsStr::from_bytes(unsafe {
        slice::from_raw_parts(output_path, output_path_len)
    }));
    let sprout_path = if sprout_path.is_null() {
        None
    } else {
        Some(Path::new(OsStr::from_bytes(unsafe {
            slice::from_raw_parts(sprout_path, sprout_path_len)
        })))
    };

    init_zksnark_params(
        spend_path,
        spend_hash,
        output_path,
        output_hash,
        sprout_path,
        sprout_hash,
    )
}

/// Loads the zk-SNARK parameters into memory and saves paths as necessary.
/// Only called once.
#[cfg(target_os = "windows")]
#[no_mangle]
pub extern "C" fn librustzcash_init_zksnark_params(
    spend_path: *const u16,
    spend_path_len: usize,
    spend_hash: *const c_char,
    output_path: *const u16,
    output_path_len: usize,
    output_hash: *const c_char,
    sprout_path: *const u16,
    sprout_path_len: usize,
    sprout_hash: *const c_char,
) {
    let spend_path =
        OsString::from_wide(unsafe { slice::from_raw_parts(spend_path, spend_path_len) });
    let output_path =
        OsString::from_wide(unsafe { slice::from_raw_parts(output_path, output_path_len) });
    let sprout_path = if sprout_path.is_null() {
        None
    } else {
        Some(OsString::from_wide(unsafe {
            slice::from_raw_parts(sprout_path, sprout_path_len)
        }))
    };

    init_zksnark_params(
        Path::new(&spend_path),
        spend_hash,
        Path::new(&output_path),
        output_hash,
        sprout_path.as_ref().map(|p| Path::new(p)),
        sprout_hash,
    )
}

fn init_zksnark_params(
    spend_path: &Path,
    spend_hash: *const c_char,
    output_path: &Path,
    output_hash: *const c_char,
    sprout_path: Option<&Path>,
    sprout_hash: *const c_char,
) {
    // Initialize jubjub parameters here
    lazy_static::initialize(&JUBJUB);

    let spend_hash = unsafe { CStr::from_ptr(spend_hash) }
        .to_str()
        .expect("hash should be a valid string");

    let output_hash = unsafe { CStr::from_ptr(output_hash) }
        .to_str()
        .expect("hash should be a valid string");

    let sprout_hash = if sprout_path.is_none() {
        None
    } else {
        Some(
            unsafe { CStr::from_ptr(sprout_hash) }
                .to_str()
                .expect("hash should be a valid string"),
        )
    };

    // Load params
    let (spend_params, spend_vk, output_params, output_vk, sprout_vk) = load_parameters(
        spend_path,
        spend_hash,
        output_path,
        output_hash,
        sprout_path,
        sprout_hash,
    );

    // Caller is responsible for calling this function once, so
    // these global mutations are safe.
    unsafe {
        SAPLING_SPEND_PARAMS = Some(spend_params);
        SAPLING_OUTPUT_PARAMS = Some(output_params);
        SPROUT_GROTH16_PARAMS_PATH = sprout_path.map(|p| p.to_owned());

        SAPLING_SPEND_VK = Some(spend_vk);
        SAPLING_OUTPUT_VK = Some(output_vk);
        SPROUT_GROTH16_VK = sprout_vk;
    }
}

/// Writes the "uncommitted" note value for empty leaves of the Merkle tree.
///
/// `result` must be a valid pointer to 32 bytes which will be written.
#[no_mangle]
pub extern "C" fn librustzcash_tree_uncommitted(result: *mut [c_uchar; 32]) {
    let tmp = Note::<Bls12>::uncommitted().into_repr();

    // Should be okay, caller is responsible for ensuring the pointer
    // is a valid pointer to 32 bytes that can be mutated.
    let result = unsafe { &mut *result };
    tmp.write_le(&mut result[..]).expect("length is 32 bytes");
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
    // the pointer is a valid pointer to 32 bytes, and that is the
    // size of the representation
    let a_repr = read_fr(unsafe { &*a });

    // Should be okay, because caller is responsible for ensuring
    // the pointer is a valid pointer to 32 bytes, and that is the
    // size of the representation
    let b_repr = read_fr(unsafe { &*b });

    let tmp = merkle_hash(depth, &a_repr, &b_repr);

    // Should be okay, caller is responsible for ensuring the pointer
    // is a valid pointer to 32 bytes that can be mutated.
    let result = unsafe { &mut *result };
    tmp.write_le(&mut result[..]).expect("length is 32 bytes");
}

#[no_mangle] // ToScalar
pub extern "C" fn librustzcash_to_scalar(input: *const [c_uchar; 64], result: *mut [c_uchar; 32]) {
    // Should be okay, because caller is responsible for ensuring
    // the pointer is a valid pointer to 32 bytes, and that is the
    // size of the representation
    let scalar = <Bls12 as JubjubEngine>::Fs::to_uniform(unsafe { &(&*input)[..] }).into_repr();

    let result = unsafe { &mut *result };

    scalar
        .write_le(&mut result[..])
        .expect("length is 32 bytes");
}

#[no_mangle]
pub extern "C" fn librustzcash_ask_to_ak(ask: *const [c_uchar; 32], result: *mut [c_uchar; 32]) {
    let ask = unsafe { &*ask };
    let ak = fixed_scalar_mult(ask, FixedGenerators::SpendingKeyGenerator);

    let result = unsafe { &mut *result };

    ak.write(&mut result[..]).expect("length is 32 bytes");
}

#[no_mangle]
pub extern "C" fn librustzcash_nsk_to_nk(nsk: *const [c_uchar; 32], result: *mut [c_uchar; 32]) {
    let nsk = unsafe { &*nsk };
    let nk = fixed_scalar_mult(nsk, FixedGenerators::ProofGenerationKey);

    let result = unsafe { &mut *result };

    nk.write(&mut result[..]).expect("length is 32 bytes");
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
    diversifier.g_d::<Bls12>(&JUBJUB).is_some()
}

#[no_mangle]
pub extern "C" fn librustzcash_ivk_to_pkd(
    ivk: *const [c_uchar; 32],
    diversifier: *const [c_uchar; 11],
    result: *mut [c_uchar; 32],
) -> bool {
    let ivk = read_fs(unsafe { &*ivk });
    let diversifier = Diversifier(unsafe { *diversifier });
    if let Some(g_d) = diversifier.g_d::<Bls12>(&JUBJUB) {
        let pk_d = g_d.mul(ivk, &JUBJUB);

        let result = unsafe { &mut *result };

        pk_d.write(&mut result[..]).expect("length is 32 bytes");

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
    let mut repr = FsRepr::default();
    repr.read_le(&r1[..]).expect("length is not 32 bytes");
    let _ = Fs::from_repr(repr).unwrap();
    repr.read_le(&r2[..]).expect("length is not 32 bytes");
    let _ = Fs::from_repr(repr).unwrap();
}

/// Generate uniformly random scalar in Jubjub. The result is of length 32.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_generate_r(result: *mut [c_uchar; 32]) {
    // create random 64 byte buffer
    let mut rng = OsRng;
    let mut buffer = [0u8; 64];
    rng.fill_bytes(&mut buffer);

    // reduce to uniform value
    let r = <Bls12 as JubjubEngine>::Fs::to_uniform(&buffer[..]);
    let result = unsafe { &mut *result };
    r.into_repr()
        .write_le(&mut result[..])
        .expect("result must be 32 bytes");
}

// Private utility function to get Note from C parameters
fn priv_get_note(
    diversifier: *const [c_uchar; 11],
    pk_d: *const [c_uchar; 32],
    value: u64,
    r: *const [c_uchar; 32],
) -> Result<Note<Bls12>, ()> {
    let diversifier = Diversifier(unsafe { *diversifier });
    let g_d = diversifier.g_d::<Bls12>(&JUBJUB).ok_or(())?;

    let pk_d = edwards::Point::<Bls12, Unknown>::read(&(unsafe { &*pk_d })[..], &JUBJUB)
        .map_err(|_| ())?;

    let pk_d = pk_d.as_prime_order(&JUBJUB).ok_or(())?;

    // Deserialize randomness
    let r = Fs::from_repr(read_fs(unsafe { &*r })).map_err(|_| ())?;

    let note = Note {
        value,
        g_d,
        pk_d,
        r,
    };

    Ok(note)
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
    r: *const [c_uchar; 32],
    ak: *const [c_uchar; 32],
    nk: *const [c_uchar; 32],
    position: u64,
    result: *mut [c_uchar; 32],
) -> bool {
    let note = match priv_get_note(diversifier, pk_d, value, r) {
        Ok(p) => p,
        Err(_) => return false,
    };

    let ak = match edwards::Point::<Bls12, Unknown>::read(&(unsafe { &*ak })[..], &JUBJUB) {
        Ok(p) => p,
        Err(_) => return false,
    };

    let ak = match ak.as_prime_order(&JUBJUB) {
        Some(ak) => ak,
        None => return false,
    };

    let nk = match edwards::Point::<Bls12, Unknown>::read(&(unsafe { &*nk })[..], &JUBJUB) {
        Ok(p) => p,
        Err(_) => return false,
    };

    let nk = match nk.as_prime_order(&JUBJUB) {
        Some(nk) => nk,
        None => return false,
    };

    let vk = ViewingKey { ak, nk };
    let nf = note.nf(&vk, position, &JUBJUB);
    let result = unsafe { &mut *result };
    result.copy_from_slice(&nf);

    true
}

/// Compute a Sapling commitment.
///
/// The `diversifier` parameter must be 11 bytes in length.
/// The `pk_d` and `r` parameters must be of length 32.
/// The result is also of length 32 and placed in `result`.
/// Returns false if `diversifier` or `pk_d` is not valid.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_compute_cm(
    diversifier: *const [c_uchar; 11],
    pk_d: *const [c_uchar; 32],
    value: u64,
    r: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) -> bool {
    let note = match priv_get_note(diversifier, pk_d, value, r) {
        Ok(p) => p,
        Err(_) => return false,
    };

    let result = unsafe { &mut *result };
    note.cm(&JUBJUB)
        .into_repr()
        .write_le(&mut result[..])
        .expect("length is 32 bytes");

    true
}

/// Computes \[sk\] \[8\] P for some 32-byte point P, and 32-byte Fs.
///
/// If P or sk are invalid, returns false. Otherwise, the result is written to
/// the 32-byte `result` buffer.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_ka_agree(
    p: *const [c_uchar; 32],
    sk: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) -> bool {
    // Deserialize p
    let p = match edwards::Point::<Bls12, Unknown>::read(&(unsafe { &*p })[..], &JUBJUB) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Deserialize sk
    let sk = match Fs::from_repr(read_fs(unsafe { &*sk })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Compute key agreement
    let ka = sapling_ka_agree(&sk, &p);

    // Produce result
    let result = unsafe { &mut *result };
    ka.write(&mut result[..]).expect("length is not 32 bytes");

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
    let g_d = match diversifier.g_d::<Bls12>(&JUBJUB) {
        Some(g) => g,
        None => return false,
    };

    // Deserialize esk
    let esk = match Fs::from_repr(read_fs(unsafe { &*esk })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    let p = g_d.mul(esk, &JUBJUB);

    let result = unsafe { &mut *result };
    p.write(&mut result[..]).expect("length is not 32 bytes");

    true
}

/// Validates the provided Equihash solution against the given parameters, input
/// and nonce.
#[no_mangle]
pub extern "C" fn librustzcash_eh_isvalid(
    n: u32,
    k: u32,
    input: *const c_uchar,
    input_len: size_t,
    nonce: *const c_uchar,
    nonce_len: size_t,
    soln: *const c_uchar,
    soln_len: size_t,
) -> bool {
    if (k >= n) || (n % 8 != 0) || (soln_len != (1 << k) * ((n / (k + 1)) as usize + 1) / 8) {
        return false;
    }
    let rs_input = unsafe { slice::from_raw_parts(input, input_len) };
    let rs_nonce = unsafe { slice::from_raw_parts(nonce, nonce_len) };
    let rs_soln = unsafe { slice::from_raw_parts(soln, soln_len) };
    equihash::is_valid_solution(n, k, rs_input, rs_nonce, rs_soln)
}

/// Creates a Sapling verification context. Please free this when you're done.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_verification_ctx_init() -> *mut SaplingVerificationContext {
    let ctx = Box::new(SaplingVerificationContext::new());

    Box::into_raw(ctx)
}

/// Frees a Sapling verification context returned from
/// [`librustzcash_sapling_verification_ctx_init`].
#[no_mangle]
pub extern "C" fn librustzcash_sapling_verification_ctx_free(ctx: *mut SaplingVerificationContext) {
    drop(unsafe { Box::from_raw(ctx) });
}

const GROTH_PROOF_SIZE: usize = 48 // π_A
    + 96 // π_B
    + 48; // π_C

/// Check the validity of a Sapling Spend description, accumulating the value
/// commitment into the context.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_check_spend(
    ctx: *mut SaplingVerificationContext,
    cv: *const [c_uchar; 32],
    anchor: *const [c_uchar; 32],
    nullifier: *const [c_uchar; 32],
    rk: *const [c_uchar; 32],
    zkproof: *const [c_uchar; GROTH_PROOF_SIZE],
    spend_auth_sig: *const [c_uchar; 64],
    sighash_value: *const [c_uchar; 32],
) -> bool {
    // Deserialize the value commitment
    let cv = match edwards::Point::<Bls12, Unknown>::read(&(unsafe { &*cv })[..], &JUBJUB) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Deserialize the anchor, which should be an element
    // of Fr.
    let anchor = match Fr::from_repr(read_fr(unsafe { &*anchor })) {
        Ok(a) => a,
        Err(_) => return false,
    };

    // Deserialize rk
    let rk = match redjubjub::PublicKey::<Bls12>::read(&(unsafe { &*rk })[..], &JUBJUB) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Deserialize the signature
    let spend_auth_sig = match Signature::read(&(unsafe { &*spend_auth_sig })[..]) {
        Ok(sig) => sig,
        Err(_) => return false,
    };

    // Deserialize the proof
    let zkproof = match Proof::<Bls12>::read(&(unsafe { &*zkproof })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    unsafe { &mut *ctx }.check_spend(
        cv,
        anchor,
        unsafe { &*nullifier },
        rk,
        unsafe { &*sighash_value },
        spend_auth_sig,
        zkproof,
        unsafe { SAPLING_SPEND_VK.as_ref() }.unwrap(),
        &JUBJUB,
    )
}

/// Check the validity of a Sapling Output description, accumulating the value
/// commitment into the context.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_check_output(
    ctx: *mut SaplingVerificationContext,
    cv: *const [c_uchar; 32],
    cm: *const [c_uchar; 32],
    epk: *const [c_uchar; 32],
    zkproof: *const [c_uchar; GROTH_PROOF_SIZE],
) -> bool {
    // Deserialize the value commitment
    let cv = match edwards::Point::<Bls12, Unknown>::read(&(unsafe { &*cv })[..], &JUBJUB) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Deserialize the commitment, which should be an element
    // of Fr.
    let cm = match Fr::from_repr(read_fr(unsafe { &*cm })) {
        Ok(a) => a,
        Err(_) => return false,
    };

    // Deserialize the ephemeral key
    let epk = match edwards::Point::<Bls12, Unknown>::read(&(unsafe { &*epk })[..], &JUBJUB) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Deserialize the proof
    let zkproof = match Proof::<Bls12>::read(&(unsafe { &*zkproof })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    unsafe { &mut *ctx }.check_output(
        cv,
        cm,
        epk,
        zkproof,
        unsafe { SAPLING_OUTPUT_VK.as_ref() }.unwrap(),
        &JUBJUB,
    )
}

/// Finally checks the validity of the entire Sapling transaction given
/// valueBalance and the binding signature.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_final_check(
    ctx: *mut SaplingVerificationContext,
    value_balance: i64,
    binding_sig: *const [c_uchar; 64],
    sighash_value: *const [c_uchar; 32],
) -> bool {
    let value_balance = match Amount::from_i64(value_balance) {
        Ok(vb) => vb,
        Err(()) => return false,
    };

    // Deserialize the signature
    let binding_sig = match Signature::read(&(unsafe { &*binding_sig })[..]) {
        Ok(sig) => sig,
        Err(_) => return false,
    };

    unsafe { &*ctx }.final_check(
        value_balance,
        unsafe { &*sighash_value },
        binding_sig,
        &JUBJUB,
    )
}

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
    let sprout_fs = File::open(
        unsafe { &SPROUT_GROTH16_PARAMS_PATH }
            .as_ref()
            .expect("parameters should have been initialized"),
    )
    .expect("couldn't load Sprout groth16 parameters file");

    let mut sprout_fs = BufReader::with_capacity(1024 * 1024, sprout_fs);

    let params = Parameters::<Bls12>::read(&mut sprout_fs, false)
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
        unsafe { SPROUT_GROTH16_VK.as_ref() }.expect("parameters should have been initialized"),
    )
}

/// This function (using the proving context) constructs an Output proof given
/// the necessary witness information. It outputs `cv` and the `zkproof`.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_output_proof(
    ctx: *mut SaplingProvingContext,
    esk: *const [c_uchar; 32],
    payment_address: *const [c_uchar; 43],
    rcm: *const [c_uchar; 32],
    value: u64,
    cv: *mut [c_uchar; 32],
    zkproof: *mut [c_uchar; GROTH_PROOF_SIZE],
) -> bool {
    // Grab `esk`, which the caller should have constructed for the DH key exchange.
    let esk = match Fs::from_repr(read_fs(unsafe { &*esk })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Grab the payment address from the caller
    let payment_address =
        match PaymentAddress::<Bls12>::from_bytes(unsafe { &*payment_address }, &JUBJUB) {
            Some(pa) => pa,
            None => return false,
        };

    // The caller provides the commitment randomness for the output note
    let rcm = match Fs::from_repr(read_fs(unsafe { &*rcm })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Create proof
    let (proof, value_commitment) = unsafe { &mut *ctx }.output_proof(
        esk,
        payment_address,
        rcm,
        value,
        unsafe { SAPLING_OUTPUT_PARAMS.as_ref() }.unwrap(),
        &JUBJUB,
    );

    // Write the proof out to the caller
    proof
        .write(&mut (unsafe { &mut *zkproof })[..])
        .expect("should be able to serialize a proof");

    // Write the value commitment to the caller
    value_commitment
        .write(&mut (unsafe { &mut *cv })[..])
        .expect("should be able to serialize rcv");

    true
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
    let ar = match Fs::from_repr(read_fs(unsafe { &*ar })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // The caller provides `ask`, the spend authorizing key.
    let ask = match redjubjub::PrivateKey::<Bls12>::read(&(unsafe { &*ask })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Initialize secure RNG
    let mut rng = OsRng;

    // Do the signing
    let sig = spend_sig(ask, ar, unsafe { &*sighash }, &mut rng, &JUBJUB);

    // Write out the signature
    sig.write(&mut (unsafe { &mut *result })[..])
        .expect("result should be 64 bytes");

    true
}

/// This function (using the proving context) constructs a binding signature.
///
/// You must provide the intended valueBalance so that we can internally check
/// consistency.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_binding_sig(
    ctx: *const SaplingProvingContext,
    value_balance: i64,
    sighash: *const [c_uchar; 32],
    result: *mut [c_uchar; 64],
) -> bool {
    let value_balance = match Amount::from_i64(value_balance) {
        Ok(vb) => vb,
        Err(()) => return false,
    };

    // Sign
    let sig = match unsafe { &*ctx }.binding_sig(value_balance, unsafe { &*sighash }, &JUBJUB) {
        Ok(s) => s,
        Err(_) => return false,
    };

    // Write out signature
    sig.write(&mut (unsafe { &mut *result })[..])
        .expect("result should be 64 bytes");

    true
}

/// This function (using the proving context) constructs a Spend proof given the
/// necessary witness information. It outputs `cv` (the value commitment) and
/// `rk` (so that you don't have to compute it) along with the proof.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_spend_proof(
    ctx: *mut SaplingProvingContext,
    ak: *const [c_uchar; 32],
    nsk: *const [c_uchar; 32],
    diversifier: *const [c_uchar; 11],
    rcm: *const [c_uchar; 32],
    ar: *const [c_uchar; 32],
    value: u64,
    anchor: *const [c_uchar; 32],
    merkle_path: *const [c_uchar; 1 + 33 * SAPLING_TREE_DEPTH + 8],
    cv: *mut [c_uchar; 32],
    rk_out: *mut [c_uchar; 32],
    zkproof: *mut [c_uchar; GROTH_PROOF_SIZE],
) -> bool {
    // Grab `ak` from the caller, which should be a point.
    let ak = match edwards::Point::<Bls12, Unknown>::read(&(unsafe { &*ak })[..], &JUBJUB) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // `ak` should be prime order.
    let ak = match ak.as_prime_order(&JUBJUB) {
        Some(p) => p,
        None => return false,
    };

    // Grab `nsk` from the caller
    let nsk = match Fs::from_repr(read_fs(unsafe { &*nsk })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Construct the proof generation key
    let proof_generation_key = ProofGenerationKey {
        ak: ak.clone(),
        nsk,
    };

    // Grab the diversifier from the caller
    let diversifier = Diversifier(unsafe { *diversifier });

    // The caller chooses the note randomness
    let rcm = match Fs::from_repr(read_fs(unsafe { &*rcm })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // The caller also chooses the re-randomization of ak
    let ar = match Fs::from_repr(read_fs(unsafe { &*ar })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // We need to compute the anchor of the Spend.
    let anchor = match Fr::from_repr(read_fr(unsafe { &*anchor })) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Parse the Merkle path from the caller
    let merkle_path = match MerklePath::from_slice(unsafe { &(&*merkle_path)[..] }) {
        Ok(w) => w,
        Err(_) => return false,
    };

    // Create proof
    let (proof, value_commitment, rk) = unsafe { &mut *ctx }
        .spend_proof(
            proof_generation_key,
            diversifier,
            rcm,
            ar,
            value,
            anchor,
            merkle_path,
            unsafe { SAPLING_SPEND_PARAMS.as_ref() }.unwrap(),
            unsafe { SAPLING_SPEND_VK.as_ref() }.unwrap(),
            &JUBJUB,
        )
        .expect("proving should not fail");

    // Write value commitment to caller
    value_commitment
        .write(&mut unsafe { &mut *cv }[..])
        .expect("should be able to serialize cv");

    // Write proof out to caller
    proof
        .write(&mut (unsafe { &mut *zkproof })[..])
        .expect("should be able to serialize a proof");

    // Write out `rk` to the caller
    rk.write(&mut unsafe { &mut *rk_out }[..])
        .expect("should be able to write to rk_out");

    true
}

/// Creates a Sapling proving context. Please free this when you're done.
#[no_mangle]
pub extern "C" fn librustzcash_sapling_proving_ctx_init() -> *mut SaplingProvingContext {
    let ctx = Box::new(SaplingProvingContext::new());

    Box::into_raw(ctx)
}

/// Frees a Sapling proving context returned from
/// [`librustzcash_sapling_proving_ctx_init`].
#[no_mangle]
pub extern "C" fn librustzcash_sapling_proving_ctx_free(ctx: *mut SaplingProvingContext) {
    drop(unsafe { Box::from_raw(ctx) });
}

/// Derive the master ExtendedSpendingKey from a seed.
#[no_mangle]
pub extern "C" fn librustzcash_zip32_xsk_master(
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
pub extern "C" fn librustzcash_zip32_xsk_derive(
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

/// Derive a child ExtendedFullViewingKey from a parent.
#[no_mangle]
pub extern "C" fn librustzcash_zip32_xfvk_derive(
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

/// Derive a PaymentAddress from an ExtendedFullViewingKey.
#[no_mangle]
pub extern "C" fn librustzcash_zip32_xfvk_address(
    xfvk: *const [c_uchar; 169],
    j: *const [c_uchar; 11],
    j_ret: *mut [c_uchar; 11],
    addr_ret: *mut [c_uchar; 43],
) -> bool {
    let xfvk = zip32::ExtendedFullViewingKey::read(&unsafe { *xfvk }[..])
        .expect("valid ExtendedFullViewingKey");
    let j = zip32::DiversifierIndex(unsafe { *j });

    let addr = match xfvk.address(j) {
        Ok(addr) => addr,
        Err(_) => return false,
    };

    let j_ret = unsafe { &mut *j_ret };
    let addr_ret = unsafe { &mut *addr_ret };

    j_ret.copy_from_slice(&(addr.0).0);
    addr_ret.copy_from_slice(&addr.1.to_bytes());

    true
}

fn construct_mmr_tree(
    // Consensus branch id
    cbranch: u32,
    // Length of tree in array representation
    t_len: u32,

    // Indices of provided tree nodes, length of p_len+e_len
    ni_ptr: *const u32,
    // Provided tree nodes data, length of p_len+e_len
    n_ptr: *const [c_uchar; zcash_history::MAX_ENTRY_SIZE],

    // Peaks count
    p_len: size_t,
    // Extra nodes loaded (for deletion) count
    e_len: size_t,
) -> Result<MMRTree, &'static str> {
    let (indices, nodes) = unsafe {
        (
            slice::from_raw_parts(ni_ptr, p_len + e_len),
            slice::from_raw_parts(n_ptr, p_len + e_len),
        )
    };

    let mut peaks: Vec<_> = indices
        .iter()
        .zip(nodes.iter())
        .map(
            |(index, node)| match MMREntry::from_bytes(cbranch, &node[..]) {
                Ok(entry) => Ok((*index, entry)),
                Err(_) => Err("Invalid encoding"),
            },
        )
        .collect::<Result<_, _>>()?;
    let extra = peaks.split_off(p_len);

    Ok(MMRTree::new(t_len, peaks, extra))
}

#[no_mangle]
pub extern "system" fn librustzcash_mmr_append(
    // Consensus branch id
    cbranch: u32,
    // Length of tree in array representation
    t_len: u32,
    // Indices of provided tree nodes, length of p_len
    ni_ptr: *const u32,
    // Provided tree nodes data, length of p_len
    n_ptr: *const [c_uchar; zcash_history::MAX_ENTRY_SIZE],
    // Peaks count
    p_len: size_t,
    // New node pointer
    nn_ptr: *const [u8; zcash_history::MAX_NODE_DATA_SIZE],
    // Return of root commitment
    rt_ret: *mut [u8; 32],
    // Return buffer for appended leaves, should be pre-allocated of ceiling(log2(t_len)) length
    buf_ret: *mut [c_uchar; zcash_history::MAX_NODE_DATA_SIZE],
) -> u32 {
    let new_node_bytes: &[u8; zcash_history::MAX_NODE_DATA_SIZE] = unsafe {
        match nn_ptr.as_ref() {
            Some(r) => r,
            None => {
                return 0;
            } // Null pointer passed, error
        }
    };

    let mut tree = match construct_mmr_tree(cbranch, t_len, ni_ptr, n_ptr, p_len, 0) {
        Ok(t) => t,
        _ => {
            return 0;
        } // error
    };

    let node = match MMRNodeData::from_bytes(cbranch, &new_node_bytes[..]) {
        Ok(node) => node,
        _ => {
            return 0;
        } // error
    };

    let appended = match tree.append_leaf(node) {
        Ok(appended) => appended,
        _ => {
            return 0;
        }
    };

    let return_count = appended.len();

    let root_node = tree
        .root_node()
        .expect("Just added, should resolve always; qed");
    unsafe {
        *rt_ret = root_node.data().hash();

        for (idx, next_buf) in slice::from_raw_parts_mut(buf_ret, return_count as usize)
            .iter_mut()
            .enumerate()
        {
            tree.resolve_link(appended[idx])
                .expect("This was generated by the tree and thus resolvable; qed")
                .data()
                .write(&mut &mut next_buf[..])
                .expect("Write using cursor with enough buffer size cannot fail; qed");
        }
    }

    return_count as u32
}

#[no_mangle]
pub extern "system" fn librustzcash_mmr_delete(
    // Consensus branch id
    cbranch: u32,
    // Length of tree in array representation
    t_len: u32,
    // Indices of provided tree nodes, length of p_len+e_len
    ni_ptr: *const u32,
    // Provided tree nodes data, length of p_len+e_len
    n_ptr: *const [c_uchar; zcash_history::MAX_ENTRY_SIZE],
    // Peaks count
    p_len: size_t,
    // Extra nodes loaded (for deletion) count
    e_len: size_t,
    // Return of root commitment
    rt_ret: *mut [u8; 32],
) -> u32 {
    let mut tree = match construct_mmr_tree(cbranch, t_len, ni_ptr, n_ptr, p_len, e_len) {
        Ok(t) => t,
        _ => {
            return 0;
        } // error
    };

    let truncate_len = match tree.truncate_leaf() {
        Ok(v) => v,
        _ => {
            return 0;
        } // Error
    };

    unsafe {
        *rt_ret = tree
            .root_node()
            .expect("Just generated without errors, root should be resolving")
            .data()
            .hash();
    }

    truncate_len
}

#[no_mangle]
pub extern "system" fn librustzcash_mmr_hash_node(
    cbranch: u32,
    n_ptr: *const [u8; zcash_history::MAX_NODE_DATA_SIZE],
    h_ret: *mut [u8; 32],
) -> u32 {
    let node_bytes: &[u8; zcash_history::MAX_NODE_DATA_SIZE] = unsafe {
        match n_ptr.as_ref() {
            Some(r) => r,
            None => return 1,
        }
    };

    let node = match MMRNodeData::from_bytes(cbranch, &node_bytes[..]) {
        Ok(n) => n,
        _ => return 1, // error
    };

    unsafe {
        *h_ret = node.hash();
    }

    0
}

#[no_mangle]
pub extern "system" fn librustzcash_zebra_crypto_sign_verify_detached(
    sig: *const [u8; 64],
    m: *const u8,
    mlen: u64,
    pk: *const [u8; 32],
) -> isize {
    use ed25519_zebra::{Signature, VerificationKey};
    use std::convert::TryFrom;

    let sig = Signature::from(*unsafe {
        match sig.as_ref() {
            Some(sig) => sig,
            None => return 1,
        }
    });

    let pk = match VerificationKey::try_from(*match unsafe { pk.as_ref() } {
        Some(pk) => pk,
        None => return 1,
    }) {
        Ok(pk) => pk,
        Err(_) => return 1,
    };

    let m = unsafe { slice::from_raw_parts(m, mlen as usize) };

    if pk.verify(&sig, m).is_err() {
        1
    } else {
        0
    }
}
