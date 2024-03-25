// Copyright (c) 2020-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

use std::convert::{TryFrom, TryInto};
use std::io;
use std::mem;

use bellman::groth16::Proof;
use bls12_381::Bls12;
use group::GroupEncoding;
use memuse::DynamicUsage;
use rand_core::{OsRng, RngCore};
use sapling::{
    builder::BundleType,
    circuit::{self, OutputParameters, SpendParameters},
    keys::{OutgoingViewingKey, SpendAuthorizingKey},
    note::ExtractedNoteCommitment,
    prover::{OutputProver, SpendProver},
    value::{NoteValue, ValueCommitTrapdoor, ValueCommitment},
    zip32::ExtendedSpendingKey,
    Anchor, Diversifier, MerklePath, Note, PaymentAddress, ProofGenerationKey, Rseed,
    SaplingVerificationContext,
};
use zcash_primitives::{
    consensus::sapling_zip212_enforcement,
    memo::MemoBytes,
    merkle_tree::merkle_path_from_slice,
    transaction::{
        components::{sapling as sapling_serialization, Amount},
        txid::{BlockTxCommitmentDigester, TxIdDigester},
        Authorized, Transaction, TransactionDigest,
    },
};

use super::GROTH_PROOF_SIZE;
use super::{
    de_ct, SAPLING_OUTPUT_PARAMS, SAPLING_OUTPUT_VK, SAPLING_SPEND_PARAMS, SAPLING_SPEND_VK,
};
use crate::params::Network;
use crate::{
    bundlecache::{sapling_bundle_validity_cache, sapling_bundle_validity_cache_mut, CacheEntries},
    streams::CppStream,
};

pub(crate) mod spec;
mod zip32;

const SAPLING_TREE_DEPTH: usize = 32;

pub(crate) struct Spend(sapling::bundle::SpendDescription<sapling::bundle::Authorized>);

pub(crate) fn parse_v4_sapling_spend(bytes: &[u8]) -> Result<Box<Spend>, String> {
    sapling_serialization::temporary_zcashd_read_spend_v4(&mut io::Cursor::new(bytes))
        .map(Spend)
        .map(Box::new)
        .map_err(|e| format!("{}", e))
}

impl Spend {
    pub(crate) fn cv(&self) -> [u8; 32] {
        self.0.cv().to_bytes()
    }

    pub(crate) fn anchor(&self) -> [u8; 32] {
        self.0.anchor().to_bytes()
    }

    pub(crate) fn nullifier(&self) -> [u8; 32] {
        self.0.nullifier().0
    }

    pub(crate) fn rk(&self) -> [u8; 32] {
        (*self.0.rk()).into()
    }

    pub(crate) fn zkproof(&self) -> [u8; 192] {
        *self.0.zkproof()
    }

    pub(crate) fn spend_auth_sig(&self) -> [u8; 64] {
        (*self.0.spend_auth_sig()).into()
    }
}

pub(crate) struct Output(sapling::bundle::OutputDescription<[u8; 192]>);

pub(crate) fn parse_v4_sapling_output(bytes: &[u8]) -> Result<Box<Output>, String> {
    sapling_serialization::temporary_zcashd_read_output_v4(&mut io::Cursor::new(bytes))
        .map(Output)
        .map(Box::new)
        .map_err(|e| format!("{}", e))
}

impl Output {
    pub(crate) fn cv(&self) -> [u8; 32] {
        self.0.cv().to_bytes()
    }

    pub(crate) fn cmu(&self) -> [u8; 32] {
        self.0.cmu().to_bytes()
    }

    pub(crate) fn ephemeral_key(&self) -> [u8; 32] {
        self.0.ephemeral_key().0
    }

    pub(crate) fn enc_ciphertext(&self) -> [u8; 580] {
        *self.0.enc_ciphertext()
    }

    pub(crate) fn out_ciphertext(&self) -> [u8; 80] {
        *self.0.out_ciphertext()
    }

    pub(crate) fn zkproof(&self) -> [u8; 192] {
        *self.0.zkproof()
    }

    pub(crate) fn serialize_v4(&self, writer: &mut CppStream<'_>) -> Result<(), String> {
        sapling_serialization::temporary_zcashd_write_output_v4(writer, &self.0)
            .map_err(|e| format!("Failed to write v4 Sapling Output Description: {}", e))
    }
}

