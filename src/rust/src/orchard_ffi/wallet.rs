use incrementalmerkletree::{bridgetree::BridgeTree, Frontier, Tree};
use libc::c_uchar;
use std::collections::{BTreeMap, HashMap};
use tracing::error;

use zcash_primitives::{
    consensus::BlockHeight,
    transaction::{components::Amount, TxId},
};

use orchard::{
    bundle::Authorized,
    keys::{FullViewingKey, IncomingViewingKey},
    tree::MerkleCrhOrchardOutput,
    Address, Bundle, Note,
};

use super::incremental_merkle_tree_ffi::MERKLE_DEPTH;

pub const MAX_CHECKPOINTS: usize = 100;

#[derive(Debug, Clone)]
pub struct LastObserved {
    height: BlockHeight,
    block_tx_idx: usize,
}

#[derive(Clone, PartialEq, Eq, Hash)]
pub struct OutPoint {
    txid: TxId,
    action_idx: usize,
}

pub struct DecryptedNote {
    ivk: IncomingViewingKey,
    note: Note,
    recipient: Address,
    memo: [u8; 512],
}

struct WalletTx {
    txid: TxId,
    decrypted_notes: BTreeMap<usize, DecryptedNote>,
}

pub struct Wallet {
    last_observed: Option<LastObserved>,
    payment_addresses: BTreeMap<IncomingViewingKey, Vec<Address>>,
    incoming_viewing_keys: BTreeMap<IncomingViewingKey, FullViewingKey>,
    witness_tree: BridgeTree<MerkleCrhOrchardOutput, MERKLE_DEPTH>,
    wallet_txs: HashMap<TxId, WalletTx>,
}

#[derive(Debug, Clone)]
pub enum WalletError {
    OutOfOrder(LastObserved, BlockHeight, usize),
    NoteCommitmentTreeFull,
}

impl Wallet {
    pub fn empty() -> Self {
        Wallet {
            last_observed: None,
            payment_addresses: BTreeMap::new(),
            incoming_viewing_keys: BTreeMap::new(),
            witness_tree: BridgeTree::new(MAX_CHECKPOINTS),
            wallet_txs: HashMap::new(),
        }
    }

    pub fn checkpoint_witness_tree(&mut self) {
        self.witness_tree.checkpoint();
    }

    pub fn rewind_witness_tree(&mut self) -> bool {
        self.witness_tree.rewind()
    }

    /// Add note data for those notes that are decryptable with one of this wallet's
    /// incoming viewing keys to the wallet, and return the indices of the actions
    /// that we were able to decrypt.
    pub fn add_notes_from_bundle(
        &mut self,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
    ) -> Result<(), WalletError> {
        let mut wallet_tx = WalletTx {
            txid: *txid,
            decrypted_notes: BTreeMap::new(),
        };

        let keys = self.incoming_viewing_keys.keys().cloned().collect::<Vec<_>>();
        for (action_idx, ivk, note, recipient, memo) in bundle.decrypt_outputs_for_keys(&keys) {
            // Mark the witness tree with the fact that we want to be able to compute
            // a witness for this note.
            let note_data = DecryptedNote {
                ivk: ivk.clone(),
                note,
                recipient,
                memo,
            };
            wallet_tx.decrypted_notes.insert(action_idx, note_data);
            // TODO: avoid duplicates?
            self.payment_addresses
                .entry(ivk.clone())
                .or_insert(vec![])
                .push(recipient);
        }

        if !wallet_tx.decrypted_notes.is_empty() {
            self.wallet_txs.insert(*txid, wallet_tx);
        }

        Ok(())
    }

