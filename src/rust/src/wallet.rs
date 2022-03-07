use std::convert::TryInto;
use std::ptr;
use std::slice;

use incrementalmerkletree::{bridgetree::BridgeTree, Frontier, Position, Tree};
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
    tree::{MerkleHashOrchard, MerklePath},
    Address, Bundle, Note,
};

use crate::{builder_ffi::OrchardSpendInfo, zcashd_orchard::OrderedAddress};

use super::incremental_merkle_tree_ffi::MERKLE_DEPTH;

pub const MAX_CHECKPOINTS: usize = 100;

/// A data structure tracking the last transaction whose notes
/// have been added to the wallet's note commitment tree.
#[derive(Debug, Clone)]
pub struct LastObserved {
    block_height: BlockHeight,
    block_tx_idx: Option<usize>,
}

/// A pointer to a particular action in an Orchard transaction output.
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct OutPoint {
    txid: TxId,
    action_idx: usize,
}

/// A pointer to a previous output being spent in an Orchard action.
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct InPoint {
    txid: TxId,
    action_idx: usize,
}

#[derive(Debug, Clone)]
pub struct DecryptedNote {
    note: Note,
    memo: [u8; 512],
    /// The position of the note's commitment within the global Merkle tree.
    position: Option<Position>,
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

    pub fn get_nullifier(&self, note: &Note) -> Option<Nullifier> {
        self.ivk_for_address(&note.recipient())
            .and_then(|ivk| self.viewing_keys.get(ivk))
            .map(|fvk| note.nullifier(fvk))
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
    /// The incremental Merkle tree used to track note commitments and witnesses for notes
    /// belonging to the wallet.
    witness_tree: BridgeTree<MerkleHashOrchard, MERKLE_DEPTH>,
    /// The block height at which the last checkpoint was created, if any.
    last_checkpoint: Option<BlockHeight>,
    /// The block height and transaction index of the note most recently added to
    /// `witness_tree`
    last_observed: Option<LastObserved>,
    /// Notes marked as mined as a consequence of their nullifiers having been observed
    /// in bundle action inputs in bundles appended to the commitment tree.  The keys of
    /// this map are the outpoints where the spent notes were created, and the values
    /// are the inpoints identifying the actions in which they are spent.
    mined_notes: BTreeMap<OutPoint, InPoint>,
    /// For each nullifier which appears more than once in transactions that this
    /// wallet has observed, the set of inpoints where those nullifiers were
    /// observed as as having been spent.
    potential_spends: BTreeMap<Nullifier, BTreeSet<InPoint>>,
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

#[derive(Debug, Clone)]
pub enum BundleLoadError {
    /// The action at the specified index failed to decrypt with
    /// the provided IVK.
    ActionDecryptionFailed(usize),
    /// The wallet did not contain the full viewing key corresponding
    /// to the incoming viewing key that successfullly decrypted a
    /// note.
    FvkNotFound(IncomingViewingKey),
    /// An action index identified as potentially spending one of our
    /// notes is not a valid action index for the bundle.
    InvalidActionIndex(usize),
}

/// A struct used to return metadata about how a bundle was determined
/// to be involved with the wallet.
#[derive(Debug, Clone)]
pub struct BundleWalletInvolvement {
    receive_action_metadata: BTreeMap<usize, IncomingViewingKey>,
    spend_action_metadata: Vec<usize>,
}

impl BundleWalletInvolvement {
    pub fn new() -> Self {
        BundleWalletInvolvement {
            receive_action_metadata: BTreeMap::new(),
            spend_action_metadata: Vec::new(),
        }
    }
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
            mined_notes: BTreeMap::new(),
            potential_spends: BTreeMap::new(),
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
        self.mined_notes = BTreeMap::new();
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
            .map_or(false, |last_height| block_height != last_height + 1)
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

            self.mined_notes.retain(|_, v| to_retain.contains(&v.txid));
            // nullifier and received note data are retained, because these values are stable
            // once we've observed a note for the first time. The block height at which we
            // observed the note is set to `None` as the transaction will no longer have been
            // observed as having been mined.
            for (txid, n) in self.wallet_received_notes.iter_mut() {
                // Erase block height and commitment tree information for any received
                // notes that have been un-mined by the rewind.
                if !to_retain.contains(txid) {
                    n.tx_height = None;
                    for dnote in n.decrypted_notes.values_mut() {
                        dnote.position = None;
                    }
                }
            }
            self.last_observed = Some(last_observed);
            self.last_checkpoint = if checkpoint_count > blocks_to_rewind as usize {
                Some(checkpoint_height - blocks_to_rewind)
            } else {
                // checkpoint_count == blocks_to_rewind
                None
            };

            Ok(blocks_to_rewind)
        } else {
            Err(RewindError::InsufficientCheckpoints(0))
        }
    }

