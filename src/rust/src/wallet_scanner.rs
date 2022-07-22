use core::fmt;
use std::collections::HashMap;
use std::io;
use std::mem;

use crossbeam_channel as channel;
use group::GroupEncoding;
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

type OutputReplier<D> = OutputIndex<channel::Sender<OutputIndex<Option<DecryptedNote<D>>>>>;

/// A batch of outputs to trial decrypt.
struct Batch<D: BatchDomain, Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>> {
    ivks: Vec<D::IncomingViewingKey>,
    outputs: Vec<(D, Output)>,
    repliers: Vec<OutputReplier<D>>,
}

impl<D, Output> Batch<D, Output>
where
    D: BatchDomain,
    Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>,
    D::IncomingViewingKey: Clone,
{
    /// Constructs a new batch.
    fn new(ivks: Vec<D::IncomingViewingKey>) -> Self {
        Self {
            ivks,
            outputs: vec![],
            repliers: vec![],
        }
    }

    /// Returns `true` if the batch is currently empty.
    fn is_empty(&self) -> bool {
        self.outputs.is_empty()
    }

    /// Runs the batch of trial decryptions, and reports the results.
    fn run(self) {
        assert_eq!(self.outputs.len(), self.repliers.len());
        let decrypted = batch::try_note_decryption(&self.ivks, &self.outputs);
        for (decrypted_note, (ivk, replier)) in decrypted.into_iter().zip(
            // The output of `batch::try_note_decryption` corresponds to the stream of
            // trial decryptions:
            //     (ivk0, out0), (ivk0, out1), ..., (ivk0, outN), (ivk1, out0), ...
            // So we can use the position in the stream to figure out which output was
            // decrypted and which ivk decrypted it.
            self.ivks
                .iter()
                .flat_map(|ivk| self.repliers.iter().map(move |tx| (ivk, tx))),
        ) {
            let value = decrypted_note.map(|(note, recipient, memo)| DecryptedNote {
                ivk: ivk.clone(),
                memo,
                note,
                recipient,
            });

            let output_index = replier.output_index;
            let tx = &replier.value;
            if tx
                .send(OutputIndex {
                    output_index,
                    value,
                })
                .is_err()
            {
                tracing::debug!("BatchRunner was dropped before batch finished");
                return;
            }
        }
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
        replier: channel::Sender<OutputIndex<Option<DecryptedNote<D>>>>,
    ) {
        self.outputs
            .extend(outputs.iter().cloned().map(|output| (domain(), output)));
        self.repliers
            .extend((0..outputs.len()).map(|output_index| OutputIndex {
                output_index,
                value: replier.clone(),
            }));
    }
}

type ResultKey = (BlockHash, TxId);

/// Logic to run batches of trial decryptions on the global threadpool.
struct BatchRunner<D: BatchDomain, Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>> {
    acc: Batch<D, Output>,
    pending_results: HashMap<ResultKey, channel::Receiver<OutputIndex<Option<DecryptedNote<D>>>>>,
}

impl<D, Output> BatchRunner<D, Output>
where
    D: BatchDomain,
    Output: ShieldedOutput<D, ENC_CIPHERTEXT_SIZE>,
    D::IncomingViewingKey: Clone,
{
    /// Constructs a new batch runner for the given incoming viewing keys.
    fn new(ivks: Vec<D::IncomingViewingKey>) -> Self {
        Self {
            acc: Batch::new(ivks),
            pending_results: HashMap::default(),
        }
    }
}

impl<D, Output> BatchRunner<D, Output>
where
    D: BatchDomain + Send + 'static,
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
        self.pending_results.insert((block_tag, txid), rx);

        if self.acc.outputs.len() >= BATCH_SIZE_THRESHOLD {
            self.flush();
        }
    }

    /// Runs the currently accumulated batch on the global threadpool.
    ///
    /// Subsequent calls to `Self::add_outputs` will be accumulated into a new batch.
    fn flush(&mut self) {
        if !self.acc.is_empty() {
            let mut batch = Batch::new(self.acc.ivks.clone());
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
            .remove(&(block_tag, txid))
            // We won't have a pending result if the transaction didn't have outputs of
            // this runner's kind.
            .map(|rx| {
                rx.into_iter()
                    .filter_map(
                        |OutputIndex {
                             output_index,
                             value,
                         }| {
                            value.map(|decrypted_note| ((txid, output_index), decrypted_note))
                        },
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
