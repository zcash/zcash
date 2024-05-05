// To silence lints in the `uint::construct_uint!` macro.
#![allow(clippy::assign_op_pattern)]
#![allow(clippy::ptr_offset_with_cast)]

use std::cmp;
use std::convert::{TryFrom, TryInto};
use std::io::{self, Read};

use sha2::{Digest, Sha256};
use zcash_encoding::Vector;
use zcash_primitives::{
    block::BlockHeader,
    consensus::{BlockHeight, BranchId, Network, NetworkUpgrade, Parameters},
    transaction::Transaction,
};

use crate::{
    transaction::{extract_height_from_coinbase, is_coinbase},
    Context, ZUint256,
};

const MIN_BLOCK_VERSION: i32 = 4;

uint::construct_uint! {
    pub(crate) struct U256(4);
}

impl U256 {
    fn from_compact(compact: u32) -> (Self, bool, bool) {
        let size = compact >> 24;
        let word = compact & 0x007fffff;
        let result = if size <= 3 {
            U256::from(word >> (8 * (3 - size)))
        } else {
            U256::from(word) << (8 * (size - 3))
        };
        (
            result,
            word != 0 && (compact & 0x00800000) != 0,
            word != 0
                && ((size > 34) || (word > 0xff && size > 33) || (word > 0xffff && size > 32)),
        )
    }
}

pub(crate) trait BlockParams: Parameters {
    fn equihash_n(&self) -> u32;
    fn equihash_k(&self) -> u32;
    fn pow_limit(&self) -> U256;
}

impl BlockParams for Network {
    fn equihash_n(&self) -> u32 {
        match self {
            Self::MainNetwork | Self::TestNetwork => 200,
        }
    }

    fn equihash_k(&self) -> u32 {
        match self {
            Self::MainNetwork | Self::TestNetwork => 9,
        }
    }

