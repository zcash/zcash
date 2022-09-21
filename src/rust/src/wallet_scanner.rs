use core::fmt;
use std::collections::HashMap;
use std::io;
use std::mem;
use std::sync::{
    atomic::{AtomicUsize, Ordering},
    Arc,
};

use crossbeam_channel as channel;
use group::GroupEncoding;
use memuse::DynamicUsage;
use zcash_note_encryption::{batch, BatchDomain, Domain, ShieldedOutput, ENC_CIPHERTEXT_SIZE};
use zcash_primitives::{
    block::BlockHash,
    consensus, constants,
    sapling::{self, note_encryption::SaplingDomain},
    transaction::{
        components::{sapling::GrothProofBytes, OutputDescription},
        Transaction, TxId,
    },
};

#[cxx::bridge]
mod ffi {
    #[namespace = "wallet"]
    struct SaplingDecryptionResult {
        txid: [u8; 32],
        output: u32,
        ivk: [u8; 32],
        diversifier: [u8; 11],
        pk_d: [u8; 32],
    }

    #[namespace = "wallet"]
    extern "Rust" {
        type Network;
        type BatchScanner;
        type BatchResult;

        fn network(
            network: &str,
            overwinter: i32,
            sapling: i32,
            blossom: i32,
            heartwood: i32,
            canopy: i32,
            nu5: i32,
        ) -> Result<Box<Network>>;

        fn init_batch_scanner(
            network: &Network,
            sapling_ivks: &[[u8; 32]],
        ) -> Result<Box<BatchScanner>>;
        fn get_dynamic_usage(self: &BatchScanner) -> usize;
        fn add_transaction(
            self: &mut BatchScanner,
            block_tag: [u8; 32],
            tx_bytes: &[u8],
            height: u32,
        ) -> Result<()>;
        fn flush(self: &mut BatchScanner);
        fn collect_results(
            self: &mut BatchScanner,
            block_tag: [u8; 32],
            txid: [u8; 32],
        ) -> Box<BatchResult>;

        fn get_sapling(self: &BatchResult) -> Vec<SaplingDecryptionResult>;
    }
}

/// The minimum number of outputs to trial decrypt in a batch.
///
/// TODO: Tune this.
const BATCH_SIZE_THRESHOLD: usize = 20;

const METRIC_OUTPUTS_SCANNED: &str = "zcashd.wallet.batchscanner.outputs.scanned";
const METRIC_LABEL_KIND: &str = "kind";

const METRIC_SIZE_TXS: &str = "zcashd.wallet.batchscanner.size.transactions";
const METRIC_USAGE_BYTES: &str = "zcashd.wallet.batchscanner.usage.bytes";

/// Chain parameters for the networks supported by `zcashd`.
#[derive(Clone, Copy)]
pub enum Network {
    Consensus(consensus::Network),
    RegTest {
        overwinter: Option<consensus::BlockHeight>,
        sapling: Option<consensus::BlockHeight>,
        blossom: Option<consensus::BlockHeight>,
        heartwood: Option<consensus::BlockHeight>,
        canopy: Option<consensus::BlockHeight>,
        nu5: Option<consensus::BlockHeight>,
    },
}

/// Constructs a `Network` from the given network string.
///
/// The heights are only for constructing a regtest network, and are ignored otherwise.
fn network(
    network: &str,
    overwinter: i32,
    sapling: i32,
    blossom: i32,
    heartwood: i32,
    canopy: i32,
    nu5: i32,
) -> Result<Box<Network>, &'static str> {
    let i32_to_optional_height = |n: i32| {
        if n.is_negative() {
            None
        } else {
            Some(consensus::BlockHeight::from_u32(n.unsigned_abs()))
        }
    };

    let params = match network {
        "main" => Network::Consensus(consensus::Network::MainNetwork),
        "test" => Network::Consensus(consensus::Network::TestNetwork),
        "regtest" => Network::RegTest {
            overwinter: i32_to_optional_height(overwinter),
            sapling: i32_to_optional_height(sapling),
            blossom: i32_to_optional_height(blossom),
            heartwood: i32_to_optional_height(heartwood),
            canopy: i32_to_optional_height(canopy),
            nu5: i32_to_optional_height(nu5),
        },
        _ => return Err("Unsupported network kind"),
    };

    Ok(Box::new(params))
}

