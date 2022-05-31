use std::{mem, ptr};

use libc::size_t;
use memuse::DynamicUsage;
use orchard::{
    bundle::Authorized,
    keys::OutgoingViewingKey,
    note_encryption::OrchardDomain,
    primitives::redpallas::{self, Binding, SpendAuth},
    Bundle,
};
use rand_core::OsRng;
use tracing::{debug, error};
use zcash_note_encryption::try_output_recovery_with_ovk;
use zcash_primitives::transaction::{
    components::{orchard as orchard_serialization, Amount},
    TxId,
};

use crate::streams_ffi::{CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb};

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

/// Validates the given Orchard bundle against bundle-specific consensus rules.
///
/// If `bundle == nullptr`, this returns `true`.
///
/// ## Consensus rules
///
/// [§4.6](https://zips.z.cash/protocol/protocol.pdf#actiondesc):
/// - Canonical element encodings are enforced by [`orchard_bundle_parse`].
/// - SpendAuthSig^Orchard validity is enforced by [`BatchValidator`] via
///   [`orchard_batch_validate`].
/// - Proof validity is enforced here.
///
/// [§7.1](https://zips.z.cash/protocol/protocol.pdf#txnencodingandconsensus):
/// - `bindingSigOrchard` validity is enforced by [`BatchValidator`] via
///   [`orchard_batch_validate`].
#[no_mangle]
pub extern "C" fn orchard_bundle_validate(bundle: *const Bundle<Authorized, Amount>) -> bool {
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        let vk = unsafe { crate::ORCHARD_VK.as_ref() }.unwrap();

        if bundle.verify_proof(vk).is_err() {
            error!("Invalid Orchard proof");
            return false;
        }

        true
    } else {
        // The Orchard component of a transaction without an Orchard bundle is by
        // definition valid.
        true
    }
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

/// A signature within an authorized Orchard bundle.
#[derive(Debug)]
struct BundleSignature {
    /// The signature item for validation.
    signature: redpallas::batch::Item<SpendAuth, Binding>,
}

/// Batch validation context for Orchard.
pub struct BatchValidator {
    signatures: Vec<BundleSignature>,
}

impl BatchValidator {
    fn new() -> Self {
        BatchValidator { signatures: vec![] }
    }

    fn add_bundle(&mut self, bundle: &Bundle<Authorized, Amount>, txid: TxId) {
        for action in bundle.actions().iter() {
            self.signatures.push(BundleSignature {
                signature: action
                    .rk()
                    .create_batch_item(action.authorization().clone(), txid.as_ref()),
            });
        }

        self.signatures.push(BundleSignature {
            signature: bundle.binding_validating_key().create_batch_item(
                bundle.authorization().binding_signature().clone(),
                txid.as_ref(),
            ),
        });
    }

    fn validate(&self) -> bool {
        if self.signatures.is_empty() {
            // An empty batch is always valid, but is not free to run; skip it.
            return true;
        }

        let mut validator = redpallas::batch::Verifier::new();
        for sig in self.signatures.iter() {
            validator.queue(sig.signature.clone());
        }

        match validator.verify(OsRng) {
            Ok(()) => true,
            Err(e) => {
                error!("RedPallas batch validation failed: {}", e);
                // TODO: Try sub-batches to figure out which signatures are invalid. We can
                // postpone this for now:
                // - For per-transaction batching (when adding to the mempool), we don't care
                //   which signature within the transaction failed.
                // - For per-block batching, we currently don't care which transaction failed.
                false
            }
        }
    }
}

/// Creates a RedPallas batch validation context.
///
/// Please free this when you're done.
#[no_mangle]
pub extern "C" fn orchard_batch_validation_init() -> *mut BatchValidator {
    let ctx = Box::new(BatchValidator::new());
    Box::into_raw(ctx)
}

/// Frees a RedPallas batch validation context returned from
/// [`orchard_batch_validation_init`].
#[no_mangle]
pub extern "C" fn orchard_batch_validation_free(ctx: *mut BatchValidator) {
    if !ctx.is_null() {
        drop(unsafe { Box::from_raw(ctx) });
    }
}

