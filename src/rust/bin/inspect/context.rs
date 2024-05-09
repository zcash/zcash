use std::convert::TryFrom;
use std::fmt;
use std::str::FromStr;

use serde::{
    de::{Unexpected, Visitor},
    Deserialize, Serialize, Serializer,
};
use zcash_primitives::{
    consensus::Network,
    legacy::Script,
    transaction::components::{amount::NonNegativeAmount, transparent, TxOut},
    zip32::AccountId,
};

#[derive(Clone, Copy, Debug)]
pub(crate) struct JsonNetwork(Network);

struct JsonNetworkVisitor;

impl<'de> Visitor<'de> for JsonNetworkVisitor {
    type Value = JsonNetwork;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("either \"main\" or \"test\"")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        match v {
            "main" => Ok(JsonNetwork(Network::MainNetwork)),
            "test" => Ok(JsonNetwork(Network::TestNetwork)),
            _ => Err(serde::de::Error::invalid_value(
                Unexpected::Str(v),
                &"either \"main\" or \"test\"",
            )),
        }
    }
}

impl<'de> Deserialize<'de> for JsonNetwork {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_str(JsonNetworkVisitor)
    }
}

#[derive(Clone, Copy, Debug)]
struct JsonAccountId(AccountId);

struct JsonAccountIdVisitor;

impl<'de> Visitor<'de> for JsonAccountIdVisitor {
    type Value = JsonAccountId;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a u31")
    }

    fn visit_i64<E>(self, v: i64) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        u32::try_from(v)
            .map_err(|_| E::custom(format!("u32 out of range: {}", v)))
            .and_then(|a| {
                AccountId::try_from(a).map_err(|e| E::custom(format!("AccountId invalid: {}", e)))
            })
            .map(JsonAccountId)
    }

    fn visit_i128<E>(self, v: i128) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        u32::try_from(v)
            .map_err(|_| E::custom(format!("u32 out of range: {}", v)))
            .and_then(|a| {
                AccountId::try_from(a).map_err(|e| E::custom(format!("AccountId invalid: {}", e)))
            })
            .map(JsonAccountId)
    }

    fn visit_u64<E>(self, v: u64) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        u32::try_from(v)
            .map_err(|_| E::custom(format!("u32 out of range: {}", v)))
            .and_then(|a| {
                AccountId::try_from(a).map_err(|e| E::custom(format!("AccountId invalid: {}", e)))
            })
            .map(JsonAccountId)
    }

    fn visit_u128<E>(self, v: u128) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        u32::try_from(v)
            .map_err(|_| E::custom(format!("u32 out of range: {}", v)))
            .and_then(|a| {
                AccountId::try_from(a).map_err(|e| E::custom(format!("AccountId invalid: {}", e)))
            })
            .map(JsonAccountId)
    }
}

impl<'de> Deserialize<'de> for JsonAccountId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_u32(JsonAccountIdVisitor)
    }
}

#[derive(Clone, Copy, Debug)]
pub(crate) struct ZUint256(pub [u8; 32]);

struct ZUint256Visitor;

impl<'de> Visitor<'de> for ZUint256Visitor {
    type Value = ZUint256;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a hex-encoded 32-byte array")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let mut data = [0; 32];
        hex::decode_to_slice(v, &mut data).map_err(|e| match e {
            hex::FromHexError::InvalidHexCharacter { c, .. } => {
                serde::de::Error::invalid_value(Unexpected::Char(c), &"valid hex character")
            }
            hex::FromHexError::OddLength => {
                serde::de::Error::invalid_length(v.len(), &"an even-length string")
            }
            hex::FromHexError::InvalidStringLength => {
                serde::de::Error::invalid_length(v.len(), &"a 64-character string")
            }
        })?;
        data.reverse();
        Ok(ZUint256(data))
    }
}

impl<'de> Deserialize<'de> for ZUint256 {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_str(ZUint256Visitor)
    }
}

