use ff::{PrimeField, PrimeFieldRepr};
use pairing::bls12_381::Bls12;
use rand_core::{OsRng, RngCore};
use zcash_primitives::jubjub::{edwards, JubjubBls12};
use zcash_primitives::primitives::{Diversifier, ViewingKey};

use crate::{
    librustzcash_sapling_generate_r, librustzcash_sapling_ka_agree,
    librustzcash_sapling_ka_derivepublic,
};

#[test]
fn test_key_agreement() {
    let params = JubjubBls12::new();
    let mut rng = OsRng;

    // Create random viewing key
    let vk = ViewingKey::<Bls12> {
        ak: edwards::Point::rand(&mut rng, &params).mul_by_cofactor(&params),
        nk: edwards::Point::rand(&mut rng, &params).mul_by_cofactor(&params),
    };

    // Create a random address with the viewing key
    let addr = loop {
        let mut d = [0; 11];
        rng.fill_bytes(&mut d);
        match vk.to_payment_address(Diversifier(d), &params) {
            Some(a) => break a,
            None => {}
        }
    };

    // Grab ivk from our viewing key in serialized form
    let ivk = vk.ivk();
    let mut ivk_serialized = [0u8; 32];
    ivk.into_repr().write_le(&mut ivk_serialized[..]).unwrap();

    // Create random esk
    let mut esk = [0u8; 32];
    librustzcash_sapling_generate_r(&mut esk);

    // The sender will create a shared secret with the recipient
    // by multiplying the pk_d from their address with the esk
    // we randomly generated
    let mut shared_secret_sender = [0u8; 32];

    // Serialize pk_d for the call to librustzcash_sapling_ka_agree
    let mut addr_pk_d = [0u8; 32];
    addr.pk_d().write(&mut addr_pk_d[..]).unwrap();

    assert!(librustzcash_sapling_ka_agree(
        &addr_pk_d,
        &esk,
        &mut shared_secret_sender
    ));

    // Create epk for the recipient, placed in the transaction. Computed
    // using the diversifier and esk.
    let mut epk = [0u8; 32];
    assert!(librustzcash_sapling_ka_derivepublic(
        &addr.diversifier().0,
        &esk,
        &mut epk
    ));

    // Create sharedSecret with ephemeral key
    let mut shared_secret_recipient = [0u8; 32];
    assert!(librustzcash_sapling_ka_agree(
        &epk,
        &ivk_serialized,
        &mut shared_secret_recipient
    ));

    assert!(!shared_secret_sender.iter().all(|&v| v == 0));
    assert_eq!(shared_secret_sender, shared_secret_recipient);
}