impl consensus::Parameters for Network {
    fn activation_height(&self, nu: consensus::NetworkUpgrade) -> Option<consensus::BlockHeight> {
        match self {
            Self::Consensus(params) => params.activation_height(nu),
            Self::RegTest {
                overwinter,
                sapling,
                blossom,
                heartwood,
                canopy,
                nu5,
            } => match nu {
                consensus::NetworkUpgrade::Overwinter => *overwinter,
                consensus::NetworkUpgrade::Sapling => *sapling,
                consensus::NetworkUpgrade::Blossom => *blossom,
                consensus::NetworkUpgrade::Heartwood => *heartwood,
                consensus::NetworkUpgrade::Canopy => *canopy,
                consensus::NetworkUpgrade::Nu5 => *nu5,
            },
        }
    }

    fn coin_type(&self) -> u32 {
        match self {
            Self::Consensus(params) => params.coin_type(),
            Self::RegTest { .. } => constants::regtest::COIN_TYPE,
        }
    }

    fn address_network(&self) -> Option<zcash_address::Network> {
        match self {
            Self::Consensus(params) => params.address_network(),
            Self::RegTest { .. } => Some(zcash_address::Network::Regtest),
        }
    }

    fn hrp_sapling_extended_spending_key(&self) -> &str {
        match self {
            Self::Consensus(params) => params.hrp_sapling_extended_spending_key(),
            Self::RegTest { .. } => constants::regtest::HRP_SAPLING_EXTENDED_SPENDING_KEY,
        }
    }

    fn hrp_sapling_extended_full_viewing_key(&self) -> &str {
        match self {
            Self::Consensus(params) => params.hrp_sapling_extended_full_viewing_key(),
            Self::RegTest { .. } => constants::regtest::HRP_SAPLING_EXTENDED_FULL_VIEWING_KEY,
        }
    }

    fn hrp_sapling_payment_address(&self) -> &str {
        match self {
            Self::Consensus(params) => params.hrp_sapling_payment_address(),
            Self::RegTest { .. } => constants::regtest::HRP_SAPLING_PAYMENT_ADDRESS,
        }
    }

    fn b58_pubkey_address_prefix(&self) -> [u8; 2] {
        match self {
            Self::Consensus(params) => params.b58_pubkey_address_prefix(),
            Self::RegTest { .. } => constants::regtest::B58_PUBKEY_ADDRESS_PREFIX,
        }
    }

    fn b58_script_address_prefix(&self) -> [u8; 2] {
        match self {
            Self::Consensus(params) => params.b58_script_address_prefix(),
            Self::RegTest { .. } => constants::regtest::B58_SCRIPT_ADDRESS_PREFIX,
        }
    }
}

trait OutputDomain: BatchDomain {
    // The kind of output, for metrics labelling.
    const KIND: &'static str;
}

impl<P: consensus::Parameters> OutputDomain for SaplingDomain<P> {
    const KIND: &'static str = "sapling";
}

/// A decrypted note.
struct DecryptedNote<D: Domain> {
    /// The incoming viewing key used to decrypt the note.
    ivk: D::IncomingViewingKey,
    /// The recipient of the note.
    recipient: D::Recipient,
    /// The note!
    note: D::Note,
    /// The memo sent with the note.
    memo: D::Memo,
}

impl<D: Domain> fmt::Debug for DecryptedNote<D>
where
    D::IncomingViewingKey: fmt::Debug,
    D::Recipient: fmt::Debug,
    D::Note: fmt::Debug,
    D::Memo: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("DecryptedNote")
            .field("ivk", &self.ivk)
            .field("recipient", &self.recipient)
            .field("note", &self.note)
            .field("memo", &self.memo)
            .finish()
    }
}

/// A value correlated with an output index.
struct OutputIndex<V> {
    /// The index of the output within the corresponding shielded bundle.
    output_index: usize,
    /// The value for the output index.
    value: V,
}

type OutputItem<D> = OutputIndex<DecryptedNote<D>>;

/// The sender for the result of batch scanning a specific transaction output.
struct OutputReplier<D: Domain>(OutputIndex<channel::Sender<OutputItem<D>>>);

