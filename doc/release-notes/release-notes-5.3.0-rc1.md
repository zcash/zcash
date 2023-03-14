Notable changes
===============

Wallet Performance Improvements
-------------------------------

`zcashd 5.2.0` improved the performance of wallet scanning with multithreaded
batched trial decryption of Sapling outputs. However, for some nodes this
resulted in growing memory usage that would eventually cause an OOM abort. We
have identified the cause of the growth, and made significant improvements to
reduce the memory usage of the batch scanner. In addition, the batch scanner now
has a memory limit of 100 MiB.

`zcashd` now reports the following new metrics when `-prometheusport` is set:

- (counter) `zcashd.wallet.batchscanner.outputs.scanned`
- (gauge) `zcashd.wallet.batchscanner.size.transactions`
- (gauge) `zcashd.wallet.batchscanner.usage.bytes`
- (gauge) `zcashd.wallet.synced.block.height`

RPC Interface
-------------

- The `finalorchardroot` field in the `getblock` result and the
  `orchard.commitments.finalRoot` field in the `z_gettreestate` result  have
  been changed to match the byte ordering used for the `orchard.anchor`
  field in the `getrawtransaction` result. These previously produced different
  hash values from the `orchard.anchor` field due to having been byte-flipped
  in their internal representation in zcashd.

Changelog
=========

Alex (1):
      build: update book.yml Signed-off-by: sashashura <93376818+sashashura@users.noreply.github.com>

Alex Wied (2):
      cuckoocache: Add missing header
      build: Reorder link targets to properly build on Nix

Andrés G. Aragoneses (1):
      autogen.sh: warn about needing autoconf if autoreconf is not found

Ben Woosley (1):
      doc: Correct spelling errors in comments

Conrado Gouvea (1):
      Include algorithm.h in cuckoocache.h

Daira Hopwood (8):
      Add contrib/debian/copyright entry for crc32c.
      Apply suggestions from code review
      Update doc/book/src/user/security-warnings.md
      Update librustzcash commit and adapt to changes in `DiversifierKey`.
      Avoid an implicit clone of the Orchard bundle in ContextualCheckTransaction.
      Use prepared epks and ivks in trial decryption.
      Audit dependency updates.
      Include memory usage of the `tags` vector. This fixes *one* of the bugs pointed out by @str4d at https://github.com/zcash/zcash/pull/6156/files#r979122874

DeckerSU (2):
      miner: fix MAXSOLS
      test_framework: fix AttributeError in sapling_spends_compact_digest

Greg Pfeil (11):
      Eliminate indirection for debug log
      Define some basic cross-editor configuration
      Canonicalize some user-provided paths
      Add an `rpc-tests` make target
      Add simplejson to requirements for rpc-tests
      Also canonicalize paramsdir.
      Backport tor.md changes from readthedocs
      Don't recommend -reindex-chainstate.
      Add a finalorchardroot RPC test
      Fix finalorchardroot serialization
      Apply suggestions from code review

Hennadii Stepanov (5):
      Enable ShellCheck rules
      script: Lint Gitian descriptors with ShellCheck
      script: Enable SC2006 rule for Gitian scripts
      script: Enable SC2155 rule for Gitian scripts
      script: Enable SC2001 rule for Gitian scripts

Jack Grigg (52):
      Squashed 'src/leveldb/' changes from f545dfabff..f8ae182c1e
      Squashed 'src/crc32c/' content from commit 224988680f
      depends: Update Rust to 1.62.1
      depends: Update Clang / libcxx to LLVM 14.0.6
      depends: Update Rust to 1.63.0
      CI: Migrate to published versions of cargo-vet
      rust: Update some of the pinned dependencies
      rust: Add `zcash-inspect` binary for inspecting Zcash data
      rust: Add P2PKH signature checking to `zcash-inspect`
      rust: Add address inspection to `zcash-inspect`
      rust: Add mnemonic phrase inspection to `zcash-inspect`
      rust: Simplify `next_pow2` in `zcash-inspect`
      rust: Place tighter bound on encoded heights in `zcash-inspect`
      lint: Fix shell lints
      CI: Enforce shell lints to prevent regression
      lint: Fix ShellCheck lints in Zcash scripts
      lint: Disable some ShellChecks on Gitian descriptors
      rust: Add shielded sighash to `zcash-inspect` output for txs
      rust: `zcash-inspect` 32-byte hex as maybe a commitment or nullifier
      build: Build Rust library and binaries at the same time
      rust: Migrate to latest `zcash_primitives` revision
      rust: Migrate Rust tests to latest `zcash_primitives` revision
      wallet: Use `auto&` to avoid copying inside `ThreadNotifyWallets`
      wallet: Refactor `ThreadNotifyWallets` to support batch memory limits
      wallet: Add dynamic memory usage tracking to `BatchScanner`
      wallet: Collect metrics on the number of scanned outputs
      wallet: Set a memory limit of 100 MiB for batch scanning
      wallet: Only store successful trial decryptions in batch scanner
      depends: Update Rust to 1.64.0
      qa: Postpone dependencies that require CMake
      qa: Postpone Clang 15 to retain LLVM 14 pin
      depends: Update cxx to 1.0.76
      depends: Update Boost to 1.80.0
      Fix clippy lints
      qa: Add audit policies for patched Rust crates
      metrics: Add gauge for the height to which the wallet is synced
      wallet: Move heap tracking of batch tasks behind a trait
      wallet: Correctly track heap usage of batch items
      wallet: Improve estimation of `rayon` spawned task size
      Squashed 'src/secp256k1/' changes from a4abaab793..efad3506a8
      Squashed 'src/secp256k1/' changes from efad3506a8..1758a92ffd
      build: Disable secp256k1 OpenSSL tests
      wallet: Remove lock on cs_main from CWallet::ChainTip
      rust: Update to `metrics 0.20`
      depends: Update cxx to 1.0.78
      qa: Postpone Clang 15.0.2
      rust: Update to `cpufeatures 0.2.5`
      qa: Recommend cargo-upgrades instead of cargo-outdated
      rust: Audit some dependency updates
      rust: Update remaining dependencies
      make-release.py: Versioning changes for 5.3.0-rc1.
      make-release.py: Updated manpages for 5.3.0-rc1.

James White (1):
      Add IPv6 support to qos.sh

Kris Nuttycombe (7):
      Update `z_sendmany` help to clarify what happens when sending from a UA
      Revert "redirect and update source documentation"
      Fix documentation line wrapping
      Backport changes from zcash.readthedocs.io
      Move restored documentation into the zcashd book
      Replace manual mangement of the Sapling proving context with cxx
      Apply suggestions from code review

Luke Dashjr (2):
      Bugfix: Only use git for build info if the repository is actually the right one
      Bugfix: Detect genbuild.sh in repo correctly

Marco Falke (1):
      Remove script to clean up datadirs

Nathan Wilcox (1):
      Update rust.md

Pieter Wuille (1):
      libsecp256k1 no longer has --with-bignum= configure option

Wladimir J. van der Laan (6):
      build: Update build system for new leveldb
      doc: Add crc32c subtree to developer notes
      test: Add crc32c to subtree check linter
      test: Add crc32c exception to various linters and generation scripts
      build: CRC32C build system integration
      build: Get rid of `CLIENT_DATE`

sasha (1):
      Make RUST_DIST in Makefile.am refer to rust-toolchain.toml (baf7d9e)

user (1):
      README.md: Clarify distinction between protocol vs zcashd implementation; link to Zebra; line wrapping.

