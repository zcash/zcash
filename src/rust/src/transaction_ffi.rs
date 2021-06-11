use std::slice;

use libc::{c_uchar, size_t};
use tracing::error;
use zcash_primitives::{
    consensus::BranchId,
    transaction::{Transaction, TxVersion},
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
            // Pre-NU5 transaction formats don't have auth digests; leave it empty so we
            // don't need to know the correct consensus branch ID for them.
            TxVersion::Sprout(_) | TxVersion::Overwinter | TxVersion::Sapling => (),
            _ => auth_digest_ret.copy_from_slice(tx.auth_commitment().as_bytes()),
        }
    }

    true
}
