use incrementalmerkletree::{bridgetree::BridgeTree, Frontier, Tree};
use libc::c_uchar;
use std::collections::{BTreeMap, HashMap, HashSet};
use std::io::{self, Read, Write};
use tracing::error;

use zcash_primitives::{
    consensus::BlockHeight,
    merkle_tree::incremental::{read_tree_v1, write_tree_v1},
    serialize::CompactSize,
    transaction::{components::Amount, TxId},
};

use orchard::{
    bundle::Authorized,
    keys::{FullViewingKey, IncomingViewingKey, SpendingKey},
    tree::MerkleCrhOrchardOutput,
    Address, Bundle, Note,
};

use super::incremental_merkle_tree_ffi::MERKLE_DEPTH;
use crate::streams_ffi::{CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb};

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

#[derive(Debug, Clone)]
pub struct DecryptedNote {
    note: Note,
    recipient: Address,
    memo: [u8; 512],
}

#[repr(C)]
pub struct NoteMetadata {
    txid: [u8; 32],
    action_idx: u32,
    recipient: *const Address,
    note_value: i64,
    memo: [u8; 512],
}

#[derive(Debug, Clone)]
pub struct WalletTx {
    txid: TxId,
    decrypted_notes: BTreeMap<usize, (IncomingViewingKey, Option<DecryptedNote>)>,
}

impl WalletTx {
    pub fn add_notes_from_bundle(
        &mut self,
        bundle: &Bundle<Authorized, Amount>,
        keys: &[IncomingViewingKey],
    ) {
        for (action_idx, ivk, note, recipient, memo) in bundle.decrypt_outputs_for_keys(keys) {
            // Mark the witness tree with the fact that we want to be able to compute
            // a witness for this note.
            let note_data = DecryptedNote {
                note,
                recipient,
                memo,
            };
            self.decrypted_notes
                .insert(action_idx, (ivk.clone(), Some(note_data)));
        }
    }

    /// For each note previously identified as belonging to the associated
    /// wallet, attempt to decrypt that note and cache the decrypted data.
    /// If decryption fails, return a Result::Err value containing the index
    /// in the Orchard bundle at which the action failed to decrypt with the
    /// stored viewing key.
    pub fn restore_decrypted_note_cache(
        &mut self,
        bundle: &Bundle<Authorized, Amount>,
    ) -> Result<(), usize> {
        for (idx, (ivk, dnote)) in self.decrypted_notes.iter_mut() {
            // TODO: does it make sense to skip repeated decryption like this?
            if dnote.is_none() {
                if let Some((note, recipient, memo)) = bundle.decrypt_output_with_key(*idx, ivk) {
                    *dnote = Some(DecryptedNote {
                        note,
                        recipient,
                        memo,
                    })
                } else {
                    return Err(*idx);
                }
            }
        }

        Ok(())
    }

    pub fn read<R: Read>(mut reader: R) -> io::Result<WalletTx> {
        let txid = TxId::read(&mut reader)?;
        let mut decrypted_notes = BTreeMap::new();
        let decrypted_notes_size = CompactSize::read(&mut reader)?;
        for _ in 0..decrypted_notes_size {
            let idx = CompactSize::read(&mut reader)?;
            let mut ivk_bytes = [0u8; 64];
            reader.read_exact(&mut ivk_bytes)?;
            let ivk_opt = IncomingViewingKey::from_bytes(&ivk_bytes);
            if ivk_opt.is_none().into() {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "unable to deserialize incoming view key".to_owned(),
                ));
            } else {
                decrypted_notes.insert(idx, (ivk_opt.unwrap(), None));
            }
        }

        Ok(WalletTx {
            txid,
            decrypted_notes,
        })
    }

    pub fn write<W: Write>(&self, mut writer: W) -> io::Result<()> {
        self.txid.write(&mut writer)?;
        CompactSize::write(&mut writer, self.decrypted_notes.len())?;
        for (idx, (ivk, _)) in self.decrypted_notes.iter() {
            CompactSize::write(&mut writer, *idx)?;
            writer.write_all(&ivk.to_bytes())?;
        }

        Ok(())
    }
}

#[derive(Debug, Clone)]
struct KeyMetadata {
    version: i32,
    create_time: u64,
    hd_key_path: Option<String>,
    seed_fingerprint: [u8; 32],
}

struct KeyStore {
    payment_addresses: BTreeMap<IncomingViewingKey, Vec<Address>>,
    incoming_viewing_keys: BTreeMap<IncomingViewingKey, FullViewingKey>,
    spending_keys: BTreeMap<FullViewingKey, SpendingKey>,
}

