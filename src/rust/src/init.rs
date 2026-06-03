use std::ffi::OsString;
use std::path::PathBuf;
use std::sync::LazyLock;

use bls12_381::Bls12;
use sapling::circuit::{OutputParameters, SpendParameters};
use tracing::info;

use crate::{
    ORCHARD_PK, ORCHARD_VK_FIXED, ORCHARD_VK_INSECURE, SAPLING_OUTPUT_PARAMS, SAPLING_OUTPUT_VK,
    SAPLING_SPEND_PARAMS, SAPLING_SPEND_VK, SPROUT_GROTH16_PARAMS_PATH, SPROUT_GROTH16_VK,
};

#[cxx::bridge]
mod ffi {
    #[namespace = "init"]
    extern "Rust" {
        fn rayon_threadpool();
        fn zksnark_params(sprout_path: String, load_proving_keys: bool);
    }
}

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
    SPROUT_GROTH16_PARAMS_PATH.get_or_init(|| PathBuf::from(OsString::from(sprout_path)));

    SPROUT_GROTH16_VK.get_or_init(|| {
        use bellman::groth16::{prepare_verifying_key, VerifyingKey};
        let sprout_vk_bytes = include_bytes!("sprout-groth16.vk");
        let vk = VerifyingKey::<Bls12>::read(&sprout_vk_bytes[..])
            .expect("should be able to parse Sprout verification key");
        prepare_verifying_key(&vk)
    });

    // Load params
    let (sapling_spend_params, sapling_output_params) = {
        let (spend_buf, output_buf) = wagyu_zcash_parameters::load_sapling_parameters();
        (
            SpendParameters::read(&spend_buf[..], false)
                .expect("Failed to read Sapling Spend parameters"),
            OutputParameters::read(&output_buf[..], false)
                .expect("Failed to read Sapling Output parameters"),
        )
    };

    // We need to clone these because we aren't necessarily storing the proving
    // parameters in memory.
    SAPLING_SPEND_VK.get_or_init(|| sapling_spend_params.verifying_key());
    SAPLING_OUTPUT_VK.get_or_init(|| sapling_output_params.verifying_key());

    if load_proving_keys {
        SAPLING_SPEND_PARAMS.get_or_init(|| sapling_spend_params);
        SAPLING_OUTPUT_PARAMS.get_or_init(|| sapling_output_params);
    }

    // Generate Orchard parameters.
    info!(target: "main", "Loading Orchard parameters");
    if load_proving_keys {
        ORCHARD_PK.get_or_init(orchard::circuit::ProvingKey::build);
    }
    LazyLock::force(&ORCHARD_VK_INSECURE);
    LazyLock::force(&ORCHARD_VK_FIXED);
}