impl<D: Domain> DynamicUsage for OutputReplier<D> {
    #[inline(always)]
    fn dynamic_usage(&self) -> usize {
        // We count the memory usage of items in the channel on the receiver side.
        0
    }

    #[inline(always)]
    fn dynamic_usage_bounds(&self) -> (usize, Option<usize>) {
        (0, Some(0))
    }
}

/// A batch of outputs to trial decrypt.
struct Batch<D: BatchDomain, Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>> {
    ivks: Vec<D::IncomingViewingKey>,
    /// We currently store outputs and repliers as parallel vectors, because
    /// [`batch::try_note_decryption`] accepts a slice of domain/output pairs
    /// rather than a value that implements `IntoIterator`, and therefore we
    /// can't just use `map` to select the parts we need in order to perform
    /// batch decryption. Ideally the domain, output, and output replier would
    /// all be part of the same struct, which would also track the output index
    /// (that is captured in the outer `OutputIndex` of each `OutputReplier`).
    outputs: Vec<(D, Output)>,
    repliers: Vec<OutputReplier<D>>,
    // Pointer to the parent `BatchRunner`'s heap usage tracker for running batches.
    running_usage: Arc<AtomicUsize>,
}

fn base_vec_usage<T>(c: &Vec<T>) -> usize {
    c.capacity() * mem::size_of::<T>()
}

impl<D, Output> DynamicUsage for Batch<D, Output>
where
    D: BatchDomain,
    Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>,
{
    fn dynamic_usage(&self) -> usize {
        // We don't have a `DynamicUsage` bound on `D::IncomingViewingKey`, `D`, or
        // `Output`, and we can't use newtypes because the batch decryption API takes
        // slices. But we know that we don't allocate memory inside either of these, so we
        // just compute the size directly.
        base_vec_usage(&self.ivks) + base_vec_usage(&self.outputs) + self.repliers.dynamic_usage()
    }

    fn dynamic_usage_bounds(&self) -> (usize, Option<usize>) {
        let base_usage = base_vec_usage(&self.ivks) + base_vec_usage(&self.outputs);
        let bounds = self.repliers.dynamic_usage_bounds();
        (
            base_usage + bounds.0,
            bounds.1.map(|upper| base_usage + upper),
        )
    }
}

impl<D, Output> Batch<D, Output>
where
    D: OutputDomain,
    Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>,
    D::IncomingViewingKey: Clone,
{
    /// Constructs a new batch.
    fn new(ivks: Vec<D::IncomingViewingKey>, running_usage: Arc<AtomicUsize>) -> Self {
        Self {
            ivks,
            outputs: vec![],
            repliers: vec![],
            running_usage,
        }
    }

    /// Returns `true` if the batch is currently empty.
    fn is_empty(&self) -> bool {
        self.outputs.is_empty()
    }

    /// Runs the batch of trial decryptions, and reports the results.
    fn run(self) {
        // Approximate now as when the heap cost of this running batch begins. We use the
        // size of `self` as a lower bound on the actual heap memory allocated by the
        // rayon threadpool to store this `Batch`.
        let own_usage = std::mem::size_of_val(&self) + self.dynamic_usage();
        self.running_usage.fetch_add(own_usage, Ordering::SeqCst);

        // Deconstruct self so we can consume the pieces individually.
        let Self {
            ivks,
            outputs,
            repliers,
            running_usage,
        } = self;

        assert_eq!(outputs.len(), repliers.len());
        let decryption_results = batch::try_note_decryption(&ivks, &outputs);
        metrics::counter!(
            METRIC_OUTPUTS_SCANNED,
            outputs.len() as u64,
            METRIC_LABEL_KIND => D::KIND,
        );

        for (decryption_result, OutputReplier(replier)) in
            decryption_results.into_iter().zip(repliers.into_iter())
        {
            // If `decryption_result` is `None` then we will just drop `replier`,
            // indicating to the parent `BatchRunner` that this output was not for us.
            if let Some(((note, recipient, memo), ivk_idx)) = decryption_result {
                let result = OutputIndex {
                    output_index: replier.output_index,
                    value: DecryptedNote {
                        ivk: ivks[ivk_idx].clone(),
                        recipient,
                        note,
                        memo,
                    },
                };

                if replier.value.send(result).is_err() {
                    tracing::debug!("BatchRunner was dropped before batch finished");
                    break;
                }
            }
        }

        // Signal that the heap memory for this batch is about to be freed.
        running_usage.fetch_sub(own_usage, Ordering::SeqCst);
    }
}

