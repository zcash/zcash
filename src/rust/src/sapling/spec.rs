use std::convert::TryInto;

use blake2s_simd::Params as Blake2sParams;
use group::{cofactor::CofactorGroup, GroupEncoding};
use incrementalmerkletree::Hashable;
use rand_core::{OsRng, RngCore};

use zcash_primitives::{
    constants::{CRH_IVK_PERSONALIZATION, PROOF_GENERATION_KEY_GENERATOR, SPENDING_KEY_GENERATOR},
    merkle_tree::HashSer,
    sapling::{
        merkle_hash,
        note::{ExtractedNoteCommitment, NoteCommitment},
        value::NoteValue,
        Diversifier, Node, Note, NullifierDerivingKey, PaymentAddress, Rseed,
    },
};

use crate::de_ct;

#[cxx::bridge]
mod ffi {
    #[namespace = "sapling::spec"]
    extern "Rust" {
        fn tree_uncommitted() -> [u8; 32];
        fn merkle_hash(depth: usize, lhs: &[u8; 32], rhs: &[u8; 32]) -> [u8; 32];
        fn to_scalar(input: &[u8; 64]) -> [u8; 32];
        fn ask_to_ak(ask: &[u8; 32]) -> [u8; 32];
        fn nsk_to_nk(nsk: &[u8; 32]) -> [u8; 32];
        fn crh_ivk(ak: &[u8; 32], nk: &[u8; 32]) -> [u8; 32];
        fn check_diversifier(diversifier: [u8; 11]) -> bool;
        fn ivk_to_pkd(ivk: &[u8; 32], diversifier: [u8; 11]) -> Result<[u8; 32]>;
        fn generate_r() -> [u8; 32];
        fn compute_nf(
            diversifier: &[u8; 11],
            pk_d: &[u8; 32],
            value: u64,
            rcm: &[u8; 32],
            nk: &[u8; 32],
            position: u64,
        ) -> Result<[u8; 32]>;
        fn compute_cmu(
            diversifier: [u8; 11],
            pk_d: &[u8; 32],
            value: u64,
            rcm: &[u8; 32],
        ) -> Result<[u8; 32]>;
    }
}

/// Reads an FsRepr from a [u8; 32]
/// and multiplies it by the given base.
fn fixed_scalar_mult(from: &[u8; 32], p_g: &jubjub::SubgroupPoint) -> jubjub::SubgroupPoint {
    // We only call this with `from` being a valid jubjub::Scalar.
    let f = jubjub::Scalar::from_bytes(from).unwrap();

    p_g * f
}

/// Writes the "uncommitted" note value for empty leaves of the Merkle tree.
///
/// `result` must be a valid pointer to 32 bytes which will be written.
fn tree_uncommitted() -> [u8; 32] {
    // Should be okay, caller is responsible for ensuring the pointer
    // is a valid pointer to 32 bytes that can be mutated.
    let mut result = [0; 32];
    Node::empty_leaf()
        .write(&mut result[..])
        .expect("Sapling leaves are 32 bytes");
    result
}

fn to_scalar(input: &[u8; 64]) -> [u8; 32] {
    jubjub::Scalar::from_bytes_wide(input).to_bytes()
}

pub(crate) fn ask_to_ak(ask: &[u8; 32]) -> [u8; 32] {
    let ak = fixed_scalar_mult(ask, &SPENDING_KEY_GENERATOR);
    ak.to_bytes()
}

pub(crate) fn nsk_to_nk(nsk: &[u8; 32]) -> [u8; 32] {
    let nk = fixed_scalar_mult(nsk, &PROOF_GENERATION_KEY_GENERATOR);
    nk.to_bytes()
}

pub(crate) fn crh_ivk(ak: &[u8; 32], nk: &[u8; 32]) -> [u8; 32] {
    let mut h = Blake2sParams::new()
        .hash_length(32)
        .personal(CRH_IVK_PERSONALIZATION)
        .to_state();
    h.update(ak);
    h.update(nk);
    let mut h: [u8; 32] = h.finalize().as_ref().try_into().unwrap();

    // Drop the last five bits, so it can be interpreted as a scalar.
    h[31] &= 0b0000_0111;

    h
}

pub(crate) fn check_diversifier(diversifier: [u8; 11]) -> bool {
    let diversifier = Diversifier(diversifier);
    diversifier.g_d().is_some()
}

