use std::convert::TryInto;
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
use zcash_primitives::transaction::{
    components::{sapling, Amount},
    sighash::SignableInput,
    sighash_v5::v5_signature_hash,
    txid::TxIdDigester,
    Authorization, TransactionData, TxVersion,
};

use crate::{
    transaction_ffi::{PrecomputedTxParts, TransparentAuth},
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
pub extern "C" fn orchard_builder_duplicate_nullifier_spend(
    builder: *mut Builder
) -> bool {
    let builder = unsafe { builder.as_mut() }.expect("Builder may not be null.");

    builder.add_duplicate_nullifier_spend(OsRng);

    true
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
    let pk = unsafe { ORCHARD_PK.as_ref() }.unwrap();

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
        .and_then(|b| b.apply_signatures(&mut rng, *sighash, &signing_keys));

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

/// Calculates a ZIP 244 shielded signature digest for the given under-construction
/// transaction.
///
/// Returns `false` if any of the parameters are invalid; in this case, `sighash_ret`
/// will be unaltered.
#[no_mangle]
pub extern "C" fn zcash_builder_zip244_shielded_signature_digest(
    precomputed_tx: *mut PrecomputedTxParts,
    bundle: *const Bundle<InProgress<Unproven, Unauthorized>, Amount>,
    sighash_ret: *mut [u8; 32],
) -> bool {
    let precomputed_tx = if !precomputed_tx.is_null() {
        unsafe { Box::from_raw(precomputed_tx) }
    } else {
        error!("Invalid precomputed transaction");
        return false;
    };
    if matches!(
        precomputed_tx.tx.version(),
        TxVersion::Sprout(_) | TxVersion::Overwinter | TxVersion::Sapling,
    ) {
        error!("Cannot calculate ZIP 244 digest for pre-v5 transaction");
        return false;
    }
    let bundle = unsafe { bundle.as_ref().unwrap() };

    struct Signable {}
    impl Authorization for Signable {
        type TransparentAuth = TransparentAuth;
        type SaplingAuth = sapling::Authorized;
        type OrchardAuth = InProgress<Unproven, Unauthorized>;
    }

    let txdata: TransactionData<Signable> =
        precomputed_tx
            .tx
            .map_bundles(|b| b, |b| b, |_| Some(bundle.clone()));
    let txid_parts = txdata.digest(TxIdDigester);

    let sighash = v5_signature_hash(&txdata, &SignableInput::Shielded, &txid_parts);

    // `v5_signature_hash` output is always 32 bytes.
    *unsafe { &mut *sighash_ret } = sighash.as_ref().try_into().unwrap();
    true
}
