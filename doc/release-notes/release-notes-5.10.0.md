Notable changes
===============

Network Upgrade
---------------

- This release includes support for NU6 on Testnet. A further upgrade will be
  needed for NU6 on Mainnet.

RPC Changes
-----------

- A bug in parameter handling has been fixed for the `getaddresstxids` and
  `getaddressdeltas` RPC methods. Previously, leaving the end height unset
  would cause the start height to be set to zero and therefore ignored.
  The end height is now also automatically bounded to the chain tip; passing
  an end height greater than the chain tip height no longer results in an
  error; values up to and including the chain tip are returned instead.

Platform Support
----------------

- Debian 10 (Buster) has been removed from the list of supported platforms.
  It reached EoL on June 30th 2024, and does not satisfy our Tier 2 policy
  requirements.

Other
-----

- The `zcash-inspect` tool (which was never distributed, and was present in this
  repository for debugging purposes) has been moved to the `devtools` subfolder
  of the https://github.com/zcash/librustzcash repository.

Fixes
-----

- Security fixes for vulnerabilities disclosed in
  https://bitcoincore.org/en/2024/07/03/disclose-orphan-dos/
  and https://bitcoincore.org/en/2024/07/03/disclose-inv-buffer-blowup/ have
  been backported to `zcashd`.

Changelog
=========

0xPierre (1):
      Update README.md: fix discord link

Daira-Emma Hopwood (19):
      depends: Show the URLs from which dependencies are being downloaded.
      Update Python code to work, and avoid deprecation warnings, on Python 3.12.
      Make use of the 'filter' option to `tarfile.extractall` conditional on the version of Python that added it (3.11.4).
      Work around one of the race conditions in the `wallet_deprecation` test.
      Implement "ZIP 236: Blocks must balance exactly" for NU6.
      Add `test_framework.mininode.uint256_from_reversed_hex`.
      Ensure that `create_coinbase` can work on regtest after various network upgrades: * after Blossom, there is an extra halving of the block reward; * after NU5, `nExpiryHeight` must be equal to the block height; * after NU6, we need to take into account the lockbox value.
      Extend `coinbase_funding_streams` to also test ZIP 236.
      test_validation gtest: also test more recent upgrades in `ContextualCheckInputsDetectsOldBranchId`.
      test_validation gtest: make it more concise and readable.
      test_validation gtest: also test initialization of the other chain value pool balances (Sapling, Orchard, and Lockbox). This is not a very thorough test but it will do for now.
      test_validation gtest: calls to internal functions (`SetChainPoolValues` and `ReceivedBlockTransactions`) are required to take a lock on `cs_main` in order to work when compiled with `--enable-debug`.
      test_validation gtest: ensure that there can be no UB as a result of fake `CBlockIndex` objects still being referenced at the end of the test.
      Try to reduce the incidence of some RPC test race conditions.
      Update RPC test Python dependencies: base58 is required; simplejson is not.
      Mark mempool_nu_activation as a flaky RPC test.
      Add some diff audits to avoid exemptions for arrayref, cc, and tempfile.
      Change constant names for funding streams added in NU6 to match ZIP 214.
      Delete protocol version constants that are no longer used.

Greg Griffith (1):
      removed unused code in INV message

Gregory Maxwell (4):
      This eliminates the primary leak that causes the orphan map to  always grow to its maximum size.
      Adds an expiration time for orphan tx.
      Treat orphans as implicit inv for parents, discard when parents rejected.
      Increase maximum orphan size to 100,000 bytes.

Jack Grigg (20):
      qa: Simplify description for `license-reviewed` audit criteria
      CI: Migrate from `macos-11` runner to `macos-12` runner
      docs: Document removal of support for Debian 10
      qa: Update libsodium tag detection in `updatecheck.py`
      depends: Update Rust to 1.79.0
      rust: Silence Rust 1.79 clippy lints
      depends: native_cmake 3.30.1
      depends: native_ccache 4.10.1
      depends: Update Clang / libcxx to LLVM 18.1.8
      depends: cxx 1.0.124
      depends: libsodium 1.0.20
      qa: Postpone Boost and LevelDB updates
      cargo vet prune
      cargo update
      rust: clearscreen 3
      depends: native_ccache 4.10.2
      depends: Update Rust to 1.80.0
      rust: Remove `zcash-inspect` binary
      rust: Silence new lints
      depends: native_cmake 3.30.2

John Newbery (1):
      [net processing] Only send a getheaders for one block in an INV

Kris Nuttycombe (36):
      Add constants & configuration for NU6.
      Add `lockbox` funding stream type.
      Update `getblocksubsidy` to take lockbox funding streams into account.
      Make `GetBlockSubsidy` a method of `Consensus::Params`
      Fix inverted relationship between `consensus/params.h` and `consensus/funding.h`
      cleanup: Factor out rendundant `chainparams.GetConsensus()` calls from `ConnectBlock`
      Add convenience overload of `Params::GetActiveFundingStreamElements`
      Add `nLockboxValue` and `nChainLockboxValue` to block index state.
      Add `lockbox` to value pool balance reporting.
      Add tests for lockbox funding streams.
      Add NU6 funding streams to the consensus parameters.
      Add const modifer to the `blockIndex` argument to `GetTransaction`
      Use __func__ for substitution in `ConnectBlock` error messages.
      Improve logging of miner block construction.
      Compute chain value earlier in block processing.
      Fix comments and RPC documentation for `getaddresstxids` and `getaddressdeltas`
      Apply suggestions from code review
      Update to latest `librustzcash` release versions
      Remove the invalid `librustzcash` entry from .cargo/config.toml.offline
      Update native_rust to version 1.80.1
      Update native_cxxbridge to version 1.0.126
      Update release notes for v5.10.0-rc1
      Update `RELEASE_TO_DEPRECATION_WEEKS` to ensure v5.10.0-rc1 EOS halts before the halving.
      make-release.py: Versioning changes for 5.10.0-rc1.
      make-release.py: Updated manpages for 5.10.0-rc1.
      make-release.py: Updated release notes and changelog for 5.10.0-rc1.
      make-release.py: Updated book for 5.10.0-rc1.
      Update audit metadata for Rust crates.
      Update documentation related to changing the end-of-service halt.
      Remove unnecessary audit-as-crates-io from qa/supply-chain/config.toml
      ZIP 214: Configure Testnet funding & lockbox streams for NU6
      Update `librustzcash` rust dependencies.
      Update supply-chain audits for Rust version bumps.
      Disable macos CI runners.
      make-release.py: Versioning changes for 5.10.0.
      make-release.py: Updated manpages for 5.10.0.

Larry Ruane (1):
      fix getaddresstxids and getaddressdeltas range parsing

Marius Kjærstad (1):
      New checkpoint at block 2600000 for mainnet

Matt Corallo (1):
      Remove block-request logic from INV message processing

Pieter Wuille (4):
      Track orphan by prev COutPoint rather than prev hash
      Simplify orphan processing in preparation for interruptibility
      [MOVEONLY] Move processing of orphan queue to ProcessOrphanTx
      Interrupt orphan processing after every transaction

Yass (2):
      Adding automake in secp256k1 - MacOS's CI
      Adding coreutils in univalue - MacOS's CI

dismad (1):
      Update README.md, ZecHub URL update

jimmycathy (1):
      chore: remove repetitive words

mrbandrews (1):
      [qa] Make comptool push blocks instead of relying on inv-fetch

y4ssi (1):
      deprecating buster

