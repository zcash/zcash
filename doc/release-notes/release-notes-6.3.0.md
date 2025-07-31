Notable changes
===============

Network Upgrade 6.1
-------------------

The code preparations for the Network Upgrade 6.1 consensus rules are finished and
included in this release. The following ZIPs are being deployed:

- [ZIP 271: Deferred Dev Fund Lockbox Disbursement](https://zips.z.cash/zip-0271)
- [ZIP 1016: Community and Coinholder Funding Model](https://zips.z.cash/zip-1016) (partially)

NU6.1 will activate on testnet at height **3,536,500**, and can also be activated
at a specific height in regtest mode by setting the config option
`-nuparams=4dec4df0:HEIGHT`.

As with previous network upgrades, it is possible that backwards-incompatible
changes might be made to the consensus rules in this testing phase, prior to
setting the mainnet activation height. In the event that this happens, testnet
will be rolled back in v6.10.0 and a second testnet activation will occur.

See [ZIP 255](https://zips.z.cash/zip-0255) for additional information about the
deployment process for NU6.1.

Fixed Orchard bug in transparent balance APIs
---------------------------------------------

Several RPC methods inherited from Bitcoin Core internally rely on
`CWalletTx::IsFromMe` for detecting involvement of the wallet in the input
side of a transaction. For example:
- The `getbalance` RPC method uses it as part of identifying "trusted"
  zero-confirmation transactions to include in the balance calculation.
- The `gettransaction` RPC method uses it to decide whether to include a
  `fee` field in its response.

When Orchard was integrated into `zcashd`, this method was not updated to
account for it, meaning that unshielding transactions spending Orchard notes
would not be correctly accounted for in transparent-specific RPC methods. A
similar bug involving Sprout and Sapling spends was fixed in v4.5.1.

Updates to default values
-------------------------

- The default for the `-preferredtxversion` config option has been changed from
  `4` (the v4 transaction format introduced in the 2018 Sapling Network Upgrade)
  to `5` (the v5 transaction format introduced in the 2022 NU5 Network Upgrade).
  Use `-preferredtxversion=4` to retain the previous behaviour.

Changelog
=========

Daira-Emma Hopwood (2):
      Make the handling of bip0039 languages nicer.
      Comments indicating required consensus changes for draft-ecc-lockbox-disbursement.

Jack Grigg (40):
      rust: Migrate to `bip0039 0.12`
      rust: Migrate to `zcash_primitives 0.21`
      rust: clearscreen 4
      depends: cxx 1.0.158
      rust: thiserror 2
      Squashed 'src/secp256k1/' changes from 21ffe4b22a..bdf39000b9
      Squashed 'src/secp256k1/' changes from bdf39000b9..4258c54f4e
      Squashed 'src/secp256k1/' changes from 4258c54f4e..705ce7ed8c
      Squashed 'src/secp256k1/' changes from 705ce7ed8c..c545fdc374
      Squashed 'src/secp256k1/' changes from c545fdc374..199d27cea3
      Squashed 'src/secp256k1/' changes from 199d27cea3..efe85c70a2
      rust: Migrate to `secp256k1 0.29`, `zcash_primitives 0.22`
      rust: Migrate to `zcash_primitives 0.23`
      qa: Add test exposing an Orchard bug in transparent balance RPCs
      wallet: Check spent Orchard notes in `CWallet{Tx}::IsFromMe`
      depends: native_fmt 11.2.0
      depends: native_cmake 3.31.8
      depends: native_ccache 4.11.3
      depends: tl_expected 1.2.0
      depends: cxx 1.0.160
      qa: Postpone remaining dependency updates
      cargo update
      rust: Migrate to `getrandom 0.3`
      Prefer to create v5 transactions over v4 if either is possible
      Add NU6.1 to upgrade list
      Implement one-time lockbox disbursement mechanism from ZIP XXX
      qa: Add RPC test for one-time lockbox disbursements
      Add constants for the NU6.1 Funding Streams
      Stabilise NU 6.1 contents
      Update authors of Rust code
      Update lockbox disbursement code names with ZIP 271
      qa: Postpone all dependency updates until after 6.3.0
      Set support window for v6.3.0-rc1 to 3 weeks
      make-release.py: Versioning changes for 6.3.0-rc1.
      make-release.py: Updated manpages for 6.3.0-rc1.
      make-release.py: Updated release notes and changelog for 6.3.0-rc1.
      make-release.py: Updated book for 6.3.0-rc1.
      Set support window for v6.3.0 to 14 weeks
      make-release.py: Versioning changes for 6.3.0.
      make-release.py: Updated manpages for 6.3.0.

Kris Nuttycombe (1):
      ci: Use the `clone_url` field of the `repo` object for `git fetch` to check a PR

Marius Kjærstad (1):
      New checkpoint at block 3000000 for mainnet

Pieter Wuille (1):
      Disable Python lint in src/secp256k1

dorianvp (1):
      docs(getmempoolinfo): add `fullyNotified` to help command

fanquake (1):
      build: adapt Windows builds for libsecp256k1 build changes

pacu (1):
      Fixes test/sighash_tests.cpp compiler error

y4ssi (2):
      Update ci.yml
      Update ci.yml

