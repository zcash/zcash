// Copyright (c) 2020-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

use std::convert::TryInto;

use bellman::groth16::{prepare_verifying_key, Proof};
use group::GroupEncoding;

use rand_core::OsRng;
use zcash_note_encryption::EphemeralKeyBytes;
use zcash_primitives::{
    sapling::{
        redjubjub::{self, Signature},
        Nullifier,
    },
    transaction::{
        components::{sapling, Amount},
        txid::{BlockTxCommitmentDigester, TxIdDigester},
        Authorized, TransactionDigest,
    },
};
use zcash_proofs::sapling::{self as sapling_proofs, SaplingVerificationContext};

use super::GROTH_PROOF_SIZE;
use super::{de_ct, SAPLING_OUTPUT_VK, SAPLING_SPEND_VK};
use crate::bundlecache::{
    sapling_bundle_validity_cache, sapling_bundle_validity_cache_mut, CacheEntries,
};

#[cxx::bridge(namespace = "sapling")]
mod ffi {
    extern "Rust" {
        type Bundle;
        type BundleAssembler;
        fn new_bundle_assembler() -> Box<BundleAssembler>;
        fn add_spend(
            self: &mut BundleAssembler,
            cv: &[u8; 32],
            anchor: &[u8; 32],
            nullifier: [u8; 32],
            rk: &[u8; 32],
            zkproof: [u8; 192], // GROTH_PROOF_SIZE
            spend_auth_sig: &[u8; 64],
        ) -> bool;
        fn add_output(
            self: &mut BundleAssembler,
            cv: &[u8; 32],
            cmu: &[u8; 32],
            ephemeral_key: [u8; 32],
            enc_ciphertext: [u8; 580],
            out_ciphertext: [u8; 80],
            zkproof: [u8; 192], // GROTH_PROOF_SIZE
        ) -> bool;
        fn finish_bundle_assembly(
            assembler: Box<BundleAssembler>,
            value_balance: i64,
            binding_sig: [u8; 64],
        ) -> Box<Bundle>;

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

        type BatchValidator;
        fn init_batch_validator(cache_store: bool) -> Box<BatchValidator>;
        fn check_bundle(self: &mut BatchValidator, bundle: Box<Bundle>, sighash: [u8; 32]) -> bool;
        fn validate(self: &mut BatchValidator) -> bool;
    }
}

struct Bundle(sapling::Bundle<sapling::Authorized>);

impl Bundle {
    fn commitment<D: TransactionDigest<Authorized>>(&self, digester: D) -> D::SaplingDigest {
        digester.digest_sapling(Some(&self.0))
    }
}

struct BundleAssembler {
    shielded_spends: Vec<sapling::SpendDescription<sapling::Authorized>>,
    shielded_outputs: Vec<sapling::OutputDescription<[u8; 192]>>, // GROTH_PROOF_SIZE
}

fn new_bundle_assembler() -> Box<BundleAssembler> {
    Box::new(BundleAssembler {
        shielded_spends: vec![],
        shielded_outputs: vec![],
    })
}

