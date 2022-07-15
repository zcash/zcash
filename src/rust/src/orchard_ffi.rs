use std::{convert::TryInto, mem, ptr};

use libc::size_t;
use memuse::DynamicUsage;
use orchard::{
    bundle::Authorized, keys::OutgoingViewingKey, note_encryption::OrchardDomain, Bundle,
};
use rand_core::OsRng;
use tracing::{debug, error};
use zcash_note_encryption::try_output_recovery_with_ovk;
use zcash_primitives::transaction::components::{orchard as orchard_serialization, Amount};

use crate::{
    bundlecache::{orchard_bundle_validity_cache, orchard_bundle_validity_cache_mut, CacheEntries},
    streams_ffi::{CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb},
};

#[no_mangle]
pub extern "C" fn orchard_bundle_clone(
    bundle: *const Bundle<Authorized, Amount>,
) -> *mut Bundle<Authorized, Amount> {
    unsafe { bundle.as_ref() }
        .map(|bundle| Box::into_raw(Box::new(bundle.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_bundle_free(bundle: *mut Bundle<Authorized, Amount>) {
    if !bundle.is_null() {
        drop(unsafe { Box::from_raw(bundle) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_bundle_recursive_dynamic_usage(
    bundle: *const Bundle<Authorized, Amount>,
) -> size_t {
    unsafe { bundle.as_ref() }
        // Bundles are boxed on the heap, so we count their own size as well as the size
        // of `Vec`s they allocate.
        .map(|bundle| mem::size_of_val(bundle) + bundle.dynamic_usage())
        // If the transaction has no Orchard component, nothing is allocated for it.
        .unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn orchard_bundle_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
    bundle_ret: *mut *mut Bundle<Authorized, Amount>,
) -> bool {
    let reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    match orchard_serialization::read_v5_bundle(reader) {
        Ok(parsed) => {
            unsafe {
                *bundle_ret = if let Some(bundle) = parsed {
                    Box::into_raw(Box::new(bundle))
                } else {
                    ptr::null_mut::<Bundle<Authorized, Amount>>()
                };
            };
            true
        }
        Err(e) => {
            error!("Failed to parse Orchard bundle: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_bundle_serialize(
    bundle: *const Bundle<Authorized, Amount>,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let bundle = unsafe { bundle.as_ref() };
    let writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());

    match orchard_serialization::write_v5_bundle(bundle, writer) {
        Ok(()) => true,
        Err(e) => {
            error!("{}", e);
            false
        }
    }
}

#[no_mangle]

pub extern "C" fn orchard_bundle_value_balance(bundle: *const Bundle<Authorized, Amount>) -> i64 {
    unsafe { bundle.as_ref() }
        .map(|bundle| (*bundle.value_balance()).into())
        // From section 7.1 of the Zcash prototol spec:
        // If valueBalanceOrchard is not present, then v^balanceOrchard is defined to be 0.
        .unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn orchard_bundle_actions_len(bundle: *const Bundle<Authorized, Amount>) -> usize {
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        bundle.actions().len()
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn orchard_bundle_nullifiers(
    bundle: *const Bundle<Authorized, Amount>,
    nullifiers_ret: *mut [u8; 32],
    nullifiers_len: usize,
) -> bool {
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        if nullifiers_len == bundle.actions().len() {
            let res = unsafe {
                assert!(!nullifiers_ret.is_null());
                std::slice::from_raw_parts_mut(nullifiers_ret, nullifiers_len)
            };

            for (action, nf_ret) in bundle.actions().iter().zip(res.iter_mut()) {
                *nf_ret = action.nullifier().to_bytes();
            }
            true
        } else {
            false
        }
    } else {
        nullifiers_len == 0
    }
}

#[no_mangle]
pub extern "C" fn orchard_bundle_anchor(
    bundle: *const Bundle<Authorized, Amount>,
    anchor_ret: *mut [u8; 32],
) -> bool {
    if let Some((bundle, ret)) = unsafe { bundle.as_ref() }.zip(unsafe { anchor_ret.as_mut() }) {
        ret.copy_from_slice(&bundle.anchor().to_bytes());

        true
    } else {
        false
    }
}

pub struct BatchValidator {
    validator: orchard::bundle::BatchValidator,
    queued_entries: CacheEntries,
}

/// Creates an Orchard bundle batch validation context.
///
/// Please free this when you're done.
#[no_mangle]
pub extern "C" fn orchard_batch_validation_init(cache_store: bool) -> *mut BatchValidator {
    let ctx = Box::new(BatchValidator {
        validator: orchard::bundle::BatchValidator::new(),
        queued_entries: CacheEntries::new(cache_store),
    });
    Box::into_raw(ctx)
}

/// Frees an Orchard bundle batch validation context returned from
/// [`orchard_batch_validation_init`].
#[no_mangle]
pub extern "C" fn orchard_batch_validation_free(ctx: *mut BatchValidator) {
    if !ctx.is_null() {
        drop(unsafe { Box::from_raw(ctx) });
    }
}

/// Adds an Orchard bundle to this batch.
#[no_mangle]
pub extern "C" fn orchard_batch_add_bundle(
    batch: *mut BatchValidator,
    bundle: *const Bundle<Authorized, Amount>,
    sighash: *const [u8; 32],
) {
    let batch = unsafe { batch.as_mut() };
    let bundle = unsafe { bundle.as_ref() };
    let sighash = unsafe { sighash.as_ref() };

    match (batch, bundle, sighash) {
        (Some(batch), Some(bundle), Some(sighash)) => {
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
                    sighash,
                )
            };

            // Check if this bundle's validation result exists in the cache.
            if !cache.contains(cache_entry, &mut batch.queued_entries) {
                // The bundle has been added to `inner.queued_entries` because it was not
                // in the cache. We now add its authorization to the validation batch.
                batch.validator.add_bundle(bundle, *sighash);
            }
        }
        (_, _, None) => error!("orchard_batch_add_bundle() called without sighash!"),
        (Some(_), None, Some(_)) => debug!("Tx has no Orchard component"),
        (None, Some(_), _) => debug!("Orchard BatchValidator not provided, assuming disabled."),
        (None, None, _) => (), // Boring, don't bother logging.
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
#[no_mangle]
pub extern "C" fn orchard_batch_validate(batch: *mut BatchValidator) -> bool {
    if !batch.is_null() {
        let batch = unsafe { Box::from_raw(batch) };
        let vk =
            unsafe { crate::ORCHARD_VK.as_ref() }.expect("ORCHARD_VK should have been initialized");
        if batch.validator.validate(vk, OsRng) {
            // `BatchValidator::validate()` is only called if every
            // `BatchValidator::check_bundle()` returned `true`, so at this point
            // every bundle that was added to `inner.queued_entries` has valid
            // authorization.
            orchard_bundle_validity_cache_mut().insert(batch.queued_entries);
            true
        } else {
            false
        }
    } else {
        // The orchard::BatchValidator C++ class uses null to represent a disabled batch
        // validator.
        debug!("Orchard BatchValidator not provided, assuming disabled.");
        true
    }
}

#[no_mangle]
pub extern "C" fn orchard_bundle_outputs_enabled(
    bundle: *const Bundle<Authorized, Amount>,
) -> bool {
    let bundle = unsafe { bundle.as_ref() };
    bundle.map(|b| b.flags().outputs_enabled()).unwrap_or(false)
}

#[no_mangle]
pub extern "C" fn orchard_bundle_spends_enabled(bundle: *const Bundle<Authorized, Amount>) -> bool {
    let bundle = unsafe { bundle.as_ref() };
    bundle.map(|b| b.flags().spends_enabled()).unwrap_or(false)
}

/// Returns whether all actions contained in the Orchard bundle
/// can be decrypted with the all-zeros OVK. Returns `true`
/// if no Orchard actions are present.
///
/// This should only be called on an Orchard bundle that is
/// an element of a coinbase transaction.
#[no_mangle]
pub extern "C" fn orchard_bundle_coinbase_outputs_are_valid(
    bundle: *const Bundle<Authorized, Amount>,
) -> bool {
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        for act in bundle.actions() {
            if try_output_recovery_with_ovk(
                &OrchardDomain::for_action(act),
                &OutgoingViewingKey::from([0u8; 32]),
                act,
                act.cv_net(),
                &act.encrypted_note().out_ciphertext,
            )
            .is_none()
            {
                return false;
            }
        }
    }

    // Either there are no Orchard actions, or all of the outputs
    // are decryptable with the all-zeros OVK.
    true
}
