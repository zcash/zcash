use std::convert::TryFrom;
use std::ptr;
use std::slice;

use incrementalmerkletree::Hashable;
use libc::size_t;
use orchard::keys::SpendingKey;
use orchard::{
    builder::{Builder, InProgress, Unauthorized, Unproven},
    bundle::{Authorized, Flags},
    keys::{FullViewingKey, OutgoingViewingKey},
    tree::{MerkleHashOrchard, MerklePath},
    value::NoteValue,
    Bundle, Note,
};
use rand_core::OsRng;
use tracing::error;
use zcash_primitives::{
    consensus::BranchId,
    transaction::{
        components::{sapling, Amount},
        sighash::{signature_hash, SignableInput},
        txid::TxIdDigester,
        Authorization, Transaction, TransactionData,
    },
};

use crate::{
    bridge::ffi::OrchardUnauthorizedBundlePtr,
    transaction_ffi::{MapTransparent, TransparentAuth},
    ORCHARD_PK,
};

pub struct OrchardSpendInfo {
    fvk: FullViewingKey,
    note: Note,
    merkle_path: MerklePath,
}

impl OrchardSpendInfo {
    pub fn from_parts(fvk: FullViewingKey, note: Note, merkle_path: MerklePath) -> Self {
        OrchardSpendInfo {
            fvk,
            note,
            merkle_path,
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_spend_info_free(spend_info: *mut OrchardSpendInfo) {
    if !spend_info.is_null() {
        drop(unsafe { Box::from_raw(spend_info) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_builder_new(
    spends_enabled: bool,
    outputs_enabled: bool,
    anchor: *const [u8; 32],
) -> *mut Builder {
    let anchor = unsafe { anchor.as_ref() }
        .map(|a| orchard::Anchor::from_bytes(*a).unwrap())
        .unwrap_or_else(|| MerkleHashOrchard::empty_root(32.into()).into());
    Box::into_raw(Box::new(Builder::new(
        Flags::from_parts(spends_enabled, outputs_enabled),
        anchor,
    )))
}

#[no_mangle]
pub extern "C" fn orchard_builder_add_spend(
    builder: *mut Builder,
    orchard_spend_info: *mut OrchardSpendInfo,
) -> bool {
    let builder = unsafe { builder.as_mut() }.expect("Builder may not be null.");
    let orchard_spend_info = unsafe { Box::from_raw(orchard_spend_info) };

    match builder.add_spend(
        orchard_spend_info.fvk,
        orchard_spend_info.note,
        orchard_spend_info.merkle_path,
    ) {
        Ok(()) => true,
        Err(e) => {
            error!("Failed to add Orchard spend: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_builder_add_recipient(
    builder: *mut Builder,
    ovk: *const [u8; 32],
    recipient: *const orchard::Address,
    value: u64,
    memo: *const [u8; 512],
) -> bool {
    let builder = unsafe { builder.as_mut() }.expect("Builder may not be null.");
    let ovk = unsafe { ovk.as_ref() }
        .copied()
        .map(OutgoingViewingKey::from);
    let recipient = unsafe { recipient.as_ref() }.expect("Recipient may not be null.");
    let value = NoteValue::from_raw(value);
    let memo = unsafe { memo.as_ref() }.copied();

    match builder.add_recipient(ovk, *recipient, value, memo) {
        Ok(()) => true,
        Err(e) => {
            error!("Failed to add Orchard recipient: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_builder_free(builder: *mut Builder) {
    if !builder.is_null() {
        drop(unsafe { Box::from_raw(builder) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_builder_build(
    builder: *mut Builder,
) -> *mut Bundle<InProgress<Unproven, Unauthorized>, Amount> {
    if builder.is_null() {
        error!("Called with null builder");
        return ptr::null_mut();
    }
    let builder = unsafe { Box::from_raw(builder) };

    match builder.build(OsRng) {
        Ok(bundle) => Box::into_raw(Box::new(bundle)),
        Err(e) => {
            error!("Failed to build Orchard bundle: {:?}", e);
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_unauthorized_bundle_free(
    bundle: *mut Bundle<InProgress<Unproven, Unauthorized>, Amount>,
) {
    if !bundle.is_null() {
        drop(unsafe { Box::from_raw(bundle) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_unauthorized_bundle_prove_and_sign(
    bundle: *mut Bundle<InProgress<Unproven, Unauthorized>, Amount>,
    keys: *const *const SpendingKey,
    keys_len: size_t,
    sighash: *const [u8; 32],
) -> *mut Bundle<Authorized, Amount> {
    let bundle = unsafe { Box::from_raw(bundle) };
    let keys = unsafe { slice::from_raw_parts(keys, keys_len) };
    let sighash = unsafe { sighash.as_ref() }.expect("sighash pointer may not be null.");
    let pk = unsafe { ORCHARD_PK.as_ref() }
        .expect("Parameters not loaded: ORCHARD_PK should have been initialized");

    let signing_keys = keys
        .iter()
        .map(|sk| {
            unsafe { sk.as_ref() }
                .expect("SpendingKey pointers must not be null")
                .into()
        })
        .collect::<Vec<_>>();

    let mut rng = OsRng;
    let res = bundle
        .create_proof(pk, &mut rng)
        .and_then(|b| b.apply_signatures(rng, *sighash, &signing_keys));

    match res {
        Ok(signed) => Box::into_raw(Box::new(signed)),
        Err(e) => {
            error!(
                "An error occurred while authorizing the orchard bundle: {:?}",
                e
            );
            std::ptr::null_mut()
        }
    }
}

/// Calculates a shielded signature digest for the given under-construction transaction.
///
/// Returns `false` if any of the parameters are invalid; in this case, `sighash_ret`
/// will be unaltered.
pub(crate) fn shielded_signature_digest(
    consensus_branch_id: u32,
    tx_bytes: &[u8],
    all_prev_outputs: &[u8],
    sapling_bundle: &crate::sapling::SaplingUnauthorizedBundle,
    orchard_bundle: *const OrchardUnauthorizedBundlePtr,
) -> Result<[u8; 32], String> {
    // TODO: This is also parsing a transaction that may have partially-filled fields.
    // This doesn't matter for transparent components (the only such field would be the
    // scriptSig fields of transparent inputs, which would serialize as empty Scripts),
    // but is ill-defined for shielded components (we'll be serializing 64 bytes of zeroes
    // for each signature). This is an internal FFI so it's fine for now, but we should
    // refactor the transaction builder (which is the only source of partially-created
    // shielded components) to use a different FFI for obtaining the sighash, that passes
    // across the transaction components and then constructs the TransactionData. This is
    // already being done as part of the Orchard changes to the transaction builder, since
    // the Orchard bundle will already be built on the Rust side, and we can avoid passing
    // it back and forward across the FFI with this change. We should similarly refactor
    // the Sapling code to do the same.
    let consensus_branch_id = BranchId::try_from(consensus_branch_id)
        .expect("caller should provide a valid consensus branch ID");
    let tx = Transaction::read(tx_bytes, consensus_branch_id)
        .map_err(|e| format!("Failed to parse transaction: {}", e))?;
    // This method should only be called with an in-progress transaction that contains no
    // Sapling or Orchard component.
    assert!(tx.sapling_bundle().is_none());
    assert!(tx.orchard_bundle().is_none());

    let f_transparent = MapTransparent::parse(all_prev_outputs, &tx)?;
    let orchard_bundle = unsafe {
        orchard_bundle
            .cast::<Bundle<InProgress<Unproven, Unauthorized>, Amount>>()
            .as_ref()
    };

    #[derive(Debug)]
    struct Signable {}
    impl Authorization for Signable {
        type TransparentAuth = TransparentAuth;
        type SaplingAuth = sapling::builder::Unauthorized;
        type OrchardAuth = InProgress<Unproven, Unauthorized>;
    }

    let txdata: TransactionData<Signable> = tx.into_data().map_bundles(
        |b| b.map(|b| b.map_authorization(f_transparent)),
        |_| sapling_bundle.bundle.clone(),
        |_| orchard_bundle.cloned(),
    );
    let txid_parts = txdata.digest(TxIdDigester);

    let sighash = signature_hash(&txdata, &SignableInput::Shielded, &txid_parts);

    Ok(*sighash.as_ref())
}
