Notable changes
===============

Network Upgrade 3: Heartwood
----------------------------

The code preparations for the Heartwood network upgrade are finished and
included in this release. The following ZIPs are being deployed:

- [ZIP 213: Shielded Coinbase](https://zips.z.cash/zip-0213)
- [ZIP 221: FlyClient - Consensus-Layer Changes](https://zips.z.cash/zip-0221)

Heartwood will activate on testnet at height 903800, and can also be activated
at a specific height in regtest mode by setting the config option
`-nuparams=f5b9230b:HEIGHT`.

As a reminder, because the Heartwood activation height is not yet specified for
mainnet, version 2.1.2 will behave similarly as other pre-Heartwood releases
even after a future activation of Heartwood on the network. Upgrading from 2.1.2
will be required in order to follow the Heartwood network upgrade on mainnet.

See [ZIP 250](https://zips.z.cash/zip-0250) for additional information about the
deployment process for Heartwood.

### Mining to Sapling addresses

Miners and mining pools that wish to test the new "shielded coinbase" support on
the Heartwood testnet can generate a new Sapling address with `z_getnewaddress`,
add the config option `mineraddress=SAPLING_ADDRESS` to their `zcash.conf` file,
and then restart their `zcashd` node. `getblocktemplate` will then return
coinbase transactions containing a shielded miner output.

Note that `mineraddress` should only be set to a Sapling address after the
Heartwood network upgrade has activated; setting a Sapling address prior to
Heartwood activation will cause `getblocktemplate` to return block templates
that cannot be mined.

Sapling viewing keys support
----------------------------

Support for Sapling viewing keys (specifically, Sapling extended full viewing
keys, as described in [ZIP 32](https://zips.z.cash/zip-0032)), has been added to
the wallet. Nodes will track both sent and received transactions for any Sapling
addresses associated with the imported Sapling viewing keys.

- Use the `z_exportviewingkey` RPC method to obtain the viewing key for a
  shielded address in a node's wallet. For Sapling addresses, these always begin
  with "zxviews" (or "zxviewtestsapling" for testnet addresses).

- Use `z_importviewingkey` to import a viewing key into another node.  Imported
  Sapling viewing keys will be stored in the wallet, and remembered across
  restarts.

- `z_getbalance` will show the balance of a Sapling address associated with an
  imported Sapling viewing key. Balances for Sapling viewing keys will be
  included in the output of `z_gettotalbalance` when the `includeWatchonly`
  parameter is set to `true`.

- RPC methods for viewing shielded transaction information (such as
  `z_listreceivedbyaddress`) will return information for Sapling addresses
  associated with imported Sapling viewing keys.

Details about what information can be viewed with these Sapling viewing keys,
and what guarantees you have about that information, can be found in
[ZIP 310](https://zips.z.cash/zip-0310).

Removal of time adjustment and the -maxtimeadjustment= option
-------------------------------------------------------------

Prior to v2.1.1-1, `zcashd` would adjust the local time that it used by up
to 70 minutes, according to a median of the times sent by the first 200 peers
to connect to it. This mechanism was inherently insecure, since an adversary
making multiple connections to the node could effectively control its time
within that +/- 70 minute window (this is called a "timejacking attack").

In the v2.1.1-1 security release, in addition to other mitigations for
timejacking attacks, the maximum time adjustment was set to zero by default.
This effectively disabled time adjustment; however, a `-maxtimeadjustment=`
option was provided to override this default.

As a simplification the time adjustment code has now been completely removed,
together with `-maxtimeadjustment=`. Node operators should instead ensure that
their local time is set reasonably accurately.

If it appears that the node has a significantly different time than its peers,
a warning will still be logged and indicated on the metrics screen if enabled.

View shielded information in wallet transactions
------------------------------------------------

In previous `zcashd` versions, to obtain information about shielded transactions
you would use either the `z_listreceivedbyaddress` RPC method (which returns all
notes received by an address) or `z_listunspent` (which returns unspent notes,
optionally filtered by addresses). There were no RPC methods that directly
returned details about spends, or anything equivalent to the `gettransaction`
method (which returns transparent information about in-wallet transactions).

This release introduces a new RPC method `z_viewtransaction` to fill that gap.
Given the ID of a transaction in the wallet, it decrypts the transaction and
returns detailed shielded information for all decryptable new and spent notes,
including:

- The address that each note belongs to.
- Values in both decimal ZEC and zatoshis.
- The ID of the transaction that each spent note was received in.
- An `outgoing` flag on each new note, which will be `true` if the output is not
  for an address in the wallet.
- A `memoStr` field for each new note, containing its text memo (if its memo
  field contains a valid UTF-8 string).

Information will be shown for any address that appears in `z_listaddresses`;
this includes watch-only addresses linked to viewing keys imported with
`z_importviewingkey`, as well as addresses with spending keys (both generated
with `z_getnewaddress` and imported with `z_importkey`).

Better error messages for rejected transactions after network upgrades
----------------------------------------------------------------------

The Zcash network upgrade process includes several features designed to protect
users. One of these is the "consensus branch ID", which prevents transactions
created after a network upgrade has activated from being replayed on another
chain (that might have occurred due to, for example, a
[friendly fork](https://electriccoin.co/blog/future-friendly-fork/)). This is
known as "two-way replay protection", and is a core requirement by
[various](https://blog.bitgo.com/bitgos-approach-to-handling-a-hard-fork-71e572506d7d?gi=3b80c02e027e)
[members](https://trezor.io/support/general/hard-forks/) of the cryptocurrency
ecosystem for supporting "hard fork"-style changes like our network upgrades.

One downside of the way replay protection is implemented in Zcash, is that there
is no visible difference between a transaction being rejected by a `zcashd` node
due to targeting a different branch, and being rejected due to an invalid
signature. This has caused issues in the past when a user had not upgraded their
wallet software, or when a wallet lacked support for the new network upgrade's
consensus branch ID; the resulting error messages when users tried to create
transactions were non-intuitive, and particularly cryptic for transparent
transactions.

Starting from this release, `zcashd` nodes will re-verify invalid transparent
and Sprout signatures against the consensus branch ID from before the most
recent network upgrade. If the signature then becomes valid, the transaction
will be rejected with the error message `old-consensus-branch-id`. This error
can be handled specifically by wallet providers to inform the user that they
need to upgrade their wallet software.

Wallet software can also automatically obtain the latest consensus branch ID
from their (up-to-date) `zcashd` node, by calling `getblockchaininfo` and
looking at `{'consensus': {'nextblock': BRANCH_ID, ...}, ...}` in the JSON
output.

Expired transactions notifications
----------------------------------

A new config option `-txexpirynotify` has been added that will cause `zcashd` to
execute a command when a transaction in the mempool expires. This can be used to
notify external systems about transaction expiry, similar to the existing
`-blocknotify` config option that notifies when the chain tip changes.

RPC methods
-----------

- The `z_importkey` and `z_importviewingkey` RPC methods now return the type of
  the imported spending or viewing key (`sprout` or `sapling`), and the
  corresponding payment address.

- Negative heights are now permitted in `getblock` and `getblockhash`, to select
  blocks backwards from the chain tip. A height of `-1` corresponds to the last
  known valid block on the main chain.

- A new RPC method `getexperimentalfeatures` returns the list of enabled
  experimental features.

Build system
------------

- The `--enable-lcov`, `--disable-tests`, and `--disable-mining` flags for
  `zcutil/build.sh` have been removed. You can pass these flags instead by using
  the `CONFIGURE_FLAGS` environment variable. For example, to enable coverage
  instrumentation (thus enabling "make cov" to work), call:

  ```
  CONFIGURE_FLAGS="--enable-lcov --disable-hardening" ./zcutil/build.sh
  ```

- The build system no longer defaults to verbose output. You can re-enable
  verbose output with `./zcutil/build.sh V=1`

Changelog
=========

Alfredo Garcia (40):
      remove SignatureHash from python rpc tests
      add negative height to getblock
      allow negative index to getblockhash
      update docs
      add additional tests to rpc_wallet_z_getnewaddress
      change convention
      change regex
      Return address and type of imported key in z_importkey
      Delete travis file
      dedup decode keys and addresses
      remove unused imports
      add txexpirynotify
      fix rpx_wallet_tests
      remove debug noise from 2 gtests
      make type and size a pair in DecodeAny arguments
      add missing calls to DecodeAny
      add destination wrappers
      change tuples to classes
      change cm() to cmu() in SaplingNote class
      change the cm member of OutputDescription to cmu
      change maybe_cm to maybe_cmu
      add getexperimentalfeatures rpc call
      refactor experimental features
      make fInsightExplorer a local
      add check_node_log utility function
      remove space after new line
      move check_node_log framework test to a new file
      use check_node_log in turnstile.py
      add stop_node argument to check_node_log, use it in shieldingcoinbase
      change constructors
      minor comment fix
      preserve test semantics
      remove unused import
      multiple debug categories documentation
      return address info in z_importviewingkey
      add expected address check to tests
      change unclear wording in z_import calls address returned
      Lock with cs_main inside gtests that call chainActive.Height()
      add -lightwalletd experimental option
      compute more structures in mempool DynamicMemoryUsage

Carl Dong (1):
      autoconf: Sane --enable-debug defaults.

Chun Kuan Lee (1):
      Reset default -g -O2 flags when enable debug

Cory Fields (3):
      bench: switch to std::chrono for time measurements
      bench: prefer a steady clock if the resolution is no worse
      build: Split hardening/fPIE options out

Dagur Valberg Johannsson (1):
      Improve z_getnewaddress tests

Daira Hopwood (25):
      Add missing cases for Blossom in ContextualCheckBlock tests.
      Revert "Add -maxtimeadjustment with default of 0 instead of the 4200 seconds used in Bitcoin Core."
      Remove uses of GetTimeOffset().
      Replace time adjustment with warning only.
      Update GetAdjustedTime() to GetTime().
      Sort entries in zcash_gtest_SOURCES (other than test_tautology which is deliberately first).
      Add release notes for removal of -maxtimeadjustment.
      Resolve a race condition on `chainActive.Tip()` in initialization (introduced in #4379).
      Setting a std::atomic variable in a signal handler only has defined behaviour if it is lock-free.
      Add comment to `MilliSleep` documenting that it is an interruption point.
      Exit init early if we request shutdown before having loaded the genesis block.
      Fix typos/minor errors in comments, and wrap some lines.
      Avoid a theoretical possibility of division-by-zero introduced in #4368.
      Make the memo a mandatory argument for SendManyRecipient
      Add a `zcutil/clean.sh` script that works (unlike `make clean`).
      Split into clean.sh and distclean.sh.
      Minor refactoring.
      Executables end with .exe on Windows.
      Avoid spurious error messages when cleaning up directories.
      Address review comments.
      Use `SA_RESTART` in `sa_flags` when setting up signal handlers.
      Remove a redundant `rm -f` command.
      Refer to altitude instead of height for history tree peaks
      Address review comments: `target` and `depends/work` should be cleaned by clean.sh.
      Clarify definition of NETWORK_UPGRADE_PEER_PREFERENCE_BLOCK_PERIOD.

Dimitris Apostolou (8):
      Fix Boost compilation on macOS
      Remove libsnark preprocessor flags
      Fix typo
      End diff with LF character
      Remove stale comment
      Point at support community on Discord
      Update documentation info
      Fix typos

Eirik Ogilvie-Wigley (2):
      Include shielded transaction data when calculating RecursiveDynamicUsage of transactions
      Account for malloc overhead

Evan Klitzke (2):
      Add --with-sanitizers option to configure
      Make --enable-debug to pick better options

Gavin Andresen (2):
      Simple benchmarking framework
      Support very-fast-running benchmarks

Gregory Maxwell (4):
      Avoid integer division in the benchmark inner-most loop.
      Move GetWarnings and related globals to util.
      Eliminate data races for strMiscWarning and fLargeWork*Found.
      Move GetWarnings() into its own file.

Jack Grigg (95):
      Revert "Add configure flags for enabling ASan/UBSan and TSan"
      configure: Re-introduce additional sanitizer flags
      RPC: z_viewtransaction
      depends: Add utfcpp to dependencies
      RPC: Display valid UTF-8 memos in z_viewtransaction
      RPC: Use OutgoingViewingKeys to recover non-wallet Sapling outputs
      test: Check z_viewtransaction output in wallet_listreceived RPC test
      Benchmark Zcash verification operations
      Simulate worst-case block verification
      zcutil/build.sh: Remove lcov and mining flags
      configure: Change default Proton to match build.sh
      zcutil/build.sh: Turn off verbosity by default
      Make -fwrapv conditional on --enable-debug=no
      Move default -g flag into configure.ac behind --enable-debug=no
      Add build system changes to release notes
      test: Hard-code hex memo in wallet_listreceived for Python3 compatibility
      test: Fix pyflakes warnings
      bench: "Use" result of crypto_sign_verify_detached
      Add test vectors for small-order Ed25519 pubkeys
      Patch libsodium 1.0.15 pubkey validation onto 1.0.18
      Patch libsodium 1.0.15 signature validation onto 1.0.18
      Add release notes for z_viewtransaction
      Deduplicate some wallet keystore logic
      Move Sprout and Sapling address logic into separate files
      Move ZIP 32 classes inside zcash/Address.hpp
      SaplingFullViewingKey -> SaplingExtendedFullViewingKey in keystore maps
      Remove default address parameter from Sapling keystore methods
      test: Add test for CBasicKeyStore::AddSaplingFullViewingKey
      Add encoding and decoding for Sapling extended full viewing keys
      Add Sapling ExtFVK support to z_exportviewingkey
      Add in-memory Sapling ExtFVK support to z_importviewingkey
      Store imported Sapling ExtFVKs in wallet database
      OutputDescriptionInfo::Build()
      ZIP 213 consensus rules
      Add support for Sapling addresses in -mineraddress
      wallet: Include coinbase txs in Sapling note selection
      Add regtest-only -nurejectoldversions option
      test: Minor tweaks to comments in LibsodiumPubkeyValidation
      test: RPC test for shielded coinbase
      Adjust comments on ZIP 213 logic
      Use DoS level constants and parameters for ZIP 213 rejections
      test: Check that shielded coinbase can be spent to a t-address
      init: Inform on error that -mineraddress must be Sapling or transparent
      test: Explicitly check Sapling consensus rules apply to shielded coinbase
      Migrate GitHub issue template to new format
      Add GitHub issue templates for feature requests and UX reports
      depends: Remove comments from libsodium signature validation patch
      Bring in librustzcash crate
      Bring in Cargo.lock from librustzcash repo
      rust: Pin toolchain to 1.36.0, matching depends system
      rust: Adjust Cargo.toml so that it compiles
      Update .gitignore for Rust code
      Replace librustzcash from depends system with src/rust
      Move root of Rust crate into repo root
      depends: Remove unused vendored crates
      Fix Rust static library linking for Windows builds
      test: Rename FakeCoinsViewDB -> ValidationFakeCoinsViewDB
      test: Modify ValidationFakeCoinsViewDB to optionally contain a coin
      test: Add missing parameter selection to Validation.ReceivedBlockTransactions
      mempool: Check transparent signatures against the previous network upgrade
      mempool: Remove duplicate consensusBranchId from AcceptToMemoryPool
      test: Add Overwinter and Sapling support to GetValidTransaction() helper
      consensus: Check JoinSplit signatures against the previous network upgrade
      depends: Use Rust 1.42.0 toolchain
      Bring in updates to librustzcash crate
      depends: Define Rust target in a single location
      depends: Hard-code Rust target for all Darwin hosts
      Add ZIP 221 logic to block index
      Add ZIP 221 support to miner and getblocktemplate
      Implement ZIP 221 consensus rules
      Return the correct root from librustzcash_mmr_{append, delete}
      Use a C array for HistoryEntry instead of std::array
      test: Verify ZIP 221 logic against reference implementation
      build: Move cargo arguments into RUST_BUILD_OPTS
      build: Correctly remove generated files from .cargo
      test: Build Rust tests as part of qa/zcash/full_test_suite.py
      build: Connect cargo verbosity to make verbosity
      test: Assert that GetValidTransaction supports the given branch ID
      Comment tweaks and cleanups
      test: Add an extra assertion to feature_zip221.py
      Remove unnecessary else case in CCoinsViewCache::PreloadHistoryTree
      Improve documentation of CCoinsViewCache::PreloadHistoryTree
      Truncate HistoryCache.appends correctly for zero-indexed entries
      Comment clarifications and fixes
      Make peak_pos zero-indexed in CCoinsViewCache::PreloadHistoryTree
      test: Ignore timestamps in addressindex checks
      test: Add a second Sapling note to WalletTests.ClearNoteWitnessCache
      test: Run Equihash test vectors on both C++ and Rust validators
      Pass the block height through to CheckEquihashSolution()
      consensus: From Heartwood activation, use Rust Equihash validator
      zcutil/make-release.py: Fix to run with Python 3
      zcutil/make-release.py: Check for release dependencies
      Update release notes for v2.1.2
      zcutil/release-notes.py: Add Python 3 execution header
      Set hashFinalSaplingRoot and hashChainHistoryRoot in AddToBlockIndex

James O'Beirne (1):
      Add basic coverage reporting for RPC tests

Jeremy Rubin (3):
      Add Basic CheckQueue Benchmark
      Address ryanofsky feedback on CCheckQueue benchmarks. Eliminated magic numbers, fixed scoping of vectors (and memory movement component of benchmark).
      Add prevector destructor benchmark

Karl-Johan Alm (1):
      Refactoring: Removed using namespace <xxx> from bench/ and test/ source files.

Larry Ruane (2):
      zcutil/fetch-params.sh unneeded --testnet arg should warn user
      util: CBufferedFile fixes

LitecoinZ (1):
      Fix issue #3772

Marshall Gaucher (1):
      Update qa/rpc-tests/addressindex.py

Matt Corallo (2):
      Remove countMaskInv caching in bench framework
      Require a steady clock for bench with at least micro precision

MeshCollider (3):
      Fix race for mapBlockIndex in AppInitMain
      Make fReindex atomic to avoid race
      Consistent parameter names in txdb.h

NicolasDorier (1):
      [qa] assert_start_raises_init_error

NikVolf (3):
      push/pop history with tests
      update chain history in ConnectBlock and DisconnectBlock
      use iterative platform-independent log2i

Patrick Strateman (1):
      Acquire lock to check for genesis block.

Pavel Janík (3):
      Rewrite help texts for features enabled by default.
      Ignore bench_bitcoin binary.
      Prevent warning: variable 'x' is uninitialized

Philip Kaufmann (1):
      [Trivial] ensure minimal header conventions

Pieter Wuille (3):
      Benchmark rolling bloom filter
      Introduce FastRandomContext::randbool()
      FastRandom benchmark

Sean Bowe (15):
      Initialize ThreadNotifyWallets before additional blocks are imported.
      Handle case of fresh wallets in ThreadNotifyWallets.
      Clarify comment
      Add librustzcash tests to the full test suite.
      Add release profile optimizations and turn off panic unwinding in librustzcash.
      Minor typo fixes.
      Simplification for MacOS in rust-test.
      make-release.py: Versioning changes for 2.1.2-rc1.
      make-release.py: Updated manpages for 2.1.2-rc1.
      make-release.py: Updated release notes and changelog for 2.1.2-rc1.
      Add Rust resources to distribution tarball.
      Add test_random.h to distribution tarball.
      Set Heartwood activation height for testnet to 903800.
      make-release.py: Versioning changes for 2.1.2.
      make-release.py: Updated manpages for 2.1.2.

Taylor Hornby (15):
      Make the equihash validator macro set its output to false when throwing an exception.
      Add test for unused bits in the Equihash solution encoding.
      Add Python script for checking if dependencies have updates.
      Add GitHub API credential
      Update list of dependencies to check
      Wrap long lines
      Cache releases to reduce network usage and improve performance
      Make updatecheck.py compatible with python2
      Have make clean delete temporary AFL build directory
      Add AFL build directory to .gitignore
      Have make clean delete AFL output directories.
      Fix bug in updatecheck.py and add utfcpp to its dependency list
      Fix typo in updatecheck.py
      Update updatecheck.py with the new Rust dependencies and improve the error message in case the untracked dependency list becomes out of date.
      Fix undefined behavior in CScriptNum

Wladimir J. van der Laan (7):
      bench: Add crypto hash benchmarks
      Kill insecure_random and associated global state
      bench: Fix subtle counting issue when rescaling iteration count
      bench: Add support for measuring CPU cycles
      bench: Fix initialization order in registration
      util: Don't set strMiscWarning on every exception
      test_framework: detect failure of bitcoind startup

Yuri Zhykin (1):
      bench: Added base58 encoding/decoding benchmarks

avnish (14):
      changed block_test to BlockTests
      changed test names from _ to CamelCase
      changed header_size_is_expected to HeaderSizeIsExpected
      changed "equihash_tests" to EquihashTests
      changed founders_reward_test to FoundersRewardTest
      changes tests to camelcase
      chnged keystore_tests to KeystoreTests
      changed libzcash_utils to LibzcashUtils
      changed test to CamelCase
      changed test to CamelCase
      changed test to CamelCase
      changed  seven_eq_seven to SevenEqSeven
      changed txid_tests to TxidTests
      changed wallet_zkeys_test to WalletZkeysTest

avnish98 (1):
      requested changes are rectified

ca333 (2):
      update libsodium to v1.0.18
      fix dead openssl download path

gladcow (4):
      Show reindex state in metrics
      Use processed file size as progress in metrics during reindex
      Byte sizes format
      Move reindex progress globals to metrics.h/cpp

Marshall Gaucher (74):
      update /usr/bin/env; fix print conventions
      update test_framework modules
      Update rpc-test/test_framework to Py3 convention,modules,encoding
      Update ignored testScriptsExt to Python3
      Update python3 env path, remove python 2.7 assert
      Update hexlify for encoding, update to py3 io module
      Update py3 env path, remove py2 assert
      Update py2 conventions to py3, remove py2 env and assert
      Update py2 conventions to py3, update Decimal calls
      Update py2 env path, remove py2 assert
      Update py2 env path, remove py2 assert
      Update py2 env path, remove py2 assert, update filter to return list for py3
      Update py2 env path, remove py2 assert, update http module and assert encoding
      Update cmp to py3 functions, update map return to list for py3
      Standard py2 to py3 updates
      Update py2 modules to py3, update encoding to be py3 compatible
      Update to py3 conventions, update decimal calls to be consistent
      Update to py3 conventions, update filter to return list
      update to py3 conventions, update range to return list for py3
      update to py3 convention, update execfile to py3 call
      update to py3 conventions, update cmp to be py3 compatible, update map to return list for py3
      update to py3 conventions, preserve ipv6 patch
      update str cast to prevent address assert issues
      clean up binascii call
      Add keyerror execption
      update to py3 env path
      update to py3 conventions, update functions to be upstream consistent
      update to py3 conventions, clean up code to be upstream consistent
      update to py3 encodings
      update encoding, decoding, serialize funcs for py3
      Update type to be decimal
      update to py3 conventions, BUG with last assert_equal
      Update io modules for py3, ISSUE with create_transaction function
      Update to py3, ISSUE with encoding
      Update to py3, ISSUE with encoding
      Update to py3, ISSUE with encoding in create_block
      Update to py3, ISSUE with encoding in create_block
      Clean up code not needed from upstream
      update io module, fix py3 division, and string encoding
      update remaining encoding issues, add pyblake2
      Use more meaningful assert_equal from our original codebase
      Clean up code from upstream we dont use
      fix except bug for undefined url
      Remove semi colons
      make import urlparse module consistent,httplib update to py3
      correct update to python3
      clean-up imports, keep string notation consistent, remove spacing
      clean up
      Use upstream encoding for encodeDecimal
      fix type issue
      fix initialize statements for imports
      clean up initiliaze statements from imports
      update type for decimal 0
      remove debug lines from prior commits
      clean up to minimize diff
      remove u encoding
      Fix decimal 0 issues
      Clean up import calls
      clean up
      clean up
      clean up
      fix url and port issue
      cleanups and fixing odd casting
      Update json to simplejson to remove unicode and str issue from py2 to py3
      Update py3 division
      fix pyflakes errors
      clean up conventions and whitespace
      fix string pattern issue on byte object
      update comment regarding prior py2 exception
      Fix remaining python3 conventions
      Update remaining Python3 conventions
      Updating remaining python3 conventions
      Update #! env for python3
      Update RPCs to support cross platform paths and libs

murrayn (1):
      Add build support for 'gprof' profiling.

practicalswift (8):
      build: Show enabled sanitizers in configure output
      Add -ftrapv to DEBUG_CXXFLAGS when --enable-debug is used
      Assert that what might look like a possible division by zero is actually unreachable
      Replace boost::function with std::function (C++11)
      Avoid static analyzer warnings regarding uninitialized arguments
      Restore default format state of cout after printing with std::fixed/setprecision
      Initialize recently introduced non-static class member lastCycles to zero in constructor
      Replace boost::function with std::function (C++11)

ptschip (1):
      Enable python tests for Native Windows

zancas (3):
      update comment, to correctly specify number of methods injected
      replace "virtual" with "override" in subclasses
      Remove remaining instances of boost::function

