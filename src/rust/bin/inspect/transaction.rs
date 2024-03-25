use std::{
    collections::HashMap,
    convert::{TryFrom, TryInto},
};

use bellman::groth16;
use group::GroupEncoding;
use orchard::note_encryption::OrchardDomain;
use sapling::{note_encryption::SaplingDomain, SaplingVerificationContext};
use secp256k1::{Secp256k1, VerifyOnly};
use zcash_address::{
    unified::{self, Encoding},
    ToAddress, ZcashAddress,
};
use zcash_note_encryption::try_output_recovery_with_ovk;
#[allow(deprecated)]
use zcash_primitives::{
    consensus::{sapling_zip212_enforcement, BlockHeight},
    legacy::{keys::pubkey_to_address, Script, TransparentAddress},
    memo::{Memo, MemoBytes},
    transaction::{
        components::{amount::NonNegativeAmount, transparent},
        sighash::{signature_hash, SignableInput, TransparentAuthorizingContext},
        txid::TxIdDigester,
        Authorization, Transaction, TransactionData, TxId, TxVersion,
    },
};

use crate::{
    context::{Context, ZTxOut},
    GROTH16_PARAMS, ORCHARD_VK,
};

pub fn is_coinbase(tx: &Transaction) -> bool {
    tx.transparent_bundle()
        .map(|b| b.is_coinbase())
        .unwrap_or(false)
}

pub fn extract_height_from_coinbase(tx: &Transaction) -> Option<BlockHeight> {
    const OP_0: u8 = 0x00;
    const OP_1NEGATE: u8 = 0x4f;
    const OP_1: u8 = 0x51;
    const OP_16: u8 = 0x60;

    tx.transparent_bundle()
        .and_then(|bundle| bundle.vin.get(0))
        .and_then(|input| match input.script_sig.0.first().copied() {
            // {0, -1} will never occur as the first byte of a coinbase scriptSig.
            Some(OP_0 | OP_1NEGATE) => None,
            // Blocks 1 to 16.
            Some(h @ OP_1..=OP_16) => Some(BlockHeight::from_u32((h - OP_1 + 1).into())),
            // All other heights use CScriptNum encoding, which will never be longer
            // than 5 bytes for Zcash heights. These have the format
            // `[len(encoding)] || encoding`.
            Some(h @ 1..=5) => {
                let rest = &input.script_sig.0[1..];
                let encoding_len = h as usize;
                if rest.len() < encoding_len {
                    None
                } else {
                    // Parse the encoding.
                    let encoding = &rest[..encoding_len];
                    if encoding.last().unwrap() & 0x80 != 0 {
                        // Height is never negative.
                        None
                    } else {
                        let mut height: u64 = 0;
                        for (i, b) in encoding.iter().enumerate() {
                            height |= (*b as u64) << (8 * i);
                        }
                        height.try_into().ok()
                    }
                }
            }
            // Anything else is an invalid height encoding.
            _ => None,
        })
}

fn render_value(value: u64) -> String {
    format!(
        "{} zatoshis ({} ZEC)",
        value,
        (value as f64) / 1_0000_0000f64
    )
}

fn render_memo(memo_bytes: MemoBytes) -> String {
    match Memo::try_from(memo_bytes) {
        Ok(Memo::Empty) => "No memo".to_string(),
        Ok(Memo::Text(memo)) => format!("Text memo: '{}'", String::from(memo)),
        Ok(memo) => format!("{:?}", memo),
        Err(e) => format!("Invalid memo: {}", e),
    }
}

#[derive(Clone, Debug)]
pub(crate) struct TransparentAuth {
    all_prev_outputs: Vec<transparent::TxOut>,
}

impl transparent::Authorization for TransparentAuth {
    type ScriptSig = Script;
}

impl TransparentAuthorizingContext for TransparentAuth {
    fn input_amounts(&self) -> Vec<NonNegativeAmount> {
        self.all_prev_outputs
            .iter()
            .map(|prevout| prevout.value)
            .collect()
    }