    /// Add note data for those notes that are decryptable with one of this wallet's
    /// incoming viewing keys to the wallet, and return a map from each decrypted
    /// action's index to the incoming vieing key that successfully decrypted that
    /// action.
    pub fn add_notes_from_bundle(
        &mut self,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
    ) -> BundleWalletInvolvement {
        let mut involvement = BundleWalletInvolvement::new();
        // If we recognize any of our notes as being consumed as inputs to actions
        // in this bundle, record them as potential spends.
        involvement.spend_action_metadata = self.add_potential_spends(txid, bundle);

        let keys = self
            .key_store
            .viewing_keys
            .keys()
            .cloned()
            .collect::<Vec<_>>();

        for (action_idx, ivk, note, recipient, memo) in bundle.decrypt_outputs_for_keys(&keys) {
            assert!(self.add_decrypted_note(
                None,
                txid,
                action_idx,
                ivk.clone(),
                note,
                recipient,
                memo
            ));
            involvement.receive_action_metadata.insert(action_idx, ivk);
        }

        involvement
    }

    /// Add note data to the wallet by attempting to
    /// incoming viewing keys to the wallet, and return a map from incoming viewing
    /// key to the vector of action indices that that key decrypts.
    pub fn load_bundle(
        &mut self,
        tx_height: Option<BlockHeight>,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
        hints: BTreeMap<usize, &IncomingViewingKey>,
        potential_spend_idxs: &[u32],
    ) -> Result<(), BundleLoadError> {
        for action_idx in potential_spend_idxs {
            let action_idx: usize = (*action_idx).try_into().unwrap();
            if action_idx < bundle.actions().len() {
                self.add_potential_spend(
                    bundle.actions()[action_idx].nullifier(),
                    InPoint {
                        txid: *txid,
                        action_idx,
                    },
                );
            } else {
                return Err(BundleLoadError::InvalidActionIndex(action_idx));
            }
        }

        for (action_idx, ivk) in hints.into_iter() {
            if let Some((note, recipient, memo)) = bundle.decrypt_output_with_key(action_idx, ivk) {
                if !self.add_decrypted_note(
                    tx_height,
                    txid,
                    action_idx,
                    ivk.clone(),
                    note,
                    recipient,
                    memo,
                ) {
                    return Err(BundleLoadError::FvkNotFound(ivk.clone()));
                }
            } else {
                return Err(BundleLoadError::ActionDecryptionFailed(action_idx));
            }
        }
        Ok(())
    }

    // Common functionality for add_notes_from_bundle and load_bundle
    #[allow(clippy::too_many_arguments)]
    fn add_decrypted_note(
        &mut self,
        tx_height: Option<BlockHeight>,
        txid: &TxId,
        action_idx: usize,
        ivk: IncomingViewingKey,
        note: Note,
        recipient: Address,
        memo: [u8; 512],
    ) -> bool {
        // Generate the nullifier for the received note and add it to the nullifiers map so
        // that we can detect when the note is later spent.
        if let Some(fvk) = self.key_store.viewing_keys.get(&ivk) {
            let outpoint = OutPoint {
                txid: *txid,
                action_idx,
            };

            // Generate the nullifier for the received note and add it to the nullifiers map so
            // that we can detect when the note is later spent.
            let nf = note.nullifier(fvk);
            self.nullifiers.insert(nf, outpoint);

            // add the decrypted note data to the wallet
            let note_data = DecryptedNote {
                note,
                memo,
                position: None,
            };
            self.wallet_received_notes
                .entry(*txid)
                .or_insert_with(|| TxNotes {
                    tx_height,
                    decrypted_notes: BTreeMap::new(),
                })
                .decrypted_notes
                .insert(action_idx, note_data);

            self.nullifiers.insert(nf, outpoint);

            // add the association between the address and the IVK used
            // to decrypt the note
            self.key_store.add_raw_address(recipient, ivk.clone());

            true
        } else {
            false
        }
    }