#[derive(Clone)]
pub(crate) struct Bundle(pub(crate) Option<sapling::Bundle<sapling::bundle::Authorized, Amount>>);

pub(crate) fn none_sapling_bundle() -> Box<Bundle> {
    Box::new(Bundle(None))
}

/// Parses an authorized Sapling bundle from the given stream.
pub(crate) fn parse_v5_sapling_bundle(reader: &mut CppStream<'_>) -> Result<Box<Bundle>, String> {
    Bundle::parse_v5(reader)
}

impl Bundle {
    /// Returns a copy of the value.
    pub(crate) fn box_clone(&self) -> Box<Self> {
        Box::new(self.clone())
    }

    /// Parses an authorized Sapling bundle from the given stream.
    fn parse_v5(reader: &mut CppStream<'_>) -> Result<Box<Self>, String> {
        match Transaction::temporary_zcashd_read_v5_sapling(reader) {
            Ok(parsed) => Ok(Box::new(Bundle(parsed))),
            Err(e) => Err(format!("Failed to parse v5 Sapling bundle: {}", e)),
        }
    }

    pub(crate) fn serialize_v4_components(
        &self,
        writer: &mut CppStream<'_>,
        has_sapling: bool,
    ) -> Result<(), String> {
        sapling_serialization::temporary_zcashd_write_v4_components(
            writer,
            self.0.as_ref(),
            has_sapling,
        )
        .map_err(|e| format!("{}", e))
    }

    /// Serializes an authorized Sapling bundle to the given stream.
    ///
    /// If `bundle == None`, this serializes `nSpendsSapling = nOutputsSapling = 0`.
    pub(crate) fn serialize_v5(&self, writer: &mut CppStream<'_>) -> Result<(), String> {
        Transaction::temporary_zcashd_write_v5_sapling(self.inner(), writer)
            .map_err(|e| format!("Failed to serialize Sapling bundle: {}", e))
    }

    pub(crate) fn inner(&self) -> Option<&sapling::Bundle<sapling::bundle::Authorized, Amount>> {
        self.0.as_ref()
    }

    /// Returns the amount of dynamically-allocated memory used by this bundle.
    pub(crate) fn recursive_dynamic_usage(&self) -> usize {
        self.inner()
            // Bundles are boxed on the heap, so we count their own size as well as the size
            // of `Vec`s they allocate.
            .map(|bundle| mem::size_of_val(bundle) + bundle.dynamic_usage())
            // If the transaction has no Sapling component, nothing is allocated for it.
            .unwrap_or(0)
    }

    /// Returns whether the Sapling bundle is present.
    pub(crate) fn is_present(&self) -> bool {
        self.0.is_some()
    }

    pub(crate) fn spends(&self) -> Vec<Spend> {
        self.0
            .iter()
            .flat_map(|b| b.shielded_spends().iter())
            .cloned()
            .map(Spend)
            .collect()
    }

    pub(crate) fn outputs(&self) -> Vec<Output> {
        self.0
            .iter()
            .flat_map(|b| b.shielded_outputs().iter())
            .cloned()
            .map(Output)
            .collect()
    }

    pub(crate) fn num_spends(&self) -> usize {
        self.inner().map(|b| b.shielded_spends().len()).unwrap_or(0)
    }

    pub(crate) fn num_outputs(&self) -> usize {
        self.inner()
            .map(|b| b.shielded_outputs().len())
            .unwrap_or(0)
    }

    /// Returns the value balance for this Sapling bundle.
    ///
    /// A transaction with no Sapling component has a value balance of zero.
    pub(crate) fn value_balance_zat(&self) -> i64 {
        self.inner()
            .map(|b| b.value_balance().into())
            // From section 7.1 of the Zcash prototol spec:
            // If valueBalanceSapling is not present, then v^balanceSapling is defined to be 0.
            .unwrap_or(0)
    }

    /// Returns the binding signature for the bundle.
    ///
    /// # Panics
    ///
    /// Panics if the bundle is not present.
    pub(crate) fn binding_sig(&self) -> [u8; 64] {
        self.inner()
            .expect("Bundle actions should have been checked to be non-empty")
            .authorization()
            .binding_sig
            .into()
    }

    fn commitment<D: TransactionDigest<Authorized>>(&self, digester: D) -> D::SaplingDigest {
        digester.digest_sapling(self.inner())
    }
}