    fn input_scriptpubkeys(&self) -> Vec<Script> {
        self.all_prev_outputs
            .iter()
            .map(|prevout| prevout.script_pubkey.clone())
            .collect()
    }
}

struct MapTransparent {
    auth: TransparentAuth,
}

impl transparent::MapAuth<transparent::Authorized, TransparentAuth> for MapTransparent {
    fn map_script_sig(
        &self,
        s: <transparent::Authorized as transparent::Authorization>::ScriptSig,
    ) -> <TransparentAuth as transparent::Authorization>::ScriptSig {
        s
    }

    fn map_authorization(&self, _: transparent::Authorized) -> TransparentAuth {
        // TODO: This map should consume self, so we can move self.auth
        self.auth.clone()
    }
}

pub(crate) struct PrecomputedAuth;

impl Authorization for PrecomputedAuth {
    type TransparentAuth = TransparentAuth;
    type SaplingAuth = sapling::bundle::Authorized;
    type OrchardAuth = orchard::bundle::Authorized;
}

pub(crate) fn inspect(tx: Transaction, context: Option<Context>) {
    eprintln!("Zcash transaction");
    eprintln!(" - ID: {}", tx.txid());
    eprintln!(" - Version: {:?}", tx.version());
    match tx.version() {
        // TODO: If pre-v5 and no branch ID provided in context, disable signature checks.
        TxVersion::Sprout(_) | TxVersion::Overwinter | TxVersion::Sapling => (),
        TxVersion::Zip225 => {
            eprintln!(" - Consensus branch ID: {:?}", tx.consensus_branch_id());
        }
    }

    let is_coinbase = is_coinbase(&tx);
    let height = if is_coinbase {
        eprintln!(" - Coinbase");
        extract_height_from_coinbase(&tx)
    } else {
        None
    };

    let transparent_coins = match (
        tx.transparent_bundle().map_or(false, |b| !b.vin.is_empty()),
        context.as_ref().and_then(|ctx| ctx.transparent_coins()),
    ) {
        (true, coins) => coins,
        (false, Some(_)) => {
            eprintln!("⚠️  Context was given \"transparentcoins\" but this transaction has no transparent inputs");
            Some(vec![])
        }
        (false, None) => Some(vec![]),
    };

    let sighash_params = transparent_coins.as_ref().map(|coins| {
        let f_transparent = MapTransparent {
            auth: TransparentAuth {
                all_prev_outputs: coins.clone(),
            },
        };

        // We don't have tx.clone()
        let mut buf = vec![];
        tx.write(&mut buf).unwrap();
        let tx = Transaction::read(&buf[..], tx.consensus_branch_id()).unwrap();

        let tx: TransactionData<PrecomputedAuth> =
            tx.into_data().map_authorization(f_transparent, (), ());
        let txid_parts = tx.digest(TxIdDigester);
        (tx, txid_parts)
    });

    let common_sighash = sighash_params
        .as_ref()
        .map(|(tx, txid_parts)| signature_hash(tx, &SignableInput::Shielded, txid_parts));
    if let Some(sighash) = &common_sighash {
        if tx.sprout_bundle().is_some()
            || tx.sapling_bundle().is_some()
            || tx.orchard_bundle().is_some()
        {
            eprintln!(
                " - Sighash for shielded signatures: {}",
                hex::encode(sighash.as_ref()),
            );
        }
    }

    if let Some(bundle) = tx.transparent_bundle() {
        assert!(!(bundle.vin.is_empty() && bundle.vout.is_empty()));
        if !(bundle.vin.is_empty() || is_coinbase) {
            eprintln!(" - {} transparent input(s)", bundle.vin.len());
            if let Some(coins) = &transparent_coins {
                if bundle.vin.len() != coins.len() {
                    eprintln!("  ⚠️  \"transparentcoins\" has length {}", coins.len());
                }

                let (tx, txid_parts) = sighash_params.as_ref().unwrap();
                let ctx = Secp256k1::<VerifyOnly>::gen_new();

                for (i, (txin, coin)) in bundle.vin.iter().zip(coins).enumerate() {
                    eprintln!(
                        "   - prevout: txid {}, index {}",
                        TxId::from_bytes(*txin.prevout.hash()),
                        txin.prevout.n()
                    );
                    match coin.recipient_address() {
                        Some(addr @ TransparentAddress::PublicKeyHash(_)) => {
                            // Format is [sig_and_type_len] || sig || [hash_type] || [pubkey_len] || pubkey
                            // where [x] encodes a single byte.
                            let sig_and_type_len = txin.script_sig.0.first().map(|l| *l as usize);
                            let pubkey_len = sig_and_type_len
                                .and_then(|sig_len| txin.script_sig.0.get(1 + sig_len))
                                .map(|l| *l as usize);
                            let script_len = sig_and_type_len.zip(pubkey_len).map(
                                |(sig_and_type_len, pubkey_len)| {
                                    1 + sig_and_type_len + 1 + pubkey_len
                                },
                            );

                            if Some(txin.script_sig.0.len()) != script_len {
                                eprintln!(
                                    "    ⚠️  \"transparentcoins\" {} is P2PKH; txin {} scriptSig has length {} but data {}",
                                    i,
                                    i,
                                    txin.script_sig.0.len(),
                                    if let Some(l) = script_len {
                                        format!("implies length {}.", l)
                                    } else {
                                        "would cause an out-of-bounds read.".to_owned()
                                    },
                                );
                            } else {
                                let sig_len = sig_and_type_len.unwrap() - 1;

                                let sig = secp256k1::ecdsa::Signature::from_der(
                                    &txin.script_sig.0[1..1 + sig_len],
                                );
                                let hash_type = txin.script_sig.0[1 + sig_len];
                                let pubkey_bytes = &txin.script_sig.0[1 + sig_len + 2..];
                                let pubkey = secp256k1::PublicKey::from_slice(pubkey_bytes);

                                if let Err(e) = sig {
                                    eprintln!(
                                        "    ⚠️  Txin {} has invalid signature encoding: {}",
                                        i, e
                                    );
                                }
                                if let Err(e) = pubkey {
                                    eprintln!(
                                        "    ⚠️  Txin {} has invalid pubkey encoding: {}",
                                        i, e
                                    );
                                }
                                if let (Ok(sig), Ok(pubkey)) = (sig, pubkey) {
                                    #[allow(deprecated)]
                                    if pubkey_to_address(&pubkey) != addr {
                                        eprintln!("    ⚠️  Txin {} pubkey does not match coin's script_pubkey", i);
                                    }

                                    let sighash = signature_hash(
                                        tx,
                                        &SignableInput::Transparent {
                                            hash_type,
                                            index: i,
                                            // For P2PKH these are the same.
                                            script_code: &coin.script_pubkey,
                                            script_pubkey: &coin.script_pubkey,
                                            value: coin.value,
                                        },
                                        txid_parts,
                                    );
                                    let msg = secp256k1::Message::from_slice(sighash.as_ref())
                                        .expect("signature_hash() returns correct length");

                                    if let Err(e) = ctx.verify_ecdsa(&msg, &sig, &pubkey) {
                                        eprintln!("    ⚠️  Spend {} is invalid: {}", i, e);
                                        eprintln!(
                                            "     - sighash is {}",
                                            hex::encode(sighash.as_ref())
                                        );
                                        eprintln!("     - pubkey is {}", hex::encode(pubkey_bytes));
                                    }
                                }
                            }
                        }
                        // TODO: Check P2SH structure.
                        Some(TransparentAddress::ScriptHash(_)) => {
                            eprintln!("  🔎 \"transparentcoins\"[{}] is a P2SH coin.", i);
                        }
                        // TODO: Check arbitrary scripts.
                        None => {
                            eprintln!(
                                "  🔎 \"transparentcoins\"[{}] has a script we can't check yet.",
                                i
                            );
                        }
                    }
                }
            } else {
                eprintln!(
                    "  🔎 To check transparent inputs, add \"transparentcoins\" array to context."
                );
                eprintln!("     The following transparent inputs are required: ");
                for txin in &bundle.vin {
                    eprintln!(
                        "     - txid {}, index {}",
                        TxId::from_bytes(*txin.prevout.hash()),
                        txin.prevout.n()
                    )
                }
            }
        }
        if !bundle.vout.is_empty() {
            eprintln!(" - {} transparent output(s)", bundle.vout.len());
            for txout in &bundle.vout {
                eprintln!(
                    "     - {}",
                    serde_json::to_string(&ZTxOut::from(txout.clone())).unwrap()
                );
            }
        }
    }

    if let Some(bundle) = tx.sprout_bundle() {
        eprintln!(" - {} Sprout JoinSplit(s)", bundle.joinsplits.len());

        // TODO: Verify Sprout proofs once we can access the Sprout bundle parts.

        match ed25519_zebra::VerificationKey::try_from(bundle.joinsplit_pubkey) {
            Err(e) => eprintln!("  ⚠️  joinsplitPubkey is invalid: {}", e),
            Ok(vk) => {
                if let Some(sighash) = &common_sighash {
                    if let Err(e) = vk.verify(
                        &ed25519_zebra::Signature::from(bundle.joinsplit_sig),
                        sighash.as_ref(),
                    ) {
                        eprintln!("  ⚠️  joinsplitSig is invalid: {}", e);
                    }
                } else {
                    eprintln!(
                        "  🔎 To check Sprout JoinSplit(s), add \"transparentcoins\" array to context"
                    );
                }
            }
        }
    }

    if let Some(bundle) = tx.sapling_bundle() {
        assert!(!(bundle.shielded_spends().is_empty() && bundle.shielded_outputs().is_empty()));

        // TODO: Separate into checking proofs, signatures, and other structural details.
        let mut ctx = SaplingVerificationContext::new();

        if !bundle.shielded_spends().is_empty() {
            eprintln!(" - {} Sapling Spend(s)", bundle.shielded_spends().len());
            if let Some(sighash) = &common_sighash {
                for (i, spend) in bundle.shielded_spends().iter().enumerate() {
                    if !ctx.check_spend(
                        spend.cv(),
                        *spend.anchor(),
                        &spend.nullifier().0,
                        *spend.rk(),
                        sighash.as_ref(),
                        *spend.spend_auth_sig(),
                        groth16::Proof::read(&spend.zkproof()[..]).unwrap(),
                        &GROTH16_PARAMS.spend_vk,
                    ) {
                        eprintln!("  ⚠️  Spend {} is invalid", i);
                    }
                }
            } else {
                eprintln!(
                    "  🔎 To check Sapling Spend(s), add \"transparentcoins\" array to context"
                );
            }
        }

        if !bundle.shielded_outputs().is_empty() {
            eprintln!(" - {} Sapling Output(s)", bundle.shielded_outputs().len());
            for (i, output) in bundle.shielded_outputs().iter().enumerate() {
                if is_coinbase {
                    if let Some((params, addr_net)) = context
                        .as_ref()
                        .and_then(|ctx| ctx.network().zip(ctx.addr_network()))
                    {
                        if let Some((note, addr, memo)) = try_output_recovery_with_ovk(
                            &SaplingDomain::new(sapling_zip212_enforcement(
                                &params,
                                height.unwrap(),
                            )),
                            &sapling::keys::OutgoingViewingKey([0; 32]),
                            output,
                            output.cv(),
                            output.out_ciphertext(),
                        ) {
                            if note.value().inner() == 0 {
                                eprintln!("   - Output {} (dummy output):", i);
                            } else {
                                let zaddr = ZcashAddress::from_sapling(addr_net, addr.to_bytes());

                                eprintln!("   - Output {}:", i);
                                eprintln!("     - {}", zaddr);
                                eprintln!("     - {}", render_value(note.value().inner()));
                            }
                            let memo = MemoBytes::from_bytes(&memo).expect("correct length");
                            eprintln!("     - {}", render_memo(memo));
                        } else {
                            eprintln!(
                                "  ⚠️  Output {} is not recoverable with the all-zeros OVK",
                                i
                            );
                        }
                    } else {
                        eprintln!("  🔎 To check Sapling coinbase rules, add \"network\" to context (either \"main\" or \"test\")");
                    }
                }

                if !ctx.check_output(
                    output.cv(),
                    *output.cmu(),
                    jubjub::ExtendedPoint::from_bytes(&output.ephemeral_key().0).unwrap(),
                    groth16::Proof::read(&output.zkproof()[..]).unwrap(),
                    &GROTH16_PARAMS.output_vk,
                ) {
                    eprintln!("  ⚠️  Output {} is invalid", i);
                }
            }
        }

        if let Some(sighash) = &common_sighash {
            if !ctx.final_check(
                *bundle.value_balance(),
                sighash.as_ref(),
                bundle.authorization().binding_sig,
            ) {
                eprintln!("⚠️  Sapling bindingSig is invalid");
            }
        } else {
            eprintln!("🔎 To check Sapling bindingSig, add \"transparentcoins\" array to context");
        }
    }

    if let Some(bundle) = tx.orchard_bundle() {
        eprintln!(" - {} Orchard Action(s)", bundle.actions().len());

        // Orchard nullifiers must not be duplicated within a transaction.
        let mut nullifiers = HashMap::<[u8; 32], Vec<usize>>::default();
        for (i, action) in bundle.actions().iter().enumerate() {
            nullifiers
                .entry(action.nullifier().to_bytes())
                .or_insert_with(Vec::new)
                .push(i);
        }
        for (_, indices) in nullifiers {
            if indices.len() > 1 {
                eprintln!("⚠️  Nullifier is duplicated between actions {:?}", indices);
            }
        }

        if is_coinbase {
            // All coinbase outputs must be decryptable with the all-zeroes OVK.
            for (i, action) in bundle.actions().iter().enumerate() {
                let ovk = orchard::keys::OutgoingViewingKey::from([0u8; 32]);
                if let Some((note, addr, memo)) = try_output_recovery_with_ovk(
                    &OrchardDomain::for_action(action),
                    &ovk,
                    action,
                    action.cv_net(),
                    &action.encrypted_note().out_ciphertext,
                ) {
                    if note.value().inner() == 0 {
                        eprintln!("   - Output {} (dummy output):", i);
                    } else {
                        eprintln!("   - Output {}:", i);

                        if let Some(net) = context.as_ref().and_then(|ctx| ctx.addr_network()) {
                            assert_eq!(note.recipient(), addr);
                            // Construct a single-receiver UA.
                            let zaddr = ZcashAddress::from_unified(
                                net,
                                unified::Address::try_from_items(vec![unified::Receiver::Orchard(
                                    addr.to_raw_address_bytes(),
                                )])
                                .unwrap(),
                            );
                            eprintln!("     - {}", zaddr);
                        } else {
                            eprintln!("    🔎 To show recipient address, add \"network\" to context (either \"main\" or \"test\")");
                        }

                        eprintln!("     - {}", render_value(note.value().inner()));
                    }
                    eprintln!(
                        "     - {}",
                        render_memo(MemoBytes::from_bytes(&memo).unwrap())
                    );
                } else {
                    eprintln!(
                        "  ⚠️  Output {} is not recoverable with the all-zeros OVK",
                        i
                    );
                }
            }
        }

        if let Some(sighash) = &common_sighash {
            for (i, action) in bundle.actions().iter().enumerate() {
                if let Err(e) = action.rk().verify(sighash.as_ref(), action.authorization()) {
                    eprintln!("  ⚠️  Action {} spendAuthSig is invalid: {}", i, e);
                }
            }
        } else {
            eprintln!(
                "🔎 To check Orchard Action signatures, add \"transparentcoins\" array to context"
            );
        }

        if let Err(e) = bundle.verify_proof(&ORCHARD_VK) {
            eprintln!("⚠️  Orchard proof is invalid: {:?}", e);
        }
    }
}