pub(crate) fn ivk_to_pkd(ivk: &[u8; 32], diversifier: [u8; 11]) -> Result<[u8; 32], String> {
    let ivk = de_ct(jubjub::Scalar::from_bytes(ivk));
    let diversifier = Diversifier(diversifier);
    if let (Some(ivk), Some(g_d)) = (ivk, diversifier.g_d()) {
        let pk_d = g_d * ivk;

        Ok(pk_d.to_bytes())
    } else {
        Err("Diversifier is invalid".to_owned())
    }
}

/// Test generation of commitment randomness
#[test]
fn test_gen_r() {
    // Verify different r values are generated
    let r1 = generate_r();
    let r2 = generate_r();
    assert_ne!(r1, r2);

    // Verify r values are valid in the field
    let _ = jubjub::Scalar::from_bytes(&r1).unwrap();
    let _ = jubjub::Scalar::from_bytes(&r2).unwrap();
}

/// Generates a uniformly random scalar in Jubjub.
fn generate_r() -> [u8; 32] {
    // create random 64 byte buffer
    let mut rng = OsRng;
    let mut buffer = [0u8; 64];
    rng.fill_bytes(&mut buffer);

    // reduce to uniform value
    let r = jubjub::Scalar::from_bytes_wide(&buffer);
    r.to_bytes()
}

// Private utility function to get Note from C parameters
fn priv_get_note(
    diversifier: &[u8; 11],
    pk_d: &[u8; 32],
    value: u64,
    rcm: &[u8; 32],
) -> Result<Note, String> {
    let recipient_bytes = {
        let mut tmp = [0; 43];
        tmp[..11].copy_from_slice(&diversifier[..]);
        tmp[11..].copy_from_slice(&pk_d[..]);
        tmp
    };
    let recipient = PaymentAddress::from_bytes(&recipient_bytes)
        .ok_or_else(|| "Invalid recipient encoding".to_string())?;

    // Deserialize randomness
    // If this is after ZIP 212, the caller has calculated rcm, and we don't need to call
    // Note::derive_esk, so we just pretend the note was using this rcm all along.
    let rseed = Rseed::BeforeZip212(
        de_ct(jubjub::Scalar::from_bytes(rcm)).ok_or_else(|| "Invalid rcm encoding".to_string())?,
    );

    Ok(Note::from_parts(
        recipient,
        NoteValue::from_raw(value),
        rseed,
    ))
}

/// Computes a Sapling nullifier.
pub(crate) fn compute_nf(
    diversifier: &[u8; 11],
    pk_d: &[u8; 32],
    value: u64,
    rcm: &[u8; 32],
    nk: &[u8; 32],
    position: u64,
) -> Result<[u8; 32], String> {
    // ZIP 216: Nullifier derivation is not consensus-critical
    // (nullifiers are revealed, not calculated by consensus).
    // In any case, ZIP 216 is now enabled retroactively.
    let note = priv_get_note(diversifier, pk_d, value, rcm)?;

    let nk = match de_ct(jubjub::ExtendedPoint::from_bytes(nk)) {
        Some(p) => p,
        None => return Err("Invalid nk encoding".to_owned()),
    };

    let nk = match de_ct(nk.into_subgroup()) {
        Some(nk) => NullifierDerivingKey(nk),
        None => return Err("nk is not in the prime-order subgroup".to_owned()),
    };

    let nf = note.nf(&nk, position);
    Ok(nf.0)
}

/// Computes a Sapling commitment.
pub(crate) fn compute_cmu(
    diversifier: [u8; 11],
    pk_d: &[u8; 32],
    value: u64,
    rcm: &[u8; 32],
) -> Result<[u8; 32], String> {
    let diversifier = Diversifier(diversifier);
    let g_d = diversifier
        .g_d()
        .ok_or_else(|| "Invalid diversifier".to_string())?;

    let pk_d =
        de_ct(jubjub::ExtendedPoint::from_bytes(pk_d)).ok_or_else(|| "Invalid pk_d".to_string())?;
    let pk_d = de_ct(pk_d.into_subgroup())
        .ok_or_else(|| "pk_d is not in the prime-order subgroup".to_string())?;

    let rcm = de_ct(jubjub::Scalar::from_bytes(rcm)).ok_or_else(|| "Invalid rcm".to_string())?;

    let cmu = ExtractedNoteCommitment::from(NoteCommitment::temporary_zcashd_derive(
        g_d.to_bytes(),
        pk_d.to_bytes(),
        NoteValue::from_raw(value),
        rcm,
    ));

    Ok(cmu.to_bytes())
}
