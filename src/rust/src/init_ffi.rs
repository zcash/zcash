use std::ffi::OsString;
use std::path::PathBuf;
use std::sync::Once;

use bellman::groth16::Parameters;
use bls12_381::Bls12;
use tracing::info;

use crate::{
    ORCHARD_PK, ORCHARD_VK, SAPLING_OUTPUT_PARAMS, SAPLING_OUTPUT_VK, SAPLING_SPEND_PARAMS,
    SAPLING_SPEND_VK, SPROUT_GROTH16_PARAMS_PATH, SPROUT_GROTH16_VK,
};

#[cxx::bridge]
mod ffi {
    #[namespace = "init"]
    extern "Rust" {
        fn rayon_threadpool();
        fn zksnark_params(sprout_path: String, load_proving_keys: bool);
    }
}

static PROOF_PARAMETERS_LOADED: Once = Once::new();

fn rayon_threadpool() {
    rayon::ThreadPoolBuilder::new()
        .thread_name(|i| format!("zc-rayon-{}", i))
        .build_global()
        .expect("Only initialized once");
}

/// Loads the zk-SNARK parameters into memory and saves paths as necessary.
/// Only called once.
///
/// If `load_proving_keys` is `false`, the proving keys will not be loaded, making it
/// impossible to create proofs. This flag is for the Boost test suite, which never
/// creates shielded transactions, but exercises code that requires the verifying keys to
/// be present even if there are no shielded components to verify.
fn zksnark_params(sprout_path: String, load_proving_keys: bool) {
    PROOF_PARAMETERS_LOADED.call_once(|| {
        let sprout_path = PathBuf::from(OsString::from(sprout_path));

        let sprout_vk = {
            use bellman::groth16::{prepare_verifying_key, VerifyingKey};
            let sprout_vk_bytes = include_bytes!("sprout-groth16.vk");
            let vk = VerifyingKey::<Bls12>::read(&sprout_vk_bytes[..])
                .expect("should be able to parse Sprout verification key");
            prepare_verifying_key(&vk)
        };

        // Load params
        let (sapling_spend_params, sapling_output_params) = {
            let (spend_buf, output_buf) = wagyu_zcash_parameters::load_sapling_parameters();
            let spend_params = Parameters::<Bls12>::read(&spend_buf[..], false)
                .expect("couldn't deserialize Sapling spend parameters");
            let output_params = Parameters::<Bls12>::read(&output_buf[..], false)
                .expect("couldn't deserialize Sapling spend parameters");
            (spend_params, output_params)
        };

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
            SPROUT_GROTH16_PARAMS_PATH = Some(sprout_path);

            SAPLING_SPEND_VK = Some(sapling_spend_vk);
            SAPLING_OUTPUT_VK = Some(sapling_output_vk);
            SPROUT_GROTH16_VK = Some(sprout_vk);

            ORCHARD_PK = orchard_pk;
            ORCHARD_VK = Some(orchard_vk);
        }
    });
}
