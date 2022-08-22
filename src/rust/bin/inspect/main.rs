use std::env;
use std::io;
use std::io::Cursor;
use std::process;

use gumdrop::{Options, ParsingStyle};
use lazy_static::lazy_static;
use secrecy::Zeroize;
use zcash_address::ZcashAddress;
use zcash_primitives::zip339;
use zcash_primitives::{block::BlockHeader, consensus::BranchId, transaction::Transaction};
use zcash_proofs::{default_params_folder, load_parameters, ZcashParameters};

mod context;
use context::{Context, ZUint256};

mod address;
mod block;
mod keys;
mod transaction;

lazy_static! {
    static ref GROTH16_PARAMS: ZcashParameters = {
        let folder = default_params_folder().unwrap();
        load_parameters(
            &folder.join("sapling-spend.params"),
            &folder.join("sapling-output.params"),
            Some(&folder.join("sprout-groth16.params")),
        )
    };
    static ref ORCHARD_VK: orchard::circuit::VerifyingKey = orchard::circuit::VerifyingKey::build();
}

#[derive(Debug, Options)]
struct CliOptions {
    #[options(help = "Print this help output")]
    help: bool,

    #[options(free, required, help = "String or hex-encoded bytes to inspect")]
    data: String,

    #[options(
        free,
        help = "JSON object with keys corresponding to requested context information"
    )]
    context: Option<Context>,
}

fn main() {
    let args = env::args().collect::<Vec<_>>();
    let mut opts =
        CliOptions::parse_args(&args[1..], ParsingStyle::default()).unwrap_or_else(|e| {
            eprintln!("{}: {}", args[0], e);
            process::exit(2);
        });

    if opts.help_requested() {
        println!("Usage: {} data [context]", args[0]);
        println!();
        println!("{}", CliOptions::usage());
        return;
    }

    let lang = zip339::Language::English;

    if let Ok(mnemonic) = zip339::Mnemonic::from_phrase_in(lang, &opts.data) {
        opts.data.zeroize();
        keys::inspect_mnemonic(mnemonic, lang, opts.context);
    } else if let Ok(bytes) = hex::decode(&opts.data) {
        inspect_bytes(bytes, opts.context);
    } else if let Ok(addr) = ZcashAddress::try_from_encoded(&opts.data) {
        address::inspect(addr);
    } else {
        // Unknown data format.
        eprintln!("String does not match known Zcash data formats.");
        process::exit(2);
    }
}

/// Ensures that the given reader completely consumes the given bytes.
fn complete<F, T>(bytes: &[u8], f: F) -> Option<T>
where
    F: FnOnce(&mut Cursor<&[u8]>) -> io::Result<T>,
{
    let mut cursor = Cursor::new(bytes);
    let res = f(&mut cursor);
    res.ok().and_then(|t| {
        if cursor.position() >= bytes.len() as u64 {
            Some(t)
        } else {
            None
        }
    })
}

fn inspect_bytes(bytes: Vec<u8>, context: Option<Context>) {
    if let Some(block) = complete(&bytes, |r| block::Block::read(r)) {
        block::inspect(&block, context);
    } else if let Some(header) = complete(&bytes, |r| BlockHeader::read(r)) {
        block::inspect_header(&header, context);
    } else if let Some(tx) = complete(&bytes, |r| Transaction::read(r, BranchId::Sprout)) {
        // TODO: Take the branch ID used above from the context if present.
        transaction::inspect(tx, context);
    } else {
        // It's not a known variable-length format. check fixed-length data formats.
        inspect_fixed_length_bytes(bytes);
    }
}

fn inspect_fixed_length_bytes(bytes: Vec<u8>) {
    match bytes.len() {
        32 => {
            eprintln!(
                "This is most likely a hash of some sort, or maybe a commitment or nullifier."
            );
            if bytes.iter().take(4).all(|c| c == &0) {
                eprintln!("- It could be a mainnet block hash.");
            }
        }
        64 => {
            // Could be a signature
            eprintln!("This is most likely a signature.");
        }
        _ => {
            eprintln!("Binary data does not match known Zcash data formats.");
            process::exit(2);
        }
    }
}