impl<D: BatchDomain, Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE> + Clone> Batch<D, Output> {
    /// Adds the given outputs to this batch.
    ///
    /// `replier` will be called with the result of every output.
    fn add_outputs(
        &mut self,
        domain: impl Fn() -> D,
        outputs: &[Output],
        replier: channel::Sender<OutputItem<D>>,
    ) {
        self.outputs
            .extend(outputs.iter().cloned().map(|output| (domain(), output)));
        self.repliers.extend((0..outputs.len()).map(|output_index| {
            OutputReplier(OutputIndex {
                output_index,
                value: replier.clone(),
            })
        }));
    }
}

/// A `HashMap` key for looking up the result of a batch scanning a specific transaction.
#[derive(PartialEq, Eq, Hash)]
struct ResultKey(BlockHash, TxId);

impl DynamicUsage for ResultKey {
    #[inline(always)]
    fn dynamic_usage(&self) -> usize {
        0
    }

    #[inline(always)]
    fn dynamic_usage_bounds(&self) -> (usize, Option<usize>) {
        (0, Some(0))
    }
}

/// The receiver for the result of batch scanning a specific transaction.
struct BatchReceiver<D: Domain>(channel::Receiver<OutputItem<D>>);

impl<D: Domain> DynamicUsage for BatchReceiver<D> {
    fn dynamic_usage(&self) -> usize {
        // We count the memory usage of items in the channel on the receiver side.
        let num_items = self.0.len();

        // We know we use unbounded channels, so the items in the channel are stored as a
        // linked list. `crossbeam_channel` allocates memory for the linked list in blocks
        // of 31 items.
        const ITEMS_PER_BLOCK: usize = 31;
        let num_blocks = (num_items + ITEMS_PER_BLOCK - 1) / ITEMS_PER_BLOCK;

        // The structure of a block is:
        // - A pointer to the next block.
        // - For each slot in the block:
        //   - Space for an item.
        //   - The state of the slot, stored as an AtomicUsize.
        const PTR_SIZE: usize = std::mem::size_of::<usize>();
        let item_size = std::mem::size_of::<OutputItem<D>>();
        const ATOMIC_USIZE_SIZE: usize = std::mem::size_of::<AtomicUsize>();
        let block_size = PTR_SIZE + ITEMS_PER_BLOCK * (item_size + ATOMIC_USIZE_SIZE);

        num_blocks * block_size
    }

    fn dynamic_usage_bounds(&self) -> (usize, Option<usize>) {
        let usage = self.dynamic_usage();
        (usage, Some(usage))
    }
}

/// Logic to run batches of trial decryptions on the global threadpool.
struct BatchRunner<D: BatchDomain, Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>> {
    // The batch currently being accumulated.
    acc: Batch<D, Output>,
    // The dynamic memory usage of the running batches.
    running_usage: Arc<AtomicUsize>,
    // Receivers for the results of the running batches.
    pending_results: HashMap<ResultKey, BatchReceiver<D>>,
}

impl<D, Output> DynamicUsage for BatchRunner<D, Output>
where
    D: BatchDomain,
    Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>,
{
    fn dynamic_usage(&self) -> usize {
        self.acc.dynamic_usage()
            + self.running_usage.load(Ordering::Relaxed)
            + self.pending_results.dynamic_usage()
    }

    fn dynamic_usage_bounds(&self) -> (usize, Option<usize>) {
        let running_usage = self.running_usage.load(Ordering::Relaxed);

        let bounds = (
            self.acc.dynamic_usage_bounds(),
            self.pending_results.dynamic_usage_bounds(),
        );
        (
            bounds.0 .0 + running_usage + bounds.1 .0,
            bounds
                .0
                 .1
                .zip(bounds.1 .1)
                .map(|(a, b)| a + running_usage + b),
        )
    }
}