    fn pow_limit(&self) -> U256 {
        match self {
            Self::MainNetwork => U256::from_big_endian(
                &hex::decode("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
                    .unwrap(),
            ),
            Self::TestNetwork => U256::from_big_endian(
                &hex::decode("07ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
                    .unwrap(),
            ),
        }
    }
}

pub(crate) fn guess_params(header: &BlockHeader) -> Option<Network> {
    // If the block target falls between the testnet and mainnet powLimit, assume testnet.
    let (target, is_negative, did_overflow) = U256::from_compact(header.bits);
    if !(is_negative || did_overflow)
        && target > Network::MainNetwork.pow_limit()
        && target <= Network::TestNetwork.pow_limit()
    {
        return Some(Network::TestNetwork);
    }

    None
}

fn check_equihash_solution(header: &BlockHeader, params: Network) -> Result<(), equihash::Error> {
    let eh_input = {
        let mut eh_input = vec![];
        header.write(&mut eh_input).unwrap();
        eh_input.truncate(4 + 32 + 32 + 32 + 4 + 4);
        eh_input
    };
    equihash::is_valid_solution(
        params.equihash_n(),
        params.equihash_k(),
        &eh_input,
        &header.nonce,
        &header.solution,
    )
}

fn check_proof_of_work(header: &BlockHeader, params: Network) -> Result<(), &str> {
    let (target, is_negative, did_overflow) = U256::from_compact(header.bits);
    let hash = U256::from_little_endian(&header.hash().0);

    if is_negative {
        Err("nBits is negative")
    } else if target.is_zero() {
        Err("target is zero")
    } else if did_overflow {
        Err("nBits overflowed")
    } else if target > params.pow_limit() {
        Err("target is larger than powLimit")
    } else if hash > target {
        Err("block hash larger than target")
    } else {
        Ok(())
    }
}

fn derive_block_commitments_hash(
    chain_history_root: [u8; 32],
    auth_data_root: [u8; 32],
) -> [u8; 32] {
    blake2b_simd::Params::new()
        .hash_length(32)
        .personal(b"ZcashBlockCommit")
        .to_state()
        .update(&chain_history_root)
        .update(&auth_data_root)
        .update(&[0; 32])
        .finalize()
        .as_bytes()
        .try_into()
        .unwrap()
}

pub(crate) struct Block {
    header: BlockHeader,
    txs: Vec<Transaction>,
}

impl Block {
    pub(crate) fn read<R: Read>(mut reader: R) -> io::Result<Self> {
        let header = BlockHeader::read(&mut reader)?;
        let txs = Vector::read(reader, |r| Transaction::read(r, BranchId::Sprout))?;

        Ok(Block { header, txs })
    }

    pub(crate) fn guess_params(&self) -> Option<Network> {
        guess_params(&self.header)
    }

    fn extract_height(&self) -> Option<BlockHeight> {
        self.txs.first().and_then(extract_height_from_coinbase)
    }

    /// Builds the Merkle tree for this block and returns its root.
    ///
    /// The returned `bool` indicates whether mutation was detected in the Merkle tree (a
    /// duplication of transactions in the block leading to an identical Merkle root).
    fn build_merkle_root(&self) -> ([u8; 32], bool) {
        // Safe upper bound for the number of total nodes.
        let mut merkle_tree = Vec::with_capacity(self.txs.len() * 2 + 16);
        for tx in &self.txs {
            merkle_tree.push(sha2::digest::generic_array::GenericArray::from(
                *tx.txid().as_ref(),
            ));
        }
        let mut size = self.txs.len();
        let mut j = 0;
        let mut mutated = false;
        while size > 1 {
            let mut i = 0;
            while i < size {
                let i2 = cmp::min(i + 1, size - 1);
                if i2 == i + 1 && i2 + 1 == size && merkle_tree[j + i] == merkle_tree[j + i2] {
                    // Two identical hashes at the end of the list at a particular level.
                    mutated = true;
                }
                let mut inner_hasher = Sha256::new();
                inner_hasher.update(merkle_tree[j + i]);
                inner_hasher.update(merkle_tree[j + i2]);
                merkle_tree.push(Sha256::digest(inner_hasher.finalize()));
                i += 2;
            }
            j += size;
            size = (size + 1) / 2;
        }
        (
            merkle_tree
                .last()
                .copied()
                .map(|root| root.into())
                .unwrap_or([0; 32]),
            mutated,
        )
    }

    fn build_auth_data_root(&self) -> [u8; 32] {
        fn next_pow2(x: u64) -> u64 {
            // Fails if `x` is greater than `1u64 << 63`, but this can't occur because a
            // block can't feasibly contain that many transactions.
            1u64 << (64 - x.saturating_sub(1).leading_zeros())
        }

        let perfect_size = next_pow2(self.txs.len() as u64) as usize;
        assert_eq!((perfect_size & (perfect_size - 1)), 0);
        let expected_size = cmp::max(perfect_size * 2, 1) - 1; // The total number of nodes.
        let mut tree = Vec::with_capacity(expected_size);

        // Add the leaves to the tree. v1-v4 transactions will append empty leaves.
        for tx in &self.txs {
            tree.push(<[u8; 32]>::try_from(tx.auth_commitment().as_bytes()).unwrap());
        }
        // Append empty leaves until we get a perfect tree.
        tree.resize(perfect_size, [0; 32]);

        let mut j = 0;
        let mut layer_width = perfect_size;
        while layer_width > 1 {
            let mut i = 0;
            while i < layer_width {
                tree.push(
                    blake2b_simd::Params::new()
                        .hash_length(32)
                        .personal(b"ZcashAuthDatHash")
                        .to_state()
                        .update(&tree[j + i])
                        .update(&tree[j + i + 1])
                        .finalize()
                        .as_bytes()
                        .try_into()
                        .unwrap(),
                );
                i += 2;
            }

            // Move to the next layer.
            j += layer_width;
            layer_width /= 2;
        }

        assert_eq!(tree.len(), expected_size);
        tree.last().copied().unwrap_or([0; 32])
    }
}

pub(crate) fn inspect_header(header: &BlockHeader, context: Option<Context>) {
    eprintln!("Zcash block header");
    inspect_header_inner(
        header,
        guess_params(header).or_else(|| context.and_then(|c| c.network())),
    );
}

fn inspect_header_inner(header: &BlockHeader, params: Option<Network>) {
    eprintln!(" - Hash: {}", header.hash());
    eprintln!(" - Version: {}", header.version);
    if header.version < MIN_BLOCK_VERSION {
        // zcashd: version-too-low
        eprintln!("⚠️  Version too low",);
    }
    if let Some(params) = params {
        if let Err(e) = check_equihash_solution(header, params) {
            // zcashd: invalid-solution
            eprintln!("⚠️  Invalid Equihash solution: {}", e);
        }
        if let Err(e) = check_proof_of_work(header, params) {
            // zcashd: high-hash
            eprintln!("⚠️  Invalid Proof-of-Work: {}", e);
        }
    } else {
        eprintln!("🔎 To check contextual rules, add \"network\" to context (either \"main\" or \"test\")");
    }
}

pub(crate) fn inspect(block: &Block, context: Option<Context>) {
    eprintln!("Zcash block");
    let params = block
        .guess_params()
        .or_else(|| context.as_ref().and_then(|c| c.network()));
    inspect_header_inner(&block.header, params);

    let height = match block.txs.len() {
        0 => {
            // zcashd: bad-cb-missing
            eprintln!("⚠️  Missing coinbase transaction");
            None
        }
        txs => {
            eprintln!(" - {} transaction(s) including coinbase", txs);

            if !is_coinbase(&block.txs[0]) {
                // zcashd: bad-cb-missing
                eprintln!("⚠️  vtx[0] is not a coinbase transaction");
                None
            } else {
                let height = block.extract_height();
                match height {
                    Some(h) => eprintln!(" - Height: {}", h),
                    // zcashd: bad-cb-height
                    None => eprintln!("⚠️  No height in coinbase transaction"),
                }
                height
            }
        }
    };

    for (i, tx) in block.txs.iter().enumerate().skip(1) {
        if is_coinbase(tx) {
            // zcashd: bad-cb-multiple
            eprintln!("⚠️  vtx[{}] is a coinbase transaction", i);
        }
    }

    let (merkle_root, merkle_root_mutated) = block.build_merkle_root();
    if merkle_root != block.header.merkle_root {
        // zcashd: bad-txnmrklroot
        eprintln!("⚠️  header.merkleroot doesn't match transaction Merkle tree root");
        eprintln!("   - merkleroot (calc): {}", ZUint256(merkle_root));
        eprintln!(
            "   - header.merkleroot: {}",
            ZUint256(block.header.merkle_root)
        );
    }
    if merkle_root_mutated {
        // zcashd: bad-txns-duplicate
        eprintln!("⚠️  Transaction Merkle tree is malleable");
    }

    // The rest of the checks require network parameters and a block height.
    let (params, height) = match (params, height) {
        (Some(params), Some(height)) => (params, height),
        _ => return,
    };

    if params.is_nu_active(NetworkUpgrade::Nu5, height) {
        if block.txs[0].expiry_height() != height {
            // zcashd: bad-cb-height
            eprintln!(
                "⚠️  [NU5] coinbase expiry height ({}) doesn't match coinbase scriptSig height ({})",
                block.txs[0].expiry_height(),
                height
            );
        }

        if let Some(chain_history_root) = context.and_then(|c| c.chainhistoryroot) {
            let auth_data_root = block.build_auth_data_root();
            let block_commitments_hash =
                derive_block_commitments_hash(chain_history_root.0, auth_data_root);

            if block_commitments_hash != block.header.final_sapling_root {
                // zcashd: bad-block-commitments-hash
                eprintln!(
                    "⚠️  [NU5] header.blockcommitments doesn't match ZIP 244 block commitment"
                );
                eprintln!("   - chainhistoryroot:        {}", chain_history_root);
                eprintln!("   - authdataroot:            {}", ZUint256(auth_data_root));
                eprintln!(
                    "   - blockcommitments (calc): {}",
                    ZUint256(block_commitments_hash)
                );
                eprintln!(
                    "   - header.blockcommitments: {}",
                    ZUint256(block.header.final_sapling_root)
                );
            }
        } else {
            eprintln!("🔎 To check header.blockcommitments, add \"chainhistoryroot\" to context");
        }
    } else if Some(height) == params.activation_height(NetworkUpgrade::Heartwood) {
        if block.header.final_sapling_root != [0; 32] {
            // zcashd: bad-heartwood-root-in-block
            eprintln!("⚠️  This is the block that activates Heartwood but header.blockcommitments is not null");
        }
    } else if params.is_nu_active(NetworkUpgrade::Heartwood, height) {
        if let Some(chain_history_root) = context.and_then(|c| c.chainhistoryroot) {
            if chain_history_root.0 != block.header.final_sapling_root {
                // zcashd: bad-heartwood-root-in-block
                eprintln!(
                "⚠️  [Heartwood] header.blockcommitments doesn't match provided chain history root"
            );
                eprintln!("   - chainhistoryroot:        {}", chain_history_root);
                eprintln!(
                    "   - header.blockcommitments: {}",
                    ZUint256(block.header.final_sapling_root)
                );
            }
        } else {
            eprintln!("🔎 To check header.blockcommitments, add \"chainhistoryroot\" to context");
        }
    }
}
