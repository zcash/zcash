Notable changes
===============

Node Performance Improvements
-----------------------------

This release makes several changes to improve the performance of node operations.
These include:

- Backported CuckooCache from upstream to improve the performance of signature
  caching.

- Added caching of proof and signature validation results for Sapling and
  Orchard to eliminate redundant computation.

- Backported SHA-256 assembly optimizations from upstream.

Wallet Performance Improvements
-------------------------------

This release makes several changes to improve the performance of wallet operations.
These include:

- We now parallelize and batch trial decryption of Sapling outputs.

- We now prune witness data in the wallet for notes spent more than 100 blocks
  in the past, so that we can avoid unnecessarily updating those witnesses.
  In order to take advantage of this performance improvement, users will need
  to start their nodes with `-rescan` the first time .

- The process for incrementing the witnesses for notes the wallet is tracking
  has been optimized to avoid redundant passes over the wallet.

- Removed an assertion that was causing a slowdown in wallet scanning post-NU5.

RPC Interface Changes
=====================

- A `version` field was added to the result for the `gettransaction` RPC call to
  avoid the need to make an extra call to `getrawtransaction` just to retrieve
  the version.

Fixes
=====

- Fixed a regression that caused an incorrect process name to appear in the
  process list.

Changelog
=========

Aditya Kulkarni (1):
      Add tx version

Ben Woosley (1):
      build: Detect gmtime_* definitions via configure

Chun Kuan Lee (1):
      Use __cpuid_count for gnu C to avoid gitian build fail.

Cory Fields (2):
      time: add runtime sanity check
      build: always attempt to enable targeted sse42 cxxflags

Daira Hopwood (3):
      This reverts part of 1f1810c37d00cb46d00d8553e6de3c6fdb991010 in #5959. Leaving the main thread unnamed causes it to be displayed as the executable name (i.e. `zcashd`) or command line in process monitoring tools. fixes #6066
      Apply doc suggestions from code review
      Use crossbeam-channel instead of std::sync::mpsc.

Dan Raviv (1):
      Fix header guards using reserved identifiers

Gregory Maxwell (1):
      Add an explanation of quickly hashing onto a non-power of two range.

Hennadii Stepanov (1):
      Use correct C++11 header for std::swap()

Jack Grigg (16):
      lint: Fix include guards
      wallet: Prune witnesses for notes spent more than 100 blocks ago
      wallet: Make `{Increment, Decrement}NoteWitnesses`-internal helpers static
      Cache Sapling and Orchard bundle validation
      Add bundle kind to `BundleValidityCache` initialization log message
      wallet: Throw error if `ReadBlockFromDisk` fails
      wallet: Improve documentation of `SproutNotData` and `SaplingNoteData`
      Move explicit instantiations for `BundleValidityCache` into `zcash/cache.cpp`
      bench: Fix ConnectBlock large block benchmarks
      wallet: Add `BatchScanner` interface to `CValidationInterface`
      wallet: Pass `Consensus::Params` into `CWallet::FindMySaplingNotes`
      wallet: Migrate `CWallet` to `CValidationInterface::InitBatchScanner`
      rust: Implement multithreaded batched trial decryption for Sapling
      wallet: Use batch trial decryption for Sapling outputs
      wallet: Enforce an assumption about how wallet data evolves
      wallet: Domain-separate batched txids with a "block tag"

Jeremy Rubin (4):
      Add CuckooCache implementation and replace the sigcache map_type with it
      Add unit tests for the CuckooCache
      Decrease testcase sizes in cuckoocache tests
      Deduplicate SignatureCacheHasher

Jim Posen (1):
      scripted-diff: Move util files to separate directory.

Jon Layton (1):
      doc: Doxygen-friendly CuckooCache comments