impl<D, Output> BatchRunner<D, Output>
where
    D: OutputDomain,
    Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>,
    D::IncomingViewingKey: Clone,
{
    /// Constructs a new batch runner for the given incoming viewing keys.
    fn new(ivks: Vec<D::IncomingViewingKey>) -> Self {
        let running_usage = Arc::new(AtomicUsize::new(0));
        Self {
            acc: Batch::new(ivks, running_usage.clone()),
            running_usage,
            pending_results: HashMap::default(),
        }
    }
}

impl<D, Output> BatchRunner<D, Output>
where
    D: OutputDomain + Send + 'static,
    D::IncomingViewingKey: Clone + Send,
    D::Memo: Send,
    D::Note: Send,
    D::Recipient: Send,
    Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE> + Clone + Send + 'static,
{
    /// Batches the given outputs for trial decryption.
    ///
    /// `block_tag` is the hash of the block that triggered this txid being added to the
    /// batch, or the all-zeros hash to indicate that no block triggered it (i.e. it was a
    /// mempool change).
    ///
    /// If after adding the given outputs, the accumulated batch size is at least
    /// `BATCH_SIZE_THRESHOLD`, `Self::flush` is called. Subsequent calls to
    /// `Self::add_outputs` will be accumulated into a new batch.
    fn add_outputs(
        &mut self,
        block_tag: BlockHash,
        txid: TxId,
        domain: impl Fn() -> D,
        outputs: &[Output],
    ) {
        let (tx, rx) = channel::unbounded();
        self.acc.add_outputs(domain, outputs, tx);
        self.pending_results
            .insert(ResultKey(block_tag, txid), BatchReceiver(rx));

        if self.acc.outputs.len() >= BATCH_SIZE_THRESHOLD {
            self.flush();
        }
    }

    /// Runs the currently accumulated batch on the global threadpool.
    ///
    /// Subsequent calls to `Self::add_outputs` will be accumulated into a new batch.
    fn flush(&mut self) {
        if !self.acc.is_empty() {
            let mut batch = Batch::new(self.acc.ivks.clone(), self.running_usage.clone());
            mem::swap(&mut batch, &mut self.acc);
            rayon::spawn_fifo(|| batch.run());
        }
    }

    /// Collects the pending decryption results for the given transaction.
    ///
    /// `block_tag` is the hash of the block that triggered this txid being added to the
    /// batch, or the all-zeros hash to indicate that no block triggered it (i.e. it was a
    /// mempool change).
    fn collect_results(
        &mut self,
        block_tag: BlockHash,
        txid: TxId,
    ) -> HashMap<(TxId, usize), DecryptedNote<D>> {
        self.pending_results
            .remove(&ResultKey(block_tag, txid))
            // We won't have a pending result if the transaction didn't have outputs of
            // this runner's kind.
            .map(|BatchReceiver(rx)| {
                // This iterator will end once the channel becomes empty and disconnected.
                // We created one sender per output, and each sender is dropped after the
                // batch it is in completes (and in the case of successful decryptions,
                // after the decrypted note has been sent to the channel). Completion of
                // the iterator therefore corresponds to complete knowledge of the outputs
                // of this transaction that could be decrypted.
                rx.into_iter()
                    .map(
                        |OutputIndex {
                             output_index,
                             value,
                         }| { ((txid, output_index), value) },
                    )
                    .collect()
            })
            .unwrap_or_default()
    }
}

/// A batch scanner for the `zcashd` wallet.
struct BatchScanner {
    params: Network,
    sapling_runner: Option<BatchRunner<SaplingDomain<Network>, OutputDescription<GrothProofBytes>>>,
}

impl DynamicUsage for BatchScanner {
    fn dynamic_usage(&self) -> usize {
        self.sapling_runner.dynamic_usage()
    }

    fn dynamic_usage_bounds(&self) -> (usize, Option<usize>) {
        self.sapling_runner.dynamic_usage_bounds()
    }
}

fn init_batch_scanner(
    network: &Network,
    sapling_ivks: &[[u8; 32]],
) -> Result<Box<BatchScanner>, &'static str> {
    let sapling_runner = if sapling_ivks.is_empty() {
        None
    } else {
        let ivks = sapling_ivks
            .iter()
            .map(|ivk| {
                let ivk: Option<sapling::SaplingIvk> =
                    jubjub::Fr::from_bytes(ivk).map(sapling::SaplingIvk).into();
                ivk.ok_or("Invalid Sapling ivk passed to wallet::init_batch_scanner()")
            })
            .collect::<Result<_, _>>()?;
        Some(BatchRunner::new(ivks))
    };

    Ok(Box::new(BatchScanner {
        params: *network,
        sapling_runner,
    }))
}

