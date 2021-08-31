Notable changes
===============

Switch to ed25519-zebra for consensus signature checks
------------------------------------------------------
This removes the zcashd dependency upon libsodium for ed25519
signature checks and instead uses the Rust implementation in
ed25519-zebra, which has been active for signature verification
since the Canopy upgrade. For more information on the conditions
that led to this change see https://hdevalence.ca/blog/2020-10-04-its-25519am

Update default fees according to ZIP-313
----------------------------------------
Reduce default fees to 0.00001 ZEC as specified in ZIP-313 and 
ensure that transactions paying at least the new minimum fee meet
the transaction relay threshold irrespective of transaction size.

Improve getblocktemplate rpc performance when using shielded coinbase
---------------------------------------------------------------------
This change precomputes future block templates to permit miners to
begin working atop newly arrived blocks as quickly as possible, rather
than waiting for a new template to be generated after a block has arrived.
It also reduces the initial the wait time for incorporating new mempool 
transactions into a block from 1 minute to 10 seconds; the previous value
was inherited from the upstream bitcoin codebase but is inappropriate for
our block timing.

Migrate from rpc-tests.sh to rpc-tests.py
-----------------------------------------
This unifies and simplifies the RPC testing framework, as has
been done in the upstream Bitcoin codebase.

Changelog
=========

Alex Morcos (2):
      Reorder RPC tests for running time
      remove obsolete run-bitcoind-for-test.sh

Alfredo Garcia (1):
      add address to z_importviewingkey error

Casey Rodarmor (1):
      Add p2p-fullblocktest.py

Chun Kuan Lee (2):
      gui: get special folder in unicode
      refactor: Drop boost::this_thread::interruption_point and boost::thread_interrupted in main thread

Cory Fields (1):
      build: a few ugly hacks to get the rpc tests working out-of-tree

Daira Hopwood (7):
      Windows cross-build generates .lib files, which should be ignored by git and removed by clean.
      Partial revert of "Update links". See #4904
      Fix a typo introduced in #4904.
      Reduce the default fee for z_* operations, and the "low fee penalty" threshold for mempool limiting, to 1000 zatoshis.
      Always allow transactions paying at least DEFAULT_FEE to be relayed, and do not rate-limit them. In other words, make sure that the "minimum relay fee" is no greater than DEFAULT_FEE.
      gtest/test_mempoollimit: the test failed to properly ensure that the "low fee penalty" threshold matches the new ZIP 313 fee.
      Revert changes in #4916 that assumed arguments represent fees, when they are actually number of confirmations.

Daniel Kraft (1):
      Fix crash when mining with empty keypool.

Dimitris Apostolou (5):
      Fix typo
      readelf is Linux only
      Fix readelf configuration
      Update links
      Remove workaround affecting old Boost version

Elliot Olds (1):
      Check if zmq is installed in tests, update docs

Eric Lombrozo (1):
      Added fPowNoRetargeting field to Consensus::Params that disables nBits recalculation.

Jack Grigg (32):
      test: Use default shielded address in RPC tests where the type is irrelevant
      Revert "remove SignatureHash from python rpc tests"
      test: Remove FindAndDelete from RPC test framework
      test: Fix SignatureHash RPC test helper
      test: Set hashFinalSaplingRoot default correctly in create_block
      test: Re-enable regtest difficulty adjustment for unit tests that use it
      test: Adjust some Zcash RPC tests to work with parallel runner
      test: Run rpc-tests.py in full_test_suite.py
      lint: Remove boost/foreach.hpp from allowed includes
      test: Silence pyflakes unused import warning
      test: Run shielding-heavy RPC tests in serial
      Fix Hungarian notation nit from Daira
      Switch to ed25519-zebra for consensus signature checks
      test: Use ed25519_verify in ConsensusTests
      Update minimum chain work and set activation block hashes for mainnet
      Update minimum chain work and set activation block hashes for testnet
      net: Rework CNode spans
      rpc: Reload CNode spans after reloading the log filter
      rpc: Log the new filter when we set it
      mempool: Log all accepted txids at INFO level
      Add <variant> header to files that will need it
      scripted-diff: Migrate from boost::variant to std::variant
      Finish migrating to std::variant
      Migrate from boost::optional::get to boost::optional::value
      Add <optional> header to files that will need it
      scripted-diff: Migrate from boost::optional to std::optional
      Finish migrating to std::optional
      lint: Remove boost::variant and boost::optional from allowed includes
      test: Fix test_bitcoin compilation on macOS High Sierra
      cargo update
      tracing: Remove unnecessary tracing_init_inner internal function
      tracing: Simplify init logic using optional layers

James O'Beirne (2):
      Add tests for gettxoutsetinfo, CLevelDBBatch, CLevelDBIterator
      Add basic coverage reporting for RPC tests

Jeff Garzik (1):
      qa/pull-tester/rpc-tests.py: chmod 0755

Jesse Cohen (1):
      [doc][trivial] no retargeting in regtest mode

John Newbery (6):
      Use configparser in rpc-tests.py
      Use argparse in rpc_tests.py
      Improve rpc-tests.py arguments
      Refactor rpc-tests.py
      Various review markups for rpc-tests.py improvements
      Add exclude option to rpc-tests.py

Jorge Timón (8):
      Small preparations for Q_FOREACH, PAIRTYPE and #include <boost/foreach.hpp> removal
      scripted-diff: Fully remove BOOST_FOREACH
      scripted-diff: Remove PAIRTYPE
      Introduce src/reverse_iterator.hpp and include it...
      Fix const_reverse_iterator constructor (pass const ptr)
      scripted-diff: Remove BOOST_REVERSE_FOREACH
      scripted-diff: Remove #include <boost/foreach.hpp>
      clang-format: Delete ForEachMacros