    /// For each Orchard action in the provided bundle, if the wallet
    /// is tracking a note corresponding to the action's revealed nullifer,
    /// mark that note as potentially spent.
    pub fn add_potential_spends(
        &mut self,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
    ) -> Vec<usize> {
        // Check for spends of our notes by matching against the nullifiers
        // we're tracking, and when we detect one, associate the current
        // txid and action as spending the note.
        let mut spend_action_idxs = vec![];
        for (action_idx, action) in bundle.actions().iter().enumerate() {
            let nf = action.nullifier();
            // If a nullifier corresponds to one of our notes, add its inpoint as a
            // potential spend (the transaction may not end up being mined).
            if self.nullifiers.contains_key(nf) {
                self.add_potential_spend(
                    nf,
                    InPoint {
                        txid: *txid,
                        action_idx,
                    },
                );
                spend_action_idxs.push(action_idx);
            }
        }
        spend_action_idxs
    }

    fn add_potential_spend(&mut self, nf: &Nullifier, inpoint: InPoint) {
        self.potential_spends
            .entry(*nf)
            .or_insert_with(BTreeSet::new)
            .insert(inpoint);
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

        let mut my_notes_for_tx = self.wallet_received_notes.get_mut(txid);
        for (action_idx, action) in bundle.actions().iter().enumerate() {
            // append the note commitment for each action to the witness tree
            if !self
                .witness_tree
                .append(&MerkleHashOrchard::from_cmx(action.cmx()))
            {
                return Err(WalletError::NoteCommitmentTreeFull);
            }

            // for notes that are ours, witness the current state of the tree
            if let Some(dnote) = my_notes_for_tx
                .as_mut()
                .and_then(|n| n.decrypted_notes.get_mut(&action_idx))
            {
                let (pos, cmx) = self.witness_tree.witness().expect("tree is not empty");
                assert_eq!(cmx, MerkleHashOrchard::from_cmx(action.cmx()));
                dnote.position = Some(pos);
            }

            // For nullifiers that are ours that we detect as spent by this action,
            // we will record that input as being mined.
            if let Some(outpoint) = self.nullifiers.get(action.nullifier()) {
                self.mined_notes.insert(
                    *outpoint,
                    InPoint {
                        txid: *txid,
                        action_idx,
                    },
                );
            }
        }

        Ok(())
    }

    /// Returns whether the transaction contains any notes either sent to or spent by this
    /// wallet.
    pub fn tx_involves_my_notes(&self, txid: &TxId) -> bool {
        self.wallet_received_notes.contains_key(txid)
            || self.nullifiers.values().any(|v| v.txid == *txid)
    }

