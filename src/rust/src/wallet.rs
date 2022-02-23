use incrementalmerkletree::{bridgetree::BridgeTree, Frontier, Tree};
use libc::c_uchar;
use std::collections::{BTreeMap, BTreeSet};
use tracing::error;

use zcash_primitives::{
    consensus::BlockHeight,
    transaction::{components::Amount, TxId},
};

use orchard::{
    bundle::Authorized,
    keys::{FullViewingKey, IncomingViewingKey, SpendingKey},
    note::Nullifier,
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
    block_height: BlockHeight,
    block_tx_idx: Option<usize>,
}

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct OutPoint {
    txid: TxId,
    action_idx: usize,
}

#[derive(Debug, Clone)]
pub struct DecryptedNote {
    note: Note,
    memo: [u8; 512],
}

/// A data structure tracking the note data that was decrypted from a single transaction,
/// along with the height at which that transaction was discovered in the chain.
#[derive(Debug, Clone)]
pub struct TxNotes {
    /// The block height containing the transaction from which these
    /// notes were decrypted.
    tx_height: Option<BlockHeight>,
    /// A map from the index of the Orchard action from which this note
    /// was decrypted to the decrypted note value.
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
        // When we add a full viewing key, we need to add both the internal and external
        // incoming viewing keys.
        let external_ivk = IncomingViewingKey::from(&fvk);
        let internal_ivk = IncomingViewingKey::from(&fvk.derive_internal());
        self.viewing_keys.insert(external_ivk, fvk.clone());
        self.viewing_keys.insert(internal_ivk, fvk);
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
    /// The in-memory index from txid to notes from the associated transaction that have
    /// been decrypted with the IVKs known to this wallet.
    wallet_received_notes: BTreeMap<TxId, TxNotes>,
    /// The in-memory index from nullifier to the outpoint of the note from which that
    /// nullifier was derived.
    nullifiers: BTreeMap<Nullifier, OutPoint>,
    /// The incremental merkle tree used to track note commitments and witnesses for notes
    /// belonging to the wallet.
    witness_tree: BridgeTree<MerkleHashOrchard, MERKLE_DEPTH>,
    /// The block height at which the last checkpoint was created, if any.
    last_checkpoint: Option<BlockHeight>,
    /// The block height and transaction index of the note most recently added to
    /// `witness_tree`
    last_observed: Option<LastObserved>,
    /// Notes marked as locked (currently reserved for pending transactions)
    locked_notes: BTreeSet<OutPoint>,
    /// Notes marked as spent as a consequence of their nullifiers having been
    /// observed in bundle action inputs. The keys of this map are the notes
    /// that have been spent, and the values are the outpoints identifying
    /// the actions in which they are spent.
    spent_notes: BTreeMap<OutPoint, OutPoint>,
    /// For each nullifier which appears more than once in transactions that this
    /// wallet has observed, the set of outpoints corresponding to those nullifiers.
    conflicts: BTreeMap<Nullifier, BTreeSet<OutPoint>>,
}

#[derive(Debug, Clone)]
pub enum WalletError {
    OutOfOrder(LastObserved, BlockHeight, usize),
    NoteCommitmentTreeFull,
}

#[derive(Debug, Clone)]
pub enum RewindError {
    /// The witness tree does not contain enough checkpoints to
    /// rewind to the requested height. The number of blocks that
    /// it is possible to rewind is returned as the payload of
    /// this error.
    InsufficientCheckpoints(usize),
}

impl Wallet {
    pub fn empty() -> Self {
        Wallet {
            key_store: KeyStore::empty(),
            wallet_received_notes: BTreeMap::new(),
            nullifiers: BTreeMap::new(),
            witness_tree: BridgeTree::new(MAX_CHECKPOINTS),
            last_checkpoint: None,
            last_observed: None,
            locked_notes: BTreeSet::new(),
            spent_notes: BTreeMap::new(),
            conflicts: BTreeMap::new(),
        }
    }

    /// Reset the state of the wallet to be suitable for rescan from the NU5 activation
    /// height.  This removes all witness and spentness information from the wallet. The
    /// keystore is unmodified and decrypted note, nullifier, and conflict data are left
    /// in place with the expectation that they will be overwritten and/or updated in
    /// the rescan process.
    pub fn reset(&mut self) {
        for tx_notes in self.wallet_received_notes.values_mut() {
            tx_notes.tx_height = None;
        }
        self.witness_tree = BridgeTree::new(MAX_CHECKPOINTS);
        self.last_checkpoint = None;
        self.last_observed = None;
        self.locked_notes = BTreeSet::new();
        self.spent_notes = BTreeMap::new();
    }

