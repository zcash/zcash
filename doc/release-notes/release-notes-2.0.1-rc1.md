Notable changes
===============

Hierarchical Deterministic Key Generation for Sapling
-----------------------------------------------------
All Sapling addresses will use hierarchical deterministic key generation
according to ZIP 32 (keypath m/32'/133'/k' on mainnet). Transparent and
Sprout addresses will still use traditional key generation.

Backups of HD wallets, regardless of when they have been created, can
therefore be used to re-generate all possible Sapling private keys, even the
ones which haven't already been generated during the time of the backup.
Regular backups are still necessary, however, in order to ensure that
transparent and Sprout addresses are not lost.

[Pull request](https://github.com/zcash/zcash/pull/3492), [ZIP 32](https://github.com/zcash/zips/blob/master/zip-0032.mediawiki)

Changelog
=========

David Mercer (2):
      libsnark: convert long long and unsigned long to C++11 fixed-width types
      libsnark: convert 0ul to UINT64_C(0)

Eirik Ogilvie-Wigley (22):
      Rename map to include sprout
      Add sapling spending keys to z_exportwallet
      Rename AddZKey to include sprout
      Move AddSpendingKeyToWallet
      Return more information when adding a spending key
      Add sapling support to z_importwallet
      Export comment on HDSeed and fingerprint with wallet
      Export zip32 metadata with sapling keys
      Don't export empty zip32 metadata
      Allow more information to be returned when an async rpc fails
      Use utility method to wait for async operations
      Remove unneeded semicolons
      Remove unused imports
      Allow passing timeout parameter to wait_and_assert_operationid_status
      Add test for signing raw transactions offline
      Incorporate APPROX_RELEASE_HEIGHT when determining what consensus branch to sign with
      Allow passing branchId when calling signrawtransaction
      Remove unused import
      Address need not be optional when adding sapling keys
      Use max priority for all shielded transfers
      Move FIXME comment to where the fix should happen
      Add newly discovered sapling addresses to the wallet

George Tankersley (2):
      Refactor ContextualCheckBlock tests (#3187)
      Refactor ContextualCheckBlock tests

Jack Grigg (83):
      [ci-workers] Install Python modules in a virtualenv
      [ci-workers] Handle user home directories outside /home
      [ci-workers] Handle ansible_processor being either a string or a list
      [ci-workers] Add support for MacOSX
      [ci-workers] Add a tag for updating dependencies
      [ci-workers] Add curl and cmake to dependencies
      [ci-workers] README cleanups
      [ci-workers] Add pkg-config to general dependencies
      depends: Correctly configure Rust when cross-compiling
      depends: Configure librustzcash for cross-compiling
      depends: Fix BDB naming issue when cross-compiling
      zcutil/build.sh: Use $HOST to specify the depends prefix
      configure: Don't require RELRO and BIND_NOW when cross-compiling
      Measure Windows console size for metrics UI
      Use -O1 for darwin and mingw32 release builds
      Clean up libzcash CPPFLAGS, CXXFLAGS, and LDFLAGS
      zcutil/build.sh: Use config.site to set default ./configure settings
      zcutil/build.sh: Remove --enable-werror from default configuration
      Pass correct compiler, linker, and flags into libsnark
      Use boost::filesystem::path::string() instead of path::native()
      Metrics UI: Enable virtual terminal sequence processing on Windows
      Metrics UI: Tell Windows users how to stop zcashd
      depends: Pass correct compiler, linker, and flags into googletest
      configure: Don't add -ldl to RUST_LIBS for mingw32
      test: Fix comment in WalletTests.FindMySaplingNotes
      Add Sapling support to GetFilteredNotes() and GetUnspentFilteredNotes()
      Add Sapling support to z_getbalance and z_gettotalbalance
      Metrics UI: Fall back to 80 cols if GetConsoleScreenBufferInfo() fails
      libsnark: Adjust SHA256 K value type to match the constant
      libsnark: Use mp_limb_t cast instead of uint64_t when masking bigint.data
      libsnark: Fix stale comment
      rpc: Clarify Sprout shielded addresses in help text
      rpc: Clarify ivk balance issues in help text
      Move GetSpendingKeyForPaymentAddress visitor into wallet.h
      wallet: Add HaveSpendingKeyForPaymentAddress visitor
      rpcwallet: Add TransactionBuilder argument to AsyncRPCOperation_sendmany
      rpcwallet: Prevent use of both Sprout and Sapling addresses in z_sendmany
      rpcwallet: Add Sapling support to z_sendmany
      Define additional booleans for readability
      Ensure SCOPED_TRACE falls out of scope when necessary
      Revert NU activation heights in reverse order
      Fix test after refactor to check bacd-cb-height rule on a genesis block
      Rename GetFirstBlockTransaction() to GetFirstBlockCoinbaseTx()
      libsnark: Force constants used in test comparisons to be unsigned
      libsnark: Use format macro constants for printing fixed-width values
      Rename z_inputs_ to z_sprout_inputs_
      Minor cleanups
      Fix RPC test that checks exact wording of cleaned-up error message
      Fix file permissions of wallet_sapling RPC test
      Update librustzcash with ZIP 32 APIs
      ZIP 32 Sapling structs
      Store HDSeed in CBasicKeyStore
      Store HDSeed in CCryptoKeyStore
      wallet: Store HDSeed and chain data
      wallet: Store Sapling key metadata indexed by ivk
      wallet: Switch from SaplingSpendingKey to SaplingExtendedSpendingKey
      init: Generate a new HD seed on startup
      wallet: Comment out HDSeed and CHDChain persistence to disk
      Add ZIP 32 usage to release notes
      wallet: Don't allow an HDSeed to be overwritten
      Bugfix: Use time instead of block height for Sapling key metadata
      net: Check against the current epoch's version when rejecting nodes
      Extract a helper method for finding the next epoch
      net: Check against the next epoch's version when evicting peers
      net: Check against the current epoch's version when disconnecting peers
      qa: Test both Overwinter and Sapling peer management
      Use ovk directly in the TransactionBuilder API instead of fvk
      Generate an ovk to encrypt outCiphertext for t-addr senders
      Revert "Disable Sapling features on mainnet"
      Use the correct empty memo for Sapling outputs
      Add Sapling support to z_shieldcoinbase
      Revert "Get rid of consensus.fPowAllowMinDifficultyBlocks."
      Revert "Remove testnet-only difficulty rules"
      Allow minimum-difficulty blocks on testnet and regtest
      Only enable min-difficulty blocks on testnet from a particular height
      Update wallet_listreceived test for now-correct empty Sapling memos
      Rename min-difficulty flag to remove off-by-one in the name
      Explicitly check the min-difficulty flag against boost::none
      Position PoW.MinDifficultyRules test after rule activates
      Fix pyflakes warnings
      Store ExtFVK with encrypted Sapling spending key instead of FVK
      Persist Sapling payment address to IVK map
      Ignore decoding errors during -zapwallettxes

Jay Graber (5):
      s/jsoutindex/outindex for sapling outputs
      z_listunspent sapling support - needs refactor
      Add rpc test for sprout txs z_listunspent
      Modify comments
      Modify GetNullifiersForAddresses for Sapling

Jonas Schnelli (3):
      [Wallet] extend CKeyMetadata with HD keypath     Zcash: modified for zip32
      [Wallet] print hd masterkeyid in getwalletinfo     Zcash: modified for zip32
      [Wallet] ensure CKeyMetadata.hdMasterKeyID will be cleared during SetNull()     Zcash: modified for zip32

Larry Ruane (5):
      generalize mininode.py protocol versioning
      Test peer banning logic in both pre- and post-initial block download states
      Sapling support for z_listreceivedbyaddress
      z_listunspent rpc unit test: add testing for Sapling
      fix z_listunspent includeWatchonly logic

Marius Kjærstad (3):
      Fix for license not being valid
      Update debian package copyright license
      Missing comma in debian package copyright license

Sean Bowe (1):
      Check commitment validity within the decryption API for Sapling note plaintexts.

Simon Liu (59):
      Rename FindMyNotes to FindMySproutNotes.
      Rename GetNoteNullifier to GetSproutNoteNullifier.
      Rename mapNullifiersToNotes to mapSproutNullifiersToNotes.
      Rename CWallet::AddToSpends methods for clarity.
      Rename mapTxNullifiers to mapTxSproutNullifiers.
      Add ivk member variable and equality comparators to SaplingNoteData class.
      Update CWallet::MarkAffectedTransactionsDirty() for Sapling.
      Update CWallet::UpdatedNoteData() for Sapling.
      Create CWallet::AddToSaplingSpends() to track Sapling nullifiers.
      Update test to pass in required cm to SaplingNotePlaintext::decrypt().
      Create CWallet::FindMySaplingNotes()
      Rename IsFromMe(nullifier) to IsSproutNullifierFromMe(nullifier).
      Create CWallet::IsSaplingNullifierFromMe()
      Remove dead code in CWalletTx::GetAmounts() as filed in issue #3434.
      Cleanup CWalletTx::GetAmounts() for clarity.  No-op.
      Update CWalletTx::GetAmounts() to return COutputEntry for Sapling valueBalance.
      Add caching and updating of Sapling note nullifier.
      Update CWallet::IsSpent() to check Sapling nullifiers.
      Clean up names of unit tests in gtest/test_wallet.cpp.
      Add test for CWalletTx::SetSaplingNoteData()
      Iterate over mapSaplingFullViewingKeys with ivk->fvk mapping (1:1).
      Refactor IsSpent(nullifier) for Sprout and Sapling domain separation.
      Fix code review nits.
      Add two new wallet tests: FindMySaplingNotes, SaplingNullifierIsSpent.
      Add new wallet test: NavigateFromSaplingNullifierToNote
      Add new wallet test: UpdatedSaplingNoteData.
      Add new wallet tests: SpentSaplingNoteIsFromMe.
      Rename wallet tests for clarity between Sprout and Sapling.
      Fix typo in variable name in test.
      Fix inaccurate comments in test.
      Fix typo in parameter name.
      Update CWallet::GetConflicts for Sapling.
      Add new wallet test: SetSaplingNoteAddrsInCWalletTx.
      Add new wallet test: GetConflictedSaplingNotes
      Add new wallet test: MarkAffectedSaplingTransactionsDirty
      Update wallet unit tests to revert upgraded network parameters.
      Clean up wallet unit tests: replace .value() with .get() for clarity.
      Fix comment in CWallet::SyncMetaData.
      Refactor: rename setLockedNotes -> setLockedSproutNotes
      Refactor: rename UnlockAllNotes -> UnlockAllSproutNotes
      Refactor: rename ListLockedNotes -> ListLockedSproutNotes
      Add methods to store SaplingOutPoint in setLockedSaplingNotes
      Add unit test SaplingNoteLocking
      Update comment for test ContextualCheckBlockTest.BlockSproutRulesRejectOtherTx
      Add Sapling fields to JSON RPC output using TxToJSON.
      Update qa test to check for Sapling related JSON fields.
      Closes #3534. Do not use APPROX_RELEASE_HEIGHT to get consensus branch     id when in regtest mode.
      For #3533. Replace asserts with JSON errors.
      Update qa test as offline regtest nodes need branch id passed in.
      Fix rebasing of CWallet::GetNullifiersForAddresses
      Cleanup to address review comments.
      Add test that Sapling shielded transactions have MAX_PRIORITY
      Closes #3560. Update Sapling note data correctly when importing a key.
      For #3546. Shielded tx with missing inputs are not treated as orphans.
      For #3546. Improve estimated tx size for Sapling outputs.
      Fix deadlock from calling CWallet::AddSaplingIncomingViewingKey instead of CBasicKeyStore::AddSaplingIncomingViewingKey
      Fix file permissions of QA test wallet_persistence.py
      Update wallet_persistence test to verify wallet txs are persisted across restarts.
      Update wallet_persistence test to verify spending notes after restart.

WO (4):
      Fix a bug of Windows binary
      Add an assert for num_bits function
      long -> int64_t
      The long data type is replaced with int64_t

Za Wilcox (1):
      Revise help output for z_sendmany

mdr0id (8):
      Resolve final edits for README
      Revert "wallet: Comment out HDSeed and CHDChain persistence to disk"
      Persist Sapling key material in the wallet to disk
      Serialize Sapling data in CWalletTx
      Adding in rpc wallet sap for test_bitcoin
      Add gtest coverage of Sapling wallet persistence
      make-release.py: Versioning changes for 2.0.1-rc1.
      make-release.py: Updated manpages for 2.0.1-rc1.