    /// Add note commitment data to the note commitment tree, and mark the tree for
    /// incoming viewing keys to the wallet, and return the indices of the actions
    /// that we were able to decrypt.
    ///
    /// * `block_height` - Height of the block containing the transaction that provided this bundle.
    /// * `txidx` - Index of the transaction within the block
    /// * `bundle` - Orchard component of the transaction.
    /// * `to_witness` - Indices of the actions that are to be marked as ours in the note commitment tree.
    pub fn append_bundle_commitments(
        &mut self,
        block_height: BlockHeight,
        block_tx_idx: usize,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
    ) -> Result<(), WalletError> {
        // Check that the wallet is in the correct state to update the note commitment tree with
        // new outputs.
        if !self.last_observed.iter().all(|last_observed| {
            (block_height == last_observed.height && block_tx_idx == last_observed.block_tx_idx + 1)
                || (block_height == last_observed.height + 1 && block_tx_idx == 0)
        }) {
            return Err(WalletError::OutOfOrder(
                // this unwrap is safe, because `all` above ensures that if we have not observed
                // any blocks, we cannot be observing a block out of order
                self.last_observed.as_ref().unwrap().clone(),
                block_height,
                block_tx_idx,
            ));
        }

        let wallet_tx = self.wallet_txs.get(&txid);
        for (action_idx, action) in bundle.actions().iter().enumerate() {
            if !self
                .witness_tree
                .append(&MerkleCrhOrchardOutput::from_cmx(action.cmx()))
            {
                return Err(WalletError::NoteCommitmentTreeFull);
            }

            if let Some(wallet_tx) = wallet_tx {
                if wallet_tx.decrypted_notes.contains_key(&action_idx) {
                    self.witness_tree.witness();
                }
            }
        }

        Ok(())
    }

    pub fn is_mine(&self, txid: &TxId) -> bool {
        return match self.wallet_txs.get(&txid) {
            Some(wtx) if !wtx.decrypted_notes.is_empty() => true,
            _ => false,
        };
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_new() -> *mut Wallet {
    let empty_wallet = Wallet::empty();
    Box::into_raw(Box::new(empty_wallet))
}

#[no_mangle]
pub extern "C" fn orchard_wallet_free(wallet: *mut Wallet) {
    if !wallet.is_null() {
        drop(unsafe { Box::from_raw(wallet) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_checkpoint(wallet: *mut Wallet) {
    let wallet = unsafe { &mut *wallet };
    wallet.checkpoint_witness_tree();
}

#[no_mangle]
pub extern "C" fn orchard_wallet_rewind(wallet: *mut Wallet) -> bool {
    let wallet = unsafe { &mut *wallet };
    wallet.rewind_witness_tree()
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_notes_from_bundle(
    wallet: *mut Wallet,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
) -> bool {
    let wallet = unsafe { &mut *wallet };
    let txid = TxId::from_bytes(unsafe { &*txid }.clone());
    match unsafe { bundle.as_ref() } {
        Some(bundle) => match wallet.add_notes_from_bundle(&txid, bundle) {
            Err(e) => {
                error!("An error occurred adding the Orchard bundle's notes to the wallet for txid {:?}: {:?}", txid, e);
                false
            }
            Ok(_) => true,
        },
        None => true,
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_append_bundle_commitments(
    wallet: *mut Wallet,
    block_height: u32,
    block_tx_idx: usize,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
) -> bool {
    let wallet = unsafe { &mut *wallet };
    let txid = TxId::from_bytes(unsafe { &*txid }.clone());
    match unsafe { bundle.as_ref() } {
        Some(bundle) => {
            match wallet.append_bundle_commitments(block_height.into(), block_tx_idx, &txid, bundle)
            {
                Err(e) => {
                    error!("An error occurred adding the Orchard bundle's notes to the note commitment tree: {:?}", e);
                    false
                }
                Ok(()) => true,
            }
        }
        None => true,
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_is_mine(
    wallet: *mut Wallet,
    txid: *const [c_uchar; 32],
) -> bool {
    let wallet = unsafe { &mut *wallet };
    let txid = TxId::from_bytes(unsafe { &*txid }.clone());

    wallet.is_mine(&txid)
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_data_new() -> *mut Vec<usize> {
    let v = vec![];
    Box::into_raw(Box::new(v))
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_data_free(tx_data: *mut Vec<usize>) {
    if !tx_data.is_null() {
        drop(unsafe { Box::from_raw(tx_data) });
    }
}