    /// Checkpoints the note commitment tree. This returns `false` and leaves the note
    /// commitment tree unmodified if the block height does not immediately succeed
    /// the last checkpointed block height (unless the note commitment tree is empty,
    /// in which case it unconditionally succeeds). This must be called exactly once
    /// per block.
    pub fn checkpoint(&mut self, block_height: BlockHeight) -> bool {
        // checkpoints must be in order of sequential block height and every
        // block must be checkpointed
        if self
            .last_checkpoint
            .iter()
            .any(|last_height| block_height != *last_height + 1)
        {
            return false;
        }

        self.witness_tree.checkpoint();
        self.last_checkpoint = Some(block_height);
        true
    }

    /// Returns whether or not a checkpoint has been created. If no checkpoint exists,
    /// the wallet has not yet observed any blocks.
    pub fn is_checkpointed(&self) -> bool {
        self.last_checkpoint.is_some()
    }

    /// Rewinds the note commitment tree to the given height, removes notes and
    /// spentness information for transactions mined in the removed blocks, and returns
    /// the number of blocks by which the tree has been rewound if successful. Returns  
    /// `RewindError` if not enough checkpoints exist to execute the full rewind
    /// requested. If the requested height is greater than or equal to the height of the
    /// latest checkpoint, this returns `Ok(0)` and leaves the wallet unmodified.
    pub fn rewind(&mut self, to_height: BlockHeight) -> Result<u32, RewindError> {
        if let Some(checkpoint_height) = self.last_checkpoint {
            if to_height >= checkpoint_height {
                return Ok(0);
            }

            let blocks_to_rewind = <u32>::from(checkpoint_height) - <u32>::from(to_height);

            // if the rewind length exceeds the number of checkpoints we have stored,
            // return failure.
            let checkpoint_count = self.witness_tree.checkpoints().len();
            if blocks_to_rewind as usize > checkpoint_count {
                return Err(RewindError::InsufficientCheckpoints(checkpoint_count));
            }

            for _ in 0..blocks_to_rewind {
                // we've already checked that we have enough checkpoints to fully
                // rewind by the requested amount.
                assert!(self.witness_tree.rewind());
            }

            // reset our last observed height to ensure that notes added in the future are
            // from a new block
            let last_observed = LastObserved {
                block_height: checkpoint_height - blocks_to_rewind,
                block_tx_idx: None,
            };

            // retain notes that correspond to transactions that are not "un-mined" after
            // the rewind
            let to_retain: BTreeSet<_> = self
                .wallet_received_notes
                .iter()
                .filter_map(|(txid, n)| {
                    if n.tx_height
                        .map_or(true, |h| h <= last_observed.block_height)
                    {
                        Some(*txid)
                    } else {
                        None
                    }
                })
                .collect();

            self.spent_notes.retain(|_, v| to_retain.contains(&v.txid));
            self.nullifiers.retain(|_, v| to_retain.contains(&v.txid));
            for (txid, n) in self.wallet_received_notes.iter_mut() {
                // Erase block height information for any received notes
                // that have been un-mined by the rewind.
                if !to_retain.contains(txid) {
                    n.tx_height = None;
                }
            }
            self.last_observed = Some(last_observed);

            Ok(blocks_to_rewind)
        } else {
            Err(RewindError::InsufficientCheckpoints(0))
        }
    }

