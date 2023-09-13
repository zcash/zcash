use std::fs::File;
use std::io::BufReader;

use bellman::groth16::Parameters;
use zcash_proofs::sprout;

use crate::{GROTH_PROOF_SIZE, SPROUT_GROTH16_PARAMS_PATH, SPROUT_GROTH16_VK};

#[allow(clippy::too_many_arguments)]
#[cxx::bridge]
mod ffi {
    #[namespace = "sprout"]
    extern "Rust" {
        fn prove(
            phi: [u8; 32],
            rt: [u8; 32],
            h_sig: [u8; 32],
            in_sk1: [u8; 32],
            in_value1: u64,
            in_rho1: [u8; 32],
            in_r1: [u8; 32],
            in_auth1: &[u8; 966], // sprout::WITNESS_PATH_SIZE
            in_sk2: [u8; 32],
            in_value2: u64,
            in_rho2: [u8; 32],
            in_r2: [u8; 32],
            in_auth2: &[u8; 966], // sprout::WITNESS_PATH_SIZE
            out_pk1: [u8; 32],
            out_value1: u64,
            out_r1: [u8; 32],
            out_pk2: [u8; 32],
            out_value2: u64,
            out_r2: [u8; 32],
            vpub_old: u64,
            vpub_new: u64,
        ) -> [u8; 192]; // GROTH_PROOF_SIZE

        fn verify(
            proof: &[u8; 192], // GROTH_PROOF_SIZE
            rt: &[u8; 32],
            h_sig: &[u8; 32],
            mac1: &[u8; 32],
            mac2: &[u8; 32],
            nf1: &[u8; 32],
            nf2: &[u8; 32],
            cm1: &[u8; 32],
            cm2: &[u8; 32],
            vpub_old: u64,
            vpub_new: u64,
        ) -> bool;
    }
}

/// Sprout JoinSplit proof generation.
#[allow(clippy::too_many_arguments)]
fn prove(
    phi: [u8; 32],
    rt: [u8; 32],
    h_sig: [u8; 32],

    // First input
    in_sk1: [u8; 32],
    in_value1: u64,
    in_rho1: [u8; 32],
    in_r1: [u8; 32],
    in_auth1: &[u8; sprout::WITNESS_PATH_SIZE],

    // Second input
    in_sk2: [u8; 32],
    in_value2: u64,
    in_rho2: [u8; 32],
    in_r2: [u8; 32],
    in_auth2: &[u8; sprout::WITNESS_PATH_SIZE],

    // First output
    out_pk1: [u8; 32],
    out_value1: u64,
    out_r1: [u8; 32],

    // Second output
    out_pk2: [u8; 32],
    out_value2: u64,
    out_r2: [u8; 32],

    // Public value
    vpub_old: u64,
    vpub_new: u64,
) -> [u8; GROTH_PROOF_SIZE] {
    let params = {
        use std::io::Read;

        // Load parameters from disk
        let sprout_path = unsafe { &SPROUT_GROTH16_PARAMS_PATH }.as_ref().expect(
            "Parameters not loaded: SPROUT_GROTH16_PARAMS_PATH should have been initialized",
        );
        const HOW_TO_FIX: &str = "
Please download this file from https://download.z.cash/downloads/sprout-groth16.params
and put it at ";
        let sprout_fs = File::open(sprout_path).unwrap_or_else(|err| {
            panic!(
                "Couldn't load Sprout Groth16 parameters file: {}{}{}",
                err,
                HOW_TO_FIX,
                &sprout_path.to_string_lossy()
            )
        });

        let mut sprout_fs = BufReader::with_capacity(1024 * 1024, sprout_fs);

        let mut sprout_params_file = vec![];
        sprout_fs
            .read_to_end(&mut sprout_params_file)
            .unwrap_or_else(|err| {
                panic!(
                    "Couldn't read Sprout Groth16 parameters file: {}{}{}",
                    err,
                    HOW_TO_FIX,
                    &sprout_path.to_string_lossy()
                )
            });

        let hash = blake2b_simd::Params::new()
            .hash_length(64)
            .hash(&sprout_params_file);

        // b2sum sprout-groth16.params
        if hash.as_bytes() != hex::decode("e9b238411bd6c0ec4791e9d04245ec350c9c5744f5610dfcce4365d5ca49dfefd5054e371842b3f88fa1b9d7e8e075249b3ebabd167fa8b0f3161292d36c180a").unwrap().as_slice() {
            panic!("Hash of Sprout Groth16 parameters file is incorrect.{}{}", HOW_TO_FIX, &sprout_path.to_string_lossy());
        }

        Parameters::read(&sprout_params_file[..], false)
            .expect("Couldn't deserialize Sprout Groth16 parameters file")
    };

    let proof = sprout::create_proof(
        phi, rt, h_sig, in_sk1, in_value1, in_rho1, in_r1, in_auth1, in_sk2, in_value2, in_rho2,
        in_r2, in_auth2, out_pk1, out_value1, out_r1, out_pk2, out_value2, out_r2, vpub_old,
        vpub_new, &params,
    );

    let mut proof_out = [0; GROTH_PROOF_SIZE];
    proof
        .write(&mut proof_out[..])
        .expect("should be able to serialize a proof");
    proof_out
}

/// Sprout JoinSplit proof verification.
#[allow(clippy::too_many_arguments)]
fn verify(
    proof: &[u8; GROTH_PROOF_SIZE],
    rt: &[u8; 32],
    h_sig: &[u8; 32],
    mac1: &[u8; 32],
    mac2: &[u8; 32],
    nf1: &[u8; 32],
    nf2: &[u8; 32],
    cm1: &[u8; 32],
    cm2: &[u8; 32],
    vpub_old: u64,
    vpub_new: u64,
) -> bool {
    sprout::verify_proof(
        proof,
        rt,
        h_sig,
        mac1,
        mac2,
        nf1,
        nf2,
        cm1,
        cm2,
        vpub_old,
        vpub_new,
        unsafe { SPROUT_GROTH16_VK.as_ref() }
            .expect("Parameters not loaded: SPROUT_GROTH16_VK should have been initialized"),
    )
}
