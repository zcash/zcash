/// FFI bridges between `zcashd`'s Rust and C++ code.
///
/// These are all collected into a single file because we can't use the same Rust type
/// across multiple bridges until https://github.com/dtolnay/cxx/issues/496 is closed.
///
/// The bridges that we leave separate are either standalone Rust code, or exporting C++
/// types to Rust (because we _can_ use the same C++ type across multiple bridges).
//
// This file can't use a module comment (`//! comment`) because it causes compilation issues in zcash_script.
use crate::{
    bundlecache::init as bundlecache_init,
    merkle_frontier::{new_orchard, orchard_empty_root, parse_orchard, Orchard, OrchardWallet},
    note_encryption::{
        try_sapling_note_decryption, try_sapling_output_recovery, DecryptedSaplingOutput,
    },
    orchard_bundle::{
        none_orchard_bundle, orchard_bundle_from_raw_box, parse_orchard_bundle, Action, Bundle,
    },
    orchard_ffi::{orchard_batch_validation_init, BatchValidator as OrchardBatchValidator},
    params::{network, Network},
    sapling::{
        finish_bundle_assembly, init_batch_validator as init_sapling_batch_validator, init_prover,
        init_verifier, new_bundle_assembler, BatchValidator as SaplingBatchValidator,
        Bundle as SaplingBundle, BundleAssembler as SaplingBundleAssembler, Prover, Verifier,
    },
    streams::{
        from_auto_file, from_buffered_file, from_data, from_hash_writer, from_size_computer,
        CppStream,
    },
    wallet_scanner::{init_batch_scanner, BatchResult, BatchScanner},
};

#[allow(clippy::needless_lifetimes)]
#[cxx::bridge]
pub(crate) mod ffi {
    extern "C++" {
        include!("hash.h");
        include!("streams.h");

        #[cxx_name = "RustDataStream"]
        type RustStream = crate::streams::ffi::RustStream;
        type CAutoFile = crate::streams::ffi::CAutoFile;
        type CBufferedFile = crate::streams::ffi::CBufferedFile;
        type CHashWriter = crate::streams::ffi::CHashWriter;
        type CSizeComputer = crate::streams::ffi::CSizeComputer;
    }
    #[namespace = "stream"]
    extern "Rust" {
        type CppStream<'a>;

        fn from_data(stream: Pin<&mut RustStream>) -> Box<CppStream<'_>>;
        fn from_auto_file(file: Pin<&mut CAutoFile>) -> Box<CppStream<'_>>;
        fn from_buffered_file(file: Pin<&mut CBufferedFile>) -> Box<CppStream<'_>>;
        fn from_hash_writer(writer: Pin<&mut CHashWriter>) -> Box<CppStream<'_>>;
        fn from_size_computer(sc: Pin<&mut CSizeComputer>) -> Box<CppStream<'_>>;
    }

    #[namespace = "consensus"]
    extern "Rust" {
        type Network;

        fn network(
            network: &str,
            overwinter: i32,
            sapling: i32,
            blossom: i32,
            heartwood: i32,
            canopy: i32,
            nu5: i32,
        ) -> Result<Box<Network>>;
    }

    #[namespace = "libzcash"]
    unsafe extern "C++" {
        include!("zcash/cache.h");

        type BundleValidityCache;

        fn NewBundleValidityCache(kind: &str, bytes: usize) -> UniquePtr<BundleValidityCache>;
        fn insert(self: Pin<&mut BundleValidityCache>, entry: [u8; 32]);
        fn contains(&self, entry: &[u8; 32], erase: bool) -> bool;
    }
    #[namespace = "bundlecache"]
    extern "Rust" {
        #[rust_name = "bundlecache_init"]
        fn init(cache_bytes: usize);
    }