    /// Add note data for those notes that are decryptable with one of this wallet's
    /// incoming viewing keys to the wallet, and return the indices of the actions that we
    /// were able to decrypt.
    pub fn add_notes_from_bundle(
        &mut self,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
    ) -> Vec<usize> {
        let mut tx_notes = TxNotes {
            tx_height: None,
            decrypted_notes: BTreeMap::new(),
        };

        // Check for spends of our notes by matching against the nullifiers
        // we're tracking, and when we detect one, associate the current
        // txid as spending the note.
        for (action_idx, action) in bundle.actions().iter().enumerate() {
            let nf = action.nullifier();
            if let Some(outpoint) = self.nullifiers.get(nf) {
                self.spent_notes.insert(
                    *outpoint,
                    OutPoint {
                        txid: *txid,
                        action_idx,
                    },
                );
            }
        }

        let keys = self
            .key_store
            .viewing_keys
            .keys()
            .cloned()
            .collect::<Vec<_>>();
        let mut result = vec![];
        for (action_idx, ivk, note, recipient, memo) in bundle.decrypt_outputs_for_keys(&keys) {
            let outpoint = OutPoint {
                txid: *txid,
                action_idx,
            };

            // Generate the nullifier for the received note and add it to the nullifiers map so
            // that we can detect when the note is later spent.
            if let Some(fvk) = self.key_store.viewing_keys.get(&ivk) {
                let nf = note.nullifier(fvk);
                // if we already have an entry for this nullifier that does not correspond
                // to the current outpoint, then add the existing outpoint to the
                // conflicts map.
                if let Some(op) = self.nullifiers.get(&nf) {
                    if op != &outpoint {
                        let conflicts = self.conflicts.entry(nf).or_insert_with(BTreeSet::new);
                        conflicts.insert(*op);
                        conflicts.insert(outpoint);
                    };
                }

                self.nullifiers.insert(nf, outpoint);
            }

            // add the decrypted note data to the wallet
            let note_data = DecryptedNote { note, memo };
            tx_notes.decrypted_notes.insert(action_idx, note_data);

            // add the association between the address and the IVK used
            // to decrypt the note
            self.key_store.add_raw_address(recipient, ivk);
            result.push(action_idx);
        }

        if !tx_notes.decrypted_notes.is_empty() {
            self.wallet_received_notes.insert(*txid, tx_notes);
        }

        result
    }

    /// Add note commitments for the Orchard components of a transaction to the note
    /// commitment tree, and mark the tree at the notes decryptable by this wallet so that
    /// in the future we can produce authentication paths to those notes.
    ///
    /// * `block_height` - Height of the block containing the transaction that provided
    ///   this bundle.
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
            if !(
                // we are observing a subsequent transaction in the same block
                (block_height == last.block_height && last.block_tx_idx.map_or(false, |idx| idx < block_tx_idx))
                // or we are observing a new block
                || block_height > last.block_height
            ) {
                return Err(WalletError::OutOfOrder(
                    last.clone(),
                    block_height,
                    block_tx_idx,
                ));
            }
        }

        self.last_observed = Some(LastObserved {
            block_height,
            block_tx_idx: Some(block_tx_idx),
        });

        // update the block height recorded for the transaction
        if let Some(tx_notes) = self.wallet_received_notes.get_mut(txid) {
            tx_notes.tx_height = Some(block_height);
        }

        let my_notes_for_tx = self.wallet_received_notes.get(txid);
        for (action_idx, action) in bundle.actions().iter().enumerate() {
            // append the note commitment for each action to the witness tree
            if !self
                .witness_tree
                .append(&MerkleHashOrchard::from_cmx(action.cmx()))
            {
                return Err(WalletError::NoteCommitmentTreeFull);
            }

            // for notes that are ours, witness the current state of the tree
            if my_notes_for_tx
                .iter()
                .any(|n| n.decrypted_notes.contains_key(&action_idx))
            {
                self.witness_tree.witness();
            }
        }

        Ok(())
    }

    /// Returns whether the transaction contains any notes either sent to or spent by this
    /// wallet.
    pub fn tx_contains_my_notes(&self, txid: &TxId) -> bool {
        self.wallet_received_notes.get(txid).is_some()
            || self.nullifiers.values().any(|v| v.txid == *txid)
    }

    pub fn get_filtered_notes(
        &self,
        ivk: Option<&IncomingViewingKey>,
        ignore_spent: bool,
        ignore_locked: bool,
        require_spending_key: bool,
    ) -> Vec<(OutPoint, DecryptedNote)> {
        self.wallet_received_notes
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
                            .filter(|dnote_ivk| ivk.map_or(true, |ivk| &ivk == dnote_ivk))
                            .and_then(|dnote_ivk| {
                                if (ignore_spent && self.spent_notes.contains_key(&outpoint))
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
pub extern "C" fn orchard_wallet_checkpoint(wallet: *mut Wallet, block_height: u32) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    wallet.checkpoint(block_height.into())
}

#[no_mangle]
pub extern "C" fn orchard_wallet_is_checkpointed(wallet: *const Wallet) -> bool {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null");
    wallet.is_checkpointed()
}

#[no_mangle]
pub extern "C" fn orchard_wallet_rewind(
    wallet: *mut Wallet, 
    to_height: BlockHeight,
    blocks_rewound: *mut u32) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let blocks_rewound = unsafe { blocks_rewound.as_mut() }.expect("Return value pointer may not be null.");
    match wallet.rewind(to_height) {
        Ok(rewound) => {
            *blocks_rewound = rewound;
            true
        }
        Err(e) => {
            error!("Unable to rewind the wallet to height {:?}: {:?}", to_height, e);
            false
        }
    }
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
