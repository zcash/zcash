// Copyright (c) 2020-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

use bellman::groth16::Proof;
use group::GroupEncoding;

use zcash_primitives::{
    sapling::redjubjub::{self, Signature},
    transaction::components::Amount,
};
use zcash_proofs::sapling::SaplingVerificationContext;

use super::GROTH_PROOF_SIZE;
use super::{de_ct, SAPLING_OUTPUT_VK, SAPLING_SPEND_VK};

#[cxx::bridge(namespace = "sapling")]
mod ffi {
    extern "Rust" {
        type Verifier;

        fn init_verifier() -> Box<Verifier>;
        fn check_spend(
            &mut self,
            cv: &[u8; 32],
            anchor: &[u8; 32],
            nullifier: &[u8; 32],
            rk: &[u8; 32],
            zkproof: &[u8; 192], // GROTH_PROOF_SIZE
            spend_auth_sig: &[u8; 64],
            sighash_value: &[u8; 32],
        ) -> bool;
        fn check_output(
            &mut self,
            cv: &[u8; 32],
            cm: &[u8; 32],
            ephemeral_key: &[u8; 32],
            zkproof: &[u8; 192], // GROTH_PROOF_SIZE
        ) -> bool;
        fn final_check(
            &self,
            value_balance: i64,
            binding_sig: &[u8; 64],
            sighash_value: &[u8; 32],
        ) -> bool;
    }
}

struct Verifier(SaplingVerificationContext);

fn init_verifier() -> Box<Verifier> {
    // We consider ZIP 216 active all of the time because blocks prior to NU5
    // activation (on mainnet and testnet) did not contain Sapling transactions
    // that violated its canonicity rule.
    Box::new(Verifier(SaplingVerificationContext::new(true)))
}

impl Verifier {
    fn check_spend(
        &mut self,
        cv: &[u8; 32],
        anchor: &[u8; 32],
        nullifier: &[u8; 32],
        rk: &[u8; 32],
        zkproof: &[u8; GROTH_PROOF_SIZE],
        spend_auth_sig: &[u8; 64],
        sighash_value: &[u8; 32],
    ) -> bool {
        // Deserialize the value commitment
        let cv = match de_ct(jubjub::ExtendedPoint::from_bytes(cv)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the anchor, which should be an element
        // of Fr.
        let anchor = match de_ct(bls12_381::Scalar::from_bytes(anchor)) {
            Some(a) => a,
            None => return false,
        };

        // Deserialize rk
        let rk = match redjubjub::PublicKey::read(&rk[..]) {
            Ok(p) => p,
            Err(_) => return false,
        };

        // Deserialize the signature
        let spend_auth_sig = match Signature::read(&spend_auth_sig[..]) {
            Ok(sig) => sig,
            Err(_) => return false,
        };

        // Deserialize the proof
        let zkproof = match Proof::read(&zkproof[..]) {
            Ok(p) => p,
            Err(_) => return false,
        };

        self.0.check_spend(
            cv,
            anchor,
            nullifier,
            rk,
            sighash_value,
            spend_auth_sig,
            zkproof,
            unsafe { SAPLING_SPEND_VK.as_ref() }.unwrap(),
        )
    }
    fn check_output(
        &mut self,
        cv: &[u8; 32],
        cm: &[u8; 32],
        ephemeral_key: &[u8; 32],
        zkproof: &[u8; GROTH_PROOF_SIZE],
    ) -> bool {
        // Deserialize the value commitment
        let cv = match de_ct(jubjub::ExtendedPoint::from_bytes(cv)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the commitment, which should be an element
        // of Fr.
        let cm = match de_ct(bls12_381::Scalar::from_bytes(cm)) {
            Some(a) => a,
            None => return false,
        };

        // Deserialize the ephemeral key
        let ephemeral_key = match de_ct(jubjub::ExtendedPoint::from_bytes(ephemeral_key)) {
            Some(p) => p,
            None => return false,
        };

        // Deserialize the proof
        let zkproof = match Proof::read(&zkproof[..]) {
            Ok(p) => p,
            Err(_) => return false,
        };

        self.0.check_output(
            cv,
            cm,
            ephemeral_key,
            zkproof,
            unsafe { SAPLING_OUTPUT_VK.as_ref() }.unwrap(),
        )
    }
    fn final_check(
        &self,
        value_balance: i64,
        binding_sig: &[u8; 64],
        sighash_value: &[u8; 32],
    ) -> bool {
        let value_balance = match Amount::from_i64(value_balance) {
            Ok(vb) => vb,
            Err(()) => return false,
        };

        // Deserialize the signature
        let binding_sig = match Signature::read(&binding_sig[..]) {
            Ok(sig) => sig,
            Err(_) => return false,
        };

        self.0
            .final_check(value_balance, sighash_value, binding_sig)
    }
}
