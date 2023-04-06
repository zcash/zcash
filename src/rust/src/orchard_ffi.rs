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
}

pub(crate) struct BatchValidator(Option<BatchValidatorInner>);

/// Creates an Orchard bundle batch validation context.
pub(crate) fn orchard_batch_validation_init(cache_store: bool) -> Box<BatchValidator> {
    Box::new(BatchValidator(Some(BatchValidatorInner {
        validator: orchard::bundle::BatchValidator::new(),
        queued_entries: CacheEntries::new(cache_store),
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
            let vk = unsafe { crate::ORCHARD_VK.as_ref() }
                .expect("Parameters not loaded: ORCHARD_VK should have been initialized");
            if inner.validator.validate(vk, OsRng) {
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
