use bridgetree::{self, BridgeTree};
use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};
use incrementalmerkletree::Position;
use libc::c_uchar;
use std::collections::{BTreeMap, BTreeSet};
use std::convert::TryInto;
use std::io;
use std::ptr;
use std::slice;
use tracing::error;

use zcash_encoding::{Optional, Vector};
use zcash_primitives::{
    consensus::BlockHeight,
    merkle_tree::{read_position, write_position},
    transaction::{components::Amount, TxId},
};

use orchard::{
    bundle::Authorized,
    keys::{FullViewingKey, IncomingViewingKey, OutgoingViewingKey, Scope, SpendingKey},
    note::Nullifier,
    tree::{MerkleHashOrchard, MerklePath},
    Address, Bundle, Note,
};

use crate::{
    builder_ffi::OrchardSpendInfo,
    incremental_merkle_tree::{read_tree, write_tree},
    streams_ffi::{CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb},
    wallet_scanner::{
        DecryptedNote as BatchDecryptedNote, OrchardDecryptedOutputs,
        OrchardPreparedIncomingViewingKeys,
    },
    zcashd_orchard::OrderedAddress,
};

pub const MAX_CHECKPOINTS: usize = 100;

/// A data structure tracking the last transaction whose notes
/// have been added to the wallet's note commitment tree.
#[derive(Debug, Clone)]
pub struct LastObserved {
    block_height: BlockHeight,
    block_tx_idx: Option<usize>,
}

/// A pointer to a particular action in an Orchard transaction output.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct OutPoint {
    txid: TxId,
    action_idx: usize,
}

/// A pointer to a previous output being spent in an Orchard action.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct InPoint {
    txid: TxId,
    action_idx: usize,
}

#[derive(Debug, Clone)]
pub struct DecryptedNote {
    note: Note,
    memo: [u8; 512],
}

/// A data structure tracking the note data that was decrypted from a single transaction.
#[derive(Debug, Clone)]
pub struct TxNotes {
    /// A map from the index of the Orchard action from which this note
    /// was decrypted to the decrypted note value.
    decrypted_notes: BTreeMap<usize, DecryptedNote>,
}

/// A data structure holding chain position information for a single transaction.
#[derive(Clone, Debug)]
struct NotePositions {
    /// The height of the block containing the transaction.
    tx_height: BlockHeight,
    /// A map from the index of an Orchard action tracked by this wallet, to the position
    /// of the output note's commitment within the global Merkle tree.
    note_positions: BTreeMap<usize, Position>,
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
        let external_ivk = fvk.to_ivk(Scope::External);
        let internal_ivk = fvk.to_ivk(Scope::Internal);
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