Josh Ellithorpe (1):
      Include transaction hex in verbose getblock output

Kris Nuttycombe (10):
      Write down the folklore about nSequence
      Prefer explicit passing of CChainParams to the Params() global.
      Remove vestigial OSX_SDK_VERSION from darwin build.
      Update boost to 1.75, postpone other updates.
      Don't log to stdout if a file logger is configured.
      make-release.py: Versioning changes for 4.2.0-rc1.
      make-release.py: Updated manpages for 4.2.0-rc1.
      make-release.py: Updated release notes and changelog for 4.2.0-rc1.
      make-release.py: Versioning changes for 4.2.0.
      make-release.py: Updated manpages for 4.2.0.

Larry Ruane (2):
      add more version information to getinfo rpc
      improve getblocktemplate performance for shielded coinbase

Luv Khemani (1):
      Add autocomplete to bitcoin-qt's console window.

Marco Falke (37):
      [doc] trivial: fix markdown syntax in qa/rpc-tests/README.md
      [rpc-tests] fundrawtransaction: Update fee after minRelayTxFee increase
      [rpc-tests] Check return code
      [qa] Split README.md to /qa and /qa/rpc-tests
      [qa] Extend README.md
      [qa] keypool: Fix white space to prepare transition to test framework
      [qa] keypool: DRY: Use test framework
      [qa] pull-tester: Cleanup (run keypool, tidy stdout)
      [qa] Use python2/3 syntax
      [qa] rpc-tests: Properly use integers, floats
      [qa] mininode: Catch exceptions in got_data
      [qa] pull-tester: Don't mute zmq ImportError
      [qa] pull-tester: Exit early when no tests are run
      [qa] rpc-tests: Fix link in comment and label error msg
      [qa] Switch to py3
      [qa] Refactor test_framework and pull tester
      [qa] Update README.md
      [qa] Stop other nodes, even when one fails to stop
      [qa] pull-tester: Adjust comment
      [qa] pull-tester: Run rpc test in parallel
      [qa] Add option --portseed to test_framework
      [qa] Remove hardcoded "4 nodes" from test_framework
      [qa] test_framework: Append portseed to tmpdir
      [qa] test_framework: Use different rpc_auth_pair for each node
      [qa] pull-tester: Fix assertion and check for run_parallel
      [qa] pull-tester: Start longest test first
      [qa] Adjust timeouts for micro-optimization of run time
      [qa] Use single cache dir for chains
      [qa] Remove unused code
      [qa] pull-tester: Don't mute zmq ImportError
      [qa] create_cache: Delete temp dir when done
      [qa] Refactor RPCTestHandler to prevent TimeoutExpired
      [qa] pull-tester: Only print output when failed
      [qa] test_framework: Exit when tmpdir exists
      [qa] rpc-tests: Apply random offset to portseed
      qa: Set correct path for binaries in rpc tests
      util: Replace boost::signals2 with std::function

Nate Wilcox (1):
      configure.ac: Introduce macros to simplify requiring tools.

Suhas Daftuar (2):
      Remove unmaintained example test script_test.py
      Tests: add timeout to sync_blocks() and sync_mempools()

Wladimir J. van der Laan (2):
      build: don't distribute tests_config.py
      test: don't override BITCOIND and BITCOINCLI if they're set

fanquake (4):
      [doc] Add OS X ZMQ requirement to QA readme
      [trivial] Add tests_config.ini to .gitignore
      [qa][doc] Correct rpc test options in readme
      build: set minimum supported macOS to 10.14

furszy (15):
      wallet:AvailableCoins fOnlySpendable filtering flag implemented + connected to sendmany async operation.
      wallet:AvailableCoins nMinDepth filter implemented + connected to sendmany async operation.
      asyncrpcoperation_sendmany::find_utxos removing a redundant coinbase check, coinbases are already being filtered by the AvailableCoins flag.
      wallet:AvailableCoins filter by destination/s feature implemented + connected to sendmany async operation.
      sendmany::find_utxo removing an unneeded recursive lock, AvailableCoins is already locking cs_main and cs_wallet.
      wallet:COutput adding fIsCoinbase member.
      COutput: implemented Value() method.
      asyncRPCOperation_sendmany:find_utxos, removing a redundant loop over all of the available utxo in the wallet.
      sendmany: removing now unused SendManyInputUTXO class.
      move-only: asyncOp_sendmany, target amount calculation moved before find inputs (utxos and notes).
      sendmany operation: Creating TxValues struct to store the transaction values in a more organized manner.
      Improving asyncoperation_sendmany, removing another redundant for loop over all of the available utxos.
      rpc_wallet_tests: changed "Insufficient funds" error message to a proper "Insufficient transparent funds". This is because we are now throwing the insufficient transparent balance rpc error inside load_utxo.
      asyncOp sendmany: moved inputs total amount check inside load_utxo before the dust validation.
      sendmany::find_unspent_notes removing an unneeded recursive lock, GetFilteredNotes is already locking cs_main and cs_wallet.

isle2983 (1):
      [doc] - clarify statement about parallel jobs in rpc-tests.py

Marshall Gaucher (1):
      Update expected fails for  Sprout txns flows on Canopy

ptschip (1):
      Migrated rpc-tests.sh to all python rpc-tests.py

whythat (2):
      [qa]: add parsing for '<host>:<port>' argument form to rpc_url()
      [qa]: enable rpcbind_test

