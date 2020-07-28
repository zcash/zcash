Notable changes
===============

Network Upgrade 4: Canopy
--------------------------

The code preparations for the Canopy network upgrade are finished and included in this release. The following ZIPs are being deployed:

- [ZIP 207: Funding Streams](https://zips.z.cash/zip-0207)
- [ZIP 211: Disabling Addition of New Value to the Sprout Value Pool](https://zips.z.cash/zip-0211)
- [ZIP 212: Allow Recipient to Derive Sapling Ephemeral Secret from Note Plaintext](https://zips.z.cash/zip-0212)
- [ZIP 214: Consensus rules for a Zcash Development Fund](https://zips.z.cash/zip-0214)
- [ZIP 215: Explicitly Defining and Modifying Ed25519 Validation Rules](https://zips.z.cash/zip-0215)

Canopy will activate on testnet at height 1028500, and can also be activated at a specific height in regtest mode by setting the config option `-nuparams=0xe9ff75a6:HEIGHT`. Note that v3.1.0 enables Canopy support on the testnet.

Canopy will activate on mainnet at height 1046400.

See [ZIP 251](https://zips.z.cash/zip-0251) for additional information about the deployment process for Canopy.

Flush witness data to disk only when it's consistent
-----------------------------------------------------
This fix prevents the wallet database from getting into an inconsistent state. By flushing witness data to disk from the wallet thread instead of the main thread, we ensure that the on-disk block height is always the same as the witness data height. Previously, the database occasionally got into a state where the latest block height was one ahead of the witness data. This then triggered an assertion failure in `CWallet::IncrementNoteWitnesses()` upon restarting after a zcashd shutdown.

Note that this code change will not automatically repair a data directory that has been affected by this problem; that requires starting zcashd with the `-rescan` or `-reindex` options.

New DNS seeders
----------------
DNS seeders hosted at "zfnd.org" and "yolo.money" have been added to the list in `chainparams.cpp`. They're running [CoreDNS](https://coredns.io) with a [Zcash crawler plugin](https://github.com/ZcashFoundation/dnsseeder), the result of a Zcash Foundation in-house development effort to replace `zcash-seeder` with something memory-safe and easier to maintain.

These are validly operated seeders per the [existing policy](https://zcash.readthedocs.io/en/latest/rtd_pages/dnsseed_policy.html). For general questions related to either seeder, contact george@zfnd.org or mention @gtank in the Zcash Foundation's Discord. For bug reports, open an issue on the [dnsseeder](https://github.com/ZcashFoundation/dnsseeder) repo.

Changed command-line options
-----------------------------
- `-debuglogfile=<file>` can be used to specify an alternative debug logging file.

RPC methods
------------
- `joinSplitPubKey` and `joinSplitSig` have been added to verbose transaction outputs. This enables the transaction's binary form to be fully reconstructed from the RPC output.
- The output of `getblockchaininfo` now includes an `estimatedheight` parameter. This can be shown in UIs as an indication of the current chain height while `zcashd` is syncing, but should not be relied upon when creating transactions.

Metrics screen
-----------------------
- A progress bar is now visible when in Initial Block Download mode, showing both the prefetched headers and validated blocks. It is only printed for TTY output. Additionally, the "not mining" message is no longer shown on mainnet, as the built-in CPU miner is not effective at the current network difficulty.
- The number of block headers prefetched during Initial Block Download is now displayed alongside the number of validated blocks. With current compile-time defaults, a Zcash node prefetches up to 160 block headers per request without a limit on how far it can prefetch, but only up to 16 full blocks at a time.

Changelog
=========

Alfredo Garcia (28):
      add estimatedheight to getblockchaininfo
      add documentation and command line parsing to afl scripts
      get fuzzing options from directory
      add bool argument to get balance in satoshis to rpc getreceivedbyaddress
      add documentation to flag
      change argument name
      add boolean inZat to getreceivedbyaccount
      add boolean inZat to getbalance
      add boolean inZat to z_getbalance
      add amountZat field to listreceivedbyaddress and listreceivedbyaccount
      add amountZat field to listtransactions, gettransaction and listsinceblock
      add amountZat field to listunspent
      add amountZat field to z_listreceivedbyaddress
      replace with AssertionError assert_equal in receivedby.py
      Fix casting in wallet.py
      simplify inzat balances logic
      Fix casting in listtransactions.py
      add MINOR_CURRENCY_UNIT
      remove additional not needed casts from py tests
      change name of harden option
      fix test cases
      fix sort of options
      remove not needed comments from wallet.py
      update docs
      add new parameters to rpc client and fix some bugs
      initialize size_t
      fix/improve docs
      add log aporximation to metrics solution rates

Anthony Towns (1):
      test: Add tests for `-debuglogfile` with subdirs

Daira Hopwood (24):
      Rename NU4 to Canopy in constant and function names.
      Rename golden/nu4.tar.gz to canopy.tar.gz.
      Missing NU4->Canopy renames.
      Remove unused import in qa/rpc-tests/listtransactions.py
      Remove an unused CCriticalSection.
      Add GetActiveFundingStreams function.
      Tests for changes to getblocksubsidy.
      Change getblocksubsidy RPC to take into account funding streams.
      Use ValueFromAmount instead of double arithmetic, and improve variable names.
      Cosmetic spacing changes.
      Apply suggestions from code review
      Change the format of `getblocksubsidy` output to use an array of funding stream objects.
      Clean up some iterator usage.
      Remove an unnecessary iterator increment.
      Another cleanup.
      Add key_constants.h to src/Makefile.am.
      Fix an unintended consensus change in decryption of coinbase outputs.
      More iterator cleanups.
      src/metrics.cpp: cosmetic whitespace changes.
      Metrics screen: display hash rates using SI prefixes rather than as powers of 2.
      Add unit tests for DisplayDuration, DisplaySize, and DisplayHashRate.
      Fix the formatting of the 3.0.0 release notes.
      Fix --disable-mining build regression. closes #4634
      Allow Equihash validation tests to be compiled with --disable-mining.

Danny Willems (3):
      librustzcash: make the header C compatible
      Use assert.h instead of define manually static_assert
      Use preprocessor for ENTRY_SERIALIZED_LENGTH

Eirik Ogilvie-Wigley (2):
      Resolve decimal vs float issues
      Various improvements

George Tankersley (1):
      Add ZF and gtank's DNS seeders

Jack Grigg (21):
      Use the cached consensusBranchId in DisconnectBlock
      qa: Smoke test driver
      qa: Run Zcash node for smoke tests
      qa: Simple smoke tests
      qa: Transaction chain smoke test
      qa: Use slick-bitcoinrpc for smoke tests
      qa: Don't allow smoke tests with mainnet wallet.dat
      qa: Improve reliability of smoke tests
      qa: Improve reliability of smoke test cleanup
      metrics: Fix indents
      metrics: Draw IBD progress bar showing headers and blocks
      metrics: Don't show "not mining" text for mainnet
      qa: Add --use-faucet flag to smoke tests
      qa: Remove unused timeout configuration from wait_for_balance
      qa: Add --automate flag to smoke tests
      metrics: Switch to ANSI colour codes for progress bar
      metrics: Only print IBD progress bar on TTY
      Implement zip-207 and zip-214.
      Use Rust Equihash validator unconditionally
      Remove C++ Equihash validator
      Revert "Pass the block height through to CheckEquihashSolution()"

Kris Nuttycombe (31):
      Identify `-fundingstream` parameter as being regtest-only
      Use for..: rather than BOOST_FOREACH
      Trivial error message fix.
      Minor fixes for ZIP-207 review comments.
      Trivial copyright fix.
      Replace BOOST_FOREACH with for..:
      Qualified imports of std:: types
      Capitalization fixes from code review
      Minor naming change FundingStreamShare -> FundingStreamElement
      Record-constructor syntax for funding stream initialization.
      Update HalvingHeight documentation.
      Fix pyflakes.
      Fix funding stream end-height-exclusion bugs
      Add `RegtestDeactivateCanopy` calls to restore shared regtest params.
      Move test-only code into test sources.
      Trivial comment correction.
      Minor help message correction.
      Pass by const reference where possible.
      Use uint32_t for vFundingStreams indexing.
      Fix incorrect subtraction of Halving(blossomActivationHeight) from halvingIndex
      Fix ordering of transparent outputs such that miner reward is vout[0]
      Use ed25519-zebra from crates.io.
      Remove assertion that was breaking regtest in the case that blossom activates after the halving.
      Use for..in rather than an indexed loop.
      Make evident the relationship between chainparams and key IO.
      Rename KeyInfo -> KeyConstants and move out of Consensus namespace.
      Fix typo in constant.
      Fix assertion check in GetBlockSubsidy
      Apply suggestions from code review
      Trivial whitespace fix.
      Zero-initialize HistoryNode values.

Larry Ruane (7):
      add python test to reproduce bug 4301
      flush witness cache correctly
      review, cleanup: eliminate uninitialized variable
      self.sync_all(), not time.sleep(4)
      fix pyflakes CI errors
      undo flushing witness data on shutdown
      sync before stopping nodes

Rod Vagg (1):
      Add joinSplitPubKey and joinSplitSig to RPC

Sean Bowe (23):
      Add implementations of PRF_expand calls that obtain esk and rcm.
      Remove bare SaplingNote constructor.
      Add a getter method to obtain rcm from a Sapling note plaintext.
      Add support for receiving v2 Sapling note plaintexts.
      Change transaction builder and miner to use v2 Sapling note plaintexts after Canopy activates.
      Make ed25519-zebra available via librustzcash.
      Change to version of ed25519-zebra crate which is compliant with ZIP 215.
      Enforce ZIP 215 rules upon activation of Canopy.
      Add test that a weird signature successfully validates.
      Remove bincode crate.
      Remove unused curve25519-dalek dev-dependency.
      Minor adjustments to librustzcash and tests.
      Redirect git checkouts of ebfull/ed25519-zebra through our vendored sources in offline mode.
      Require that shielded coinbase output note plaintexts are version 2 if Canopy is active.
      Make transaction builder take the next block height into account for use of v2 note plaintexts.
      Turn return values for libsodium-like API into constants for clarity.
      Add more exhaustive tests for ZIP 215 compatibility.
      Cargo fmt
      Remove unused imports from remove_sprout_shielding RPC test.
      Migrate ZIP 215 test vectors to gtest suite.
      Change LIBSODIUM_ERROR to -1.
      Hash "Zcash" to align tests with ZIP 215 test vectors.
      Remove outdated comment.

Solar Designer (2):
      Fix typos in ProcessMessage() "headers"
      During initial blocks download, also report the number of headers

Taylor Hornby (3):
      Fix undefined behavior in gtest tests
      Add missing <stdexcept> header for std::invalid_argument
      Fix bug in CScheduler

Wladimir J. van der Laan (3):
      Add `-debuglogfile` option
      test: Add test for `-debuglogfile`
      doc: Update release notes for `-debuglogfile`

ewillbefull@gmail.com (1):
      Add dev fund addresses for testnet NU4 activation.

Marshall Gaucher (1):
      Add helpers for tapping and donating to the testnet faucet

therealyingtong (35):
      Add RPC tests for post-Heartwood rollback
      Reject v1 plaintexts after grace period
      Check epk vs esk whenever caller has esk
      Refactor SaplingNotePlaintext::decrypt
      Add gtests for v2 plaintexts
      Add contextual check to main.cpp
      Add gtests
      Add checks to z_ methods in rpcwallet
      Add RPC tests
      Replace leadByte in SaplingNote with is_zip_212
      Throw error in plaintext deserialization
      Pass pindex to AddToWalletIfInvolvingMe()
      Remove plaintext check from AddSaplingSpend
      Remove plaintext check from GetFilteredNotes
      Refactor bool is_zip_212 to enum Zip212Enabled
      Minor changes
      Remove old SaplingNote() constructor
      Pass nHeight instead of pindex to AddToWalletIfInvolvingMe()
      Directly call RegtestActivate* in gtests
      Update release notes for v3.1.0
      make-release.py: Versioning changes for 3.1.0-rc1.
      make-release.py: Updated manpages for 3.1.0-rc1.
      make-release.py: Updated release notes and changelog for 3.1.0-rc1.
      Undo manual DEPRECATION_HEIGHT
      make-release.py: Versioning changes for 3.1.0-rc2.
      make-release.py: Updated manpages for 3.1.0-rc2.
      make-release.py: Updated release notes and changelog for 3.1.0-rc2.
      Edit release notes to specify that rc2 does not enable Canopy support on the testnet
      Set Canopy testnet activation height to 1020500
      Set PROTOCOL_VERSION to 170012
      Pass HistoryNode struct to librustzcash FFI
      Delay testnet activation height by one week
      Use 51 Testnet Dev Fund addresses, and adjust the end heights.
      make-release.py: Versioning changes for 3.1.0.
      make-release.py: Updated manpages for 3.1.0.

ying tong (1):
      Apply suggestions from code review