pub(crate) struct BundleAssembler {
    value_balance: Amount,
    shielded_spends: Vec<sapling::bundle::SpendDescription<sapling::bundle::Authorized>>,
    shielded_outputs: Vec<sapling::bundle::OutputDescription<[u8; 192]>>, // GROTH_PROOF_SIZE
}

pub(crate) fn new_bundle_assembler() -> Box<BundleAssembler> {
    Box::new(BundleAssembler {
        value_balance: Amount::zero(),
        shielded_spends: vec![],
        shielded_outputs: vec![],
    })
}

pub(crate) fn parse_v4_sapling_components(
    reader: &mut CppStream<'_>,
    has_sapling: bool,
) -> Result<Box<BundleAssembler>, String> {
    BundleAssembler::parse_v4_components(reader, has_sapling)
        .map_err(|e| format!("Failed to parse v4 Sapling bundle: {}", e))
}

impl BundleAssembler {
    pub(crate) fn parse_v4_components(
        reader: &mut CppStream<'_>,
        has_sapling: bool,
    ) -> io::Result<Box<Self>> {
        let (value_balance, shielded_spends, shielded_outputs) =
            sapling_serialization::temporary_zcashd_read_v4_components(reader, has_sapling)?;

        Ok(Box::new(Self {
            value_balance,
            shielded_spends,
            shielded_outputs,
        }))
    }

    pub(crate) fn have_actions(&self) -> bool {
        !(self.shielded_spends.is_empty() && self.shielded_outputs.is_empty())
    }
}

#[allow(clippy::boxed_local)]
pub(crate) fn finish_bundle_assembly(
    assembler: Box<BundleAssembler>,
    binding_sig: [u8; 64],
) -> Box<Bundle> {
    let binding_sig = redjubjub::Signature::from(binding_sig);

    Box::new(Bundle(sapling::Bundle::from_parts(
        assembler.shielded_spends,
        assembler.shielded_outputs,
        assembler.value_balance,
        sapling::bundle::Authorized { binding_sig },
    )))
}

pub(crate) struct StaticTxProver;

impl SpendProver for StaticTxProver {
    type Proof = Proof<Bls12>;

    fn prepare_circuit(
        proof_generation_key: ProofGenerationKey,
        diversifier: Diversifier,
        rseed: Rseed,
        value: NoteValue,
        alpha: jubjub::Fr,
        rcv: ValueCommitTrapdoor,
        anchor: bls12_381::Scalar,
        merkle_path: MerklePath,
    ) -> Option<circuit::Spend> {
        SpendParameters::prepare_circuit(
            proof_generation_key,
            diversifier,
            rseed,
            value,
            alpha,
            rcv,
            anchor,
            merkle_path,
        )
    }

    fn create_proof<R: RngCore>(&self, circuit: circuit::Spend, rng: &mut R) -> Self::Proof {
        unsafe { SAPLING_SPEND_PARAMS.as_ref() }
            .expect("Parameters not loaded: SAPLING_SPEND_PARAMS should have been initialized")
            .create_proof(circuit, rng)
    }

    fn encode_proof(proof: Self::Proof) -> sapling::bundle::GrothProofBytes {
        SpendParameters::encode_proof(proof)
    }
}

impl OutputProver for StaticTxProver {
    type Proof = Proof<Bls12>;

    fn prepare_circuit(
        esk: jubjub::Fr,
        payment_address: PaymentAddress,
        rcm: jubjub::Fr,
        value: NoteValue,
        rcv: ValueCommitTrapdoor,
    ) -> circuit::Output {
        OutputParameters::prepare_circuit(esk, payment_address, rcm, value, rcv)
    }

    fn create_proof<R: RngCore>(&self, circuit: circuit::Output, rng: &mut R) -> Self::Proof {
        unsafe { SAPLING_OUTPUT_PARAMS.as_ref() }
            .expect("Parameters not loaded: SAPLING_OUTPUT_PARAMS should have been initialized")
            .create_proof(circuit, rng)
    }

    fn encode_proof(proof: Self::Proof) -> sapling::bundle::GrothProofBytes {
        OutputParameters::encode_proof(proof)
    }
}

pub(crate) struct SaplingBuilder {
    builder: sapling::builder::Builder,
    signing_keys: Vec<SpendAuthorizingKey>,
}

