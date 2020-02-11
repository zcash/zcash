Notable changes
===============

`z_mergetoaddress` promoted out of experimental status
------------------------------------------------------

The `z_mergetoaddress` API merges funds from t-addresses, z-addresses, or both,
and sends them to a single t-address or z-address. It was added in v1.0.15 as an
experimental feature, to simplify the process of combining many small UTXOs and
notes into a few larger ones.

In this release we are promoting `z_mergetoaddress` out of experimental status.
It is now a stable RPC, and any future changes to it will only be additive. See
`zcash-cli help z_mergetoaddress` for API details and usage.

Unlike most other RPC methods, `z_mergetoaddress` operates over a particular
quantity of UTXOs and notes, instead of a particular amount of ZEC. By default,
it will merge 50 UTXOs and 10 notes at a time; these limits can be adjusted with
the parameters `transparent_limit` and `shielded_limit`.

`z_mergetoaddress` also returns the number of UTXOs and notes remaining in the
given addresses, which can be used to automate the merging process (for example,
merging until the number of UTXOs falls below some value).

Option parsing behavior
-----------------------

Command line options are now parsed strictly in the order in which they are
specified. It used to be the case that `-X -noX` ends up, unintuitively, with X
set, as `-X` had precedence over `-noX`. This is no longer the case. Like for
other software, the last specified value for an option will hold.

Low-level RPC changes
---------------------

- Bare multisig outputs to our keys are no longer automatically treated as
  incoming payments. As this feature was only available for multisig outputs for
  which you had all private keys in your wallet, there was generally no use for
  them compared to single-key schemes. Furthermore, no address format for such
  outputs is defined, and wallet software can't easily send to it. These outputs
  will no longer show up in `listtransactions`, `listunspent`, or contribute to
  your balance, unless they are explicitly watched (using `importaddress` or
  `importmulti` with hex script argument). `signrawtransaction*` also still
  works for them.

Changelog
=========

Alfredo Garcia (19):
      remove duplicated prefix in errors and warnings
      Consensus: Decouple pow.cpp from util.h
      change some wallet functions from bool to void
      limit blockchain progress to a max of 1.000000
      remove z_mergetoaddress from experimental
      add version to thank you string
      add next upgrade info to metrics console
      change target spacing to up to upgrade height
      create and use SecondsLeftToHeight to display next upgrade info
      add NextUpgrade test case
      fix spacing
      add after blossom test
      change var and function names for clarity, refactor function
      reverse conditional, replace get_value_or(0) calls
      remove redundant line from test
      remove zmergetoaddress from experimental state in rpc-tests
      get UPGRADE_TESTDUMMY back to default at the end of the test
      remove dead code in init
      readd create_directories

Ben Woosley (1):
      Assert CPubKey::ValidLength to the pubkey's header-relevent size

Carl Dong (1):
      depends: tar: Always extract as yourself

Casey Rodarmor (1):
      Give a better error message if system clock is bad

Charlie O'Keefe (1):
      Add check-depends step to STAGE_COMMANDS list

Cory Fields (3):
      depends: qt/cctools: fix checksum checksum tests
      depends: bump OSX toolchain
      depends: make osx output deterministic

Dagur Valberg Johannsson (1):
      Remove option mempooltxinputlimit

Denis Lukianov (1):
      Correct importaddress help reference to importpubkey

Dimitris Apostolou (4):
      Fix typo
      Remove stale comment
      Change "protect" terminology to "shield"
      depends macOS: hide linker visibility warnings

Eirik Ogilvie-Wigley (2):
      Show time elapsed when running RPC tests
      Update team email

Gavin Andresen (1):
      Unit test doublespends in new blocks

Gregory Maxwell (1):
      Make connect=0 disable automatic outbound connections.

Gregory Sanders (1):
      Added additional config option for multiple RPC users.

Ian T (1):
      Update RPC generate help for numblocks to include required

