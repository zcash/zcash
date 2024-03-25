use std::convert::TryInto;
use std::iter;

use bech32::ToBase32;
use secrecy::Zeroize;
use zcash_address::{
    unified::{self, Encoding},
    ToAddress, ZcashAddress,
};
use zcash_primitives::{
    consensus::Parameters,
    legacy::{
        keys::{AccountPrivKey, IncomingViewingKey},
        TransparentAddress,
    },
    zip32, zip339,
};

use crate::Context;

pub(crate) fn inspect_mnemonic(
    mnemonic: zip339::Mnemonic,
    lang: zip339::Language,
    context: Option<Context>,
) {
    eprintln!("Mnemonic phrase");
    eprintln!(" - Language: {}", lang);

    if let Some(((network, addr_net), accounts)) =
        context.and_then(|c| c.network().zip(c.addr_network()).zip(c.accounts()))
    {
        let mut seed = mnemonic.to_seed("");
        for account in accounts {
            eprintln!(" - Account {}:", u32::from(account));

            let orchard_fvk = match orchard::keys::SpendingKey::from_zip32_seed(
                &seed,
                network.coin_type(),
                account,
            ) {
                Ok(sk) => Some(orchard::keys::FullViewingKey::from(&sk)),
                Err(e) => {
                    eprintln!(
                        "  ⚠️  No valid Orchard key for this account under this seed: {}",
                        e
                    );
                    None
                }
            };

            eprintln!("   - Sapling:");
            let sapling_master = sapling::zip32::ExtendedSpendingKey::master(&seed);
            let sapling_extsk = sapling::zip32::ExtendedSpendingKey::from_path(
                &sapling_master,
                &[
                    zip32::ChildIndex::hardened(32),
                    zip32::ChildIndex::hardened(network.coin_type()),
                    account.into(),
                ],
            );
            #[allow(deprecated)]
            let sapling_extfvk = sapling_extsk.to_extended_full_viewing_key();
            let sapling_default_addr = sapling_extfvk.default_address();

            let mut sapling_extsk_bytes = vec![];
            sapling_extsk.write(&mut sapling_extsk_bytes).unwrap();
            eprintln!(
                "     - ExtSK:  {}",
                bech32::encode(
                    network.hrp_sapling_extended_spending_key(),
                    sapling_extsk_bytes.to_base32(),
                    bech32::Variant::Bech32,
                )
                .unwrap(),
            );

            let mut sapling_extfvk_bytes = vec![];
            sapling_extfvk.write(&mut sapling_extfvk_bytes).unwrap();
            eprintln!(
                "     - ExtFVK: {}",
                bech32::encode(
                    network.hrp_sapling_extended_full_viewing_key(),
                    sapling_extfvk_bytes.to_base32(),
                    bech32::Variant::Bech32,
                )
                .unwrap(),
            );

            let sapling_addr_bytes = sapling_default_addr.1.to_bytes();
            eprintln!(
                "     - Default address: {}",
                bech32::encode(
                    network.hrp_sapling_payment_address(),
                    sapling_addr_bytes.to_base32(),
                    bech32::Variant::Bech32,
                )
                .unwrap(),
            );

            let transparent_fvk = match AccountPrivKey::from_seed(&network, &seed, account)
                .map(|sk| sk.to_account_pubkey())
            {
                Ok(fvk) => {
                    eprintln!("   - Transparent:");
                    match fvk.derive_external_ivk().map(|ivk| ivk.default_address().0) {
                        Ok(addr) => eprintln!(
                            "     - Default address: {}",
                            match addr {
                                TransparentAddress::PublicKeyHash(data) => ZcashAddress::from_transparent_p2pkh(addr_net, data),
                                TransparentAddress::ScriptHash(_) => unreachable!(),
                            }.encode(),
                        ),
                        Err(e) => eprintln!(
                            "    ⚠️  No valid transparent default address for this account under this seed: {:?}",
                            e
                        ),
                    }

                    Some(fvk)
                }
                Err(e) => {
                    eprintln!(
                        "  ⚠️  No valid transparent key for this account under this seed: {:?}",
                        e
                    );
                    None
                }
            };

            let items: Vec<_> = iter::empty()
                .chain(
                    orchard_fvk
                        .map(|fvk| fvk.to_bytes())
                        .map(unified::Fvk::Orchard),
                )
                .chain(Some(unified::Fvk::Sapling(
                    sapling_extfvk_bytes[41..].try_into().unwrap(),
                )))
                .chain(
                    transparent_fvk
                        .map(|fvk| fvk.serialize()[..].try_into().unwrap())
                        .map(unified::Fvk::P2pkh),
                )
                .collect();
            let item_names: Vec<_> = items
                .iter()
                .map(|item| match item {
                    unified::Fvk::Orchard(_) => "Orchard",
                    unified::Fvk::Sapling(_) => "Sapling",
                    unified::Fvk::P2pkh(_) => "Transparent",
                    unified::Fvk::Unknown { .. } => unreachable!(),
                })
                .collect();

            eprintln!("   - Unified ({}):", item_names.join(", "));
            let ufvk = unified::Ufvk::try_from_items(items).unwrap();
            eprintln!("     - UFVK: {}", ufvk.encode(&addr_net));
        }
        seed.zeroize();
    } else {
        eprintln!("🔎 To show account details, add \"network\" (either \"main\" or \"test\") and \"accounts\" array to context");
    }

    eprintln!();
    eprintln!(
        "WARNING: This mnemonic phrase is now likely cached in your terminal's history buffer."
    );
}
