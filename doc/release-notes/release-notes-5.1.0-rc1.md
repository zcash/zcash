Notable changes
===============

Faster block validation for Sapling and Orchard transactions
------------------------------------------------------------

Block validation in `zcashd` is a mostly single-threaded process, due to how the
chain update logic inherited from Bitcoin Core is written. However, certain more
computationally intensive checks are performed more efficiently than checking
everything individually:

- ECDSA signatures on transparent inputs are checked via multithreading.
- RedPallas signatures on Orchard actions are checked via batch validation.

As of this release, `zcashd` applies these techniques to more Sapling and
Orchard components:

- RedJubjub signatures on Sapling Spends are checked via batch validation.
- Groth16 proofs for Sapling Spends and Outputs are checked via batch validation
  and multithreading.
- Halo 2 proofs for Orchard Actions are checked via batch validation and
  multithreading.

This reduces worst-case block validation times for observed historic blocks by
around 80% on a Ryzen 9 5950X CPU.

The number of threads used for checking Groth16 and Halo 2 proofs (as well as
for creating them when spending funds) can be set via the `RAYON_NUM_THREADS`
environment variable.

Option handling
---------------

- A new `-preferredtxversion` argument allows the node to preferentially create
  transactions of a specified version, if a transaction does not contain
  components that necessitate creation with a specific version. For example,
  setting `-preferredtxversion=4` will cause the node to create V4 transactions
  whenever the transaction does not contain Orchard components. This can be
  helpful if recipients of transactions are likely to be using legacy wallets
  that have not yet been upgraded to support parsing V5 transactions.

RPC interface
-------------

- The `getrawtransaction` RPC method now includes details about Orchard actions
  within transactions.

Deprecated
----------

As of this release, the following previously deprecated features are disabled
by default, but may be be reenabled using `-allowdeprecated=<feature>`.

  - The `dumpwallet` RPC method is disabled. It may be reenabled with
    `allowdeprecated=dumpwallet`. `dumpwallet` should not be used; it is
    unsafe for backup purposes as it does not return any key information
    for keys used to derive shielded addresses. Use `z_exportwallet` instead.

As of this release, the following features are deprecated, but remain available
by default. These features may be disabled by setting `-allowdeprecated=none`.
After at least 3 minor-version releases, these features will be disabled by
default and the following flags to `-allowdeprecated` will be required to
permit their continued use:

  - `wallettxvjoinsplit` - controls availability of the deprecated `vjoinsplit`
    attribute returned by the `gettransaction` RPC method.

Changelog
=========

Brian Stafford (2):
      rpc: Add missing fields to getrawtransaction help text
      rpc: add valueBalanceOrchard to getrawtransaction output

Chun Kuan Lee (1):
      break circular dependency: random/sync -> util -> random/sync

Cory Fields (1):
      threads: add a thread_local autoconf check

Daira Hopwood (4):
      halo2 is now under MIT/Apache-2.0, so does not need a declaration in `contrib/debian/copyright`. fixes #5203
      COPYING: Address feedback about the use of "permissive". Also refer to zcashd instead of "Zcash".
      Upgrade to metrics 0.19.x and metrics-exporter-prometheus 0.10.x.
      Apply cosmetic suggestions

Jack Grigg (39):
      book: Add platform support information and tier policy
      lint: Fix cargo patches linter when no patches are present
      Shorten thread name prefix
      Name currently-unnamed threads that we can rename
      book: Capitalize key words in platform tier policy
      book: Add FreeBSD to tier 3 platforms list
      depends: Add cxx crate to dependencies
      depends: Add cxxbridge command to dependencies
      build: Pass `CC` etc. flags through to `cargo build`
      build: Add non-verbose output for `cargo build`
      depends: Add `rust/cxx.h` header as a dependency
      Migrate Equihash Rust FFI to `cxx`
      Migrate BLAKE2b Rust FFI to `cxx`
      depends: Vendor dependencies of native_cxxbuild
      Revert "Switched sync.{cpp,h} to std threading primitives."
      qa: Fix sprout_sapling_migration RPC test to handle wallet RPC change
      wallet: Clear witness caches on load if reindexing
      Document that `-reindex` and `-salvagewallet` both imply `-rescan`
      Update orchard license with current exception text
      Note dependence on BOSL in COPYING
      qa: `cargo vet init`
      qa: Add `crypto-reviewed` and `license-reviewed` criteria for `cargo vet`
      CI: Add workflow that runs `cargo vet --locked`
      qa: Add audits for the crates directly maintained by the ECC core team
      book: Add section about auditing Rust dependencies
      qa: Fix `qa/zcash/create_benchmark_archive.py` script
      qa: Generalise `extract_benchmark_data` in `performance-measurements.sh`
      bench: Support multiple trees in FakeCoinsViewDB
      bench: Add `ConnectBlock` benchmark using block 1708048
      cargo vet fmt
      Upgrade to `orchard 0.2.0`
      Batch-validate Orchard proofs as well as Orchard signatures
      test: Load the proof verification keys in Boost tests
      bench: Add `ConnectBlock` benchmark using block 1723244
      Use batch validation for Sapling proofs and signatures
      qa: Reformat for latest cargo-vet
      qa: Postpone dependency updates
      Update release notes
      qa: Add native_cxxbridge and rustcxx to update checker

