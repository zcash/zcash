Notable changes
===============

zcashd is being deprecated in 2025. Full nodes are being migrated to zebrad,
and the Zallet wallet is being built as a replacement for the zcashd wallet.

For some of zcashd's JSON-RPC methods, zebrad or Zallet should be a drop-in
replacement. Other JSON-RPC methods may require modified usage, and some
JSON-RPC methods will not be supported.

You can find all information about the zcashd deprecation process on this
webpage, which you can monitor for future updates:
<https://z.cash/support/zcashd-deprecation/>

We are collecting information about how zcashd users are currently using the
existing JSON-RPC methods. The above webpage has a link to a spreadsheet
containing the information we have collected so far, and the planned status
for each JSON-RPC method based on that information. If you have not provided
feedback to us about how you are using the zcashd JSON-RPC interface, please
do so as soon as possible.

To confirm that you are aware that zcashd is being deprecated and that you
will need to migrate to zebrad and/or Zallet in 2025, add the following
option to your config file:

i-am-aware-zcashd-will-be-replaced-by-zebrad-and-zallet-in-2025=1

RPC Changes
-----------

* The RPC methods `keypoolrefill`, `settxfee`, `createrawtransaction`,
  `fundrawtransaction`, and `signrawtransaction` have been deprecated, but
  are still enabled by default.
* The RPC methods `z_getbalance` (which was previously deprecated), and
  `getnetworkhashps`, and the features `deprecationinfo_deprecationheight`
  and `gbt_oldhashes`, have been disabled by default. The `addrtype` feature
  is now disabled by default even when zcashd is compiled without the
  `ENABLE_WALLET` flag.

Changelog
=========

Daira-Emma Hopwood (23):
      [doc] user/deprecation.md: add the version in which each feature was default-disabled.
      Deprecate RPC methods { `getnetworkhashps`, `keypoolrefill`, `settxfee`, `createrawtransaction`, `fundrawtransaction`, `signrawtransaction` }.
      Cosmetics in deprecation messages.
      Document that `z_getpaymentdisclosure` and `z_validatepaymentdisclosure` are deprecated.
      Default-disable the RPC methods { `z_getbalance`, `getnetworkhashps` }, and the features { `gbt_oldhashes`, `deprecationinfo_deprecationheight` }. Also make sure that the `addrtype` feature is default-disabled regardless of the `ENABLE_WALLET` flag.
      Wording changes to address review comments.
      Fix RPC tests broken by deprecations.
      Allow the RPC help to be displayed for disabled methods.
      Add `i-am-aware-zcashd-will-be-replaced-by-zebrad-and-zallet-in-2025` to release notes.
      Postpone native updates (after thorough checking with @y4ssi).
      Fix URL to the Rust Target Tier Policy.
      Update links in the README.
      Update crossbeam-channel and tokio in `Cargo.lock` to avoid vulnerable versions and pass `cargo audit`.
      Correction to the 6.2.0-rc1 release notes.
      make-release.py: Versioning changes for 6.2.0-rc1.
      make-release.py: Updated manpages for 6.2.0-rc1.
      make-release.py: Updated release notes and changelog for 6.2.0-rc1.
      make-release.py: Updated book for 6.2.0-rc1.
      Trivial doc update to poke CI.
      Update audits.
      Postpone C++ dependency updates for the v6.2.0 release.
      make-release.py: Versioning changes for 6.2.0.
      make-release.py: Updated manpages for 6.2.0.

Jack Grigg (9):
      depends: utfcpp 4.0.6
      depends: native_fmt 11.1.1
      depends: native_xxhash 0.8.3
      depends: native_cmake 3.31.3
      cargo vet prune
      depends: cxx 1.0.136
      cargo update
      CI: Migrate to `cargo-vet 0.10`
      Add a warning modal for zcashd deprecation

Larry Ruane (1):
      fix CI lint error

Maciej S. Szmigiero (1):
      dbwrapper: Bump max file size to 32 MiB

Marius Kjærstad (1):
      New checkpoint at block 2800000 for mainnet

