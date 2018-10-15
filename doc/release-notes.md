(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Enabled Sapling features for mainnet
------------------------------------
This release adds significant support for Sapling to the wallet and RPC interface. Sapling will activate at block 419200, which is expected to be mined on the 28th of October 2018. Users running v2.0.0 nodes (which are consensus-compatible with Sapling) will follow the network upgrade, but must upgrade to v2.0.1 in order to send Sapling shielded transactions.

Minimum Difficulty Blocks allowed on testnet
--------------------------------------------
Sapling activated on testnet at block 280000. Users running v2.0.1 nodes no longer need to specify `-experimentalfeatures` and `-developersapling` to use Sapling functionality on testnet. Users running v2.0.0 nodes should upgrade to v2.0.1 which introduces a consensus rule change to allow minimum difficulty blocks to be mined from block 299188, thereby splitting the chain.

[Pull request](https://github.com/zcash/zcash/pull/3559)

Hierarchical Deterministic Key Generation for Sapling
-----------------------------------------------------
All Sapling addresses will use hierarchical deterministic key generation
according to ZIP 32 (keypath m/32'/133'/k' on mainnet). Transparent and
Sprout addresses will still use traditional key generation.

Backups of HD wallets, regardless of when they have been created, can
therefore be used to re-generate all possible Sapling private keys, even the
ones which haven't already been generated during the time of the backup.
Regular backups are still necessary, however, in order to ensure that
transparent and Sprout addresses are not lost.

[Pull request](https://github.com/zcash/zcash/pull/3492), [ZIP 32](https://github.com/zcash/zips/blob/master/zip-0032.mediawiki)

Fix Signing Raw Transactions with Unsynced Offline Nodes
--------------------------------------------------------
With v2.0.0, in `signrawtransaction` the consensus branch ID (which is used to construct the transaction) was estimated using the chain height. With v2.0.1 this has been improved by also considering the `APPROX_RELEASE_HEIGHT` of the release, and a new parameter to allow the caller to manually override the consensus branch ID that zcashd will use.

[Pull request](https://github.com/zcash/zcash/pull/3520)