/// Adds an Orchard bundle to this batch.
///
/// Currently, only RedPallas signatures are batch-validated.
#[no_mangle]
pub extern "C" fn orchard_batch_add_bundle(
    batch: *mut BatchValidator,
    bundle: *const Bundle<Authorized, Amount>,
    txid: *const [u8; 32],
) {
    let batch = unsafe { batch.as_mut() };
    let bundle = unsafe { bundle.as_ref() };
    let txid = unsafe { txid.as_ref() }.cloned().map(TxId::from_bytes);

    match (batch, bundle, txid) {
        (Some(batch), Some(bundle), Some(txid)) => batch.add_bundle(bundle, txid),
        (_, _, None) => error!("orchard_batch_add_bundle() called without txid!"),
        (Some(_), None, Some(txid)) => debug!("Tx {} has no Orchard component", txid),
        (None, Some(_), _) => debug!("Orchard BatchValidator not provided, assuming disabled."),
        (None, None, _) => (), // Boring, don't bother logging.
    }
}

/// Validates this batch.
///
/// - Returns `true` if `batch` is null.
/// - Returns `false` if any item in the batch is invalid.
#[no_mangle]
pub extern "C" fn orchard_batch_validate(batch: *const BatchValidator) -> bool {
    if let Some(batch) = unsafe { batch.as_ref() } {
        batch.validate()
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

/// A type alias for a pointer to a C++ value that is the target of
/// a mutating action by a callback across the FFI
// XXX this is duplicated from wallet.rs, should it be in a common place?
pub type FFICallbackReceiver = std::ptr::NonNull<libc::c_void>;

// This layout must match struct RawBundleActionn
#[repr(C)]
pub struct FFIBundleAction {
    cv: [u8; 32],
    nullifier: [u8; 32],
    rk: [u8; 32],
    cmx: [u8; 32],
    ephemeral_key: [u8; 32],
    enc_ciphertext: [u8; 580],
    out_ciphertext: [u8; 80],
    spend_auth_sig: [u8; 64],
}

// copies (pushes) a single orchard action
pub type ActionPushCB =
    unsafe extern "C" fn(obj: Option<FFICallbackReceiver>, data: FFIBundleAction);

#[no_mangle]
pub extern "C" fn orchard_bundle_get_actions(
    bundle: *const Bundle<Authorized, Amount>,
    callback_receiver: Option<FFICallbackReceiver>,
    action_push_cb: Option<ActionPushCB>,
) -> bool {
    if let Some(_bundle) = unsafe { bundle.as_ref() } {
        // XXX need to fill in the fields of this action, dummmy values for now
        let action = FFIBundleAction {
            cv: [0; 32],
            nullifier: [2; 32],
            rk: [3; 32],
            cmx: [4; 32],
            ephemeral_key: [5; 32],
            enc_ciphertext: [6; 580],
            out_ciphertext: [7; 80],
            spend_auth_sig: [8; 64],
        };
        unsafe { (action_push_cb.unwrap())(callback_receiver, action) };
        true
    } else {
        false
    }
}

// This layout must match struct OrchardAction
// These are the values that are not per-action (common across all actions)
#[repr(C)]
pub struct FFIBundleCommon {
    enable_spends: bool,
    enable_outputs: bool,
    value_balance_zat: i64,
    anchor: [u8; 32],
    proofs: u8, // TODO *mut u8,
    binding_sig: [u8; 64],
}

// copies (pushes) the common (across all actions) parts of an Orchard bundle
pub type BundleCommonPushCB =
    unsafe extern "C" fn(obj: Option<FFICallbackReceiver>, data: FFIBundleCommon);

#[no_mangle]
pub extern "C" fn orchard_bundle_get_common(
    bundle: *const Bundle<Authorized, Amount>,
    callback_receiver: Option<FFICallbackReceiver>,
    common_push_cb: Option<BundleCommonPushCB>,
) -> bool {
    if let Some(_bundle) = unsafe { bundle.as_ref() } {
        // XXX need to fill in the fields of this action, dummmy values for now
        let common = FFIBundleCommon {
            enable_spends: true,
            enable_outputs: false,
            value_balance_zat: 9999,
            anchor: [3; 32],
            proofs: 4,
            binding_sig: [5; 64],
        };
        unsafe { (common_push_cb.unwrap())(callback_receiver, common) };
        true
    } else {
        false
    }
}