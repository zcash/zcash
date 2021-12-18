(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Wallet
------

From this release, newly-created wallets will save the chain name ("Zcash") and
network identifier (e.g. "main") to the `wallet.dat` file. This will enable the
`zcashd` node to check on subsequent starts that the `wallet.dat` file matches
the node's configuration. Existing wallets will start saving this information in
a later release.

`libzcash_script`
-----------------

Two new APIs have been added to this library (`zcash_script_legacy_sigop_count`
and `zcash_script_legacy_sigop_count_precomputed`), for counting the number of
signature operations in the transparent inputs and outputs of a transaction.
The presence of these APIs is indicated by a library API version of 2.

Updated RPCs
------------

- Fixed an issue where `ERROR: spent index not enabled` would be logged
  unnecessarily on nodes that have either insightexplorer or lightwalletd
  configuration options enabled.

- The `getmininginfo` RPC now omits `currentblockize` and `currentblocktx`
  when a block was never assembled via RPC on this node during its current
  process instantiation. (#5404)
