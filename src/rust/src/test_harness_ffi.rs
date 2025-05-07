use std::convert::TryFrom;

use group::{ff::Field, Group, GroupEncoding};
use rand::{thread_rng, Rng};
use sapling::value::ValueCommitment;
use zcash_note_encryption::EphemeralKeyBytes;
use zcash_protocol::value::ZatBalance;

pub(crate) fn test_only_invalid_sapling_bundle(
    spends: usize,
    outputs: usize,
    value_balance: i64,
) -> Box<crate::sapling::Bundle> {
    let mut rng = thread_rng();

    fn gen_array<R: Rng, const N: usize>(mut rng: R) -> [u8; N] {
        let mut tmp = [0; N];
        rng.fill(&mut tmp[..]);
        tmp
    }

    let spends = (0..spends)
        .map(|_| {
            let cv = ValueCommitment::from_bytes_not_small_order(
                &jubjub::ExtendedPoint::random(&mut rng).to_bytes(),
            )
            .unwrap();
            let anchor = jubjub::Base::random(&mut rng);
            let nullifier = sapling::Nullifier(rng.gen());
            let rk = redjubjub::VerificationKey::try_from(
                jubjub::ExtendedPoint::random(&mut rng).to_bytes(),
            )
            .unwrap();
            let zkproof = gen_array(&mut rng);
            let spend_auth_sig = {
                let tmp = gen_array::<_, 64>(&mut rng);
                redjubjub::Signature::from(tmp)
            };

            sapling::bundle::SpendDescription::from_parts(
                cv,
                anchor,
                nullifier,
                rk,
                zkproof,
                spend_auth_sig,
            )
        })
        .collect();

    let outputs = (0..outputs)
        .map(|_| {
            let cv = ValueCommitment::from_bytes_not_small_order(
                &jubjub::ExtendedPoint::random(&mut rng).to_bytes(),
            )
            .unwrap();
            let cmu = sapling::note::ExtractedNoteCommitment::from_bytes(
                &jubjub::Base::random(&mut rng).to_bytes(),
            )
            .unwrap();
            let ephemeral_key =
                EphemeralKeyBytes(jubjub::ExtendedPoint::random(&mut rng).to_bytes());
            let enc_ciphertext = gen_array(&mut rng);
            let out_ciphertext = gen_array(&mut rng);
            let zkproof = gen_array(&mut rng);

            sapling::bundle::OutputDescription::from_parts(
                cv,
                cmu,
                ephemeral_key,
                enc_ciphertext,
                out_ciphertext,
                zkproof,
            )
        })
        .collect();

    let binding_sig = {
        let tmp = gen_array::<_, 64>(&mut rng);
        redjubjub::Signature::from(tmp)
    };

    let bundle = sapling::Bundle::from_parts(
        spends,
        outputs,
        ZatBalance::from_i64(value_balance).unwrap(),
        sapling::bundle::Authorized { binding_sig },
    );
    Box::new(crate::sapling::Bundle(bundle))
}

pub(crate) fn test_only_replace_sapling_nullifier(
    bundle: &mut crate::sapling::Bundle,
    spend_index: usize,
    nullifier: [u8; 32],
) {
    if let Some(bundle) = bundle.0.as_mut() {
        *bundle = sapling::Bundle::from_parts(
            bundle
                .shielded_spends()
                .iter()
                .enumerate()
                .map(|(i, spend)| {
                    if i == spend_index {
                        sapling::bundle::SpendDescription::from_parts(
                            spend.cv().clone(),
                            *spend.anchor(),
                            sapling::Nullifier(nullifier),
                            *spend.rk(),
                            *spend.zkproof(),
                            *spend.spend_auth_sig(),
                        )
                    } else {
                        spend.clone()
                    }
                })
                .collect(),
            bundle.shielded_outputs().to_vec(),
            *bundle.value_balance(),
            *bundle.authorization(),
        )
        .expect("Prior bundle was valid");
    }
}

pub(crate) fn test_only_replace_sapling_output_parts(
    bundle: &mut crate::sapling::Bundle,
    output_index: usize,
    cmu: [u8; 32],
    enc_ciphertext: [u8; 580],
    out_ciphertext: [u8; 80],
) {
    if let Some(bundle) = bundle.0.as_mut() {
        *bundle = sapling::Bundle::from_parts(
            bundle.shielded_spends().to_vec(),
            bundle
                .shielded_outputs()
                .iter()
                .enumerate()
                .map(|(i, output)| {
                    if i == output_index {
                        sapling::bundle::OutputDescription::from_parts(
                            output.cv().clone(),
                            sapling::note::ExtractedNoteCommitment::from_bytes(&cmu).unwrap(),
                            output.ephemeral_key().clone(),
                            enc_ciphertext,
                            out_ciphertext,
                            *output.zkproof(),
                        )
                    } else {
                        output.clone()
                    }
                })
                .collect(),
            *bundle.value_balance(),
            *bundle.authorization(),
        )
        .expect("Prior bundle was valid");
    }
}
