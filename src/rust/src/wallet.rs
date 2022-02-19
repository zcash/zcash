use incrementalmerkletree::{bridgetree::BridgeTree, Frontier, Tree};
use libc::c_uchar;
use std::collections::{BTreeMap, HashMap, HashSet};
use tracing::error;

use zcash_primitives::{
    consensus::BlockHeight,
    transaction::{components::Amount, TxId},
};

use orchard::{
    bundle::Authorized,
    keys::{FullViewingKey, IncomingViewingKey, SpendingKey},
    tree::MerkleHashOrchard,
    Address, Bundle, Note,
};

use crate::zcashd_orchard::OrderedAddress;

use super::incremental_merkle_tree_ffi::MERKLE_DEPTH;

pub const MAX_CHECKPOINTS: usize = 100;

/// A data structure tracking the last transaction whose notes
/// have been added to the wallet's note commitment tree.
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

#[derive(Debug, Clone)]
pub struct DecryptedNote {
    note: Note,
    memo: [u8; 512],
}

struct TxNotes {
    decrypted_notes: BTreeMap<usize, DecryptedNote>,
}

/// A type used to pass note metadata across the FFI boundary.
/// This must have the same representation as `struct RawOrchardNoteMetadata`
/// in `rust/include/rust/orchard/wallet.h`.
#[repr(C)]
pub struct NoteMetadata {
    txid: [u8; 32],
    action_idx: u32,
    recipient: *const Address,
    note_value: i64,
    memo: [u8; 512],
}

struct KeyStore {
    payment_addresses: BTreeMap<OrderedAddress, IncomingViewingKey>,
    viewing_keys: BTreeMap<IncomingViewingKey, FullViewingKey>,
    spending_keys: BTreeMap<FullViewingKey, SpendingKey>,
}

impl KeyStore {
    pub fn empty() -> Self {
        KeyStore {
            payment_addresses: BTreeMap::new(),
            viewing_keys: BTreeMap::new(),
            spending_keys: BTreeMap::new(),
        }
    }

    pub fn add_full_viewing_key(&mut self, fvk: FullViewingKey) {
        let ivk = IncomingViewingKey::from(&fvk);
        self.viewing_keys.insert(ivk, fvk);
    }

    pub fn add_spending_key(&mut self, sk: SpendingKey) {
        let fvk = FullViewingKey::from(&sk);
        self.add_full_viewing_key(fvk.clone());
        self.spending_keys.insert(fvk, sk);
    }

    /// Adds an address/ivk pair to the wallet, and returns `true` if the IVK
    /// corresponds to a FVK known by this wallet, `false` otherwise.
    pub fn add_raw_address(&mut self, addr: Address, ivk: IncomingViewingKey) -> bool {
        let has_fvk = self.viewing_keys.contains_key(&ivk);
        self.payment_addresses
            .insert(OrderedAddress::new(addr), ivk);
        has_fvk
    }

    pub fn spending_key_for_ivk(&self, ivk: &IncomingViewingKey) -> Option<&SpendingKey> {
        self.viewing_keys
            .get(ivk)
            .and_then(|fvk| self.spending_keys.get(fvk))
    }

    pub fn ivk_for_address(&self, addr: &Address) -> Option<&IncomingViewingKey> {
        self.payment_addresses.get(&OrderedAddress::new(*addr))
    }
}

pub struct Wallet {
    /// The in-memory index of keys and addresses known to the wallet.
    key_store: KeyStore,
    /// The in-memory index from txid to notes from the associated transaction
    /// that have been decrypted with the IVKs known to this wallet.
    wallet_tx_notes: HashMap<TxId, TxNotes>,
    /// The incremental merkle tree used to track note commitments
    /// and witnesses for notes belonging to the wallet.
    witness_tree: BridgeTree<MerkleHashOrchard, MERKLE_DEPTH>,
    /// The block height and transaction index of the note most recently added to `witness_tree`
    last_observed: Option<LastObserved>,
    /// Notes marked as locked (currently reserved for pending transactions)
    locked_notes: HashSet<OutPoint>,
    /// Notes marked as spent
    spent_notes: HashSet<OutPoint>,
}

