Notable changes
===============

Deprecation of `fetch-params.sh`
--------------------------------

The `fetch-params.sh` script (also `zcash-fetch-params`) is now deprecated. The
`zcashd` binary now bundles zk-SNARK parameters directly and so parameters do not
need to be downloaded or stored separately. The script will now do nothing but
state that it is deprecated; it will be removed in a future release.

Previously, parameters were stored by default in these locations:

* `~/.zcash-params` (on Linux); or
* `~/Library/Application Support/ZcashParams` (on Mac); or
* `C:\Users\Username\AppData\Roaming\ZcashParams` (on Windows)

Unless you need to generate transactions using the deprecated Sprout shielded
pool, you can delete the parameter files stored in these directories to save
space as they are no longer read or used by `zcashd`. If you do wish to use the
Sprout pool, you will need the `sprout-groth16.params` file in the
aforementioned directory. This file is currently available for download
[here](https://download.z.cash/downloads/sprout-groth16.params).

Mempool metrics
---------------

`zcashd` now reports the following new metrics when `-prometheusport` is set:

- (gauge) `zcash.mempool.actions.unpaid { "bk" = ["< 0.2", "< 0.4", "< 0.6", "< 0.8", "< 1"] }`
- (gauge) `zcash.mempool.actions.paid`
- (gauge) `zcash.mempool.size.weighted { "bk" = ["< 1", "1", "> 1", "> 2", "> 3"] }`

The `zcash.mempool.actions` metrics count the number of [logical actions] across
the transactions in the mempool, while `zcash.mempool.size.weighted` is a
weight-bucketed version of the `zcash.mempool.size.bytes` metric.

The [ZIP 317 weight ratio][weight_ratio] of a transaction is used to bucket its
logical actions and byte size. A weight ratio of at least 1 means that the
transaction's fee is at least the ZIP 317 conventional fee, and all of its
logical actions are considered "paid". A weight ratio lower than 1 corresponds
to the fraction of the transaction's logical actions that are "paid". The
remaining fraction (i.e. 1 - weight ratio) are subject to the unpaid action
limit that miners configure for their blocks with `-blockunpaidactionlimit`.

[logical actions]: https://zips.z.cash/zip-0317#fee-calculation
[weight_ratio]: https://zips.z.cash/zip-0317#recommended-algorithm-for-block-template-construction

Changelog
=========

Daira Emma Hopwood (2):
      wallet_listreceived.py: fix an assertion message and remove unnecessary use of LEGACY_DEFAULT_FEE.
      Update ed25519-zebra to 4.0.0. This deduplicates sha2, rand-core, block-buffer, digest, and ahash. (It adds duplications for hashbrown and libm which are less important.)

Daira Hopwood (1):
      Ensure that the panic when Sprout parameters cannot be loaded says how to fix it by downloading them.

Elijah Hampton (1):
      Updates getblockcount help message to the appropriate message.

Jack Grigg (53):
      CI: Update `apt` before installing build dependencies
      CI: De-duplicate logic to get the number of available processing cores
      CI: Use `hw.logicalcpu` instead of `hw.ncpu` on macOS
      Move mempool metrics updates into a `CTxMemPool::UpdateMetrics` method
      metrics: Track mempool actions and size bucketed by weight
      contrib: Update Grafana dashboard to show mempool composition
      CI: Add a lint that checks for headers missing from makefiles
      Retroactively use Rust to decrypt shielded coinbase before soft fork
      Remove now-unused C++ Sapling note encryption logic
      test: Skip `WalletTests.WalletNetworkSerialization`
      Rename reject reason for invalid shielded coinbase ciphertexts
      test: Set `-limitdescendantcount` to match viable iteration limit
      qa: Migrate to `cargo-vet 0.8`
      qa: Remove audit policies for crates we no longer patch
      qa: Replace Windows crate audits with a trust policy for Microsoft
      depends: native_ccache 4.8.2
      depends: cxx 1.0.97
      cargo update
      CI: Add `cargo deny check licenses` job
      depends: tl_expected 1.1.0
      cargo update again
      Use `cxx::bridge` for Sprout proofs
      Use `cxx::bridge` for `zcash_history`
      Use `cxx::bridge` for initialization functions
      Use `cxx::bridge` to load ZKP parameters
      Use `cxx::bridge` for Sapling specification components
      Remove unused Sapling logic
      Use `cxx::bridge` for Sapling ZIP 32 wrappers
      Use `cxx::bridge` for `getrandom`
      rust: Rename modules that no longer contain raw FFI functions
      depends: libsodium 1.0.19
      depends: utfcpp 3.2.4
      depends: native_ccache 4.8.3
      depends: Boost 1.83.0
      depends: native_cmake 3.27.4
      qa: Bump postponed dependencies
      qa: cargo vet prune
      cargo update
      depends: cxx 1.0.107
      depends: native_cmake 3.27.5
      qa: Postpone Clang and Rust updates
      depends: native_cmake 3.27.6
      qa: Replace `cargo vet` ECC self-audits with trust declarations
      rust: Upgrade Zcash Rust crates
      Remove CentOS 8 from CI builder files
      doc: Update release notes for 5.7.0
      make-release.py: Versioning changes for 5.7.0-rc1.
      make-release.py: Updated manpages for 5.7.0-rc1.
      make-release.py: Updated release notes and changelog for 5.7.0-rc1.
      make-release.py: Updated book for 5.7.0-rc1.
      depends: utfcpp 3.2.5
      make-release.py: Versioning changes for 5.7.0.
      make-release.py: Updated manpages for 5.7.0.

Kris Nuttycombe (4):
      Update network upgrade golden tests for serialization to include nu5.
      Make a few small improvements to the release process doc.
      Remove audit claim for allocator-api2
      Fix description of transaction weight ratio in v5.7.0 release notes.

Marius Kjærstad (1):
      New checkpoint at block 2200000 for mainnet

Sean Bowe (5):
      Bundle the Sprout (Groth16) verification key in librustzcash.
      Bundle the Sapling zk-SNARK parameters using the `wagyu-zcash-parameters` crate.
      Deprecate the `fetch-params.sh` script.
      cargo fmt
      Hash the Sprout parameter file during proving before deserialization.

Yasser (1):
      Update ZCASH_SIGNING_KEY_ID in Dockerfile

ebfull (1):
      Update zcutil/fetch-params.sh