Jesse Cohen (1):
      Annotate AssertLockHeld() with ASSERT_CAPABILITY() for thread safety analysis

John Newbery (1):
      [logging] Comment all continuing logs.

João Barbosa (1):
      Remove unused fTry from push_lock

Kris Nuttycombe (27):
      Fix incorrect links in 5.0.0 release notes.
      Fix inconsistent caplitalization in copyright notices.
      scripted-diff: Update Zcash copyrights to 2022
      scripted-diff: Add 2016-2022 copyright headers for files added/modified in 2016
      scripted-diff: Add 2017-2022 copyright headers for files added/modified in 2017
      scripted-diff: Add 2018-2022 copyright headers for files added/modified in 2018
      scripted-diff: Add 2019-2022 copyright headers for files added/modified in 2019
      scripted-diff: Add 2020-2022 copyright headers for files added/modified in 2020
      scripted-diff: Add 2021-2022 copyright headers for files added/modified in 2021
      Ensure that anchor depth is > 0 when selecting an Orchard anchor.
      Add configure~ to .gitignore
      Return an error if attempting to use z_shieldcoinbase for Orchard shielding.
      Only return active protocol components from z_gettreestate.
      Revert "Only return active protocol components from z_gettreestate."
      Only return `skipHash` for Orchard & Sapling roots at heights >= activation.
      Add a CLI flag to preferentially send V4 tx.
      Revert "Make `-reindex` and `-reindex-chainstate` imply `-rescan`"
      Do not attempt to begin a rescan if reindexing.
      Disable wallet commands that are unavailable in safe mode during -reindex
      Guard map accesses.
      Mark the `dumpwallet` RPC method as disabled.
      Add a clock for testing with an offset from the system clock.
      Apply suggestions from code review
      Note that `gettransaction` doesn't provide shielded info in RPC help.
      Deprecate the `vjoinsplit` field of `gettransaction` results.
      Revert "Merge pull request #6037 from nuttycom/feature/clock_capability"
      Replace "Disabled" Orchard AuthValidator with std::nullopt

Marco Falke (3):
      qa: Initialize lockstack to prevent null pointer deref
      sync: Add RecursiveMutex type alias
      doc: Add comment to cs_main and mempool::cs

Matt Corallo (8):
      Split CNode::cs_vSend: message processing and message sending
      Make the cs_sendProcessing a LOCK instead of a TRY_LOCK
      Lock cs_vSend and cs_inventory in a consistent order even in TRY
      Always enforce lock strict lock ordering (try or not)
      Fixup style a bit by moving { to the same line as if statements
      Further-enforce lockordering by enforcing directly after TRY_LOCKs
      Fix -Wthread-safety-analysis warnings. Change the sync.h primitives to std from boost.
      Fix fast-shutdown hang on ThreadImport+GenesisWait

Pieter Wuille (2):
      Use a signal to continue init after genesis activation
      Do diskspace check before import thread is started

Russell Yanofsky (5):
      Add unit test for DEBUG_LOCKORDER code
      MOVEONLY Move AnnotatedMixin declaration
      Make LOCK, LOCK2, TRY_LOCK work with CWaitableCriticalSection
      Use LOCK macros for non-recursive locks
      scripted-diff: Small locking rename

Sean Bowe (8):
      Fix "transparent" example that should be "p2pkh"
      Make shielded requirements error "debug" level rather than an error.
      Introduce new Sapling verification API via cxx and switch consensus rules to use the new API.
      Enable ZIP 216 for blocks prior to NU5 activation
      Remove the old Sapling verification FFI APIs.
      cargo fmt
      Address clippy lints.
      Update minimum chain work and set NU5 activation block hash for mainnet

Thomas Snider (1):
      Switched sync.{cpp,h} to std threading primitives.

Marshall Gaucher (2):
      add rpc parallel test group logic
      Update walletbackup.py

practicalswift (3):
      Remove unused code
      Use -Wthread-safety-analysis if available (+ -Werror=thread-safety-analysis if --enable-werror)
      Fix typos

sasha (2):
      make-release.py: Versioning changes for 5.1.0-rc1.
      make-release.py: Updated manpages for 5.1.0-rc1.

Ying Tong Lai (3):
      Add orchard_bundle FFI.
      Use orchard_bundle ffi in getrawtransaction.
      Test getrawtransaction in wallet_orchard.py