    /// Returns prepared incoming viewing keys for batched trial decryption, or `None` if
    /// there aren't any.
    pub fn prepare_ivks(&self) -> Option<OrchardPreparedIncomingViewingKeys> {
        (!self.viewing_keys.is_empty())
            .then(|| OrchardPreparedIncomingViewingKeys::new(self.viewing_keys.keys()))
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
    /// The in-memory index from txid to note positions from the associated transaction.
    /// This map should always have a subset of the keys in `wallet_received_notes`.
    wallet_note_positions: BTreeMap<TxId, NotePositions>,
    /// The in-memory index from nullifier to the outpoint of the note from which that
    /// nullifier was derived.
    nullifiers: BTreeMap<Nullifier, OutPoint>,
    /// The incremental Merkle tree used to track note commitments and witnesses for notes
    /// belonging to the wallet.
    // TODO: Replace this with an `orchard` crate constant (they happen to be the same).
    commitment_tree: BridgeTree<MerkleHashOrchard, u32, { sapling::NOTE_COMMITMENT_TREE_DEPTH }>,
    /// The block height at which the last checkpoint was created, if any.
    last_checkpoint: Option<BlockHeight>,
    /// The block height and transaction index of the note most recently added to
    /// `commitment_tree`
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

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub enum WalletError {
    OutOfOrder(LastObserved, BlockHeight, usize),
    NoteCommitmentTreeFull,
}

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub enum RewindError {
    /// The note commitment tree does not contain enough checkpoints to
    /// rewind to the requested height. The number of blocks that
    /// it is possible to rewind is returned as the payload of
    /// this error.
    InsufficientCheckpoints(usize),
}

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub enum BundleLoadError {
    /// The action at the specified index failed to decrypt with
    /// the provided IVK.
    ActionDecryptionFailed(usize),
    /// The wallet did not contain the full viewing key corresponding
    /// to the incoming viewing key that successfully decrypted a
    /// note.
    FvkNotFound(IncomingViewingKey),
    /// An action index identified as potentially spending one of our
    /// notes is not a valid action index for the bundle.
    InvalidActionIndex(usize),
}

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub enum SpendRetrievalError {
    DecryptedNoteNotFound(OutPoint),
    NoIvkForRecipient(Address),
    FvkNotFound(IncomingViewingKey),
    NoteNotPositioned(OutPoint),
    WitnessNotAvailableAtDepth(usize),
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
            wallet_note_positions: BTreeMap::new(),
            nullifiers: BTreeMap::new(),
            commitment_tree: BridgeTree::new(MAX_CHECKPOINTS),
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
        self.wallet_note_positions.clear();
        self.commitment_tree = BridgeTree::new(MAX_CHECKPOINTS);
        self.last_checkpoint = None;
        self.last_observed = None;
        self.mined_notes = BTreeMap::new();
    }

    /// Checkpoints the note commitment tree. This returns `false` and leaves the note
    /// commitment tree unmodified if the block height does not immediately succeed
    /// the last checkpointed block height (unless the note commitment tree is empty,
    /// in which case it unconditionally succeeds). This must be called exactly once
    /// per block.
    #[tracing::instrument(level = "trace", skip(self))]
    pub fn checkpoint(&mut self, block_height: BlockHeight) -> bool {
        // checkpoints must be in order of sequential block height and every
        // block must be checkpointed
        if let Some(last_height) = self.last_checkpoint {
            let expected_height = last_height + 1;
            if block_height != expected_height {
                tracing::error!(
                    "Expected checkpoint height {}, given {}",
                    expected_height,
                    block_height
                );
                return false;
            }
        }

        self.commitment_tree.checkpoint(block_height.into());
        self.last_checkpoint = Some(block_height);
        true
    }

    /// Returns the last checkpoint if any. If no checkpoint exists, the wallet has not
    /// yet observed any blocks.
    pub fn last_checkpoint(&self) -> Option<BlockHeight> {
        self.last_checkpoint
    }

    /// Rewinds the note commitment tree to the given height, removes notes and spentness
    /// information for transactions mined in the removed blocks, and returns the height to which
    /// the tree has been rewound if successful. Returns  `RewindError` if not enough checkpoints
    /// exist to execute the full rewind requested and the wallet has witness information that
    /// would be invalidated by the rewind. If the requested height is greater than or equal to the
    /// height of the latest checkpoint, this returns a successful result containing the height of
    /// the last checkpoint.
    ///
    /// In the case that no checkpoints exist but the note commitment tree also records no witness
    /// information, we allow the wallet to continue to rewind, under the assumption that the state
    /// of the note commitment tree will be overwritten prior to the next append.
    #[tracing::instrument(level = "trace", skip(self))]
    pub fn rewind(&mut self, to_height: BlockHeight) -> Result<BlockHeight, RewindError> {
        if let Some(checkpoint_height) = self.last_checkpoint {
            if to_height >= checkpoint_height {
                tracing::trace!("Last checkpoint is before the rewind height, nothing to do.");
                return Ok(checkpoint_height);
            }

            tracing::trace!("Rewinding note commitment tree");
            let blocks_to_rewind = <u32>::from(checkpoint_height) - <u32>::from(to_height);
            let checkpoint_count = self.commitment_tree.checkpoints().len();
            for _ in 0..blocks_to_rewind {
                // If the rewind fails, we have no more checkpoints. This is fine in the
                // case that we have a recently-initialized tree, so long as we have no
                // witnessed indices. In the case that we have any witnessed notes, we
                // have hit the maximum rewind limit, and this is an error.
                if !self.commitment_tree.rewind() {
                    assert!(self.commitment_tree.checkpoints().is_empty());
                    if !self.commitment_tree.marked_indices().is_empty() {
                        return Err(RewindError::InsufficientCheckpoints(checkpoint_count));
                    }
                }
            }

            // retain notes that correspond to transactions that are not "un-mined" after
            // the rewind
            let to_retain: BTreeSet<_> = self
                .wallet_note_positions
                .iter()
                .filter_map(|(txid, n)| {
                    if n.tx_height <= to_height {
                        Some(*txid)
                    } else {
                        None
                    }
                })
                .collect();
            tracing::trace!("Retaining notes in transactions {:?}", to_retain);

            self.mined_notes.retain(|_, v| to_retain.contains(&v.txid));

            // nullifier and received note data are retained, because these values are stable
            // once we've observed a note for the first time. The block height at which we
            // observed the note is removed along with the note positions, because the
            // transaction will no longer have been observed as having been mined.
            self.wallet_note_positions
                .retain(|txid, _| to_retain.contains(txid));

            // reset our last observed height to ensure that notes added in the future are
            // from a new block
            self.last_observed = Some(LastObserved {
                block_height: to_height,
                block_tx_idx: None,
            });

            self.last_checkpoint = if checkpoint_count > blocks_to_rewind as usize {
                Some(to_height)
            } else {
                // checkpoint_count <= blocks_to_rewind
                None
            };

            Ok(to_height)
        } else if self.commitment_tree.marked_indices().is_empty() {
            tracing::trace!("No witnessed notes in tree, allowing rewind without checkpoints");

            // If we have no witnessed notes, it's okay to keep "rewinding" even though
            // we have no checkpoints. We then allow last_observed to assume the height
            // to which we have reset the tree state.
            self.last_observed = Some(LastObserved {
                block_height: to_height,
                block_tx_idx: None,
            });

            Ok(to_height)
        } else {
            Err(RewindError::InsufficientCheckpoints(0))
        }
    }

    /// Add note data for those notes that are decryptable with one of this wallet's
    /// incoming viewing keys to the wallet, and return a data structure that describes
    /// the actions that are involved with this wallet, either spending notes belonging
    /// to this wallet or creating new notes owned by this wallet.
    #[tracing::instrument(level = "trace", skip(self, decrypted_outputs))]
    pub fn add_notes_from_bundle(
        &mut self,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
        decrypted_outputs: Option<&OrchardDecryptedOutputs>,
    ) -> BundleWalletInvolvement {
        let mut involvement = BundleWalletInvolvement::new();
        // If we recognize any of our notes as being consumed as inputs to actions
        // in this bundle, record them as potential spends.
        involvement.spend_action_metadata = self.add_potential_spends(txid, bundle);

        if let Some(decrypted) = decrypted_outputs {
            for ((output_txid, action_idx), decrypted_note) in &decrypted.inner {
                assert_eq!(txid, output_txid);

                let BatchDecryptedNote {
                    ivk_tag,
                    recipient,
                    note,
                    memo,
                } = decrypted_note;
                let ivk = IncomingViewingKey::from_bytes(ivk_tag).unwrap();

                assert!(self.add_decrypted_note(
                    txid,
                    *action_idx,
                    ivk.clone(),
                    *note,
                    *recipient,
                    *memo
                ));
                involvement.receive_action_metadata.insert(*action_idx, ivk);
            }
        } else {
            let keys = self
                .key_store
                .viewing_keys
                .keys()
                .cloned()
                .collect::<Vec<_>>();

            for (action_idx, ivk, note, recipient, memo) in bundle.decrypt_outputs_with_keys(&keys)
            {
                assert!(self.add_decrypted_note(
                    txid,
                    action_idx,
                    ivk.clone(),
                    note,
                    recipient,
                    memo
                ));
                involvement.receive_action_metadata.insert(action_idx, ivk);
            }
        }

        involvement
    }

    /// Restore note and potential spend data from a bundle using the provided
    /// metadata.
    ///
    /// - `txid`: The ID for the transaction from which the provided bundle was
    ///   extracted.
    /// - `bundle`: the bundle to decrypt notes from
    /// - `hints`: a map from action index to the incoming viewing key that decrypts
    ///   that action. If the IVK does not decrypt the action, or if it is not
    ///   associated with a FVK in this wallet, `load_bundle` will return an error.
    /// - `potential_spend_idxs`: a list of action indices that were previously
    ///   detected as spending our notes. If an index is out of range, `load_bundle`
    ///   will return an error.
    #[tracing::instrument(level = "trace", skip(self))]
    pub fn load_bundle(
        &mut self,
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
                if !self.add_decrypted_note(txid, action_idx, ivk.clone(), note, recipient, memo) {
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
    #[tracing::instrument(level = "trace", skip(self))]
    fn add_decrypted_note(
        &mut self,
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
            tracing::trace!("Adding decrypted note to the wallet");
            let outpoint = OutPoint {
                txid: *txid,
                action_idx,
            };

            // Generate the nullifier for the received note and add it to the nullifiers map so
            // that we can detect when the note is later spent.
            let nf = note.nullifier(fvk);
            self.nullifiers.insert(nf, outpoint);

            // add the decrypted note data to the wallet
            let note_data = DecryptedNote { note, memo };
            self.wallet_received_notes
                .entry(*txid)
                .or_insert_with(|| TxNotes {
                    decrypted_notes: BTreeMap::new(),
                })
                .decrypted_notes
                .insert(action_idx, note_data);

            // add the association between the address and the IVK used
            // to decrypt the note
            self.key_store.add_raw_address(recipient, ivk.clone());

            true
        } else {
            tracing::trace!("Can't add decrypted note to the wallet, missing FVK");
            false
        }
    }

    /// For each Orchard action in the provided bundle, if the wallet
    /// is tracking a note corresponding to the action's revealed nullifer,
    /// mark that note as potentially spent.
    #[tracing::instrument(level = "trace", skip(self))]
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
        tracing::trace!(
            "Adding potential spend of nullifier {:?} in {:?}",
            nf,
            inpoint
        );
        self.potential_spends
            .entry(*nf)
            .or_default()
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
    #[tracing::instrument(level = "trace", skip(self))]
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
        let my_notes_for_tx = self.wallet_received_notes.get(txid);
        if my_notes_for_tx.is_some() {
            tracing::trace!("Tx is ours, marking as mined");
            assert!(self
                .wallet_note_positions
                .insert(
                    *txid,
                    NotePositions {
                        tx_height: block_height,
                        note_positions: BTreeMap::default(),
                    },
                )
                .is_none());
        }

        for (action_idx, action) in bundle.actions().iter().enumerate() {
            // append the note commitment for each action to the note commitment tree
            if !self
                .commitment_tree
                .append(MerkleHashOrchard::from_cmx(action.cmx()))
            {
                return Err(WalletError::NoteCommitmentTreeFull);
            }

            // for notes that are ours, mark the current state of the tree
            if my_notes_for_tx
                .as_ref()
                .and_then(|n| n.decrypted_notes.get(&action_idx))
                .is_some()
            {
                tracing::trace!("Witnessing Orchard note ({}, {})", txid, action_idx);
                let pos = self.commitment_tree.mark().expect("tree is not empty");
                assert!(self
                    .wallet_note_positions
                    .get_mut(txid)
                    .expect("We created this above")
                    .note_positions
                    .insert(action_idx, pos)
                    .is_none());
            }

            // For nullifiers that are ours that we detect as spent by this action,
            // we will record that input as being mined.
            if let Some(outpoint) = self.nullifiers.get(action.nullifier()) {
                assert!(self
                    .mined_notes
                    .insert(
                        *outpoint,
                        InPoint {
                            txid: *txid,
                            action_idx,
                        },
                    )
                    .is_none());
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

    #[tracing::instrument(level = "trace", skip(self))]
    pub fn get_filtered_notes(
        &self,
        ivk: Option<&IncomingViewingKey>,
        ignore_mined: bool,
        require_spending_key: bool,
    ) -> Vec<(OutPoint, DecryptedNote)> {
        tracing::trace!("Filtering notes");
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
                                    tracing::trace!("Selected note at {:?}", outpoint);
                                    Some((outpoint, (*dnote).clone()))
                                }
                            })
                    })
            })
            .collect()
    }

    /// Returns the root of the Orchard note commitment tree, as of the specified checkpoint
    /// depth. A depth of 0 corresponds to the chain tip.
    pub fn note_commitment_tree_root(&self, checkpoint_depth: usize) -> Option<MerkleHashOrchard> {
        self.commitment_tree.root(checkpoint_depth)
    }

    /// Fetches the information necessary to spend the note at the given `OutPoint`,
    /// relative to the specified root of the Orchard note commitment tree.
    ///
    /// Returns `None` if the `OutPoint` is not known to the wallet, or the Orchard bundle
    /// containing the note had not been passed to `Wallet::append_bundle_commitments` at
    /// the specified checkpoint depth.
    #[tracing::instrument(level = "trace", skip(self))]
    pub fn get_spend_info(
        &self,
        outpoint: OutPoint,
        checkpoint_depth: usize,
    ) -> Result<OrchardSpendInfo, SpendRetrievalError> {
        tracing::trace!("Searching for {:?}", outpoint);
        let dnote = self
            .wallet_received_notes
            .get(&outpoint.txid)
            .and_then(|tx_notes| tx_notes.decrypted_notes.get(&outpoint.action_idx))
            .ok_or(SpendRetrievalError::DecryptedNoteNotFound(outpoint))?;
        tracing::trace!("Found decrypted note for outpoint: {:?}", dnote);

        let fvk = self
            .key_store
            .ivk_for_address(&dnote.note.recipient())
            .ok_or_else(|| SpendRetrievalError::NoIvkForRecipient(dnote.note.recipient()))
            .and_then(|ivk| {
                self.key_store
                    .viewing_keys
                    .get(ivk)
                    .ok_or_else(|| SpendRetrievalError::FvkNotFound(ivk.clone()))
            })?;
        tracing::trace!("Found FVK for note");

        let position = self
            .wallet_note_positions
            .get(&outpoint.txid)
            .and_then(|tx_notes| tx_notes.note_positions.get(&outpoint.action_idx))
            .ok_or(SpendRetrievalError::NoteNotPositioned(outpoint))?;
        tracing::trace!("Found position for note: {:?}", position);

        assert_eq!(
            self.commitment_tree
                .get_marked_leaf(*position)
                .expect("This note has been marked as one of ours."),
            &MerkleHashOrchard::from_cmx(&dnote.note.commitment().into()),
        );

        let path = self
            .commitment_tree
            .witness(*position, checkpoint_depth)
            .map_err(|_| SpendRetrievalError::WitnessNotAvailableAtDepth(checkpoint_depth))?;

        Ok(OrchardSpendInfo::from_parts(
            fvk.clone(),
            dnote.note,
            MerklePath::from_parts(
                u64::from(*position).try_into().unwrap(),
                path.try_into().unwrap(),
            ),
        ))
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
pub extern "C" fn orchard_wallet_checkpoint(
    wallet: *mut Wallet,
    block_height: BlockHeight,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    wallet.checkpoint(block_height)
}

#[no_mangle]
pub extern "C" fn orchard_wallet_get_last_checkpoint(
    wallet: *const Wallet,
    block_height_ret: *mut u32,
) -> bool {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null");
    let block_height_ret =
        unsafe { block_height_ret.as_mut() }.expect("Block height return pointer may not be null");
    if let Some(height) = wallet.last_checkpoint() {
        *block_height_ret = height.into();
        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_rewind(
    wallet: *mut Wallet,
    to_height: BlockHeight,
    result_height: *mut BlockHeight,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let result_height =
        unsafe { result_height.as_mut() }.expect("Return value pointer may not be null.");
    match wallet.rewind(to_height) {
        Ok(result) => {
            *result_height = result;
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
    decrypted_outputs: *const OrchardDecryptedOutputs,
    cb_receiver: Option<FFICallbackReceiver>,
    action_ivk_push_cb: Option<ActionIvkPushCb>,
    spend_idx_push_cb: Option<SpendIndexPushCb>,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));
    let decrypted_outputs = unsafe { decrypted_outputs.as_ref() };
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        let added = wallet.add_notes_from_bundle(&txid, bundle, decrypted_outputs);
        let involved =
            !(added.receive_action_metadata.is_empty() && added.spend_action_metadata.is_empty());
        for (action_idx, ivk) in added.receive_action_metadata.into_iter() {
            let action_ivk = FFIActionIvk {
                action_idx: action_idx.try_into().unwrap(),
                ivk_ptr: Box::into_raw(Box::new(ivk.clone())),
            };
            unsafe { (action_ivk_push_cb.unwrap())(cb_receiver, action_ivk) };
        }
        for action_idx in added.spend_action_metadata {
            unsafe { (spend_idx_push_cb.unwrap())(cb_receiver, action_idx.try_into().unwrap()) };
        }
        involved
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_load_bundle(
    wallet: *mut Wallet,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
    hints: *const FFIActionIvk,
    hints_len: usize,
    potential_spend_idxs: *const u32,
    potential_spend_idxs_len: usize,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null");
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

    match wallet.load_bundle(&txid, bundle, hints, potential_spend_idxs) {
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
    checkpoint_depth: usize,
    root_ret: *mut [u8; 32],
) -> bool {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null");
    let root_ret = unsafe { root_ret.as_mut() }.expect("Cannot return to the null pointer.");

    // there is always a valid note commitment tree root at depth 0
    // (it may be the empty root)
    if let Some(root) = wallet.note_commitment_tree_root(checkpoint_depth) {
        *root_ret = root.to_bytes();
        true
    } else {
        false
    }
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
pub extern "C" fn orchard_wallet_prepare_ivks(
    wallet: *const Wallet,
) -> *mut OrchardPreparedIncomingViewingKeys {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");

    wallet
        .key_store
        .prepare_ivks()
        .map(|ivks| Box::into_raw(Box::new(ivks)))
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

/// A type used to pass decrypted spend information across the FFI boundary.
/// This must have the same representation as `struct RawOrchardSpendData`
/// in `rust/include/rust/orchard/wallet.h`.
#[repr(C)]
pub struct FFIActionSpend {
    spend_action_idx: u32,
    outpoint_txid: [u8; 32],
    outpoint_action_idx: u32,
    received_at: *mut Address,
    value: i64,
}

/// A type used to pass decrypted output information across the FFI boundary.
/// This must have the same representation as `struct RawOrchardOutputData`
/// in `rust/include/rust/orchard/wallet.h`.
#[repr(C)]
pub struct FFIActionOutput {
    action_idx: u32,
    recipient: *mut Address,
    value: i64,
    memo: [u8; 512],
    is_outgoing: bool,
}

/// A C++-allocated function pointer that can send a FFIActionSpend value
/// to a receiver.
pub type SpendPushCB = unsafe extern "C" fn(obj: Option<FFICallbackReceiver>, data: FFIActionSpend);

/// A C++-allocated function pointer that can send a FFIActionOutput value
/// to a receiver.
pub type OutputPushCB =
    unsafe extern "C" fn(obj: Option<FFICallbackReceiver>, data: FFIActionOutput);

#[no_mangle]
pub extern "C" fn orchard_wallet_get_txdata(
    wallet: *const Wallet,
    bundle: *const Bundle<Authorized, Amount>,
    raw_ovks: *const [u8; 32],
    raw_ovks_len: usize,
    callback_receiver: Option<FFICallbackReceiver>,
    spend_push_cb: Option<SpendPushCB>,
    output_push_cb: Option<OutputPushCB>,
) -> bool {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let raw_ovks = unsafe { slice::from_raw_parts(raw_ovks, raw_ovks_len) };
    let ovks: Vec<OutgoingViewingKey> = raw_ovks
        .iter()
        .map(|k| OutgoingViewingKey::from(*k))
        .collect();
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        let ivks = wallet
            .key_store
            .viewing_keys
            .keys()
            .cloned()
            .collect::<Vec<_>>();

        let incoming: BTreeMap<usize, (Note, Address, [u8; 512])> = bundle
            .decrypt_outputs_with_keys(&ivks)
            .into_iter()
            .map(|(idx, _, note, addr, memo)| (idx, (note, addr, memo)))
            .collect();

        let outgoing: BTreeMap<usize, (Note, Address, [u8; 512])> = bundle
            .recover_outputs_with_ovks(&ovks)
            .into_iter()
            .map(|(idx, _, note, addr, memo)| (idx, (note, addr, memo)))
            .collect();

        for (idx, action) in bundle.actions().iter().enumerate() {
            let nf = action.nullifier();
            if let Some(spend) = wallet.nullifiers.get(nf).and_then(|outpoint| {
                wallet
                    .wallet_received_notes
                    .get(&outpoint.txid)
                    .and_then(|txnotes| txnotes.decrypted_notes.get(&outpoint.action_idx))
                    .map(|dnote| FFIActionSpend {
                        spend_action_idx: idx as u32,
                        outpoint_txid: *outpoint.txid.as_ref(),
                        outpoint_action_idx: outpoint.action_idx as u32,
                        received_at: Box::into_raw(Box::new(dnote.note.recipient())),
                        value: dnote.note.value().inner() as i64,
                    })
            }) {
                unsafe { (spend_push_cb.unwrap())(callback_receiver, spend) };
            }

            if let Some(((note, addr, memo), is_outgoing)) = incoming
                .get(&idx)
                .map(|n| (n, false))
                .or_else(|| outgoing.get(&idx).map(|n| (n, true)))
            {
                let output = FFIActionOutput {
                    action_idx: idx as u32,
                    recipient: Box::into_raw(Box::new(*addr)),
                    value: note.value().inner() as i64,
                    memo: *memo,
                    is_outgoing,
                };
                unsafe { (output_push_cb.unwrap())(callback_receiver, output) };
            }
        }
        true
    } else {
        false
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
pub extern "C" fn orchard_wallet_get_potential_spends_from_nullifier(
    wallet: *const Wallet,
    nullifier: *const [c_uchar; 32],
    result: Option<FFICallbackReceiver>,
    push_cb: Option<PushTxId>,
) {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let nullifier =
        Nullifier::from_bytes(unsafe { nullifier.as_ref() }.expect("nullifier may not be null."));

    if let Some(inpoints) = wallet.potential_spends.get(&nullifier.unwrap()) {
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
    checkpoint_depth: usize,
) -> *mut OrchardSpendInfo {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let txid = TxId::from_bytes(*unsafe { txid.as_ref() }.expect("txid may not be null."));
    let outpoint = OutPoint { txid, action_idx };

    match wallet.get_spend_info(outpoint, checkpoint_depth) {
        Ok(ret) => Box::into_raw(Box::new(ret)),
        Err(e) => {
            tracing::error!(
                "Error obtaining spend info for outpoint {:?}: {:?}",
                outpoint,
                e
            );
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_gc_note_commitment_tree(wallet: *mut Wallet) {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null.");
    wallet.commitment_tree.garbage_collect();
}

const NOTE_STATE_V1: u8 = 1;

#[allow(clippy::needless_borrows_for_generic_args)]
#[no_mangle]
pub extern "C" fn orchard_wallet_write_note_commitment_tree(
    wallet: *const Wallet,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");
    let mut writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());

    let write_v1 = move |mut writer: CppStreamWriter| -> io::Result<()> {
        Optional::write(&mut writer, wallet.last_checkpoint, |w, h| {
            w.write_u32::<LittleEndian>(h.into())
        })?;
        write_tree(&mut writer, &wallet.commitment_tree)?;

        // Write note positions.
        Vector::write_sized(
            &mut writer,
            wallet.wallet_note_positions.iter(),
            |mut w, (txid, tx_notes)| {
                txid.write(&mut w)?;
                w.write_u32::<LittleEndian>(tx_notes.tx_height.into())?;
                Vector::write_sized(
                    w,
                    tx_notes.note_positions.iter(),
                    |w, (action_idx, position)| {
                        w.write_u32::<LittleEndian>(*action_idx as u32)?;
                        write_position(w, *position)
                    },
                )
            },
        )?;

        Ok(())
    };

    match writer
        .write_u8(NOTE_STATE_V1)
        .and_then(|()| write_v1(writer))
    {
        Ok(()) => true,
        Err(e) => {
            error!("Failure in writing Orchard note commitment tree: {}", e);
            false
        }
    }
}

#[allow(clippy::needless_borrows_for_generic_args)]
#[no_mangle]
pub extern "C" fn orchard_wallet_load_note_commitment_tree(
    wallet: *mut Wallet,
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null.");
    let mut reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    let mut read_v1 = move |mut reader: CppStreamReader| -> io::Result<()> {
        let last_checkpoint = Optional::read(&mut reader, |r| {
            r.read_u32::<LittleEndian>().map(BlockHeight::from)
        })?;
        let commitment_tree = read_tree(&mut reader)?;

        // Read note positions.
        wallet.wallet_note_positions = Vector::read_collected(&mut reader, |mut r| {
            Ok((
                TxId::read(&mut r)?,
                NotePositions {
                    tx_height: r.read_u32::<LittleEndian>().map(BlockHeight::from)?,
                    note_positions: Vector::read_collected(r, |r| {
                        Ok((
                            r.read_u32::<LittleEndian>().map(|idx| idx as usize)?,
                            read_position(r)?,
                        ))
                    })?,
                },
            ))
        })?;

        wallet.last_checkpoint = last_checkpoint;
        wallet.commitment_tree = commitment_tree;
        Ok(())
    };

    match reader.read_u8() {
        Err(e) => {
            error!(
                "Failed to read Orchard note position serialization flag: {}",
                e
            );
            false
        }
        Ok(NOTE_STATE_V1) => match read_v1(reader) {
            Ok(_) => true,
            Err(e) => {
                error!(
                    "Failed to read Orchard note commitment or last checkpoint height: {}",
                    e
                );
                false
            }
        },
        Ok(flag) => {
            error!(
                "Unrecognized Orchard note position serialization version: {}",
                flag
            );
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_init_from_frontier(
    wallet: *mut Wallet,
    frontier: *const bridgetree::Frontier<
        MerkleHashOrchard,
        { sapling::NOTE_COMMITMENT_TREE_DEPTH },
    >,
) -> bool {
    let wallet = unsafe { wallet.as_mut() }.expect("Wallet pointer may not be null.");
    let frontier = unsafe { frontier.as_ref() }.expect("Wallet pointer may not be null.");

    if wallet.commitment_tree.checkpoints().is_empty()
        && wallet.commitment_tree.marked_indices().is_empty()
    {
        wallet.commitment_tree = frontier.value().map_or_else(
            || BridgeTree::new(MAX_CHECKPOINTS),
            |nonempty_frontier| {
                BridgeTree::from_frontier(MAX_CHECKPOINTS, nonempty_frontier.clone())
            },
        );
        true
    } else {
        // if we have any checkpoints in the tree, or if we have any witnessed notes,
        // don't allow reinitialization
        error!(
            "Invalid attempt to reinitialize note commitment tree: {} checkpoints present.",
            wallet.commitment_tree.checkpoints().len()
        );
        false
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_unspent_notes_are_spendable(wallet: *const Wallet) -> bool {
    let wallet = unsafe { wallet.as_ref() }.expect("Wallet pointer may not be null.");

    wallet
        .get_filtered_notes(None, true, true)
        .iter()
        .all(|(outpoint, _)| wallet.get_spend_info(*outpoint, 0).is_ok())
}