    pub fn get_filtered_notes(
        &self,
        ivk: Option<&IncomingViewingKey>,
        ignore_mined: bool,
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
                                if (ignore_mined && self.mined_notes.contains_key(&outpoint))
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

    pub fn note_commitment_tree_root(&self) -> MerkleHashOrchard {
        self.witness_tree.root()
    }

    /// Fetches the information necessary to spend the note at the given `OutPoint`,
    /// relative to the current root known to the wallet of the Orchard commitment tree.
    ///
    /// Returns `None` if the `OutPoint` is not known to the wallet, or the Orchard bundle
    /// containing the note has not been passed to `Wallet::append_bundle_commitments`.
    pub fn get_spend_info(&self, outpoint: OutPoint) -> Option<OrchardSpendInfo> {
        // TODO: Take `confirmations` parameter and obtain the Merkle path to the root at
        // that checkpoint, not the latest root.
        self.wallet_received_notes
            .get(&outpoint.txid)
            .and_then(|tx_notes| tx_notes.decrypted_notes.get(&outpoint.action_idx))
            .and_then(|dnote| {
                self.key_store
                    .ivk_for_address(&dnote.note.recipient())
                    .and_then(|ivk| self.key_store.viewing_keys.get(ivk))
                    .zip(dnote.position)
                    .map(|(fvk, position)| {
                        let path = self
                            .witness_tree
                            .authentication_path(
                                position,
                                &MerkleHashOrchard::from_cmx(&dnote.note.commitment().into()),
                            )
                            .expect("wallet always has paths to positioned notes");
                        OrchardSpendInfo::from_parts(
                            fvk.clone(),
                            dnote.note,
                            MerklePath::from_parts(
                                u64::from(position).try_into().unwrap(),
                                path.try_into().unwrap(),
                            ),
                        )
                    })
            })
    }
}

//
// FFI
//

/// A type alias for a pointer to a C++ value that is the target of
/// a mutating action by a callback across the FFI
pub type FFICallbackReceiver = std::ptr::NonNull<libc::c_void>;

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
pub extern "C" fn orchard_wallet_reset(wallet: *mut Wallet) {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    wallet.reset();
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
    blocks_rewound: *mut u32,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let blocks_rewound =
        unsafe { blocks_rewound.as_mut() }.expect("Return value pointer may not be null.");
    match wallet.rewind(to_height) {
        Ok(rewound) => {
            *blocks_rewound = rewound;
            true
        }
        Err(e) => {
            error!(
                "Unable to rewind the wallet to height {:?}: {:?}",
                to_height, e
            );
            false
        }
    }
}

/// A type used to pass action decryption metadata across the FFI boundary.
/// This must have the same representation as `struct RawOrchardActionIVK`
/// in `rust/include/rust/orchard/wallet.h`. action_idx is
#[repr(C)]
pub struct FFIActionIvk {
    action_idx: u64,
    ivk_ptr: *mut IncomingViewingKey,
}

/// A C++-allocated function pointer that can pass a FFIActionIVK value
/// to a C++ callback receiver.
pub type ActionIvkPushCb =
    unsafe extern "C" fn(obj: Option<FFICallbackReceiver>, value: FFIActionIvk);

/// A C++-allocated function pointer that can pass a FFIActionIVK value
/// to a C++ callback receiver.
pub type SpendIndexPushCb = unsafe extern "C" fn(obj: Option<FFICallbackReceiver>, value: u32);

#[no_mangle]
pub extern "C" fn orchard_wallet_add_notes_from_bundle(
    wallet: *mut Wallet,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
    cb_receiver: Option<FFICallbackReceiver>,
    action_ivk_push_cb: Option<ActionIvkPushCb>,
    spend_idx_push_cb: Option<SpendIndexPushCb>,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        let added = wallet.add_notes_from_bundle(&txid, bundle);
        let mut involved = false;
        for (action_idx, ivk) in added.receive_action_metadata.into_iter() {
            let action_ivk = FFIActionIvk {
                action_idx: action_idx.try_into().unwrap(),
                ivk_ptr: Box::into_raw(Box::new(ivk.clone())),
            };
            unsafe { (action_ivk_push_cb.unwrap())(cb_receiver, action_ivk) };
            involved = true;
        }
        for action_idx in added.spend_action_metadata {
            unsafe { (spend_idx_push_cb.unwrap())(cb_receiver, action_idx.try_into().unwrap()) };
            involved = true;
        }
        involved
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_load_bundle(
    wallet: *mut Wallet,
    block_height: *const u32,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
    hints: *const FFIActionIvk,
    hints_len: usize,
    potential_spend_idxs: *const u32,
    potential_spend_idxs_len: usize,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let block_height = unsafe { block_height.as_ref() }.map(|h| BlockHeight::from(*h));
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));
    let bundle = unsafe { bundle.as_ref() }.expect("bundle pointer may not be null.");

    let hints_data = unsafe { slice::from_raw_parts(hints, hints_len) };
    let potential_spend_idxs =
        unsafe { slice::from_raw_parts(potential_spend_idxs, potential_spend_idxs_len) };

    let mut hints = BTreeMap::new();
    for action_ivk in hints_data {
        hints.insert(
            action_ivk.action_idx.try_into().unwrap(),
            unsafe { action_ivk.ivk_ptr.as_ref() }.expect("ivk pointer may not be null"),
        );
    }