impl BundleAssembler {
    fn add_spend(
        self: &mut BundleAssembler,
        cv: &[u8; 32],
        anchor: &[u8; 32],
        nullifier: [u8; 32],
        rk: &[u8; 32],
        zkproof: [u8; 192], // GROTH_PROOF_SIZE
        spend_auth_sig: &[u8; 64],
    ) -> bool {
        // Deserialize the value commitment
        let cv = match de_ct(jubjub::ExtendedPoint::from_bytes(cv)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the anchor, which should be an element
        // of Fr.
        let anchor = match de_ct(bls12_381::Scalar::from_bytes(anchor)) {
            Some(a) => a,
            None => return false,
        };

        // Deserialize rk
        let rk = match redjubjub::PublicKey::read(&rk[..]) {
            Ok(p) => p,
            Err(_) => return false,
        };

        // Deserialize the signature
        let spend_auth_sig = match Signature::read(&spend_auth_sig[..]) {
            Ok(sig) => sig,
            Err(_) => return false,
        };

        self.shielded_spends.push(sapling::SpendDescription {
            cv,
            anchor,
            nullifier: Nullifier(nullifier),
            rk,
            zkproof,
            spend_auth_sig,
        });

        true
    }

    fn add_output(
        self: &mut BundleAssembler,
        cv: &[u8; 32],
        cm: &[u8; 32],
        ephemeral_key: [u8; 32],
        enc_ciphertext: [u8; 580],
        out_ciphertext: [u8; 80],
        zkproof: [u8; 192], // GROTH_PROOF_SIZE
    ) -> bool {
        // Deserialize the value commitment
        let cv = match de_ct(jubjub::ExtendedPoint::from_bytes(cv)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the commitment, which should be an element
        // of Fr.
        let cmu = match de_ct(bls12_381::Scalar::from_bytes(cm)) {
            Some(a) => a,
            None => return false,
        };

        self.shielded_outputs.push(sapling::OutputDescription {
            cv,
            cmu,
            ephemeral_key: EphemeralKeyBytes(ephemeral_key),
            enc_ciphertext,
            out_ciphertext,
            zkproof,
        });

        true
    }
}

#[allow(clippy::boxed_local)]
fn finish_bundle_assembly(
    assembler: Box<BundleAssembler>,
    value_balance: i64,
    binding_sig: [u8; 64],
) -> Box<Bundle> {
    let value_balance = Amount::from_i64(value_balance).expect("parsed elsewhere");
    let binding_sig = redjubjub::Signature::read(&binding_sig[..]).expect("parsed elsewhere");

    Box::new(Bundle(sapling::Bundle {
        shielded_spends: assembler.shielded_spends,
        shielded_outputs: assembler.shielded_outputs,
        value_balance,
        authorization: sapling::Authorized { binding_sig },
    }))
}

struct Verifier(SaplingVerificationContext);

fn init_verifier() -> Box<Verifier> {
    // We consider ZIP 216 active all of the time because blocks prior to NU5
    // activation (on mainnet and testnet) did not contain Sapling transactions
    // that violated its canonicity rule.
    Box::new(Verifier(SaplingVerificationContext::new(true)))
}

impl Verifier {
    #[allow(clippy::too_many_arguments)]
    fn check_spend(
        &mut self,
        cv: &[u8; 32],
        anchor: &[u8; 32],
        nullifier: &[u8; 32],
        rk: &[u8; 32],
        zkproof: &[u8; GROTH_PROOF_SIZE],
        spend_auth_sig: &[u8; 64],
        sighash_value: &[u8; 32],
    ) -> bool {
        // Deserialize the value commitment
        let cv = match de_ct(jubjub::ExtendedPoint::from_bytes(cv)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the anchor, which should be an element
        // of Fr.
        let anchor = match de_ct(bls12_381::Scalar::from_bytes(anchor)) {
            Some(a) => a,
            None => return false,
        };

        // Deserialize rk
        let rk = match redjubjub::PublicKey::read(&rk[..]) {
            Ok(p) => p,
            Err(_) => return false,
        };

        // Deserialize the signature
        let spend_auth_sig = match Signature::read(&spend_auth_sig[..]) {
            Ok(sig) => sig,
            Err(_) => return false,
        };

        // Deserialize the proof
        let zkproof = match Proof::read(&zkproof[..]) {
            Ok(p) => p,
            Err(_) => return false,
        };

        self.0.check_spend(
            cv,
            anchor,
            nullifier,
            rk,
            sighash_value,
            spend_auth_sig,
            zkproof,
            &prepare_verifying_key(unsafe { SAPLING_SPEND_VK.as_ref() }.unwrap()),
        )
    }
    fn check_output(
        &mut self,
        cv: &[u8; 32],
        cm: &[u8; 32],
        ephemeral_key: &[u8; 32],
        zkproof: &[u8; GROTH_PROOF_SIZE],
    ) -> bool {
        // Deserialize the value commitment
        let cv = match de_ct(jubjub::ExtendedPoint::from_bytes(cv)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the commitment, which should be an element
        // of Fr.
        let cm = match de_ct(bls12_381::Scalar::from_bytes(cm)) {
            Some(a) => a,
            None => return false,
        };

        // Deserialize the ephemeral key
        let epk = match de_ct(jubjub::ExtendedPoint::from_bytes(ephemeral_key)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the proof
        let zkproof = match Proof::read(&zkproof[..]) {
            Ok(p) => p,
            Err(_) => return false,
        };

        self.0.check_output(
            cv,
            cm,
            epk,
            zkproof,
            &prepare_verifying_key(unsafe { SAPLING_OUTPUT_VK.as_ref() }.unwrap()),
        )
    }
    fn final_check(
        &self,
        value_balance: i64,
        binding_sig: &[u8; 64],
        sighash_value: &[u8; 32],
    ) -> bool {
        let value_balance = match Amount::from_i64(value_balance) {
            Ok(vb) => vb,
            Err(()) => return false,
        };

        // Deserialize the signature
        let binding_sig = match Signature::read(&binding_sig[..]) {
            Ok(sig) => sig,
            Err(_) => return false,
        };

        self.0
            .final_check(value_balance, sighash_value, binding_sig)
    }
}

struct BatchValidatorInner {
    validator: sapling_proofs::BatchValidator,
    queued_entries: CacheEntries,
}

struct BatchValidator(Option<BatchValidatorInner>);

fn init_batch_validator(cache_store: bool) -> Box<BatchValidator> {
    Box::new(BatchValidator(Some(BatchValidatorInner {
        validator: sapling_proofs::BatchValidator::new(),
        queued_entries: CacheEntries::new(cache_store),
    })))
}

impl BatchValidator {
    /// Checks the bundle against Sapling-specific consensus rules, and queues its
    /// authorization for validation.
    ///
    /// Returns `false` if the bundle doesn't satisfy the checked consensus rules. This
    /// `BatchValidator` can continue to be used regardless, but some or all of the proofs
    /// and signatures from this bundle may have already been added to the batch even if
    /// it fails other consensus rules.
    ///
    /// `sighash` must be for the transaction this bundle is within.
    ///
    /// If this batch was configured to not cache the results, then if the bundle was in
    /// the global bundle validity cache, it will have been removed (and this method will
    /// return `true`).
    #[allow(clippy::boxed_local)]
    fn check_bundle(&mut self, bundle: Box<Bundle>, sighash: [u8; 32]) -> bool {
        if let Some(inner) = &mut self.0 {
            let cache = sapling_bundle_validity_cache();

            // Compute the cache entry for this bundle.
            let cache_entry = {
                let bundle_commitment = bundle.commitment(TxIdDigester).unwrap();
                let bundle_authorizing_commitment = bundle.commitment(BlockTxCommitmentDigester);
                cache.compute_entry(
                    bundle_commitment.as_bytes().try_into().unwrap(),
                    bundle_authorizing_commitment.as_bytes().try_into().unwrap(),
                    &sighash,
                )
            };

            // Check if this bundle's validation result exists in the cache.
            if cache.contains(cache_entry, &mut inner.queued_entries) {
                true
            } else {
                // The bundle has been added to `inner.queued_entries` because it was not
                // in the cache. We now check the bundle against the Sapling-specific
                // consensus rules, and add its authorization to the validation batch.
                inner.validator.check_bundle(bundle.0, sighash)
            }
        } else {
            tracing::error!("sapling::BatchValidator has already been used");
            false
        }
    }

    /// Batch-validates the accumulated bundles.
    ///
    /// Returns `true` if every proof and signature in every bundle added to the batch
    /// validator is valid, or `false` if one or more are invalid. No attempt is made to
    /// figure out which of the accumulated bundles might be invalid; if that information
    /// is desired, construct separate [`BatchValidator`]s for sub-batches of the bundles.
    ///
    /// This method MUST NOT be called if any prior call to `Self::check_bundle` returned
    /// `false`.
    ///
    /// If this batch was configured to cache the results, then if this method returns
    /// `true` every bundle added to the batch will have also been added to the global
    /// bundle validity cache.
    fn validate(&mut self) -> bool {
        if let Some(inner) = self.0.take() {
            if inner.validator.validate(
                unsafe { SAPLING_SPEND_VK.as_ref() }.unwrap(),
                unsafe { SAPLING_OUTPUT_VK.as_ref() }.unwrap(),
                OsRng,
            ) {
                // `Self::validate()` is only called if every `Self::check_bundle()`
                // returned `true`, so at this point every bundle that was added to
                // `inner.queued_entries` has valid authorization and satisfies the
                // Sapling-specific consensus rules.
                sapling_bundle_validity_cache_mut().insert(inner.queued_entries);
                true
            } else {
                false
            }
        } else {
            tracing::error!("sapling::BatchValidator has already been used");
            false
        }
    }
}