pub(crate) fn new_sapling_builder(
    network: &Network,
    target_height: u32,
    anchor: [u8; 32],
    coinbase: bool,
) -> Result<Box<SaplingBuilder>, String> {
    let bundle_type = if coinbase {
        BundleType::Coinbase
    } else {
        BundleType::DEFAULT
    };

    let anchor = Option::from(Anchor::from_bytes(anchor))
        .ok_or_else(|| "Invalid Sapling anchor".to_owned())?;

    Ok(Box::new(SaplingBuilder {
        builder: sapling::builder::Builder::new(
            sapling_zip212_enforcement(network, target_height.into()),
            bundle_type,
            anchor,
        ),
        signing_keys: vec![],
    }))
}

#[allow(clippy::boxed_local)]
pub(crate) fn build_sapling_bundle(
    builder: Box<SaplingBuilder>,
) -> Result<Box<SaplingUnauthorizedBundle>, String> {
    builder.build().map(Box::new)
}

impl SaplingBuilder {
    pub(crate) fn add_spend(
        &mut self,
        extsk: &[u8],
        recipient: [u8; 43],
        value: u64,
        rcm: [u8; 32],
        merkle_path: [u8; 1 + 33 * SAPLING_TREE_DEPTH + 8],
    ) -> Result<(), String> {
        let extsk =
            ExtendedSpendingKey::from_bytes(extsk).map_err(|_| "Invalid ExtSK".to_owned())?;
        let recipient =
            PaymentAddress::from_bytes(&recipient).ok_or("Invalid recipient address")?;
        let value = NoteValue::from_raw(value);
        // If this is after ZIP 212, the caller has calculated rcm, and we don't need to call
        // Note::derive_esk, so we just pretend the note was using this rcm all along.
        let rseed = de_ct(jubjub::Scalar::from_bytes(&rcm))
            .map(Rseed::BeforeZip212)
            .ok_or("Invalid rcm")?;
        let note = Note::from_parts(recipient, value, rseed);
        let merkle_path = merkle_path_from_slice(&merkle_path)
            .map_err(|e| format!("Invalid Sapling Merkle path: {}", e))?;

        self.builder
            .add_spend(&extsk, note, merkle_path)
            .map_err(|e| format!("Failed to add Sapling spend: {}", e))?;
        self.signing_keys.push(extsk.expsk.ask);

        Ok(())
    }

    pub(crate) fn add_recipient(
        &mut self,
        ovk: [u8; 32],
        to: [u8; 43],
        value: u64,
        memo: [u8; 512],
    ) -> Result<(), String> {
        let ovk = Some(OutgoingViewingKey(ovk));
        let to = PaymentAddress::from_bytes(&to).ok_or("Invalid recipient address")?;
        let value = NoteValue::from_raw(value);
        let _ = MemoBytes::from_bytes(&memo).map_err(|e| format!("Invalid memo: {}", e))?;

        self.builder
            .add_output(ovk, to, value, Some(memo))
            .map_err(|e| format!("Failed to add Sapling recipient: {}", e))
    }

    fn build(self) -> Result<SaplingUnauthorizedBundle, String> {
        let Self {
            builder,
            signing_keys,
        } = self;
        let prover = crate::sapling::StaticTxProver;
        let rng = OsRng;
        let bundle = builder
            .build::<StaticTxProver, StaticTxProver, _, Amount>(rng)
            .map_err(|e| format!("Failed to build Sapling bundle: {}", e))?
            .map(|(bundle, _)| bundle.create_proofs(&prover, &prover, rng, ()));
        Ok(SaplingUnauthorizedBundle {
            bundle,
            signing_keys,
        })
    }
}

pub(crate) struct SaplingUnauthorizedBundle {
    pub(crate) bundle: Option<
        sapling::Bundle<
            sapling::builder::InProgress<sapling::builder::Proven, sapling::builder::Unsigned>,
            Amount,
        >,
    >,
    signing_keys: Vec<SpendAuthorizingKey>,
}

pub(crate) fn apply_sapling_bundle_signatures(
    bundle: Box<SaplingUnauthorizedBundle>,
    sighash_bytes: [u8; 32],
) -> Result<Box<crate::sapling::Bundle>, String> {
    bundle.apply_signatures(sighash_bytes).map(Box::new)
}

