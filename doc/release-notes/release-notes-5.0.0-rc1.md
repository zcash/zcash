Notable changes
===============

Feature Deprecation and removal
-------------------------------

`zcashd` now has a [process](https://zcash.github.io/zcash/user/deprecation.html)
for how features of the public API may be deprecated and removed. Feature
deprecation follows a series of steps whereby, over a series of releases,
features first remain enabled by default (but may be explicitly disabled), then
switch to being disabled by default, and eventually are removed entirely.

A new string-valued option, `-allowdeprecated` has been introduced to allow a
user to explicitly manage the availability of deprecated `zcashd` features.  This
flag makes it possible for users to reenable deprecated methods and features
api that are currently disabled by default, or alternately to explicitly
disable all deprecated features if they so choose. Multiple instances of this
argument may be provided. A user may disable deprecated features entirely
by providing the string `none` as the argument to this parameter. In the case
that `none` is specified, multiple invocations of `-allowdeprecated` are not
permitted.

Deprecated
----------

As of this release, the following features are deprecated, but remain 
available by default. These features may be disabled by setting 
`-allowdeprecated=none`. After release 5.3.0, these features will be
disabled by default and the following flags to `-allowdeprecated` will
be required to permit their continued use:

  - `legacy_privacy` - the default "legacy" privacy policy for z_sendmany
    is deprecated. When disabled, the default behavior of z_sendmany will
    conform to the `FullPrivacy` directive (introduced in 4.7.0) in all cases
    instead of just for transactions involving unified addresses.
  - `getnewaddress` - controls availability of the `getnewaddress` RPC method.
  - `z_getnewaddress` - controls availability of the `z_getnewaddress` RPC method.
  - `addrtype` - controls availability of the deprecated `type` attribute
    returned by RPC methods that return address metadata. 

As of this release, the following previously deprecated features are disabled
by default, but may be be reenabled using `-allowdeprecated=<feature>`.

  - The `zcrawreceive` RPC method is disabled. It may be reenabled with
    `allowdeprecated=zcrawreceive`
  - The `zcrawjoinsplit` RPC method is disabled. It may be reenabled with
    `allowdeprecated=zcrawjoinsplit`
  - The `zcrawkeygen` RPC method is disabled. It may be reenabled with
    `allowdeprecated=zcrawkeygen`

Option handling
---------------

- The `-reindex` and `-reindex-chainstate` options now imply `-rescan`
  (provided that the wallet is enabled and pruning is disabled, and unless
  `-rescan=0` is specified explicitly).
- A new `-anchorconfirmations` argument has been added to allow the user
  to specify the number of blocks back from the chain tip that anchors will be
  selected from when spending notes. By default, anchors will now be selected
  to have 3 confirmations. Values greater than 100 are not supported.
- A new `-orchardactionlimit` option has been added to allow the user to
  override the default maximum of 50 Orchard actions per transaction. 
  Transactions that contain large numbers of Orchard actions can use 
  large amounts of memory for proving, so the 50-action default limit is
  imposed to guard against memory exhaustion. Systems with more than 16G
  of memory can safely set this parameter to allow 200 actions or more.

RPC Interface
-------------

- The default `minconf` value for `z_sendmany` is now 10 confirmations instead
  of 1. If `minconf` and specifies a value less than that provided for
  `-anchorconfirmations`, it will also override that value as it is not
  possible to spend notes that are more recent than the anchor. Selecting
  `minconf` values less than 3 is not recommended, as it allows the transaction
  to be distinguished from transactions using the default for
  `-anchorconfirmations`.

RPC Changes
-----------

- The deprecated `zcrawkeygen`, `zcrawreceive`, and `zcrawjoinsplit` RPC
  methods are now disabled by default. Use `-allowdeprecated=<feature>`
  to select individual features if you wish to continue using these APIs.

Build system
------------

- `zcutil/build.sh` now automatically runs `zcutil/clean.sh` to remove
  files created by previous builds. We previously recommended to do this
  manually.

Dependencies
------------

- The `boost` and `native_b2` dependencies have been updated to version 1.79.0

Tests
-----

- The environment variable that allows users of the rpc (Python) tests to
  override the default path to the `zcashd` executable has been changed
  from `BITCOIND` to `ZCASHD`.

Changelog
=========

Alex Wied (1):
      Cargo.toml: Rename hdwallet source

Charlie O'Keefe (1):
      Use bullseye apt source in Dockerfile to match debian:11 base image

Daira Hopwood (9):
      Fix to 4.7.0 release notes: testnet nodes that upgrade prior to height 1,842,420 actually still need to run with -reindex and -rescan.
      zcutil/build.sh: Run zcutil/clean.sh before building. fixes #3625
      Make `-reindex` and `-reindex-chainstate` imply `-rescan` (provided that the wallet is enabled and pruning is disabled, and unless `-rescan=0` is specified explicitly).
      zcutil/build-debian-package.sh: copy executable and man page for zcashd-wallet-tool.
      zcashd-wallet-tool: improve the error message for an invalid logging filter directive.
      Change the numbering convention for hotfixes to increment the patch number, not the hyphen number. fixes #4364
      Rename nOrchardAnchorConfirmations -> nAnchorConfirmations
      Select an anchor and notes that have 10 confirmations.
      Fix WalletTests.CachedWitnessesEmptyChain for new anchor selection.

Dimitris Apostolou (1):
      Fix typo

Jack Grigg (3):
      Update minimum chain work and set NU5 activation block hash for testnet
      make-release.py: Versioning changes for 5.0.0-rc1.
      make-release.py: Updated manpages for 5.0.0-rc1.

Kris Nuttycombe (30):
      Update boost dependencies to version 1.79.0
      Default to error logging if we can't parse the log filter.
      Fix boolean initialization in Orchard transaction builder.
      Use fallible version parsing for tags.
      Allow deprecated wallet features to be preemptively disabled.
      Remove zcrawreceive, zcrawjoinsplit, zcrawkeygen from default-allowed deprecated methods.
      Add deprecation policy to the zcashd book.
      Use ERROR level logging for fatal errors in main/init
      Remove `-allowdeprecated=all`
      Clarify documentation of the deprecation process.
      Select Orchard anchors at `-orchardanchorconfirmations` depth.
      Add anchor depth parameter to Get*NoteWitnesses
      Disallow -anchorconfirmations values > 100
      Add -anchorconfirmations to the release notes
      Fix RPC tests that depend upon -anchorconfirmations=1
      Apply suggestions from code review
      Gracefully handle Get(Sprout/Sapling)NoteWitnesses failure.
      Add parity-scale-codec licenses to contrib/debian/copyright
      Ensure transaction integer fields are zero-initialized.
      Build releases from a commit hash, rather than a named branch.
      Apply suggestions from code review
      Update release process documentation to clarify the use of release stabilization branches.
      Fix missing handling for imported transparent multisig addresses.
      Change default anchor depth from 10 confirmations to 3
      Use default anchor confirmations for minconf in z_mergetoaddress.
      Add -orchardactionlimit parameter to guard against memory exhaustion.
      Add -orchardactionlimit help text.
      Postpone dependency updates prior to v5.0.0-rc1
      Set RELEASE_TO_DEPRECATION_WEEKS to 2 weeks to provide RC EOS halt.
      Fix a typo in the release script.

Larry Ruane (2):
      Allow rpc python tests to be run standalone
      ThreadStartWalletNotifier: wait until !IBD, rather than !reindex

Marek (2):
      Document the block time in the `z_gettreestate` RPC response
      Specify the format and epoch

Steven Smith (2):
      Add orchard pool metrics
      Require wallet recovery phrase to be backed up for z_getnewaccount and z_getaddressforaccount

Taylor Hornby (1):
      Reproduce an assertion failure in the listaddresses RPC

dependabot[bot] (1):
      Bump actions/checkout from 2 to 3

sasha (28):
      Move LoadProofParameters to gtest/utils.cpp
      add tx-orchard-duplicate-nullifiers.h to Makefile.gtest.include
      Closing #1539 simplifies gtest Makefile.
      remove JoinSplitTestingSetup from sighash_tests -- it doesn't need it
      Make a LoadGlobalWallet and UnloadGlobalWallet for gtests
      Remove proof parameter loading from btests
      Separate test suite from tests, inspired by upstream's #12926
      Allow parallel btest runs using make as the parallelization tool.
      btest parallelization work: tag #if'd out test suites
      Port btest test_basic_joinsplit_verification to gtest suite Joinsplit
      Create a new gtest group WalletRPCTests
      Port btest rpc_z_shieldcoinbase_internals to gtest suite WalletRPCTests
      Port btest rpc_z_mergetoaddress_internals to gtest suite WalletRPCTests
      Port btest rpc_z_sendmany_taddr_to_sapling to gtest suite WalletRPCTests
      Downgrade btest suite coins_test to BasicTestingSetup
      Make [seed_]insecure_rand available to the gtests
      Create a new gtest suite CoinsTests
      Port CCoinsViewTest to gtest suite CoinsTests
      Port nullifier_regression_test to gtest suite CoinsTests
      Port anchor_pop_regression_test to gtest suite CoinsTests
      Port anchor_regression_test to gtest suite CoinsTests
      Port nullifiers_test to gtest suite CoinsTests
      Port anchors_flush_test to gtest suite CoinsTests
      Port anchors_test to gtest suite CoinsTests
      Update copyright header
      Update comments in newly-ported gtests to be more consistent with current codebase
      Tidy up spacing in newly-ported gtests
      Remove -developersapling since it hasn't been implemented for a long time

teor (1):
      Fix typo in getaddressbalance RPC help