Jack Grigg (32):
      Upgrade librustzcash to 0.2.0
      Migrate to librustzcash 0.2.0 API
      Remove invalid address comparison from gtest
      Cast uint8* in InterruptibleRecv to char* for recv
      Add Heartwood to upgrade list
      Initialize experimental mode in a separate function
      Fix benchmarks after removal of SelectParamsFromCommandLine()
      Handle Equihash and optional miner code in TestChain100Setup
      Add tests covering the current interaction of alerts with subver comments
      Parameterize zcash.conf in init error message
      cleanup: Comments
      Wrap long line
      Match alerts both with and without comments
      pyflakes fixes
      Revert "Remove insecurely-downloaded dependencies that we don't currently use."
      depends: Compile bdb with --disable-atomics when cross-compiling darwin
      depends: Add Rust targets for cross-compiling darwin
      configure: Don't require RELRO and BIND_NOW when cross-compiling darwin
      depends: Manually apply build_env to second half of googletest build
      Revert "depends: Explicitly set Boost toolchain during configuration"
      Add z_mergetoaddress to release notes
      ThreadNotifyRecentlyAdded -> ThreadNotifyWallets
      Move mempool tx notifying logic out of CTxMemPool
      Merge tree and boolean fields in ChainTip API
      Move block-notifying logic into ThreadNotifyWallets
      Tie sync_blocks in RPC tests to notifier thread
      Extend comment with reason for taking care with locks
      test: Add sync_all points after block generation to RPC tests
      test: Remove genesis-block Sapling activation from shorter_block_times
      test: Reverse hashtx and hashblock ordering at start of ZMQ RPC test
      test: Add missing sync_all point
      test: Update wallet RPC test with change to "absurdly high fee" limit

Jainan-Tandel (1):
      Cosmetic update to README.md .

Jim Posen (3):
      Comments: More comments on functions/globals in standard.h.
      [script] Unit tests for script/standard functions
      [script] Unit tests for IsMine

Jonas Schnelli (9):
      [autoprune] allow wallet in pruned mode
      [RPC] disable import functions in pruned mode
      [squashme] improve/corrects prune mode detection test for required wallet rescans
      Refactor parameter interaction, call it before AppInit2()
      Initialize logging before we do parameter interaction
      [Wallet] move wallet help string creation to CWallet
      [Wallet] move "load wallet phase" to CWallet
      [Wallet] optimize return value of InitLoadWallet()
      [Wallet] refactor wallet/init interaction

Jorge Timón (4):
      Chainparams: Replace CBaseChainParams::Network enum with string constants (suggested by Wladimir)
      Chainparams: Translations: DRY: options and error strings
      Globals: Decouple GetConfigFile and ReadConfigFile from global mapArgs
      Policy: MOVEONLY: Create policy/policy.h with some constants

Larry Ruane (5):
      simplify locking, merge cs_SpendingKeyStore into cs_KeyStore
      eliminate races: hold cs_KeyStore lock while reading fUseCrypto
      revert CCryptoKeyStore::SetCrypted() return value
      insightexplorer: LOCK(cs_main) during rpcs
      fix tests for enable-debug build

Luke Dashjr (8):
      Bugfix: RPC: blockchain: Display correct defaults in help for verifychain method
      Bugfix: Describe dblogsize option correctly (it refers to the wallet database, not memory pool)
      Bugfix: If genproclimit is omitted to RPC setgenerate, don't change it; also show correct default in getmininginfo
      Bugfix: Omit wallet-related options from -help when wallet is disabled
      Constrain constant values to a single location in code
      Bugfix: Omit wallet-related options from -help when wallet is not supported
      Policy: MOVEONLY: 3 functions to policy.o:
      Common argument defaults for NODE_BLOOM stuff and -wallet

