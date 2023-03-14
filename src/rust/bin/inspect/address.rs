use zcash_address::{
    unified::{self, Container, Encoding},
    ConversionError, Network, ToAddress, ZcashAddress,
};

enum AddressKind {
    Sprout([u8; 64]),
    Sapling([u8; 43]),
    Unified(unified::Address),
    P2pkh([u8; 20]),
    P2sh([u8; 20]),
}

struct Address {
    net: Network,
    kind: AddressKind,
}

impl zcash_address::TryFromAddress for Address {
    type Error = ();

    fn try_from_sprout(net: Network, data: [u8; 64]) -> Result<Self, ConversionError<Self::Error>> {
        Ok(Address {
            net,
            kind: AddressKind::Sprout(data),
        })
    }

    fn try_from_sapling(
        net: Network,
        data: [u8; 43],
    ) -> Result<Self, ConversionError<Self::Error>> {
        Ok(Address {
            net,
            kind: AddressKind::Sapling(data),
        })
    }

    fn try_from_unified(
        net: Network,
        data: unified::Address,
    ) -> Result<Self, ConversionError<Self::Error>> {
        Ok(Address {
            net,
            kind: AddressKind::Unified(data),
        })
    }

    fn try_from_transparent_p2pkh(
        net: Network,
        data: [u8; 20],
    ) -> Result<Self, ConversionError<Self::Error>> {
        Ok(Address {
            net,
            kind: AddressKind::P2pkh(data),
        })
    }

    fn try_from_transparent_p2sh(
        net: Network,
        data: [u8; 20],
    ) -> Result<Self, ConversionError<Self::Error>> {
        Ok(Address {
            net,
            kind: AddressKind::P2sh(data),
        })
    }
}

pub(crate) fn inspect(addr: ZcashAddress) {
    eprintln!("Zcash address");

    match addr.convert::<Address>() {
        // TODO: Check for valid internals once we have migrated to a newer zcash_address
        // version with custom errors.
        Err(_) => unreachable!(),
        Ok(addr) => {
            eprintln!(
                " - Network: {}",
                match addr.net {
                    Network::Main => "main",
                    Network::Test => "testnet",
                    Network::Regtest => "regtest",
                }
            );
            eprintln!(
                " - Kind: {}",
                match addr.kind {
                    AddressKind::Sprout(_) => "Sprout",
                    AddressKind::Sapling(_) => "Sapling",
                    AddressKind::Unified(_) => "Unified Address",
                    AddressKind::P2pkh(_) => "Transparent P2PKH",
                    AddressKind::P2sh(_) => "Transparent P2SH",
                }
            );

            if let AddressKind::Unified(ua) = addr.kind {
                eprintln!(" - Receivers:");
                for receiver in ua.items() {
                    match receiver {
                        unified::Receiver::Orchard(data) => {
                            eprintln!(
                                "   - Orchard ({})",
                                unified::Address::try_from_items(vec![unified::Receiver::Orchard(
                                    data
                                )])
                                .unwrap()
                                .encode(&addr.net)
                            );
                        }
                        unified::Receiver::Sapling(data) => {
                            eprintln!(
                                "   - Sapling ({})",
                                ZcashAddress::from_sapling(addr.net, data)
                            );
                        }
                        unified::Receiver::P2pkh(data) => {
                            eprintln!(
                                "   - Transparent P2PKH ({})",
                                ZcashAddress::from_transparent_p2pkh(addr.net, data)
                            );
                        }
                        unified::Receiver::P2sh(data) => {
                            eprintln!(
                                "   - Transparent P2SH ({})",
                                ZcashAddress::from_transparent_p2sh(addr.net, data)
                            );
                        }
                        unified::Receiver::Unknown { typecode, data } => {
                            eprintln!("   - Unknown");
                            eprintln!("     - Typecode: {}", typecode);
                            eprintln!("     - Payload: {}", hex::encode(data));
                        }
                    }
                }
            }
        }
    }
}