impl SaplingUnauthorizedBundle {
    fn apply_signatures(self, sighash_bytes: [u8; 32]) -> Result<crate::sapling::Bundle, String> {
        let SaplingUnauthorizedBundle {
            bundle,
            signing_keys,
        } = self;

        let authorized = if let Some(bundle) = bundle {
            let authorized = bundle
                .apply_signatures(OsRng, sighash_bytes, &signing_keys)
                .map_err(|e| format!("Failed to apply signatures to Sapling bundle: {}", e))?;
            Some(authorized)
        } else {
            None
        };

        Ok(crate::sapling::Bundle(authorized))
    }
}

pub(crate) struct Verifier(SaplingVerificationContext);

pub(crate) fn init_verifier() -> Box<Verifier> {
    Box::new(Verifier(SaplingVerificationContext::new()))
}

impl Verifier {
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn check_spend(
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
        let cv = match Option::from(ValueCommitment::from_bytes_not_small_order(cv)) {
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
        let rk = match redjubjub::VerificationKey::try_from(*rk) {
            Ok(p) => p,
            Err(_) => return false,
        };

        // Deserialize the signature
        let spend_auth_sig = redjubjub::Signature::from(*spend_auth_sig);

        // Deserialize the proof
        let zkproof = match Proof::read(&zkproof[..]) {
            Ok(p) => p,
            Err(_) => return false,
        };

        self.0.check_spend(
            &cv,
            anchor,
            nullifier,
            rk,
            sighash_value,
            spend_auth_sig,
            zkproof,
            &unsafe { SAPLING_SPEND_VK.as_ref() }
                .expect("Parameters not loaded: SAPLING_SPEND_VK should have been initialized")
                .prepare(),
        )
    }

    pub(crate) fn check_output(
        &mut self,
        cv: &[u8; 32],
        cm: &[u8; 32],
        ephemeral_key: &[u8; 32],
        zkproof: &[u8; GROTH_PROOF_SIZE],
    ) -> bool {
        // Deserialize the value commitment
        let cv = match Option::from(ValueCommitment::from_bytes_not_small_order(cv)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the extracted note commitment.
        let cmu = match Option::from(ExtractedNoteCommitment::from_bytes(cm)) {
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
            &cv,
            cmu,
            epk,
            zkproof,
            &unsafe { SAPLING_OUTPUT_VK.as_ref() }
                .expect("Parameters not loaded: SAPLING_OUTPUT_VK should have been initialized")
                .prepare(),
        )
    }

    pub(crate) fn final_check(
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
        let binding_sig = redjubjub::Signature::from(*binding_sig);

        self.0
            .final_check(value_balance, sighash_value, binding_sig)
    }
}

struct BatchValidatorInner {
    validator: sapling::BatchValidator,
    queued_entries: CacheEntries,
}

pub(crate) struct BatchValidator(Option<BatchValidatorInner>);

pub(crate) fn init_batch_validator(cache_store: bool) -> Box<BatchValidator> {
    Box::new(BatchValidator(Some(BatchValidatorInner {
        validator: sapling::BatchValidator::new(),
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
    pub(crate) fn check_bundle(&mut self, bundle: Box<Bundle>, sighash: [u8; 32]) -> bool {
        match (&mut self.0, bundle.inner()) {
            (Some(inner), Some(_)) => {
                let cache = sapling_bundle_validity_cache();

                // Compute the cache entry for this bundle.
                let cache_entry = {
                    let bundle_commitment = bundle.commitment(TxIdDigester).unwrap();
                    let bundle_authorizing_commitment =
                        bundle.commitment(BlockTxCommitmentDigester);
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
                    inner.validator.check_bundle(bundle.0.unwrap(), sighash)
                }
            }
            (Some(_), None) => {
                tracing::debug!("Tx has no Sapling component");
                true
            }
            (None, _) => {
                tracing::error!("sapling::BatchValidator has already been used");
                false
            }
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
    pub(crate) fn validate(&mut self) -> bool {
        if let Some(inner) = self.0.take() {
            if inner.validator.validate(
                unsafe { SAPLING_SPEND_VK.as_ref() }
                    .expect("Parameters not loaded: SAPLING_SPEND_VK should have been initialized"),
                unsafe { SAPLING_OUTPUT_VK.as_ref() }.expect(
                    "Parameters not loaded: SAPLING_OUTPUT_VK should have been initialized",
                ),
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
