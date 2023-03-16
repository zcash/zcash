use memuse::DynamicUsage;
use zcash_primitives::{
    consensus::{self, BlockHeight},
    constants,
};

/// Chain parameters for the networks supported by `zcashd`.
#[derive(Clone, Copy)]
pub(crate) enum Network {
    Consensus(consensus::Network),
    RegTest {
        overwinter: Option<BlockHeight>,
        sapling: Option<BlockHeight>,
        blossom: Option<BlockHeight>,
        heartwood: Option<BlockHeight>,
        canopy: Option<BlockHeight>,
        nu5: Option<BlockHeight>,
    },
}

impl DynamicUsage for Network {
    fn dynamic_usage(&self) -> usize {
        match self {
            Network::Consensus(params) => params.dynamic_usage(),
            // We know that `Option<BlockHeight>` allocates no memory.
            Network::RegTest { .. } => 0,
        }
    }

    fn dynamic_usage_bounds(&self) -> (usize, Option<usize>) {
        match self {
            Network::Consensus(params) => params.dynamic_usage_bounds(),
            // We know that `Option<BlockHeight>` allocates no memory.
            Network::RegTest { .. } => (0, Some(0)),
        }
    }
}

/// Constructs a `Network` from the given network string.
///
/// The heights are only for constructing a regtest network, and are ignored otherwise.
pub(crate) fn network(
    network: &str,
    overwinter: i32,
    sapling: i32,
    blossom: i32,
    heartwood: i32,
    canopy: i32,
    nu5: i32,
) -> Result<Box<Network>, &'static str> {
    let i32_to_optional_height = |n: i32| {
        if n.is_negative() {
            None
        } else {
            Some(BlockHeight::from_u32(n.unsigned_abs()))
        }
    };

    let params = match network {
        "main" => Network::Consensus(consensus::Network::MainNetwork),
        "test" => Network::Consensus(consensus::Network::TestNetwork),
        "regtest" => Network::RegTest {
            overwinter: i32_to_optional_height(overwinter),
            sapling: i32_to_optional_height(sapling),
            blossom: i32_to_optional_height(blossom),
            heartwood: i32_to_optional_height(heartwood),
            canopy: i32_to_optional_height(canopy),
            nu5: i32_to_optional_height(nu5),
        },
        _ => return Err("Unsupported network kind"),
    };

    Ok(Box::new(params))
}

impl consensus::Parameters for Network {
    fn activation_height(&self, nu: consensus::NetworkUpgrade) -> Option<consensus::BlockHeight> {
        match self {
            Self::Consensus(params) => params.activation_height(nu),
            Self::RegTest {
                overwinter,
                sapling,
                blossom,
                heartwood,
                canopy,
                nu5,
            } => match nu {
                consensus::NetworkUpgrade::Overwinter => *overwinter,
                consensus::NetworkUpgrade::Sapling => *sapling,
                consensus::NetworkUpgrade::Blossom => *blossom,
                consensus::NetworkUpgrade::Heartwood => *heartwood,
                consensus::NetworkUpgrade::Canopy => *canopy,
                consensus::NetworkUpgrade::Nu5 => *nu5,
            },
        }
    }

    fn coin_type(&self) -> u32 {
        match self {
            Self::Consensus(params) => params.coin_type(),
            Self::RegTest { .. } => constants::regtest::COIN_TYPE,
        }
    }

    fn address_network(&self) -> Option<zcash_address::Network> {
        match self {
            Self::Consensus(params) => params.address_network(),
            Self::RegTest { .. } => Some(zcash_address::Network::Regtest),
        }
    }

    fn hrp_sapling_extended_spending_key(&self) -> &str {
        match self {
            Self::Consensus(params) => params.hrp_sapling_extended_spending_key(),
            Self::RegTest { .. } => constants::regtest::HRP_SAPLING_EXTENDED_SPENDING_KEY,
        }
    }

    fn hrp_sapling_extended_full_viewing_key(&self) -> &str {
        match self {
            Self::Consensus(params) => params.hrp_sapling_extended_full_viewing_key(),
            Self::RegTest { .. } => constants::regtest::HRP_SAPLING_EXTENDED_FULL_VIEWING_KEY,
        }
    }

    fn hrp_sapling_payment_address(&self) -> &str {
        match self {
            Self::Consensus(params) => params.hrp_sapling_payment_address(),
            Self::RegTest { .. } => constants::regtest::HRP_SAPLING_PAYMENT_ADDRESS,
        }
    }

    fn b58_pubkey_address_prefix(&self) -> [u8; 2] {
        match self {
            Self::Consensus(params) => params.b58_pubkey_address_prefix(),
            Self::RegTest { .. } => constants::regtest::B58_PUBKEY_ADDRESS_PREFIX,
        }
    }

    fn b58_script_address_prefix(&self) -> [u8; 2] {
        match self {
            Self::Consensus(params) => params.b58_script_address_prefix(),
            Self::RegTest { .. } => constants::regtest::B58_SCRIPT_ADDRESS_PREFIX,
        }
    }
}