impl BatchScanner {
    /// FFI helper to access the `DynamicUsage` trait.
    fn get_dynamic_usage(&self) -> usize {
        let usage = self.dynamic_usage();

        // Since we've measured it, we may as well update the metric.
        metrics::gauge!(METRIC_USAGE_BYTES, usage as f64);

        usage
    }

    /// Adds the given transaction's shielded outputs to the various batch runners.
    ///
    /// `block_tag` is the hash of the block that triggered this txid being added to the
    /// batch, or the all-zeros hash to indicate that no block triggered it (i.e. it was a
    /// mempool change).
    ///
    /// After adding the outputs, any accumulated batch of sufficient size is run on the
    /// global threadpool. Subsequent calls to `Self::add_transaction` will accumulate
    /// those output kinds into new batches.
    fn add_transaction(
        &mut self,
        block_tag: [u8; 32],
        tx_bytes: &[u8],
        height: u32,
    ) -> Result<(), io::Error> {
        let block_tag = BlockHash(block_tag);
        // The consensusBranchId parameter is ignored; it is not used in trial decryption
        // and does not affect transaction parsing.
        let tx = Transaction::read(tx_bytes, consensus::BranchId::Sprout)?;
        let txid = tx.txid();
        let height = consensus::BlockHeight::from_u32(height);

        // If we have any Sapling IVKs, and the transaction has any Sapling outputs, queue
        // the outputs for trial decryption.
        if let Some((runner, bundle)) = self.sapling_runner.as_mut().zip(tx.sapling_bundle()) {
            let params = self.params;
            runner.add_outputs(
                block_tag,
                txid,
                || SaplingDomain::for_height(params, height),
                &bundle.shielded_outputs,
            );
        }

        // Update the size of the batch scanner.
        metrics::increment_gauge!(METRIC_SIZE_TXS, 1.0);
        metrics::gauge!(METRIC_USAGE_BYTES, self.dynamic_usage() as f64);

        Ok(())
    }

    /// Runs the currently accumulated batches on the global threadpool.
    ///
    /// Subsequent calls to `Self::add_transaction` will be accumulated into new batches.
    fn flush(&mut self) {
        if let Some(runner) = &mut self.sapling_runner {
            runner.flush();
        }
    }

    /// Collects the pending decryption results for the given transaction.
    ///
    /// `block_tag` is the hash of the block that triggered this txid being added to the
    /// batch, or the all-zeros hash to indicate that no block triggered it (i.e. it was a
    /// mempool change).
    ///
    /// TODO: Return the `HashMap`s directly once `cxx` supports it.
    fn collect_results(&mut self, block_tag: [u8; 32], txid: [u8; 32]) -> Box<BatchResult> {
        let block_tag = BlockHash(block_tag);
        let txid = TxId::from_bytes(txid);

        let sapling = self
            .sapling_runner
            .as_mut()
            .map(|runner| runner.collect_results(block_tag, txid))
            .unwrap_or_default();

        // Update the size of the batch scanner.
        metrics::decrement_gauge!(METRIC_SIZE_TXS, 1.0);
        metrics::gauge!(METRIC_USAGE_BYTES, self.dynamic_usage() as f64);

        Box::new(BatchResult { sapling })
    }
}

struct BatchResult {
    sapling: HashMap<(TxId, usize), DecryptedNote<SaplingDomain<Network>>>,
}

impl BatchResult {
    fn get_sapling(&self) -> Vec<ffi::SaplingDecryptionResult> {
        self.sapling
            .iter()
            .map(
                |((txid, output), decrypted_note)| ffi::SaplingDecryptionResult {
                    txid: *txid.as_ref(),
                    output: *output as u32,
                    ivk: decrypted_note.ivk.to_repr(),
                    diversifier: decrypted_note.recipient.diversifier().0,
                    pk_d: decrypted_note.recipient.pk_d().to_bytes(),
                },
            )
            .collect()
    }
}