impl KeyStore {
    pub fn empty() -> Self {
        KeyStore {
            payment_addresses: BTreeMap::new(),
            incoming_viewing_keys: BTreeMap::new(),
            spending_keys: BTreeMap::new(),
        }
    }

    pub fn add_full_viewing_key(&mut self, fvk: &FullViewingKey) {
        let ivk = IncomingViewingKey::from(fvk);

        self.incoming_viewing_keys.insert(ivk, fvk.clone());
    }

    pub fn add_spending_key(&mut self, sk: &SpendingKey) {
        let fvk = FullViewingKey::from(sk);

        self.add_full_viewing_key(&fvk);
        self.spending_keys.insert(fvk, sk.clone());
    }

    pub fn append_payment_address(&mut self, ivk: &IncomingViewingKey, addr: &Address) {
        // TODO: avoid duplicate addresses?
        self.payment_addresses
            .entry(ivk.clone())
            .or_insert(vec![])
            .push(*addr);
    }

    pub fn has_spending_key_for_address(&self, addr: &Address) -> bool {
        self.payment_addresses
            .iter()
            .find_map(|(k, ax)| if ax.contains(addr) { Some(k) } else { None })
            .and_then(|ivk| self.incoming_viewing_keys.get(ivk))
            .and_then(|fvk| self.spending_keys.get(fvk))
            .is_some()
    }
}

