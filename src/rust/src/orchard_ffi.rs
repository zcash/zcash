use std::convert::TryInto;

use rand_core::OsRng;
use tracing::{debug, error};

use crate::{
    bundlecache::{orchard_bundle_validity_cache, orchard_bundle_validity_cache_mut, CacheEntries},
    orchard_bundle::Bundle,
};

struct BatchValidatorInner {
    validator: orchard::bundle::BatchValidator,
    queued_entries: CacheEntries,
    // The verifying key against which this batch's proofs are checked. NU6.2 changed the
    // Orchard circuit (and thus the verifying key), so a batch is typed to a specific circuit
    // at construction: the NU6.2-onward (fixed) key from NU6.2, or the pre-NU6.2 (insecure)
    // key before it. A batch never mixes epochs (one batch per block, and a fresh batch per
    // transaction during mempool acceptance), so a single key per batch is correct. The key is
    // fixed at construction rather than passed to `validate`, because some validation work is
    // performed in `add_bundle`.
    vk: &'static orchard::circuit::VerifyingKey,
}

pub(crate) struct BatchValidator(Option<BatchValidatorInner>);

/// Creates an Orchard bundle batch validation context for the circuit in force at the height
/// of the block being validated: the NU6.2 fixed circuit if `nu6_2_active`, else the pre-NU6.2
/// circuit.
pub(crate) fn orchard_batch_validation_init(
    cache_store: bool,
    nu6_2_active: bool,
) -> Box<BatchValidator> {
    let vk: &'static orchard::circuit::VerifyingKey = if nu6_2_active {
        &crate::ORCHARD_VK_FIXED
    } else {
        &crate::ORCHARD_VK_INSECURE
    };
    Box::new(BatchValidator(Some(BatchValidatorInner {
        validator: orchard::bundle::BatchValidator::new(),
        queued_entries: CacheEntries::new(cache_store),
        vk,
    })))
}

impl BatchValidator {
    /// Adds an Orchard bundle to this batch.
    pub(crate) fn add_bundle(&mut self, bundle: Box<Bundle>, sighash: [u8; 32]) {
        let batch = self.0.as_mut();
        let bundle = bundle.inner();

        match (batch, bundle) {
            (Some(batch), Some(bundle)) => {
                let cache = orchard_bundle_validity_cache();

                // Compute the cache entry for this bundle.
                let cache_entry = {
                    let bundle_commitment = bundle.commitment();
                    let bundle_authorizing_commitment = bundle.authorizing_commitment();
                    cache.compute_entry(
                        bundle_commitment.0.as_bytes().try_into().unwrap(),
                        bundle_authorizing_commitment
                            .0
                            .as_bytes()
                            .try_into()
                            .unwrap(),
                        &sighash,
                    )
                };

                // Check if this bundle's validation result exists in the cache.
                if !cache.contains(cache_entry, &mut batch.queued_entries) {
                    // The bundle has been added to `inner.queued_entries` because it was not
                    // in the cache. We now add its authorization to the validation batch.
                    batch.validator.add_bundle(bundle, sighash);
                }
            }
            (Some(_), None) => debug!("Tx has no Orchard component"),
            (None, _) => error!("orchard::BatchValidator has already been used"),
        }
    }

    /// Validates this batch.
    ///
    /// - Returns `true` if `batch` is null.
    /// - Returns `false` if any item in the batch is invalid.
    ///
    /// The batch validation context is freed by this function.
    ///
    /// ## Consensus rules
    ///
    /// [§4.6](https://zips.z.cash/protocol/protocol.pdf#actiondesc):
    /// - Canonical element encodings are enforced by [`orchard_bundle_parse`].
    /// - SpendAuthSig^Orchard validity is enforced here.
    /// - Proof validity is enforced here.
    ///
    /// [§7.1](https://zips.z.cash/protocol/protocol.pdf#txnencodingandconsensus):
    /// - `bindingSigOrchard` validity is enforced here.
    pub(crate) fn validate(&mut self) -> bool {
        if let Some(inner) = self.0.take() {
            // The verifying key for this batch's circuit was fixed at construction
            // (`orchard_batch_validation_init`).
            if inner.validator.validate(inner.vk, OsRng) {
                // `BatchValidator::validate()` is only called if every
                // `BatchValidator::check_bundle()` returned `true`, so at this point
                // every bundle that was added to `inner.queued_entries` has valid
                // authorization.
                orchard_bundle_validity_cache_mut().insert(inner.queued_entries);
                true
            } else {
                false
            }
        } else {
            error!("orchard::BatchValidator has already been used");
            false
        }
    }
}