    #[namespace = "sapling"]
    extern "Rust" {
        #[rust_name = "SaplingBundle"]
        type Bundle;

        #[rust_name = "SaplingBundleAssembler"]
        type BundleAssembler;

        fn new_bundle_assembler() -> Box<SaplingBundleAssembler>;
        fn add_spend(
            self: &mut SaplingBundleAssembler,
            cv: &[u8; 32],
            anchor: &[u8; 32],
            nullifier: [u8; 32],
            rk: &[u8; 32],
            zkproof: [u8; 192], // GROTH_PROOF_SIZE
            spend_auth_sig: &[u8; 64],
        ) -> bool;
        fn add_output(
            self: &mut SaplingBundleAssembler,
            cv: &[u8; 32],
            cmu: &[u8; 32],
            ephemeral_key: [u8; 32],
            enc_ciphertext: [u8; 580],
            out_ciphertext: [u8; 80],
            zkproof: [u8; 192], // GROTH_PROOF_SIZE
        ) -> bool;
        fn finish_bundle_assembly(
            assembler: Box<SaplingBundleAssembler>,
            value_balance: i64,
            binding_sig: [u8; 64],
        ) -> Box<SaplingBundle>;

        type Prover;

        fn init_prover() -> Box<Prover>;
        #[allow(clippy::too_many_arguments)]
        fn create_spend_proof(
            self: &mut Prover,
            ak: &[u8; 32],
            nsk: &[u8; 32],
            diversifier: &[u8; 11],
            rcm: &[u8; 32],
            ar: &[u8; 32],
            value: u64,
            anchor: &[u8; 32],
            merkle_path: &[u8; 1065], // 1 + 33 * SAPLING_TREE_DEPTH + 8
            cv: &mut [u8; 32],
            rk_out: &mut [u8; 32],
            zkproof: &mut [u8; 192], // GROTH_PROOF_SIZE
        ) -> bool;
        fn create_output_proof(
            self: &mut Prover,
            esk: &[u8; 32],
            payment_address: &[u8; 43],
            rcm: &[u8; 32],
            value: u64,
            cv: &mut [u8; 32],
            zkproof: &mut [u8; 192], // GROTH_PROOF_SIZE
        ) -> bool;
        fn binding_sig(
            self: &mut Prover,
            value_balance: i64,
            sighash: &[u8; 32],
            result: &mut [u8; 64],
        ) -> bool;

        type Verifier;

        fn init_verifier() -> Box<Verifier>;
        #[allow(clippy::too_many_arguments)]
        fn check_spend(
            self: &mut Verifier,
            cv: &[u8; 32],
            anchor: &[u8; 32],
            nullifier: &[u8; 32],
            rk: &[u8; 32],
            zkproof: &[u8; 192], // GROTH_PROOF_SIZE
            spend_auth_sig: &[u8; 64],
            sighash_value: &[u8; 32],
        ) -> bool;
        fn check_output(
            self: &mut Verifier,
            cv: &[u8; 32],
            cm: &[u8; 32],
            ephemeral_key: &[u8; 32],
            zkproof: &[u8; 192], // GROTH_PROOF_SIZE
        ) -> bool;
        fn final_check(
            self: &Verifier,
            value_balance: i64,
            binding_sig: &[u8; 64],
            sighash_value: &[u8; 32],
        ) -> bool;

        #[cxx_name = "BatchValidator"]
        type SaplingBatchValidator;
        #[cxx_name = "init_batch_validator"]
        fn init_sapling_batch_validator(cache_store: bool) -> Box<SaplingBatchValidator>;
        fn check_bundle(
            self: &mut SaplingBatchValidator,
            bundle: Box<SaplingBundle>,
            sighash: [u8; 32],
        ) -> bool;
        fn validate(self: &mut SaplingBatchValidator) -> bool;
    }

