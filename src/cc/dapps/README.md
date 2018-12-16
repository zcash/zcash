# CryptoCondition dApps

## Compiling

To compile all dapps in this directory:

    make

## zmigrate - Sprout to Sapling Migration dApp

This tool converts Sprout zaddress funds into Sapling funds in a new Sapling address.

### Usage

    ./zmigrate COIN zsaplingaddr

The above command may need to be run multiple times to complete the process.

This CLI implementation will be called by GUI wallets, average users do not
need to worry about using this low-level tool.

## oraclefeed - feed of price data using oracles

### Usage

    ./oraclefeed $ACNAME $ORACLETXID $MYPUBKEY $FORMAT $BINDTXID [refcoin_cli]

Supported formats are L and Ihh. Price data from CoinDesk API.
