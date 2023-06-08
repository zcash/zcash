Notable changes
===============

Change to Transaction Relay Policy
----------------------------------

Transactions paying less than the [ZIP 317](https://zips.z.cash/zip-0317)
conventional fee to the extent that they have more than `-txunpaidactionlimit`
unpaid actions (default: 50) will not be accepted to the mempool or relayed.
For the default values of `-txunpaidactionlimit` and `-blockunpaidactionlimit`,
these transactions would never be mined by the ZIP 317 block construction
algorithm. (If the transaction has been prioritised by `prioritisetransaction`,
the modified fee is used to calculate the number of unpaid actions.)

Removal of Fee Estimation
-------------------------

The `estimatefee` RPC call, which estimated the fee needed for a transaction to
be included within a target number of blocks, has been removed. The fee that
should be paid under normal circumstances is the ZIP 317 conventional fee; it
is not possible to compute that without knowing the number of logical actions
in a transaction, which was not an input to `estimatefee`. The `fee_estimates.dat`
file is no longer used.

Privacy Policy Changes
----------------------

 The `AllowRevealedSenders` privacy policy no longer allows sending from
 multiple taddrs in the same transaction. This now requires
 `AllowLinkingAccountAddresses`. Care should be taken in using
 `AllowLinkingAccountAddresses` too broadly, as it can also result in linking
 UAs when transparent funds are sent from them. The practical effect is that an
 explicit privacy policy is always required for `z_mergetoaddress`,
`z_sendmany`, and `z_shieldcoinbase` when sending from multiple taddrs, even
when using wildcards like `*` and `ANY_TADDR`.

Platform Support
----------------

- Ubuntu 18.04 LTS has been removed from the list of supported platforms. It
  reached End of Support on May 31st 2023, and no longer satisfies our Tier 2
  policy requirements.

Changelog
=========

Daira Emma Hopwood (8):
      Remove fee estimation.
      Rename DEFAULT_FEE to LEGACY_DEFAULT_FEE in C++ code and RPC tests. Also remove DEFAULT_FEE_ZATS in RPC tests which was unused.
      Fix a false positive duplicate-#include lint.
      Avoid calling `ParseNonRFCJSONValue` for string-only parameters. Follow-up to #6617.
      Transactions paying less than the ZIP 317 conventional fee to the extent that they have more than `-txunpaidactionlimit` unpaid actions, will now not be accepted to the mempool or relayed. For the default values of `-txunpaidactionlimit` and `-blockunpaidactionlimit`, these transactions would never be mined by the ZIP 317 block construction algorithm. (If the transaction has been prioritised by `prioritisetransaction`, the modified fee is used to calculate the number of unpaid actions.)
      Improve handling of UAs as account proxies
      Restructure InvalidFunds error message
      Reformat InvalidFunds error message (this only changes indentation).

Daira Hopwood (4):
      Correct "height" -> "time" in release notes
      Link to ZIP 317 in the release notes
      Use `DisplayMoney` to simplify constructing an error message
      Change incorrect uses of "is not" to "!=".

DeckerSU (1):
      txdb: remove consistency checks

Greg Pfeil (24):
      Clarify `wallet_doublespend` RPC test
      Replace `zec_in_zat` with pre-existing `COIN`
      Reduce ZEC ⇒ ZAT conversions
      Reduce the scope of some constants
      Encapsulate memos
      Include `memoStr` in RPC results with `memo`
      Centralize ReceiverType conversions
      Display pass ratio in RPC test summary
      Print all invalid receivers when there’s a failure
      Fix macOS build on CI
      Remove GitHub check for recent-enough branch
      Fix missing includes on macOS build
      Enable & error on all un-violated warnings
      Move warning flags to configure.ac
      Update comments on disabled warnings
      Fix a minor bug in an error message
      Document the Sprout cache used for RPC tests
      Make `./configure` quieter by default
      Share RPC param table between client and server
      Better error for “wrong number of params” in RPC
      Improve CONFIGURE_FLAGS handling in build.sh
      Reword RPC error messege for wrong number of params
      Strengthen AllowRevealedSenders
      Have COutput carry its CTxDestination

Hennadii Stepanov (1):
      Use C++17 [[fallthrough]] attribute, and drop -Wno-implicit-fallthrough

Jack Grigg (34):
      Squashed 'src/secp256k1/' changes from 1758a92ffd..be8d9c262f
      Squashed 'src/secp256k1/' changes from be8d9c262f..0559fc6e41
      Squashed 'src/secp256k1/' changes from 0559fc6e41..8746600eec
      Squashed 'src/secp256k1/' changes from 8746600eec..44c2452fd3
      Squashed 'src/secp256k1/' changes from 44c2452fd3..21ffe4b22a
      rust: Migrate to `secp256k1 0.26`, `hdwallet 0.4`
      Clear out v5.5.0 release notes
      Place Sapling {Spend, Output}Description fields behind getters
      Place `vShieldedSpend`, `vShieldedOutput` behind getters
      Remove Ubuntu 18.04 as a supported platform
      contrib: Update `contrib/devtools/symbol-check.py`
      Add Ubuntu 22.04 as a Tier 3 platform
      build: Consensus: Move Bitcoin script files from consensus to its own module/package
      build: Use libtool for linking `librustzcash.a` to the C++ code
      build: Fix `--with-libs` linking errors for MinGW cross-compilation
      rust: Add `CBLAKE2bWriter` support to `CppStream`
      Pass `CChainParams` into `TransactionBuilder` instead of `Consensus::Params`
      Pass ExtSK into `TransactionBuilder::AddSaplingSpend` instead of ExpSK
      test: Use correct transaction version for Orchard in `TxWithNullifiers`
      builder: Use Rust to compute shielded signature hash for v3-v4 txs
      builder: Remove `anchor` argument from `TransactionBuilder::AddSaplingSpend`
      mempool: Refactor `CTxMemPool::checkNullifiers` to use a template
      wallet: Introduce `libzcash::nullifier_t` typedef
      builder: Move all fields in `TransactionBuilder` move constructors
      rust: Add new structs and functions required for Sapling oxidation
      Oxidise the Sapling bundles
      Oxidise the Sapling benchmarks and remaining tests
      Remove now-unused Sapling logic
      test: Use `!=` instead of `is not` in `final*root` RPC tests
      Remove `TransactionBuilder` default constructor
      test: Some minor cleanups
      test: Fix non-conflicting merge conflict
      rpc: Add `z_getsubtreesbyindex` RPC method
      rpc: Add `trees` field to `getblock` RPC output

Jorge Timón (3):
      Build: Consensus: Move consensus files from common to its own module/package
      Build: Libconsensus: Move libconsensus-ready files to the consensus package
      Build: Consensus: Make libbitcoinconsensus_la_SOURCES fully dynamic and dependend on both crypto and consensus packages

Kris Nuttycombe (19):
      Note when (not) to apply the "add release notes" part of the release process.
      Add a useful comment about the CheckProofOfWork check in LoadBlockIndexGuts
      Add golden tests for Orchard wallet state at the zcashd v5.6.0 boundary.
      Persist data for wallet_golden_5_6_0 RPC test
      Apply suggestions from code review
      Fix a nondeterministic error in wallet tests caused by output shuffling.
      Fix nondeterminism in `WalletTests.UpdatedSaplingNoteData`
      Upgrade to the latest incrementalmerkletree & bridgetree versions.
      Regenerate cargo-vet exemptions
      Add Orchard subtree roots to the coins view.
      Fix a potential null-pointer dereference.
      Use a `limit` parameter instead of `end_index` for `z_getsubtreesbyindex`
      Remove `depth` property from `z_getsubtreesbyindex` result & fix docs.
      Update to released versions of patch dependencies.
      Update audits for upgraded dependencies.
      Update RPC tests to enable `-lightwalletd` flag where needed.
      Update native_cmake, native_cxxbridge, and native_rust and postpone other required updates.
      make-release.py: Versioning changes for 5.6.0-rc1.
      make-release.py: Updated manpages for 5.6.0-rc1.

Marshall Gaucher (1):
      Update performance-measurements.sh

Pieter Wuille (4):
      Remove --disable-openssl-tests for libsecp256k1 configure
      Adapt to libsecp256k1 API changes
      Add secp256k1_selftest call
      scripted-diff: rename privkey with seckey in secp256k1 interface

Sean Bowe (12):
      Add support for storing Sapling/Orchard subtree roots in leveldb.
      Add tests and fix bugs in implementation of CCoinsView subtree storage.
      Minor improvements suggested during code review.
      Minor documentation improvements suggested during review.
      Minor fixes suggested from code review.
      Remove unnecessary dependencies pulled in by proptest.
      Store 2^16 subtree roots for the Sapling note commitment tree in the coins database.
      Address comments raised during code review.
      Automatically migrate old coins database to account for complete subtrees for Sapling and Orchard
      Guard all subtree-related functionality behind lightwallet experimental feature flag.
      Apply suggestions from code review.
      Add UI messages surrounding RegenerateSubtrees.

dismad (1):
      Update README.md

ebfull (2):
      Simplify logic paths in `GetSubtreeData`
      Minor documentation typo fixes

fanquake (1):
      build: remove some no-longer-needed var unexporting from configure

practicalswift (1):
      Remove unreachable code (g_rpcSignals.PostCommand)

teor (1):
      Change module comment in bridge.rs to doc comment

