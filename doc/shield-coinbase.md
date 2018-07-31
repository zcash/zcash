# Shielding Coinbase UTXOs

**Summary**

Use `z_shieldcoinbase` RPC call to shield coinbase UTXOs.

**Who should read this document**

Miners, Mining pools, Online wallets

## Background

The current Zcash protocol includes a consensus rule that coinbase rewards must be sent to a shielded address.

## User Experience Challenges

A user can use the z_sendmany RPC call to shield coinbase funds, but the call was not designed for sweeping up many UTXOs, and offered a suboptimal user experience.

If customers send mining pool payouts to their online wallet, the service provider must sort through UTXOs to correctly determine the non-coinbase UTXO funds that can be withdrawn or transferred by customers to another transparent address.

## Solution

The z_shieldcoinbase call makes it easy to sweep up coinbase rewards from multiple coinbase UTXOs across multiple coinbase reward addresses.

    z_shieldcoinbase fromaddress toaddress (fee) (limit)

The default fee is 0.0010000 ZEC and the default limit on the maximum number of UTXOs to shield is 50.

## Examples

Sweep up coinbase UTXOs from a transparent address you use for mining:

    zcash-cli z_shieldcoinbase tMyMiningAddress zMyPrivateAddress

Sweep up coinbase UTXOs from multiple transparent addresses to a shielded address:

    zcash-cli z_shieldcoinbase "*" zMyPrivateAddress

Sweep up with a fee of 1.23 ZEC:

    zcash-cli z_shieldcoinbase tMyMiningAddress zMyPrivateAddress 1.23

Sweep up with a fee of 0.1 ZEC and set limit on the maximum number of UTXOs to shield at 25:

    zcash-cli z_shieldcoinbase "*" zMyPrivateAddress 0.1 25

### Asynchronous Call

The `z_shieldcoinbase` RPC call is an asynchronous call, so you can queue up multiple operations. 

When you invoke

    zcash-cli z_shieldcoinbase tMyMiningAddress zMyPrivateAddress

JSON will be returned immediately, with the following data fields populated:

- operationid: a temporary id to use with `z_getoperationstatus` and `z_getoperationresult` to get the status and result of the operation.
- shieldedUTXOs: number of coinbase UTXOs being shielded
- shieldedValue: value of coinbase UTXOs being shielded.
- remainingUTXOs: number of coinbase UTXOs still available for shielding.
- remainingValue: value of coinbase UTXOs still available for shielding

### Locking UTXOs

The `z_shieldcoinbase` call will lock any selected UTXOs. This prevents the selected UTXOs which are already queued up from being selected for any other send operation.  If the `z_shieldcoinbase` call fails, any locked UTXOs are unlocked.

You can use the RPC call `lockunspent` to see which UTXOs have been locked.  You can also use this call to unlock any UTXOs in the event of an unexpected system failure which leaves UTXOs in a locked state.

### Limits, Performance and Transaction Confirmation

The number of coinbase UTXOs selected for shielding can be adjusted by setting the limit parameter. The default value is 50.

If the limit parameter is set to zero, the zcashd `mempooltxinputlimit` option will be used instead, where the default value for `mempooltxinputlimit` is zero, which means no limit.

Any limit is constrained by a hard limit due to the consensus rule defining a maximum transaction size of 100,000 bytes.

In general, the more UTXOs that are selected, the longer it takes for the transaction to be verified.  Due to the quadratic hashing problem, some miners use the `mempooltxinputlimit` option to reject transactions with a large number of UTXO inputs.

Currently, as of November 2017, there is no commonly agreed upon limit, but as a rule of thumb (a form of emergent consensus) if a transaction has less than 100 UTXO inputs, the transaction will be mined promptly by the majority of mining pools, but if it has many more UTXO inputs, such as 500, it might take several days to be mined by a miner who has higher or no limits.

### Anatomy of a z_shieldcoinbase transaction

The transaction created is a shielded transaction.  It consists of a single joinsplit, which consumes coinbase UTXOs as input, and deposits value at a shielded address, minus any fee.

The number of coinbase UTXOs is determined by a user configured limit.

If no limit is set (in the case when limit parameter and `mempooltxinputlimit` options are set to zero) the behaviour of z_shieldcoinbase is to consume as many UTXOs as possible, with `z_shieldcoinbase` constructing a transaction up to the size limit of 100,000 bytes.

As a result, the maximum number of inputs that can be selected is:

- P2PKH coinbase UTXOs ~ 662
- 2-of-3 multisig P2SH coinbase UTXOs ~ 244.

Here is an example of using `z_shieldcoinbase` on testnet to shield multi-sig coinbase UTXOs.

- Block 141042 is almost ~2 MB in size (the maximum size for a block) and contains 1 coinbase reward transaction and 20 transactions, each indivually created by a call to z_shieldcoinbase.
  - https://explorer.testnet.z.cash/block/0050552a78e97c89f666713c8448d49ad1d7263274422272696187dedf6c0d03
- Drilling down into a transaction, you can see there is one joinsplit, with 244 inputs (vin) and 0 outputs (vout).
  - https://explorer.testnet.z.cash/tx/cf4f3da2e434f68b6e361303403344e22a9ff9a8fda9abc180d9520d0ca6527d


