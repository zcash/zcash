use orchard::{
    bundle::Authorized,
    primitives::redpallas::{Signature, SpendAuth},
};
use zcash_primitives::transaction::components::Amount;

#[cxx::bridge(namespace = "orchard_bundle")]
mod ffi {
    extern "Rust" {
        type Action;
        type Bundle;
        type OrchardBundle;

        fn cv(self: &Action) -> [u8; 32];
        fn nullifier(self: &Action) -> [u8; 32];
        fn rk(self: &Action) -> [u8; 32];
        fn cmx(self: &Action) -> [u8; 32];
        fn ephemeral_key(self: &Action) -> [u8; 32];
        fn enc_ciphertext(self: &Action) -> [u8; 580];
        fn out_ciphertext(self: &Action) -> [u8; 80];
        fn spend_auth_sig(self: &Action) -> [u8; 64];

        unsafe fn from_tx_bundle(bundle: *const OrchardBundle) -> Box<Bundle>;
        fn actions(self: &Bundle) -> Vec<Action>;
        fn num_actions(self: &Bundle) -> usize;
        fn enable_spends(self: &Bundle) -> bool;
        fn enable_outputs(self: &Bundle) -> bool;
        fn value_balance_zat(self: &Bundle) -> i64;
        fn anchor(self: &Bundle) -> [u8; 32];
        fn proof(self: &Bundle) -> Vec<u8>;
        fn binding_sig(self: &Bundle) -> [u8; 64];
    }
}

pub struct Action(orchard::Action<Signature<SpendAuth>>);

impl Action {
    fn cv(&self) -> [u8; 32] {
        self.0.cv_net().to_bytes()
    }

    fn nullifier(&self) -> [u8; 32] {
        self.0.nullifier().to_bytes()
    }

    fn rk(&self) -> [u8; 32] {
        self.0.rk().into()
    }

    fn cmx(&self) -> [u8; 32] {
        self.0.cmx().to_bytes()
    }

    fn ephemeral_key(&self) -> [u8; 32] {
        self.0.encrypted_note().epk_bytes
    }

    fn enc_ciphertext(&self) -> [u8; 580] {
        self.0.encrypted_note().enc_ciphertext
    }

    fn out_ciphertext(&self) -> [u8; 80] {
        self.0.encrypted_note().out_ciphertext
    }

    fn spend_auth_sig(&self) -> [u8; 64] {
        self.0.authorization().into()
    }
}

pub struct Bundle(Option<orchard::Bundle<Authorized, Amount>>);
pub struct OrchardBundle;
unsafe fn from_tx_bundle(bundle: *const OrchardBundle) -> Box<Bundle> {
    Box::new(Bundle(
        { (bundle as *const orchard::Bundle<Authorized, Amount>).as_ref() }.cloned(),
    ))
}

impl Bundle {
    fn actions(&self) -> Vec<Action> {
        self.0
            .iter()
            .flat_map(|b| b.actions().iter())
            .cloned()
            .map(Action)
            .collect()
    }

    fn num_actions(&self) -> usize {
        self.0.as_ref().map(|b| b.actions().len()).unwrap_or(0)
    }

    fn enable_spends(&self) -> bool {
        self.0
            .as_ref()
            .map(|b| b.flags().spends_enabled())
            .unwrap_or(false)
    }

    fn enable_outputs(&self) -> bool {
        self.0
            .as_ref()
            .map(|b| b.flags().outputs_enabled())
            .unwrap_or(false)
    }

    fn value_balance_zat(&self) -> i64 {
        self.0
            .as_ref()
            .map(|b| b.value_balance().into())
            .unwrap_or(0)
    }

    fn anchor(&self) -> [u8; 32] {
        self.0
            .as_ref()
            .expect("Bundle actions should have been checked to be non-empty")
            .anchor()
            .to_bytes()
    }

    fn proof(&self) -> Vec<u8> {
        self.0
            .as_ref()
            .expect("Bundle actions should have been checked to be non-empty")
            .authorization()
            .proof()
            .as_ref()
            .to_vec()
    }

    fn binding_sig(&self) -> [u8; 64] {
        self.0
            .as_ref()
            .expect("Bundle actions should have been checked to be non-empty")
            .authorization()
            .binding_signature()
            .into()
    }
}