Marco Falke (20):
      [trivial] Reuse translation and cleanup DEFAULT_* values
      [qt] Move GUI related HelpMessage() part downstream
      [trivial] init cleanup
      [wallet] Refactor to use new MIN_CHANGE
      [wallet] Add comments for doxygen
      Init: Use DEFAULT_TRANSACTION_MINFEE in help message
      [qt] Properly display required fee instead of minTxFee
      Clarify what minrelaytxfee does
      translations: Don't translate markdown or force English grammar
      transaction_tests: Be more strict checking dust
      [trivial] init: Use defaults MIN_RELAY_TX_FEE & TRANSACTION_MAXFEE
      contrib: Del. gitian downloader config and update gitian README
      rpcwallet: Clarify what settxfee does
      HelpMessage: Don't hide -mintxfee behind showDebug
      mempool: Replace maxFeeRate of 10000*minRelayTxFee with maxTxFee
      [doxygen] Actually display comment
      Fix doxygen comment for payTxFee
      [doc] Fix markdown
      Make sure LogPrintf strings are line-terminated
      [wallet] Move hardcoded file name out of log messages

Marshall Gaucher (3):
      Update `import *` to unblock pyflakes from failing
      Update z_sendmany calls passing int 0, instead of Decimal('0')
      Update to stop random race error from assert

Matt Corallo (10):
      Also remove pay-2-pubkey from watch when adding a priv key
      Split up importaddress into helper functions
      Add p2sh option to importaddress to import redeemScripts
      Add importpubkey method to import a watch-only pubkey
      Update importaddress help to push its use to script-only
      Add have-pubkey distinction to ISMINE flags
      Add logic to track pubkeys as watch-only, not just scripts
      Implement watchonly support in fundrawtransaction
      SQUASH "Add have-pubkey distinction to ISMINE flags"
      SQUASH "Implement watchonly support in fundrawtransaction"

Peter Todd (3):
      Make TX_SCRIPTHASH clear vSolutionsRet first
      Add IsPushOnly(const_iterator pc)
      Accept any sequence of PUSHDATAs in OP_RETURN outputs

Pieter Wuille (9):
      Remove template matching and pseudo opcodes
      Stop treating importaddress'ed scripts as change
      Make CScript -> CScriptID conversion explicit
      Do not expose SigVersion argument to IsMine
      Switch to a private version of SigVersion inside IsMine
      Track difference between scriptPubKey and P2SH execution in IsMine
      Do not treat bare multisig as IsMine
      Mention removal of bare multisig IsMine in release notes
      Use anonymous namespace instead of static functions

Sean Bowe (2):
      make-release.py: Versioning changes for 2.1.1-rc1.
      make-release.py: Updated manpages for 2.1.1-rc1.

Simon Liu (1):
      Closes #3911. Fix help message of RPC getwalletinfo.

Taylor Hornby (10):
      Add AFL instrumentation scripts to zcutil.
      Add configure option to replace main with a stub for fuzzing
      Add all-in-one script for starting AFL fuzzing
      Separate AFL build, run fuzz stages, and add afl argument pass-through
      Have make clean delete fuzz.cpp
      Pass AFL input file path to zcashd
      Add fuzzing stub for AddrMan deserialization
      Add fuzzing stub for ReadFeeEstimates
      Add fuzzing stub for CheckBlock
      Update proton from 0.26.0 to 0.30.0

Ulrich Kempken (1):
      depends: switch to secure download of all dependencies

Wladimir J. van der Laan (7):
      make proxy_test work on servers without ipv6
      Fix argument parsing oddity with -noX
      doc: mention change to option parsing behavior in release notes
      test: move accounting_tests and rpc_wallet_tests to wallet/test
      test: Create test fixture for wallet
      wallet_ismine.h → script/ismine.h
      test: Rename wallet.dat to wallet_test.dat

sandakersmann (3):
      Update of copyright year to 2020
      Update COPYRIGHT_YEAR in clientversion.h to 2020
      Update _COPYRIGHT_YEAR in configure.ac to 2020

Jack Grigg (2):
      Apply suggestions from code review
      Apply suggestions from code review

Benjamin Winston (1):
      Added basic fuzzing to the monolith, see ticket #4155

