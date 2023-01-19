Notable changes
===============

Fixes
-----

This release fixes an issue that could potentially cause a node to crash with the
log message "The wallet's best block hash `<hash>` was not detected in restored
chain state. Giving up; please restart with `-rescan`."

RPC Changes
-----------

- `z_sendmany` will no longer select transparent coinbase when "ANY\_TADDR" is
  used as the `fromaddress`. It was already documented to do this, but the
  previous behavior didn’t match. When coinbase notes were selected in this
  case, they would (properly) require that the transaction didn’t have any
  change, but this could be confusing, as the documentation stated that these
  two conditions (using "ANY\_TADDR" and disallowing change) wouldn’t coincide.

[Deprecations](https://zcash.github.io/zcash/user/deprecation.html)
--------------

The following features have been deprecated, but remain available by default.
These features may be disabled by setting `-allowdeprecated=none`. 18 weeks
after this release, these features will be disabled by default and the following
flags to `-allowdeprecated` will be required to permit their continued use:

- `gbt_oldhashes`: the `finalsaplingroothash`, `lightclientroothash`, and
  `blockcommitmentshash` fields in the output of `getblocktemplate` have been
  replaced by the `defaultroots` field.

The following previously-deprecated features have been disabled by default, and
will be removed in 18 weeks:

- `legacy_privacy`
- `getnewaddress`
- `getrawchangeaddress`
- `z_getbalance`
- `z_gettotalbalance`
- `z_getnewaddress`
- `z_listaddresses`
- `addrtype`
- `wallettxvjoinsplit`

The following previously-deprecated features have been removed:

- `dumpwallet`
- `zcrawreceive`
- `zcrawjoinsplit`
- `zcrawkeygen`

Platform Support
----------------

- CentOS 8 has been removed from the list of supported platforms. It reached EoL
  on December 31st 2021, and does not satisfy our Tier 2 policy requirements.

Changelog
=========

Alex Morcos (3):
      Make accessing mempool parents and children public
      Expose FormatStateMessage
      Rewrite CreateNewBlock

Carl Dong (4):
      depends: More robust cmake invocation
      depends: Cleanup CMake invocation
      depends: Prepend CPPFLAGS to C{,XX}FLAGS for CMake
      depends: Specify LDFLAGS to cmake as well

Daira Hopwood (6):
      Add tl::expected. refs #4816
      The std::expected proposal has unnecessary instances of undefined behaviour for operator->, operator*, and error(). Make these into assertion failures (this still conforms to the proposal).
      Refactor HaveShieldedRequirements to use tl::expected (example with a void T) and rename it to CheckShieldedRequirements.
      tl::expected follow-up to address @str4d's comments.
      Cleanup after removing dumpwallet.
      Change the time that the wallet will wait for the block index to load from 5 minutes to 2 hours.

Dimitris Apostolou (2):
      Fix typos
      Fix typos

Greg Pfeil (22):
      Add PrivacyPolicyMeet
      Remove trailing whitespace in fetch-params.sh
      Migrate fetch-params.sh to bash
      Scope the fetch-params lock file to the user
      Update comments to match changed tests
      Put utf8.h in the correct place
      Don’t select transparent coinbase with ANY_TADDR
      Update failing tests after fixing ANY_TADDR behavior
      Apply suggestions from code review
      Apply suggestions from code review
      Appease ShellCheck
      Defer fixing docker/entrypoint.sh lint failure
      Apply suggestions from code review
      Fix a minor bug in docker/entrypoint.sh
      Improve PrivacyPolicy comments
      Apply suggestions from code review
      Add release notes
      Update src/wallet/asyncrpcoperation_sendmany.cpp
      Fix a missing newline in the RPC docs
      No longer test_received_sprout
      Use cached sprout addresses rather than funding
      Update overwinter test to not shield to Sprout

Jack Grigg (39):
      test: Handle mining slow start inside `CreateNewBlock_validity`
      test: Improve CreateNewBlock_validity exception checks
      txdb: Remove const annotation from blockinfo iterator type
      Remove `dumpwallet` RPC method
      qa: Refactor `wallet_deprecation` test to simplify deprecation changes
      Remove `zcraw*` RPC methods
      txdb: Clean up for loop syntax in `WriteBatchSync`
      Disable previously-deprecated features by default
      Deprecate old hash fields of `getblocktemplate`
      qa: Change show_help RPC test to print out differences
      qa: Update mempool_packages RPC test after deprecation ratcheting
      qa: Import Rust crate audits from Firefox
      qa: Import Rust crate audits from the Bytecode Alliance
      qa: Import Rust crate audits from Embark Studios
      qa: Remove audit-as-crates-io for non-third-party crates
      cargo update
      zcash_primitives 0.9
      clearscreen 2.0
      depends: googletest 1.12.1
      Remove CentOS 8 as a supported platform
      depends: native_zstd 1.5.2
      depends: native_ccache 4.6.3
      depends: Add package for native_cmake 3.25.1
      depends: Force cmake to install libzstd in lib/
      build-aux: Update Boost macros to latest serials
      build: Bump required Boost version
      depends: Force Boost library to be installed in lib/
      depends: Add tl_expected to update checker
      depends: Boost 1.81.0
      depends: utfcpp 3.2.3
      qa: Postpone LLVM 15 and CCache 4.7 updates
      depends: Update cxx to 1.0.83
      cargo update
      Document -clockoffset option
      qa: Update show_help RPC test
      doc: Fix arguments to make-release.py in hotfix process
      depends: CMake 3.25.2
      make-release.py: Versioning changes for 5.4.0-rc1.
      make-release.py: Updated manpages for 5.4.0-rc1.

James O'Beirne (2):
      Clarify help messages for path args to mention datadir prefix
      Add AbsPathForConfigVal to consolidate datadir prefixing for path args

Kris Nuttycombe (6):
      Add TransactionStrategy::IsCompatibleWith
      Modify TransactionBuilder to use the standard default fee.
      Factor out memo parsing from asyncrpcoperation_sendmany
      Remove mergetoaddress_sprout test as sending to Sprout is no longer supported.
      Remove wallet_shieldcoinbase_sprout test.
      Update `mergetoaddress_mixednotes.py` to no longer send to Sprout.

Marco Falke (4):
      [init] Add missing help for args
      [init] Help Msg: Use Params(CBaseChainParams::MAIN)
      Clarify mocktime help message
      init: Fix help message for checkblockindex

Marius Kjærstad (5):
      Hardened checkpoint update at block 1860000 for mainnet
      Update src/chainparams.cpp
      Some more formatting changes to chainparams.cpp
      Forgot to add 0x
      Add some more historical checkpoints

Mark Friedenbach (1):
      Prevent block.nTime from decreasing

Marshall Gaucher (4):
      Update zcash-build-bench.yml
      Update README.md
      Update contrib/ci-builders/tekton/tekton-labs/tasks/zcash-build.yml
      Update contrib/ci-builders/tekton/tekton-labs/tasks/zcash-build-test.yml

Michał Janiszewski (1):
      Update debian/compat to version 13

Russell Yanofsky (2):
      depends: Add CMake helper for building packages
      depends: Set CMAKE_INSTALL_RPATH for native packages

Suhas Daftuar (3):
      Track transaction packages in CTxMemPoolEntry
      Add test showing bug in mempool packages
      Fix mempool package tracking edge case

Wladimir J. van der Laan (1):
      rpc: Write authcookie atomically

Marshall Gaucher (2):
      add basic tekton zcash env
      update memory targets with heaptrack

