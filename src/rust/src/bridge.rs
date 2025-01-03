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
    builder_ffi::shielded_signature_digest,
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
        apply_sapling_bundle_signatures, build_sapling_bundle, finish_bundle_assembly,
        init_batch_validator as init_sapling_batch_validator, init_verifier, new_bundle_assembler,
        new_sapling_builder, none_sapling_bundle, parse_v4_sapling_components,
        parse_v4_sapling_output, parse_v4_sapling_spend, parse_v5_sapling_bundle,
        BatchValidator as SaplingBatchValidator, Bundle as SaplingBundle,
        BundleAssembler as SaplingBundleAssembler, Output, SaplingBuilder,
        SaplingUnauthorizedBundle, Spend, Verifier,
    },
    streams::{
        from_auto_file, from_blake2b_writer, from_buffered_file, from_data, from_hash_writer,
        from_size_computer, CppStream,
    },
    test_harness_ffi::{
        test_only_invalid_sapling_bundle, test_only_replace_sapling_nullifier,
        test_only_replace_sapling_output_parts,
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
        type CBLAKE2bWriter = crate::streams::ffi::CBLAKE2bWriter;
        type CSizeComputer = crate::streams::ffi::CSizeComputer;
    }
    #[namespace = "stream"]
    extern "Rust" {
        type CppStream<'a>;

        fn from_data(stream: Pin<&mut RustStream>) -> Box<CppStream<'_>>;
        fn from_auto_file(file: Pin<&mut CAutoFile>) -> Box<CppStream<'_>>;
        fn from_buffered_file(file: Pin<&mut CBufferedFile>) -> Box<CppStream<'_>>;
        fn from_hash_writer(writer: Pin<&mut CHashWriter>) -> Box<CppStream<'_>>;
        fn from_blake2b_writer(writer: Pin<&mut CBLAKE2bWriter>) -> Box<CppStream<'_>>;
        fn from_size_computer(sc: Pin<&mut CSizeComputer>) -> Box<CppStream<'_>>;
    }

    #[namespace = "consensus"]
    extern "Rust" {
        type Network;

        #[allow(clippy::too_many_arguments)]
        fn network(
            network: &str,
            overwinter: i32,
            sapling: i32,
            blossom: i32,
            heartwood: i32,
            canopy: i32,
            nu5: i32,
            nu6: i32,
            z_future: i32,
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
        type Spend;

        #[cxx_name = "parse_v4_spend"]
        fn parse_v4_sapling_spend(bytes: &[u8]) -> Result<Box<Spend>>;
        fn cv(self: &Spend) -> [u8; 32];
        fn anchor(self: &Spend) -> [u8; 32];
        fn nullifier(self: &Spend) -> [u8; 32];
        fn rk(self: &Spend) -> [u8; 32];
        fn zkproof(self: &Spend) -> [u8; 192];
        fn spend_auth_sig(self: &Spend) -> [u8; 64];

        type Output;

        #[cxx_name = "parse_v4_output"]
        fn parse_v4_sapling_output(bytes: &[u8]) -> Result<Box<Output>>;
        fn cv(self: &Output) -> [u8; 32];
        fn cmu(self: &Output) -> [u8; 32];
        fn ephemeral_key(self: &Output) -> [u8; 32];
        fn enc_ciphertext(self: &Output) -> [u8; 580];
        fn out_ciphertext(self: &Output) -> [u8; 80];
        fn zkproof(self: &Output) -> [u8; 192];
        fn serialize_v4(self: &Output, stream: &mut CppStream<'_>) -> Result<()>;

        #[cxx_name = "Bundle"]
        type SaplingBundle;

        #[cxx_name = "none_bundle"]
        fn none_sapling_bundle() -> Box<SaplingBundle>;
        fn box_clone(self: &SaplingBundle) -> Box<SaplingBundle>;
        #[cxx_name = "parse_v5_bundle"]
        fn parse_v5_sapling_bundle(stream: &mut CppStream<'_>) -> Result<Box<SaplingBundle>>;
        fn serialize_v4_components(
            self: &SaplingBundle,
            stream: &mut CppStream<'_>,
            has_sapling: bool,
        ) -> Result<()>;
        fn serialize_v5(self: &SaplingBundle, stream: &mut CppStream<'_>) -> Result<()>;
        fn recursive_dynamic_usage(self: &SaplingBundle) -> usize;
        fn is_present(self: &SaplingBundle) -> bool;
        fn spends(self: &SaplingBundle) -> Vec<Spend>;
        fn outputs(self: &SaplingBundle) -> Vec<Output>;
        fn num_spends(self: &SaplingBundle) -> usize;
        fn num_outputs(self: &SaplingBundle) -> usize;
        fn value_balance_zat(self: &SaplingBundle) -> i64;
        fn binding_sig(self: &SaplingBundle) -> [u8; 64];

        #[cxx_name = "test_only_invalid_bundle"]
        fn test_only_invalid_sapling_bundle(
            spends: usize,
            outputs: usize,
            value_balance: i64,
        ) -> Box<SaplingBundle>;
        #[cxx_name = "test_only_replace_nullifier"]
        fn test_only_replace_sapling_nullifier(
            bundle: &mut SaplingBundle,
            spend_index: usize,
            nullifier: [u8; 32],
        );
        #[cxx_name = "test_only_replace_output_parts"]
        fn test_only_replace_sapling_output_parts(
            bundle: &mut SaplingBundle,
            output_index: usize,
            cmu: [u8; 32],
            enc_ciphertext: [u8; 580],
            out_ciphertext: [u8; 80],
        );

        #[rust_name = "SaplingBundleAssembler"]
        type BundleAssembler;

        fn new_bundle_assembler() -> Box<SaplingBundleAssembler>;
        #[cxx_name = "parse_v4_components"]
        fn parse_v4_sapling_components(
            stream: &mut CppStream<'_>,
            has_sapling: bool,
        ) -> Result<Box<SaplingBundleAssembler>>;
        fn have_actions(self: &SaplingBundleAssembler) -> bool;
        fn finish_bundle_assembly(
            assembler: Box<SaplingBundleAssembler>,
            binding_sig: [u8; 64],
        ) -> Box<SaplingBundle>;

        #[cxx_name = "Builder"]
        type SaplingBuilder;

        #[cxx_name = "new_builder"]
        fn new_sapling_builder(
            network: &Network,
            height: u32,
            anchor: [u8; 32],
            coinbase: bool,
        ) -> Result<Box<SaplingBuilder>>;
        fn add_spend(
            self: &mut SaplingBuilder,
            extsk: &[u8],
            recipient: [u8; 43],
            value: u64,
            rcm: [u8; 32],
            merkle_path: [u8; 1065],
        ) -> Result<()>;
        fn add_recipient(
            self: &mut SaplingBuilder,
            ovk: [u8; 32],
            to: [u8; 43],
            value: u64,
            memo: [u8; 512],
        ) -> Result<()>;
        #[cxx_name = "build_bundle"]
        fn build_sapling_bundle(
            builder: Box<SaplingBuilder>,
        ) -> Result<Box<SaplingUnauthorizedBundle>>;

        #[cxx_name = "UnauthorizedBundle"]
        type SaplingUnauthorizedBundle;

        #[cxx_name = "apply_bundle_signatures"]
        fn apply_sapling_bundle_signatures(
            bundle: Box<SaplingUnauthorizedBundle>,
            sighash_bytes: [u8; 32],
        ) -> Result<Box<SaplingBundle>>;

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
    struct OrchardAppendResult {
        has_subtree_boundary: bool,
        completed_subtree_root: [u8; 32],
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
        fn append_bundle(self: &mut Orchard, bundle: &Bundle) -> Result<OrchardAppendResult>;
        unsafe fn init_wallet(self: &Orchard, wallet: *mut OrchardWallet) -> bool;
    }

    unsafe extern "C++" {
        include!("rust/builder.h");
        type OrchardUnauthorizedBundlePtr;
    }
    #[namespace = "builder"]
    extern "Rust" {
        unsafe fn shielded_signature_digest(
            consensus_branch_id: u32,
            tx_bytes: &[u8],
            all_prev_outputs: &[u8],
            sapling_bundle: &SaplingUnauthorizedBundle,
            orchard_bundle: *const OrchardUnauthorizedBundlePtr,
        ) -> Result<[u8; 32]>;
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
