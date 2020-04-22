(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Network Upgrade 3: Heartwood
----------------------------

The code preparations for the Heartwood network upgrade are finished and
included in this release. The following ZIPs are being deployed:

- [ZIP 213: Shielded Coinbase](https://zips.z.cash/zip-0213)
- [ZIP 221: FlyClient - Consensus-Layer Changes](https://zips.z.cash/zip-0221)

Heartwood will activate on testnet at height 903800, and can also be activated
at a specific height in regtest mode by setting the config option
`-nuparams=f5b9230b:HEIGHT`.

As a reminder, because the Heartwood activation height is not yet specified for
mainnet, version 2.1.2 will behave similarly as other pre-Heartwood releases
even after a future activation of Heartwood on the network. Upgrading from 2.1.2
will be required in order to follow the Heartwood network upgrade on mainnet.

See [ZIP 250](https://zips.z.cash/zip-0250) for additional information about the
deployment process for Heartwood.

### Mining to Sapling addresses

Miners and mining pools that wish to test the new "shielded coinbase" support on
the Heartwood testnet can generate a new Sapling address with `z_getnewaddress`,
add the config option `mineraddress=SAPLING_ADDRESS` to their `zcash.conf` file,
and then restart their `zcashd` node. `getblocktemplate` will then return
coinbase transactions containing a shielded miner output.

Note that `mineraddress` should only be set to a Sapling address after the
Heartwood network upgrade has activated; setting a Sapling address prior to
Heartwood activation will cause `getblocktemplate` to return block templates
that cannot be mined.

Sapling viewing keys support
----------------------------

Support for Sapling viewing keys (specifically, Sapling extended full viewing
keys, as described in [ZIP 32](https://zips.z.cash/zip-0032)), has been added to
the wallet. Nodes will track both sent and received transactions for any Sapling
addresses associated with the imported Sapling viewing keys.

- Use the `z_exportviewingkey` RPC method to obtain the viewing key for a
  shielded address in a node's wallet. For Sapling addresses, these always begin
  with "zxviews" (or "zxviewtestsapling" for testnet addresses).

- Use `z_importviewingkey` to import a viewing key into another node.  Imported
  Sapling viewing keys will be stored in the wallet, and remembered across
  restarts.

- `z_getbalance` will show the balance of a Sapling address associated with an
  imported Sapling viewing key. Balances for Sapling viewing keys will be
  included in the output of `z_gettotalbalance` when the `includeWatchonly`
  parameter is set to `true`.

- RPC methods for viewing shielded transaction information (such as
  `z_listreceivedbyaddress`) will return information for Sapling addresses
  associated with imported Sapling viewing keys.

Details about what information can be viewed with these Sapling viewing keys,
and what guarantees you have about that information, can be found in
[ZIP 310](https://zips.z.cash/zip-0310).

Removal of time adjustment and the -maxtimeadjustment= option
-------------------------------------------------------------

Prior to v2.1.1-1, `zcashd` would adjust the local time that it used by up
to 70 minutes, according to a median of the times sent by the first 200 peers
to connect to it. This mechanism was inherently insecure, since an adversary
making multiple connections to the node could effectively control its time
within that +/- 70 minute window (this is called a "timejacking attack").

In the v2.1.1-1 security release, in addition to other mitigations for
timejacking attacks, the maximum time adjustment was set to zero by default.
This effectively disabled time adjustment; however, a `-maxtimeadjustment=`
option was provided to override this default.

As a simplification the time adjustment code has now been completely removed,
together with `-maxtimeadjustment=`. Node operators should instead ensure that
their local time is set reasonably accurately.

If it appears that the node has a significantly different time than its peers,
a warning will still be logged and indicated on the metrics screen if enabled.

View shielded information in wallet transactions
------------------------------------------------

In previous `zcashd` versions, to obtain information about shielded transactions
you would use either the `z_listreceivedbyaddress` RPC method (which returns all
notes received by an address) or `z_listunspent` (which returns unspent notes,
optionally filtered by addresses). There were no RPC methods that directly
returned details about spends, or anything equivalent to the `gettransaction`
method (which returns transparent information about in-wallet transactions).

This release introduces a new RPC method `z_viewtransaction` to fill that gap.
Given the ID of a transaction in the wallet, it decrypts the transaction and
returns detailed shielded information for all decryptable new and spent notes,
including:

- The address that each note belongs to.
- Values in both decimal ZEC and zatoshis.
- The ID of the transaction that each spent note was received in.
- An `outgoing` flag on each new note, which will be `true` if the output is not
  for an address in the wallet.
- A `memoStr` field for each new note, containing its text memo (if its memo
  field contains a valid UTF-8 string).

Information will be shown for any address that appears in `z_listaddresses`;
this includes watch-only addresses linked to viewing keys imported with
`z_importviewingkey`, as well as addresses with spending keys (both generated
with `z_getnewaddress` and imported with `z_importkey`).

Better error messages for rejected transactions after network upgrades
----------------------------------------------------------------------

The Zcash network upgrade process includes several features designed to protect
users. One of these is the "consensus branch ID", which prevents transactions
created after a network upgrade has activated from being replayed on another
chain (that might have occurred due to, for example, a
[friendly fork](https://electriccoin.co/blog/future-friendly-fork/)). This is
known as "two-way replay protection", and is a core requirement by
[various](https://blog.bitgo.com/bitgos-approach-to-handling-a-hard-fork-71e572506d7d?gi=3b80c02e027e)
[members](https://trezor.io/support/general/hard-forks/) of the cryptocurrency
ecosystem for supporting "hard fork"-style changes like our network upgrades.

One downside of the way replay protection is implemented in Zcash, is that there
is no visible difference between a transaction being rejected by a `zcashd` node
due to targeting a different branch, and being rejected due to an invalid
signature. This has caused issues in the past when a user had not upgraded their
wallet software, or when a wallet lacked support for the new network upgrade's
consensus branch ID; the resulting error messages when users tried to create
transactions were non-intuitive, and particularly cryptic for transparent
transactions.

Starting from this release, `zcashd` nodes will re-verify invalid transparent
and Sprout signatures against the consensus branch ID from before the most
recent network upgrade. If the signature then becomes valid, the transaction
will be rejected with the error message `old-consensus-branch-id`. This error
can be handled specifically by wallet providers to inform the user that they
need to upgrade their wallet software.

Wallet software can also automatically obtain the latest consensus branch ID
from their (up-to-date) `zcashd` node, by calling `getblockchaininfo` and
looking at `{'consensus': {'nextblock': BRANCH_ID, ...}, ...}` in the JSON
output.

Expired transactions notifications
----------------------------------

A new config option `-txexpirynotify` has been added that will cause `zcashd` to
execute a command when a transaction in the mempool expires. This can be used to
notify external systems about transaction expiry, similar to the existing
`-blocknotify` config option that notifies when the chain tip changes.

RPC methods
-----------

- The `z_importkey` and `z_importviewingkey` RPC methods now return the type of
  the imported spending or viewing key (`sprout` or `sapling`), and the
  corresponding payment address.

- Negative heights are now permitted in `getblock` and `getblockhash`, to select
  blocks backwards from the chain tip. A height of `-1` corresponds to the last
  known valid block on the main chain.

- A new RPC method `getexperimentalfeatures` returns the list of enabled
  experimental features.

Build system
------------

- The `--enable-lcov`, `--disable-tests`, and `--disable-mining` flags for
  `zcutil/build.sh` have been removed. You can pass these flags instead by using
  the `CONFIGURE_FLAGS` environment variable. For example, to enable coverage
  instrumentation (thus enabling "make cov" to work), call:

  ```
  CONFIGURE_FLAGS="--enable-lcov --disable-hardening" ./zcutil/build.sh
  ```

- The build system no longer defaults to verbose output. You can re-enable
  verbose output with `./zcutil/build.sh V=1`
