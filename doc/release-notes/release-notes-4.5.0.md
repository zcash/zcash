Changelog
=========

Aditya Kulkarni (1):
      Sort taddr txns by txindex in addition to height

Alex Morcos (4):
      Store the total sig op count of a tx.
      Add a score index to the mempool.
      Add TxPriority class and comparator
      tidy up CInv::GetCommand

Daira Hopwood (8):
      Improve error message when a block would overfill the Orchard commitment tree.
      More precise terminology: "lock free" -> "unlocked"
      ZIP 339 support.
      Update URL for Boost source download (from dl.bintray.com to boostorg.jfrog.io).
      Cargo.toml: use librustzcash after the merge of https://github.com/zcash/librustzcash/pull/424 .
      Update unified address test data to take account of HRPs in padding (https://github.com/zcash/librustzcash/pull/419).
      Avoid need to cast away const in the C caller of zip339_free_phrase.
      Update authors of librustzcash to everyone currently and formerly on Core team (in alphabetical order).

Eric Lombrozo (2):
      Removed ppszTypeName from protocol.cpp
      getdata enum issue fix

Ethan Heilman (1):
      Fix typo adddrman to addrman as requested in #8070

Ethan Heilman (1):
      Remove non-determinism which is breaking net_tests #8069

Gregory Maxwell (4):
      Eliminate TX trickle bypass, sort TX invs for privacy and priority.
      Move bloom and feerate filtering to just prior to tx sending.
      Do not use mempool for GETDATA for tx accepted after the last mempool req.
      Defer inserting into maprelay until just before relaying.

Jack Grigg (124):
      Re-include reading blocks from disk in block connection benchmark
      cargo update
      Migrate to latest zcash_* crates
      metrics 0.16 and metrics-exporter-prometheus 0.5
      Implement ZIP 216 consensus rules
      Extract SpendDescriptionV5 and OutputDescriptionV5 classes
      rust: Enable C++ streams to be passed into Rust code
      ZIP 225 tx format constants
      v5 transaction format parser
      contrib: Add BOSL to contrib/debian/copyright
      Remove early return logic from transaction parsing
      rust: Document read_callback_t and write_callback_t
      CTransaction: Make new ZIP 225 fields non-const and private
      ZIP 244 transaction digests
      ZIP 244 signature digests
      ZIP 244 hashAuthDataRoot computation
      Fix tests that assume CTxOuts can be "null"
      test: Generate valid Sapling types
      test: Small fixes to sighash test vector generation
      test: Regenerate sighash.json after generator fixes
      Throw an exception instead of asserting if Rust tx parser fails
      CI: Publish correct book directory
      CI: Build book with latest mdbook
      rust: Documentation improvements to FFI methods
      Implement Orchard authorization batch validator
      Implement Orchard signature validation consensus rules
      rust: Fix patched dependencies
      book: Add dev guide page about Rust dependencies
      Rename hashLightClientRoot to hashBlockCommitments in block header
      ZIP 244 hashBlockCommitments implementation
      test: Check for valid hashBlockCommitments construction post-NU5
      Skip hashBlockCommitments check when testing block templates
      test: Check hashBlockCommitments before, at, and after NU5 activation
      ConnectBlock: Check NU activation when deriving block commitments
      Copy authDigest in CTransaction::operator=(const CTransaction &tx)
      rust: Move history tree FFI logic into a module
      rust: Migrate to zcash_history with versioned trees
      rust: Move history tree FFI declarations into a separate header
      test: Use valid consensus branch IDs in history tree tests
      Use V2 history trees from NU5 onward
      test: Check history trees across Canopy and NU5 activations
      rpc: Document getblock RPC finalorchardroot field, omit before NU5
      rust: Document some requirements for history tree FFI methods
      test: Add test case for popping from an empty history tree
      Implement Orchard pool value tracking
      rust: Load Orchard circuit parameters at startup
      Check Orchard bundle-specific consensus rules, i.e. proofs
      test: Update CCoinsViewTest with changes to CCoinsView interface
      Include Orchard bundle in transaction dynamic usage
      ZIP 203: Enforce coinbase nExpiryHeight consensus rule from NU5
      test: Check for updated empty-tx reject messages in transaction tests
      test: Fix OverwinterExpiryHeight test after ZIP 203 contextual changes
      miner: Set coinbase expiry height to block height from NU5 activation
      Introduce libzcash::RawAddress type
      Use `libzcash::RawAddress` in `CWallet::GetFilteredNotes`
      Use a visitor for handling -mineraddress config option
      Add support for decoding and encoding Unified Addresses
      Pass network type through to UA address handling logic
      CI: Add workflow that runs general lints
      CI: Check scripted diffs
      CI: Add Rust lints
      Document why a nested call to ExtractMinerAddress is not recursive
      Add constants for UA typecodes
      Postpone dependency updates we aren't doing in this release
      depends: Update Rust to 1.54.0
      depends: Update Clang / libcxx to LLVM 12
      depends: Update utfcpp to 3.2.1
      depends: Fix issue cross-compiling BDB to Windows with Clang 12
      rust: cargo update
      rust: metrics 0.17
      CI: Use Rust 1.54 for lints
      cargo fmt
      test: Wait for transaction propagation in shorter_block_times RPC test
      test: Fix race condition in p2p_txexpiringsoon
      Revert "Remove reference to -reindex-chainstate"
      test: Flush wallet in WriteCryptedSaplingZkeyDirectToDb before reopening
      qa: Bump `sync_mempool` timeout for `prioritisetransaction.py`
      ProcessGetData(): Rework IsExpiringSoon check
      test: Print reject reason if RPC test block rejected instead of accepted
      test: Fix pyflakes warnings
      CI: Ignore errors from general lints we don't yet have passing
      lint: remove duplicate include
      lint: Add missing include guards
      test: Add NU5 test cases to existing RPC tests
      builder: Generate v5 transactions from NU5 activation
      Print `nConsensusBranchId` in `CTransaction::ToString`
      Separate the consensus and internal consistency checks for branch ID
      Parse consensus branch ID when reading v5 transaction format
      test: Use correct field of getnetworkinfo to read protocol version
      CI: Add Dependabot config to keep Actions up-to-date
      Introduce a WTxId struct
      Implement CInv message type MSG_WTX
      test: Fix bugs in mininode transaction parser
      test: Add v5 tx support to mininode
      ProcessGetData: Respond to MSG_WTX requests
      Add MSG_WTX support to inv messages
      Use wtxid for peer state management
      test: Implement CInv.__eq__() for mininode to simplify RPC test
      Postpone dependency updates that require CMake
      depends: Update Rust to 1.54.0
      test: Fix bug in mininode.SpendDescription.deserialize
      Add named constants for legacy tx authDigest placeholder value
      qa: Boost 1.77.0
      cargo update
      Migrate to latest revisions of Zcash Rust crates
      test: Set up mininodes at the start of feature_zip239
      net: Reject unknown CInv message types
      make-release.py: Versioning changes for 4.5.0-rc1.
      make-release.py: Updated manpages for 4.5.0-rc1.
      make-release.py: Updated release notes and changelog for 4.5.0-rc1.
      Migrate to latest revisions of orchard and the zcash_* crates
      contrib: Add script for generating a graph of our Rust dependencies
      cargo update
      build: Add primitives/orchard.h to list of header files
      build: Ensure that cargo uses vendored dependencies for git repos
      build: Add missing source file to zcash_gtest_SOURCES
      rust: Move Orchard batch logic into BatchValidator methods
      wallet: Batch-validate all Orchard signatures in the wallet on load
      rust: Skip running the Orchard batch validator on an empty batch
      bench: Add Orchard logic to zcbenchmarks
      cargo update
      Update halo2 and orchard dependencies with BOSL Zcash exception
      make-release.py: Versioning changes for 4.5.0.
      make-release.py: Updated manpages for 4.5.0.

John Newbery (10):
      [tests] Remove wallet accounts test
      [wallet] GetBalance can take an isminefilter filter.
      [wallet] Factor out GetWatchOnlyBalance()
      [wallet] deduplicate GetAvailableCredit logic
      [wallet] factor out GetAvailableWatchOnlyBalance()
      [wallet] GetBalance can take a min_depth argument.
      [RPC] [wallet] allow getbalance to use min_conf and watch_only without accounts.
      [wallet] Remove wallet account RPCs
      [wallet] Kill accounts
      [net] split PushInventory()

Jonas Schnelli (1):
      fix locking issue with new mempool limiting

Kris Nuttycombe (46):
      Update transaction auth commitments for pre-v5 transactions.
      Move OrchardBundle to its own header file.
      Implement the Rust side of the incremental merkle tree FFI.
      Orchard changes to coins & consensus.
      Return std::optional for GetAnchor
      Check nullifiers length against bundle actions length.
      Add Orchard bundle commitments to merkle tree.
      Add Orchard merkle tree anchor tests.
      Documentation cleanup.
      Update orchard dependency.
      Update to released version of incrementalmerkletree
      Apply suggestions from code review
      Apply suggestions from code review
      Fix Orchard incremental Merkle tree empty root.
      Fix header guards for incremental_sinsemilla_tree.h
      Apply style suggestions.
      Consistently panic on null commitment tree pointers.
      Fix implmentation of OrchardMerkleTree.DynamicMemoryUsage
      Document source of Orchard merkle tree test data.
      Apply suggestions from code review
      Add consensus check for duplicate Orchard nullifiers within a single transaction.
      Add Orchard nullifiers to nullifiers cache.
      Apply suggestions from code review
      Ensure Sapling versions are valid after NU5
      Make CTransaction::nConsensusBranchId a std::optional
      Add NU5 upper bound check on nSpendsSapling, nOutputsSapling, nActionsOrchard
      Check consensus branch ID for V5 transactions.
      Rename tx.valueBalance -> tx.valueBalanceSapling
      Make valueBalanceSapling a private non-const member of CTransaction.
      Add Orchard value balance checks.
      Account for Orchard balance in GetValueOut and GetShieldedValueIn.
      Retract partial Orchard test support.
      Add check that v5 transactions have empty Sprout joinsplits.
      Prevent undefined behaviour in `CTransaction::GetValueOut()`
      ZIP 213: Add checks to support Orchard shielded coinbase outputs.
      Add check for consistency between nActionsOrchard and Orchard flags.
      Ensure that the Orchard note commitment tree does not exceed its maximum size.
      Update Orchard commitment tree hashes to use total MerkleCRH^Orchard.
      Apply suggestions from code review
      Make Sapling Spend and Ouput count, and Orchard Action count checks be noncontextual.
      Use DOS level 100 for noncontextual checks.
      Fix error strings to correctly reflect context.
      Remove unused account-related wallet methods.
      Use manual serialization for Merkle frontiers rather than bincode.
      Fix clippy complaints.
      Lock the wallet in SetBestChainINTERNAL

Larry Ruane (1):
      ZIP 225: v5 transaction check rules

Luke Dashjr (1):
      Optimisation: Store transaction list order in memory rather than compute it every need

Marco Falke (1):
      [qa] py2: Unfiddle strings into bytes explicitly

Matt Corallo (4):
      Fix calling mempool directly, instead of pool, in ATMP
      Track (and define) ::minRelayTxFee in CTxMemPool
      Add CFeeRate += operator
      Print mempool size in KB when adding txn

Patrick Strateman (5):
      Fix insanity of CWalletDB::WriteTx and CWalletTx::WriteToDisk
      Add CWallet::ListAccountCreditDebit
      Add CWallet::ReorderTransactions and use in accounting_tests.cpp
      Move CWalletDB::ReorderTransactions to CWallet
      Move GetAccountBalance from rpcwallet.cpp into CWallet::GetAccountBalance

Pieter Wuille (16):
      Replace trickle nodes with per-node/message Poisson delays
      Change mapRelay to store CTransactions
      Make ProcessNewBlock dbp const and update comment
      Switch reindexing to AcceptBlock in-loop and ActivateBestChain afterwards
      Optimize ActivateBestChain for long chains
      Add -reindex-chainstate that does not rebuild block index
      Report reindexing progress in GUI
      Split up and optimize transaction and block inv queues
      Handle mempool requests in send loop, subject to trickle
      Return mempool queries in dependency order
      Add support for unique_ptr and shared_ptr to memusage
      Switch CTransaction storage in mempool to std::shared_ptr
      Optimize the relay map to use shared_ptr's
      Optimization: don't check the mempool at all if no mempool req ever
      Optimization: use usec in expiration and reuse nNow
      Get rid of CTxMempool::lookup() entirely

Russell Yanofsky (2):
      [wallet] Add GetLegacyBalance method to simplify getbalance RPC
      [wallet] Remove unneeded legacy getbalance code

Shaul Kfir (1):
      Add absurdly high fee message to validation state (for RPC propagation)

Suhas Daftuar (3):
      Use txid as key in mapAlreadyAskedFor
      Reverse the sort on the mempool's feerate index
      Only use AddInventoryKnown for transactions

Technetium (1):
      add missing aarch64 build deps

Wladimir J. van der Laan (17):
      streams: Add data() method to CDataStream
      streams: Remove special cases for ancient MSVC
      dbwrapper: Use new .data() method of CDataStream
      wallet: Use CDataStream.data()
      net: Consistent checksum handling
      net: Hardcode protocol sizes and use fixed-size types
      protocol.h: Move MESSAGE_START_SIZE into CMessageHeader
      protocol.h: Make enums in GetDataMsg concrete values
      Add assertion and cast before sending reject code
      Add debug message to CValidationState for optional extra information
      Introduce REJECT_INTERNAL codes for local AcceptToMempool errors
      Add function to convert CValidationState to a human-readable message
      Remove most logging from transaction validation
      Add information to errors in ConnectBlock, CheckBlock
      Move mempool rejections to new debug category
      net: Fix sent reject messages for blocks and transactions
      test: Add basic test for `reject` code

hexabot (2):
      Update depends/packages/native_clang.mk
      Update depends/packages/native_rust.mk

Marshall Gaucher (5):
      Remove sprout funding flow logic
      Add fix and note for timing issue
      Update funding logic bug
      Add usage documentation for manual and faucet driven tests
      Update funding logic

Jack Grigg (12):
      Document next_pow2 effects and algorithm source
      Improvements to CBlock::BuildAuthDataMerkleTree
      rust: Explicitly return null hash for pre-v5 auth digests
      book: Note that cargo patches work with absolute paths
      Improve docs about setting CBlockIndex hash fields
      test: Cleanups to ZIP 221 Python test code
      Minor fixes to documentation and formatting
      Fix typo in method documentation
      Track lengths when copying receiver data from C++ to Rust
      depends: Greatly simplify the Clang 12 patch
      Adjust code comments to remove topological-sort references
      Fix typos