    match wallet.load_bundle(block_height, &txid, bundle, hints, potential_spend_idxs) {
        Ok(_) => true,
        Err(e) => {
            error!("Failed to restore decrypted notes to wallet: {:?}", e);
            false
        }
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
pub extern "C" fn orchard_wallet_commitment_tree_root(
    wallet: *const Wallet,
    root_ret: *mut [u8; 32],
) {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null");
    let root_ret = unsafe { root_ret.as_mut() }.expect("Cannot return to the null pointer.");

    *root_ret = wallet.note_commitment_tree_root().to_bytes();
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
pub extern "C" fn orchard_wallet_get_spending_key_for_address(
    wallet: *const Wallet,
    addr: *const Address,
) -> *mut SpendingKey {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let addr = unsafe { addr.as_ref() }.expect("Address may not be null.");

    wallet
        .key_store
        .ivk_for_address(addr)
        .and_then(|ivk| wallet.key_store.spending_key_for_ivk(ivk))
        .map(|sk| Box::into_raw(Box::new(*sk)))
        .unwrap_or(std::ptr::null_mut())
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
pub extern "C" fn orchard_wallet_tx_involves_my_notes(
    wallet: *const Wallet,
    txid: *const [c_uchar; 32],
) -> bool {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));

    wallet.tx_involves_my_notes(&txid)
}

/// A type used to pass note metadata across the FFI boundary.
/// This must have the same representation as `struct RawOrchardNoteMetadata`
/// in `rust/include/rust/orchard/wallet.h`.
#[repr(C)]
pub struct FFINoteMetadata {
    txid: [u8; 32],
    action_idx: u32,
    recipient: *mut Address,
    note_value: i64,
    memo: [u8; 512],
}

/// A C++-allocated function pointer that can push a FFINoteMetadata value
/// onto a C++ vector.
pub type NotePushCb = unsafe extern "C" fn(obj: Option<FFICallbackReceiver>, meta: FFINoteMetadata);

#[no_mangle]
pub extern "C" fn orchard_wallet_get_filtered_notes(
    wallet: *const Wallet,
    ivk: *const IncomingViewingKey,
    ignore_mined: bool,
    require_spending_key: bool,
    result: Option<FFICallbackReceiver>,
    push_cb: Option<NotePushCb>,
) {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let ivk = unsafe { ivk.as_ref() };

    for (outpoint, dnote) in wallet.get_filtered_notes(ivk, ignore_mined, require_spending_key) {
        let metadata = FFINoteMetadata {
            txid: *outpoint.txid.as_ref(),
            action_idx: outpoint.action_idx as u32,
            recipient: Box::into_raw(Box::new(dnote.note.recipient())),
            note_value: dnote.note.value().inner() as i64,
            memo: dnote.memo,
        };
        unsafe { (push_cb.unwrap())(result, metadata) };
    }
}

pub type PushTxId = unsafe extern "C" fn(obj: Option<FFICallbackReceiver>, txid: *const [u8; 32]);

#[no_mangle]
pub extern "C" fn orchard_wallet_get_potential_spends(
    wallet: *const Wallet,
    txid: *const [c_uchar; 32],
    action_idx: u32,
    result: Option<FFICallbackReceiver>,
    push_cb: Option<PushTxId>,
) {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));

    if let Some(inpoints) = wallet
        .wallet_received_notes
        .get(&txid)
        .and_then(|txnotes| txnotes.decrypted_notes.get(&action_idx.try_into().unwrap()))
        .and_then(|dnote| wallet.key_store.get_nullifier(&dnote.note))
        .and_then(|nf| wallet.potential_spends.get(&nf))
    {
        for inpoint in inpoints {
            unsafe { (push_cb.unwrap())(result, inpoint.txid.as_ref()) };
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_get_spend_info(
    wallet: *const Wallet,
    txid: *const [c_uchar; 32],
    action_idx: usize,
) -> *mut OrchardSpendInfo {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));

    let outpoint = OutPoint { txid, action_idx };

    if let Some(ret) = wallet.get_spend_info(outpoint) {
        Box::into_raw(Box::new(ret))
    } else {
        tracing::error!(
            "Requested note in action {} of transaction {} wasn't in the wallet",
            outpoint.action_idx,
            outpoint.txid
        );
        ptr::null_mut()
    }
}
