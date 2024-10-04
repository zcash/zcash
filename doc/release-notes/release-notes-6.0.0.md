Notable changes
===============

The mainnet activation of the NU6 network upgrade is supported by the 6.0.0
release, with an activation height of 2726400, which should occur on
approximately November 23, 2024. Please upgrade to this release, or any
subsequent release, in order to follow the NU6 network upgrade.

The following ZIPs are being deployed, or have been updated, as part of this upgrade:

* [ZIP 207: Funding Streams (updated)](https://zips.z.cash/zip-0207)
* [ZIP 214: Consensus rules for a Zcash Development Fund (updated)](https://zips.z.cash/zip-0214)
* [ZIP 236: Blocks should balance exactly](https://zips.z.cash/zip-0236)
* [ZIP 253: Deployment of the NU6 Network Upgrade](https://zips.z.cash/zip-0253)
* [ZIP 1015: Block Reward Allocation for Non-Direct Development Funding](https://zips.z.cash/zip-1015)
* [ZIP 2001: Lockbox Funding Streams](https://zips.z.cash/zip-2001)

In order to help the ecosystem prepare for the mainnet activation, NU6 has
already been activated on the Zcash testnet. Any node version 5.10.0 or higher,
including this release, supports the NU6 activation on testnet.

Mining
------

- The default setting of `-blockunpaidactionlimit` is now zero, which has
  the effect of no longer allowing "unpaid actions" in [block production].
  This adapts to current network conditions. If you have overridden this
  setting as a miner, we recommend removing the override. This configuration
  option may be removed entirely in a future release.

[block production]: https://zips.z.cash/zip-0317#block-production

Platform Support
----------------

- Windows builds have been fixed.

Changelog
=========

Daira-Emma Hopwood (9):
      Ensure out-reference parameters of `CWallet::CreateTransaction` are initialized.
      Rename ecc_addresses to bp_addresses in chainparams.cpp.
      Make DEFAULT_BLOCK_UNPAID_ACTION_LIMIT zero. fixes #6899 (see that issue for rationale)
      Add more detail to the "tx unpaid action limit exceeded" message.
      Use at least the ZIP 317 fee for Sprout->Sapling migration.
      Repair the RPC tests.
      Add a regression test for the ZIP 317 default fee bug (#6956), and make the tests pass for now.
      Code of Conduct: update email addresses and remove Sean as a contact.
      Code of Conduct: add Kris and Str4d as contacts.

Jack Grigg (24):
      depends: Update Rust to 1.81.0
      depends: native_cmake 3.30.3
      depends: cxx 1.0.128
      cargo vet prune
      cargo update
      qa: Postpone Boost, LevelDB, and Clang updates
      Fix clippy lints for 1.81
      Remove `#[should_panic]` tests of `extern "C"` functions
      depends: Fix incompatibility between libsodium 1.0.20 and Clang 18
      depends: Downgrade libc++ for MinGW to 18.1.6-1
      Migrate to latest revision of Zcash Rust crates
      depends: native_cmake 3.30.4
      Update release notes
      Decrease support window to 6 weeks for 6.0.0-rc1
      make-release.py: Versioning changes for 6.0.0-rc1.
      make-release.py: Updated manpages for 6.0.0-rc1.
      make-release.py: Updated release notes and changelog for 6.0.0-rc1.
      make-release.py: Updated book for 6.0.0-rc1.
      qa: Add latest Clang release to postponed updates
      Migrate to librustzcash crates revision right before NU6 mainnet height
      Set support window back to the usual 16 weeks
      Update release notes for 6.0.0
      make-release.py: Versioning changes for 6.0.0.
      make-release.py: Updated manpages for 6.0.0.

Kris Nuttycombe (1):
      Use scopes to make it more obvious that certain variables are never used.

y4ssi (2):
      fix gitian-descriptors
      Simplify Dockerfile (#6906)

