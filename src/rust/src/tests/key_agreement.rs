use group::{Group, GroupEncoding};
use rand_core::{OsRng, RngCore};
use zcash_primitives::sapling::{Diversifier, ViewingKey};

use crate::{
    librustzcash_sapling_generate_r, librustzcash_sapling_ka_agree,
    librustzcash_sapling_ka_derivepublic,
};

#[test]
fn test_key_agreement() {
    let mut rng = OsRng;

    // Create random viewing key
    let vk = ViewingKey {
        ak: jubjub::SubgroupPoint::random(&mut rng),
        nk: jubjub::SubgroupPoint::random(&mut rng),
    };

    // Create a random address with the viewing key
    let addr = loop {
        let mut d = [0; 11];
        rng.fill_bytes(&mut d);
        if let Some(a) = vk.to_payment_address(Diversifier(d)) {
            break a;
        }
    };

    // Grab ivk from our viewing key in serialized form
    let ivk = vk.ivk();
    let ivk_serialized = ivk.to_repr();

    // Create random esk
    let mut esk = [0u8; 32];
    librustzcash_sapling_generate_r(&mut esk);

    // The sender will create a shared secret with the recipient
    // by multiplying the pk_d from their address with the esk
    // we randomly generated
    let mut shared_secret_sender = [0u8; 32];

    // Serialize pk_d for the call to librustzcash_sapling_ka_agree
    let addr_pk_d = addr.pk_d().to_bytes();

    assert!(librustzcash_sapling_ka_agree(
        true,
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
        true,
        &epk,
        &ivk_serialized,
        &mut shared_secret_recipient
    ));

    assert!(!shared_secret_sender.iter().all(|&v| v == 0));
    assert_eq!(shared_secret_sender, shared_secret_recipient);
}