#[derive(Debug, Clone)]
pub enum WalletError {
    OutOfOrder(LastObserved, BlockHeight, usize),
    NoteCommitmentTreeFull,
}

impl Wallet {
    pub fn empty() -> Self {
        Wallet {
            key_store: KeyStore::empty(),
            wallet_tx_notes: HashMap::new(),
            witness_tree: BridgeTree::new(MAX_CHECKPOINTS),
            last_observed: None,
            locked_notes: HashSet::new(),
            spent_notes: HashSet::new(),
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
    ) -> Vec<usize> {
        let mut tx_notes = TxNotes {
            decrypted_notes: BTreeMap::new(),
        };

        let keys = self
            .key_store
            .viewing_keys
            .keys()
            .cloned()
            .collect::<Vec<_>>();
        let mut result = vec![];
        for (action_idx, ivk, note, recipient, memo) in bundle.decrypt_outputs_for_keys(&keys) {
            // Mark the witness tree with the fact that we want to be able to compute
            // a witness for this note.
            let note_data = DecryptedNote { note, memo };

            tx_notes.decrypted_notes.insert(action_idx, note_data);
            self.key_store.add_raw_address(recipient, ivk);
            result.push(action_idx);
        }

        if !tx_notes.decrypted_notes.is_empty() {
            self.wallet_tx_notes.insert(*txid, tx_notes);
        }

        result
    }

    /// Add note commitments for the Orchard components of a transaction to the note commitment
    /// tree, and mark the tree at the notes decryptable by this wallet so that in the future
    /// we can produce authentication paths to those notes.
    ///
    /// * `block_height` - Height of the block containing the transaction that provided this bundle.
    /// * `block_tx_idx` - Index of the transaction within the block
    /// * `txid` - Identifier of the transaction.
    /// * `bundle` - Orchard component of the transaction.
    pub fn append_bundle_commitments(
        &mut self,
        block_height: BlockHeight,
        block_tx_idx: usize,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
    ) -> Result<(), WalletError> {
        // Check that the wallet is in the correct state to update the note commitment tree with
        // new outputs.
        if let Some(last) = &self.last_observed {
            if !((block_height == last.height && block_tx_idx == last.block_tx_idx + 1)
                || (block_height == last.height + 1 && block_tx_idx == 0))
            {
                return Err(WalletError::OutOfOrder(
                    last.clone(),
                    block_height,
                    block_tx_idx,
                ));
            }
        }

        let tx_notes = self.wallet_tx_notes.get(txid);
        for (action_idx, action) in bundle.actions().iter().enumerate() {
            if !self
                .witness_tree
                .append(&MerkleHashOrchard::from_cmx(action.cmx()))
            {
                return Err(WalletError::NoteCommitmentTreeFull);
            }

            if let Some(tx_notes) = tx_notes {
                if tx_notes.decrypted_notes.contains_key(&action_idx) {
                    self.witness_tree.witness();
                }
            }
        }

        Ok(())
    }

    pub fn tx_contains_my_notes(&self, txid: &TxId) -> bool {
        self.wallet_tx_notes.get(txid).is_some()
    }

    pub fn get_filtered_notes(
        &self,
        ivk: Option<&IncomingViewingKey>,
        ignore_spent: bool,
        ignore_locked: bool,
        require_spending_key: bool,
    ) -> Vec<(OutPoint, DecryptedNote)> {
        self.wallet_tx_notes
            .iter()
            .flat_map(|(txid, tx_notes)| {
                tx_notes
                    .decrypted_notes
                    .iter()
                    .filter_map(move |(idx, dnote)| {
                        let outpoint = OutPoint {
                            txid: *txid,
                            action_idx: *idx,
                        };

                        self.key_store
                            .ivk_for_address(&dnote.note.recipient())
                            // if `ivk` is `None`, return all notes that match the other conditions
                            .filter(|dnote_ivk| ivk.iter().all(|ivk| ivk == dnote_ivk))
                            .and_then(|dnote_ivk| {
                                if (ignore_spent && self.spent_notes.contains(&outpoint))
                                    || (ignore_locked && self.locked_notes.contains(&outpoint))
                                    || (require_spending_key
                                        && self.key_store.spending_key_for_ivk(dnote_ivk).is_none())
                                {
                                    None
                                } else {
                                    Some((outpoint, (*dnote).clone()))
                                }
                            })
                    })
            })
            .collect()
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
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    wallet.checkpoint_witness_tree();
}

#[no_mangle]
pub extern "C" fn orchard_wallet_rewind(wallet: *mut Wallet) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    wallet.rewind_witness_tree()
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_notes_from_bundle(
    wallet: *mut Wallet,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
) {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        wallet.add_notes_from_bundle(&txid, bundle);
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
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        if let Err(e) =
            wallet.append_bundle_commitments(block_height.into(), block_tx_idx, &txid, bundle)
        {
            error!("An error occurred adding the Orchard bundle's notes to the note commitment tree: {:?}", e);
            return false;
        }
    }

    true
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_spending_key(wallet: *mut Wallet, sk: *const SpendingKey) {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let sk = unsafe { sk.as_ref() }.expect("Spending key may not be null.");

    wallet.key_store.add_spending_key(*sk);
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_full_viewing_key(
    wallet: *mut Wallet,
    fvk: *const FullViewingKey,
) {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null.");
    let fvk = unsafe { fvk.as_ref() }.expect("Full viewing key pointer may not be null.");

    wallet.key_store.add_full_viewing_key(fvk.clone());
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_raw_address(
    wallet: *mut Wallet,
    addr: *const Address,
    ivk: *const IncomingViewingKey,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null.");
    let addr = unsafe { addr.as_ref() }.expect("Address may not be null.");
    let ivk = unsafe { ivk.as_ref() }.expect("Incoming viewing key may not be null.");

    wallet.key_store.add_raw_address(*addr, ivk.clone())
}

#[no_mangle]
pub extern "C" fn orchard_wallet_get_ivk_for_address(
    wallet: *const Wallet,
    addr: *const Address,
) -> *mut IncomingViewingKey {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let addr = unsafe { addr.as_ref() }.expect("Address may not be null.");

    wallet
        .key_store
        .ivk_for_address(addr)
        .map(|ivk| Box::into_raw(Box::new(ivk.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_contains_my_notes(
    wallet: *const Wallet,
    txid: *const [c_uchar; 32],
) -> bool {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));

    wallet.tx_contains_my_notes(&txid)
}

pub type VecObj = std::ptr::NonNull<libc::c_void>;
pub type PushCb = unsafe extern "C" fn(obj: Option<VecObj>, meta: NoteMetadata);

#[no_mangle]
pub extern "C" fn orchard_wallet_get_filtered_notes(
    wallet: *const Wallet,
    ivk: *const IncomingViewingKey,
    ignore_spent: bool,
    ignore_locked: bool,
    require_spending_key: bool,
    result: Option<VecObj>,
    push_cb: Option<PushCb>,
) {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let ivk = unsafe { ivk.as_ref() };

    for (outpoint, dnote) in
        wallet.get_filtered_notes(ivk, ignore_spent, ignore_locked, require_spending_key)
    {
        let recipient = Box::new(dnote.note.recipient());
        let metadata = NoteMetadata {
            txid: *outpoint.txid.as_ref(),
            action_idx: outpoint.action_idx as u32,
            recipient: Box::into_raw(recipient),
            note_value: dnote.note.value().inner() as i64,
            memo: dnote.memo,
        };
        unsafe { (push_cb.unwrap())(result, metadata) };
    }
}