pub struct Wallet {
    last_observed: Option<LastObserved>,
    key_store: KeyStore,
    witness_tree: BridgeTree<MerkleCrhOrchardOutput, MERKLE_DEPTH>,
    wallet_txs: HashMap<TxId, WalletTx>,
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
            last_observed: None,
            witness_tree: BridgeTree::new(MAX_CHECKPOINTS),
            wallet_txs: HashMap::new(),
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
    ) -> Result<(), WalletError> {
        let mut wallet_tx = WalletTx {
            txid: *txid,
            decrypted_notes: BTreeMap::new(),
        };

        let keys = self
            .key_store
            .incoming_viewing_keys
            .keys()
            .cloned()
            .collect::<Vec<_>>();

        wallet_tx.add_notes_from_bundle(bundle, &keys);
        for (ivk, dnote) in wallet_tx.decrypted_notes.values() {
            if let Some(dnote) = dnote {
                self.key_store.append_payment_address(ivk, &dnote.recipient);
            }
        }

        if !wallet_tx.decrypted_notes.is_empty() {
            self.wallet_txs.insert(*txid, wallet_tx);
        }

        Ok(())
    }

    /// Add note commitment data to the note commitment tree, and mark the tree for
    /// notes that "belong" to the incoming viewing keys known by this wallet.
    ///
    /// * `block_height` - Height of the block containing the transaction that provided this bundle.
    /// * `txidx` - Index of the transaction within the block
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

    // TODO: what are "locked" notes? re: ignoreLocked flag in CWallet::GetFilteredNotes
    pub fn get_filtered_notes(
        &self,
        addrs: &[&Address],
        ignore_spent: bool,
        require_spending_key: bool,
    ) -> Vec<(OutPoint, DecryptedNote)> {
        self.wallet_txs
            .values()
            .flat_map(|wallet_tx| {
                wallet_tx
                    .decrypted_notes
                    .iter()
                    .filter_map(move |(idx, (_, cached_dnote))| {
                        let outpoint = OutPoint {
                            txid: wallet_tx.txid,
                            action_idx: *idx,
                        };

                        cached_dnote.as_ref().and_then(|dnote| {
                            addrs
                                .iter()
                                .find(|a| ***a == dnote.recipient)
                                .and_then(|addr| {
                                    if ignore_spent && self.spent_notes.contains(&outpoint) {
                                        None
                                    } else if require_spending_key
                                        && self.key_store.has_spending_key_for_address(addr)
                                    {
                                        None
                                    } else {
                                        Some((outpoint, dnote.clone()))
                                    }
                                })
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
pub extern "C" fn orchard_wallet_restore_decrypted_note_cache(
    wallet: *mut Wallet,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
) -> bool {
    let wallet = unsafe { &mut *wallet };
    let txid = TxId::from_bytes(unsafe { &*txid }.clone());
    match wallet.wallet_txs.get_mut(&txid) {
        Some(wtx) => match unsafe { bundle.as_ref() } {
            Some(bundle) => match wtx.restore_decrypted_note_cache(bundle) {
                Err(idx) => {
                    error!("Unable to decrypt the action at index {:?}.", idx);
                    false
                }
                Ok(_) => true,
            },
            None => {
                error!("Cannot decrypt an empty Orchard bundle.");
                false
            }
        },
        None => {
            error!("No cached Orchard notes detected for transaction {}", txid);
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
pub extern "C" fn orchard_wallet_add_spending_key(wallet: *mut Wallet, sk: *const SpendingKey) {
    let wallet = unsafe { &mut *wallet };
    let sk = unsafe { &*sk };

    wallet.key_store.add_spending_key(sk);
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_full_viewing_key(
    wallet: *mut Wallet,
    fvk: *const FullViewingKey,
) {
    let wallet = unsafe { &mut *wallet };
    let fvk = unsafe { &*fvk };

    wallet.key_store.add_full_viewing_key(fvk);
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_incoming_viewing_key(
    wallet: *mut Wallet,
    ivk: *const IncomingViewingKey,
    addr: *const Address,
) {
    let wallet = unsafe { &mut *wallet };
    let ivk = unsafe { &*ivk };
    let addr = unsafe { &*addr };

    wallet.key_store.append_payment_address(ivk, addr);
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

pub type VecObj = std::ptr::NonNull<libc::c_void>;
pub type PushCb = unsafe extern "C" fn(obj: Option<VecObj>, meta: NoteMetadata);

#[no_mangle]
pub extern "C" fn orchard_wallet_get_filtered_notes(
    wallet: *const Wallet,
    addrs: *const *const Address,
    addrs_len: libc::size_t,
    ignore_spent: bool,
    require_spending_key: bool,
    result: Option<VecObj>,
    push_cb: Option<PushCb>,
) {
    let wallet = unsafe { &*wallet };
    let addrs = unsafe { std::slice::from_raw_parts(addrs, addrs_len) };
    let addrs: Vec<&Address> = addrs
        .into_iter()
        .map(|addr| unsafe { addr.as_ref().unwrap() })
        .collect();

    for (outpoint, dnote) in wallet.get_filtered_notes(&addrs, ignore_spent, require_spending_key) {
        let recipient = Box::new(dnote.recipient);
        unsafe {
            (push_cb.unwrap())(
                result,
                NoteMetadata {
                    txid: outpoint.txid.as_ref().clone(),
                    action_idx: outpoint.action_idx as u32,
                    recipient: Box::into_raw(recipient),
                    note_value: dnote.note.value().inner() as i64,
                    memo: dnote.memo.clone(),
                },
            )
        };
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_get_txdata(
    wallet: *const Wallet,
    txid: *const [c_uchar; 32],
) -> *mut WalletTx {
    let wallet = unsafe { &*wallet };
    let txid = TxId::from_bytes(unsafe { &*txid }.clone());

    match wallet.wallet_txs.get(&txid) {
        Some(wtx) => Box::into_raw(Box::new(wtx.clone())),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_note_commitment_tree_serialize(
    wallet: *const Wallet,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let wallet = unsafe { &*wallet };
    let writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());

    match write_tree_v1(writer, &wallet.witness_tree) {
        Ok(()) => true,
        Err(e) => {
            error!("{}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_note_commitment_tree_parse_update(
    wallet: *mut Wallet,
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> bool {
    let wallet = unsafe { &mut *wallet };
    let reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    match read_tree_v1(reader) {
        Ok(parsed) => {
            wallet.witness_tree = parsed;
            true
        }
        Err(e) => {
            error!("Failed to parse Orchard WalletTx: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_clone(wtx: *const WalletTx) -> *mut WalletTx {
    unsafe { wtx.as_ref() }
        .map(|wtx| Box::into_raw(Box::new(wtx.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_free(wtx: *mut WalletTx) {
    if !wtx.is_null() {
        drop(unsafe { Box::from_raw(wtx) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut WalletTx {
    let reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    match WalletTx::read(reader) {
        Ok(parsed) => Box::into_raw(Box::new(parsed)),
        Err(e) => {
            error!("Failed to parse Orchard WalletTx: {}", e);
            std::ptr::null_mut::<WalletTx>()
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_serialize(
    wtx: *const WalletTx,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let wtx = unsafe { &*wtx };
    let writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());

    match wtx.write(writer) {
        Ok(()) => true,
        Err(e) => {
            error!("{}", e);
            false
        }
    }
}