Kris Nuttycombe (15):
      scripted-diff: Move utiltest to src/util
      Add a clock for testing with an offset from the system clock.
      Apply suggestions from code review
      Add persistent Sprout test data.
      Remove the temporary test that was used for setup of the cached Sprout fixtures.
      Add RPC test initialization using the persisted Sprout chains.
      Update sprout_sapling_migration test to use persisted sprout chains.
      Update feature_zip239 test to use persisted sprout chains.
      Add missing <chrono> header to util/time.h
      Update finalsaplingroot test to use persisted sprout chains.
      Update getblocktemplate test to use persisted sprout chain
      Replace setup_clean_chain with cache_behavior in rpc test init.
      qa: Postpone recent native_rust, native_cxxbridge and rustcxx updates
      make-release.py: Versioning changes for 5.2.0-rc1.
      make-release.py: Updated manpages for 5.2.0-rc1.

Luke Dashjr (3):
      configure: Invert --enable-asm help string since default is now enabled
      configure: Skip assembly support checks, when assembly is disabled
      configure: Initialise assembly enable_* variables

Marshall Gaucher (2):
      Update entrypoint.sh
      Update contrib/docker/entrypoint.sh

Pieter Wuille (34):
      Allow non-power-of-2 signature cache sizes
      Do not store Merkle branches in the wallet.
      Avoid duplicate CheckBlock checks
      Add merkle.{h,cpp}, generic merkle root/branch algorithm
      Switch blocks to a constant-space Merkle root/branch algorithm.
      Add FastRandomContext::rand256() and ::randbytes()
      scripted-diff: Rename cuckoo tests' local rand context
      Merge test_random.h into test_bitcoin.h
      Add various insecure_rand wrappers for tests
      scripted-diff: use insecure_rand256/randrange more
      Replace more rand() % NUM by randranges
      Replace rand() & ((1 << N) - 1) with randbits(N)
      Use randbits instead of ad-hoc emulation in prevector tests
      scripted-diff: Use randbits/bool instead of randrange where possible
      scripted-diff: Use new naming style for insecure_rand* functions
      Support multi-block SHA256 transforms
      Add SHA256 dispatcher
      Add SSE4 based SHA256
      Add selftest for SHA256 transform
      Protect SSE4 code behind a compile-time flag
      Benchmark Merkle root computation
      Refactor SHA256 code
      Specialized double sha256 for 64 byte inputs
      Use SHA256D64 in Merkle root computation
      4-way SSE4.1 implementation for double SHA256 on 64-byte inputs
      8-way AVX2 implementation for double SHA256 on 64-byte inputs
      [MOVEONLY] Move unused Merkle branch code to tests
      Enable double-SHA256-for-64-byte code on 32-bit x86
      Improve coverage of SHA256 SelfTest code
      For AVX2 code, also check for AVX, XSAVE, and OS support
      [Refactor] CPU feature detection logic for SHA256
      Add SHA256 implementation using using Intel SHA intrinsics
      Use immintrin.h everywhere for intrinsics
      Avoid non-trivial global constants in SHA-NI code

Wladimir J. van der Laan (2):
      build: Rename --enable-experimental-asm to --enable-asm and enable by default
      build: Mention use of asm in summary

furszy (3):
      Fix missing vector include and vector type definition
      Rework Sprout and Sapling witnesses increment and cache workflow, so it does not loop over the entire wallet txs map indiscriminately.
      Use references instead of pointers where possible.

instagibbs (1):
      Return useful error message on ATMP failure

nathannaveen (1):
      chore: Set permissions for GitHub actions

practicalswift (1):
      Use explicit casting in cuckoocache's compute_hashes(...) to clarify integer conversion

sasha (3):
      Patch smoke_tests.py to properly handle changes in minconf behavior
      Improve smoke_test.py wait_for_balance message in the minconf!=1 case
      Patch smoke_tests.py to require 4 confirmations for z_mergetoaddress

Jack Grigg (2):
      Improve bundlecache documentation
      Minor fixes to documentation

Zancas Wilcox (1):
      match the actual two hyphen flag

