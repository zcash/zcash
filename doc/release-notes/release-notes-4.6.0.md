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

- The `getblocktemplate` RPC method output now includes a `defaultroots` field,
  which provides various tree roots and block commitments matching the contents
  of the block template. If any part of the block template marked as `mutable`
  in the RPC method output is mutated, these roots may need to be recomputed.
  For more information on the derivation process, see the block header changes
  in [ZIP 244](https://zips.z.cash/zip-0244#block-header-changes).

- Fixed an issue where `ERROR: spent index not enabled` would be logged
  unnecessarily on nodes that have either `-insightexplorer` or `-lightwalletd`
  configuration options enabled.

- The `getmininginfo` RPC now omits `currentblocksize` and `currentblocktx`
  when a block was never assembled via RPC on this node during its current
  process instantiation. (#5404)

Changelog
=========

Alex Wied (1):
      Update support for FreeBSD

Charlie O'Keefe (2):
      Add buster to the list of suites used by gitian
      Add libtinfo5 to gitian packages list

Dimitris Apostolou (1):
      Fix typos

Jack Grigg (23):
      contrib: Update Debian copyright file to follow the v1 format
      contrib: Add license information for libc++ and libevent
      cargo update
      tracing-subscriber 0.3
      Bump all postponed dependencies
      depends: Update Rust to 1.56.1
      depends: Update Clang / libcxx to LLVM 13
      rust: Move `incremental_sinsemilla_tree_ffi` into crate root
      CI: Add Pyflakes to lints workflow
      cargo update
      ed25519-zebra 3
      cargo update
      cargo update
      depends: Update Boost to 1.78.0
      depends Update Rust to 1.57.0
      qa: Postpone recent CCache releases
      Revert "lint: Fix false positive"
      rust: Remove misleading log message
      Migrate to latest revisions of Zcash Rust crates
      Update release notes
      make-release.py: Versioning changes for 4.6.0-rc1.
      make-release.py: Updated manpages for 4.6.0-rc1.
      make-release.py: Updated release notes and changelog for 4.6.0-rc1.

Janito Vaqueiro Ferreira Filho (1):
      Move `CurrentTxVersionInfo` into a new file

Kris Nuttycombe (7):
      Add BIP 44 coin type to persisted wallet state.
      Persist network id string instead of bip44 coin type.
      Add a check to test that wallet load fails if we're on the wrong network.
      Remove unused `AddDestData` method.
      Fix wallet-related wording in doc/reduce-traffic.md
      Rename OrchardMerkleTree -> OrchardMerkleFrontier
      Batch-verify Orchard transactions at the block level.

Larry Ruane (8):
      add TestSetIBD(bool) for testing
      Disable IBD for all boost tests
      better wallet network info error handling
      test: automatically add missing nuparams
      Don't log 'ERROR: spent index not enabled'
      getblocktemplate: add NU5 commitments to new `defaultroots` section
      test: fix bugs in test framework
      test: Use result of getblocktemplate to submitblock

Marco Falke (1):
      [rpc] mining: Omit uninitialized currentblockweight, currentblocktx

Marshall Gaucher (1):
      Update entrypoint.sh

Sasha (5):
      fix typo in docker run's volume argument
      update hash for librustzcash
      only librustzcash in config.offline
      make-release.py: Versioning changes for 4.6.0.
      make-release.py: Updated manpages for 4.6.0.

sgmoore (1):
      Update reduce-traffic.md - add one word

Jack Grigg (1):
      contrib: Add space between URL and period

teor (7):
      Add sigop count functions to zcash_script library
      Increment the zcash_script API version
      Remove an unused header
      Use correct copyright header
      Remove redundant variable
      Explain UINT_MAX error return value
      Explain how to get rid of a tiny duplicated function

