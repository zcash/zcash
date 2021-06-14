use std::{ptr, slice};

use blake2b_simd::Hash;
use libc::{c_uchar, size_t};
use tracing::error;
use zcash_primitives::{
    consensus::BranchId,
    legacy::Script,
    transaction::{
        components::Amount,
        sighash::{signature_hash, SignableInput},
        txid::TxIdDigester,
        Transaction, TxDigests, TxVersion,
    },
};

/// Calculates identifying and authorizing digests for the given transaction.
///
/// If either `txid_ret` or `auth_digest_ret` is `nullptr`, the corresponding digest will
/// not be calculated.
///
/// Returns `false` if the transaction is invalid.
#[no_mangle]
pub extern "C" fn zcash_transaction_digests(
    tx_bytes: *const c_uchar,
    tx_bytes_len: size_t,
    txid_ret: *mut [u8; 32],
    auth_digest_ret: *mut [u8; 32],
) -> bool {
    let tx_bytes = unsafe { slice::from_raw_parts(tx_bytes, tx_bytes_len) };

    // We use a placeholder branch ID here, since it is not used for anything.
    let tx = match Transaction::read(tx_bytes, BranchId::Canopy) {
        Ok(tx) => tx,
        Err(e) => {
            error!("Failed to parse transaction: {}", e);
            return false;
        }
    };

    if let Some(txid_ret) = unsafe { txid_ret.as_mut() } {
        *txid_ret = *tx.txid().as_ref();
    }
    if let Some(auth_digest_ret) = unsafe { auth_digest_ret.as_mut() } {
        match tx.version() {
            // Pre-NU5 transaction formats don't have authorizing data commitments; when
            // included in the authDataCommitment tree, they use the null hash.
            TxVersion::Sprout(_) | TxVersion::Overwinter | TxVersion::Sapling => {
                *auth_digest_ret = [0; 32]
            }
            _ => auth_digest_ret.copy_from_slice(tx.auth_commitment().as_bytes()),
        }
    }

    true
}

pub struct PrecomputedTxParts {
    tx: Transaction,
    txid_parts: TxDigests<Hash>,
}

/// Precomputes the `TxDigest` struct for the given transaction.
///
/// Returns `nullptr` if the transaction is invalid, or a v1-v4 transaction format.
#[no_mangle]
pub extern "C" fn zcash_transaction_precomputed_init(
    tx_bytes: *const c_uchar,
    tx_bytes_len: size_t,
) -> *mut PrecomputedTxParts {
    let tx_bytes = unsafe { slice::from_raw_parts(tx_bytes, tx_bytes_len) };

    // We use a placeholder branch ID here, since it is not used for anything.
    let tx = match Transaction::read(tx_bytes, BranchId::Canopy) {
        Ok(tx) => tx,
        Err(e) => {
            error!("Failed to parse transaction: {}", e);
            return ptr::null_mut();
        }
    };

    match tx.version() {
        TxVersion::Sprout(_) | TxVersion::Overwinter | TxVersion::Sapling => {
            // We don't support these legacy transaction formats in this API.
            ptr::null_mut()
        }
        _ => {
            let txid_parts = tx.digest(TxIdDigester);
            Box::into_raw(Box::new(PrecomputedTxParts { tx, txid_parts }))
        }
    }
}

/// Frees a precomputed transaction from `zcash_transaction_precomputed_init`.
#[no_mangle]
pub extern "C" fn zcash_transaction_precomputed_free(precomputed_tx: *mut PrecomputedTxParts) {
    if !precomputed_tx.is_null() {
        drop(unsafe { Box::from_raw(precomputed_tx) });
    }
}

/// Calculates a signature digest for the given transaction.
///
/// Returns `false` if any of the parameters are invalid; in this case, `sighash_ret`
/// will be unaltered.
#[no_mangle]
pub extern "C" fn zcash_transaction_transparent_signature_digest(
    precomputed_tx: *const PrecomputedTxParts,
    hash_type: u32,
    index: size_t,
    script_code: *const c_uchar,
    script_code_len: size_t,
    value: i64,
    sighash_ret: *mut [u8; 32],
) -> bool {
    let precomputed_tx = if let Some(res) = unsafe { precomputed_tx.as_ref() } {
        res
    } else {
        error!("Invalid precomputed transaction");
        return false;
    };
    let script_code =
        match Script::read(unsafe { slice::from_raw_parts(script_code, script_code_len) }) {
            Ok(res) => res,
            Err(e) => {
                error!("Invalid scriptCode: {}", e);
                return false;
            }
        };
    let value = match Amount::from_i64(value) {
        Ok(res) => res,
        Err(()) => {
            error!("Invalid amount");
            return false;
        }
    };

    let signable_input = SignableInput::transparent(index, &script_code, value);

    let sighash = match precomputed_tx.tx.version() {
        TxVersion::Sprout(_) | TxVersion::Overwinter | TxVersion::Sapling => {
            // We don't support these legacy transaction formats in this API.
            return false;
        }
        _ => signature_hash(
            &precomputed_tx.tx,
            hash_type,
            &signable_input,
            &precomputed_tx.txid_parts,
        ),
    };

    *unsafe { &mut *sighash_ret } = *sighash.as_ref();
    true
}