    unsafe extern "C++" {
        include!("rust/orchard.h");
        type OrchardBundlePtr;
    }
    #[namespace = "orchard_bundle"]
    extern "Rust" {
        type Action;
        type Bundle;

        fn cv(self: &Action) -> [u8; 32];
        fn nullifier(self: &Action) -> [u8; 32];
        fn rk(self: &Action) -> [u8; 32];
        fn cmx(self: &Action) -> [u8; 32];
        fn ephemeral_key(self: &Action) -> [u8; 32];
        fn enc_ciphertext(self: &Action) -> [u8; 580];
        fn out_ciphertext(self: &Action) -> [u8; 80];
        fn spend_auth_sig(self: &Action) -> [u8; 64];

        #[rust_name = "none_orchard_bundle"]
        fn none() -> Box<Bundle>;
        #[rust_name = "orchard_bundle_from_raw_box"]
        unsafe fn from_raw_box(bundle: *mut OrchardBundlePtr) -> Box<Bundle>;
        fn box_clone(self: &Bundle) -> Box<Bundle>;
        #[rust_name = "parse_orchard_bundle"]
        fn parse(stream: &mut CppStream<'_>) -> Result<Box<Bundle>>;
        fn serialize(self: &Bundle, stream: &mut CppStream<'_>) -> Result<()>;
        fn as_ptr(self: &Bundle) -> *const OrchardBundlePtr;
        fn recursive_dynamic_usage(self: &Bundle) -> usize;
        fn is_present(self: &Bundle) -> bool;
        fn actions(self: &Bundle) -> Vec<Action>;
        fn num_actions(self: &Bundle) -> usize;
        fn enable_spends(self: &Bundle) -> bool;
        fn enable_outputs(self: &Bundle) -> bool;
        fn value_balance_zat(self: &Bundle) -> i64;
        fn anchor(self: &Bundle) -> [u8; 32];
        fn proof(self: &Bundle) -> Vec<u8>;
        fn binding_sig(self: &Bundle) -> [u8; 64];
        fn coinbase_outputs_are_valid(self: &Bundle) -> bool;
    }

    #[namespace = "orchard"]
    extern "Rust" {
        #[cxx_name = "BatchValidator"]
        type OrchardBatchValidator;
        #[cxx_name = "init_batch_validator"]
        fn orchard_batch_validation_init(cache_store: bool) -> Box<OrchardBatchValidator>;
        fn add_bundle(self: &mut OrchardBatchValidator, bundle: Box<Bundle>, sighash: [u8; 32]);
        fn validate(self: &mut OrchardBatchValidator) -> bool;
    }

    #[namespace = "merkle_frontier"]
    extern "Rust" {
        type Orchard;
        type OrchardWallet;

        fn orchard_empty_root() -> [u8; 32];
        fn new_orchard() -> Box<Orchard>;
        fn box_clone(self: &Orchard) -> Box<Orchard>;
        fn parse_orchard(reader: &mut CppStream<'_>) -> Result<Box<Orchard>>;
        fn serialize(self: &Orchard, writer: &mut CppStream<'_>) -> Result<()>;
        fn serialize_legacy(self: &Orchard, writer: &mut CppStream<'_>) -> Result<()>;
        fn dynamic_memory_usage(self: &Orchard) -> usize;
        fn root(self: &Orchard) -> [u8; 32];
        fn size(self: &Orchard) -> u64;
        fn append_bundle(self: &mut Orchard, bundle: &Bundle) -> bool;
        unsafe fn init_wallet(self: &Orchard, wallet: *mut OrchardWallet) -> bool;
    }

    #[namespace = "wallet"]
    struct SaplingDecryptionResult {
        txid: [u8; 32],
        output: u32,
        ivk: [u8; 32],
        diversifier: [u8; 11],
        pk_d: [u8; 32],
    }

    #[namespace = "wallet"]
    pub(crate) struct SaplingShieldedOutput {
        cv: [u8; 32],
        cmu: [u8; 32],
        ephemeral_key: [u8; 32],
        enc_ciphertext: [u8; 580],
        out_ciphertext: [u8; 80],
    }

    #[namespace = "wallet"]
    extern "Rust" {
        fn try_sapling_note_decryption(
            network: &Network,
            height: u32,
            raw_ivk: &[u8; 32],
            output: SaplingShieldedOutput,
        ) -> Result<Box<DecryptedSaplingOutput>>;
        fn try_sapling_output_recovery(
            network: &Network,
            height: u32,
            ovk: [u8; 32],
            output: SaplingShieldedOutput,
        ) -> Result<Box<DecryptedSaplingOutput>>;

        type DecryptedSaplingOutput;
        fn note_value(self: &DecryptedSaplingOutput) -> u64;
        fn note_rseed(self: &DecryptedSaplingOutput) -> [u8; 32];
        fn zip_212_enabled(self: &DecryptedSaplingOutput) -> bool;
        fn recipient_d(self: &DecryptedSaplingOutput) -> [u8; 11];
        fn recipient_pk_d(self: &DecryptedSaplingOutput) -> [u8; 32];
        fn memo(self: &DecryptedSaplingOutput) -> [u8; 512];

        type BatchScanner;
        type BatchResult;

        fn init_batch_scanner(
            network: &Network,
            sapling_ivks: &[[u8; 32]],
        ) -> Result<Box<BatchScanner>>;
        fn add_transaction(
            self: &mut BatchScanner,
            block_tag: [u8; 32],
            tx_bytes: &[u8],
            height: u32,
        ) -> Result<()>;
        fn flush(self: &mut BatchScanner);
        fn collect_results(
            self: &mut BatchScanner,
            block_tag: [u8; 32],
            txid: [u8; 32],
        ) -> Box<BatchResult>;

        fn get_sapling(self: &BatchResult) -> Vec<SaplingDecryptionResult>;
    }
}