impl fmt::Display for ZUint256 {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut data = self.0;
        data.reverse();
        formatter.write_str(&hex::encode(data))
    }
}

#[derive(Clone, Debug)]
struct ZOutputValue(NonNegativeAmount);

struct ZOutputValueVisitor;

impl<'de> Visitor<'de> for ZOutputValueVisitor {
    type Value = ZOutputValue;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a non-negative integer number of zatoshis")
    }

    fn visit_u64<E>(self, v: u64) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        NonNegativeAmount::from_u64(v)
            .map(ZOutputValue)
            .map_err(|e| match e {
                zcash_protocol::value::BalanceError::Overflow => serde::de::Error::invalid_type(
                    Unexpected::Unsigned(v),
                    &"a valid zatoshi amount",
                ),
                zcash_protocol::value::BalanceError::Underflow => serde::de::Error::invalid_type(
                    Unexpected::Unsigned(v),
                    &"a non-negative zatoshi amount",
                ),
            })
    }
}

impl<'de> Deserialize<'de> for ZOutputValue {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_u64(ZOutputValueVisitor)
    }
}

impl Serialize for ZOutputValue {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_u64(u64::from(self.0))
    }
}

#[derive(Clone, Debug)]
struct ZScript(Script);

struct ZScriptVisitor;

impl<'de> Visitor<'de> for ZScriptVisitor {
    type Value = ZScript;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a hex-encoded Script")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let data = hex::decode(v).map_err(|e| match e {
            hex::FromHexError::InvalidHexCharacter { c, .. } => {
                serde::de::Error::invalid_value(Unexpected::Char(c), &"valid hex character")
            }
            hex::FromHexError::OddLength => {
                serde::de::Error::invalid_length(v.len(), &"an even-length string")
            }
            hex::FromHexError::InvalidStringLength => {
                serde::de::Error::invalid_length(v.len(), &"a 64-character string")
            }
        })?;
        Ok(ZScript(Script(data)))
    }
}

impl<'de> Deserialize<'de> for ZScript {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_str(ZScriptVisitor)
    }
}

impl Serialize for ZScript {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(&hex::encode(&self.0 .0))
    }
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub(crate) struct ZTxOut {
    value: ZOutputValue,
    script_pubkey: ZScript,
}

impl From<TxOut> for ZTxOut {
    fn from(out: TxOut) -> Self {
        ZTxOut {
            value: ZOutputValue(out.value),
            script_pubkey: ZScript(out.script_pubkey),
        }
    }
}

#[derive(Debug, Deserialize)]
pub(crate) struct Context {
    network: Option<JsonNetwork>,
    accounts: Option<Vec<JsonAccountId>>,
    pub(crate) chainhistoryroot: Option<ZUint256>,
    transparentcoins: Option<Vec<ZTxOut>>,
}

impl FromStr for Context {
    type Err = serde_json::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        serde_json::from_str(s)
    }
}

impl Context {
    pub(crate) fn network(&self) -> Option<Network> {
        self.network.map(|n| n.0)
    }

    pub(crate) fn addr_network(&self) -> Option<zcash_address::Network> {
        self.network().map(|params| match params {
            Network::MainNetwork => zcash_address::Network::Main,
            Network::TestNetwork => zcash_address::Network::Test,
        })
    }

    pub(crate) fn accounts(&self) -> Option<Vec<AccountId>> {
        self.accounts
            .as_ref()
            .map(|accounts| accounts.iter().map(|id| id.0).collect())
    }

    pub(crate) fn transparent_coins(&self) -> Option<Vec<transparent::TxOut>> {
        self.transparentcoins.as_ref().map(|coins| {
            coins
                .iter()
                .cloned()
                .map(|coin| transparent::TxOut {
                    value: coin.value.0,
                    script_pubkey: coin.script_pubkey.0,
                })
                .collect()
        })
    }
}
