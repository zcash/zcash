
Komodo specific changelog:

- add CC functionality and bugfixes
- set sapling activation
- change z-addr prefix byte
- blocksize increased from 2MB to 4 MB
- transaction size increased from 100KB to 200KB

Sapling related changelog:

- Decoupled Spend Authority
- Improved Performance for Shielded Transactions (using sapling instead of sprout)
- transaction format changed

Alex Morcos (1): Output line to debug.log when IsInitialBlockDownload latches to false

Ariel Gabizon (1): Extend Joinsplit tests to Groth

Charlie OKeefe (1): Remove extra slash from lockfile path

Cory Fields (1): crypter: shuffle Makefile so that crypto can be used by the wallet

Daira Hopwood (1): Support testnet rollback.

Daniel Cousens (2): move rpc* to rpc/ rpc: update inline comments to refer to new file paths

Dimitris Apostolou (1): Fix typos

Duke Leto (3): Fix absurd fee bug reported in #3281, with tests Update comment as per @arielgabizon Improve error message

Eirik Ogilvie-Wigley (24): Add more options when asserting in RPC tests Add change indicator for notes Fix test broken by change indicator Rename note data to include sprout Remove redundant curly braces Consolidate for loops Add out point for sapling note data Add sapling note data and map Decrement sapling note witnesses Clear sapling witness cache Extract method for copying previous witnesses Extract methods for incrementing witnesses Extract method for incrementing witness heights Pass sapling merkle tree when incrementing witnesses Increment sapling note witnesses Rename sprout specific methods Remove extra indentation Add getter and setter for sapling note data and update tests Add parameter for version in GetValidReceive Rename Merkle Trees to include sprout or sapling Rename Witnesses to include sprout or sapling Rename test objects to include sprout or sapling Only include the change field if we have a spending key Fix assertion and comment

Gregory Maxwell (2): IBD check uses minimumchain work instead of checkpoints. IsInitialBlockDownload no longer uses header-only timestamps.

Jack Grigg (41): Add some more checkpoints, up to the 1.1.0 release Add Sapling support to z_validateaddress Update payment-api.md with type field of z_validateaddress Alter SaplingNote::nullifier() to take a SaplingFullViewingKey Expose note position in IncrementalMerkleWitness TransactionBuilder with support for creating Sapling-only transactions TransactionBuilder: Check that all anchors in a transaction are identical Formatting test: Move ECC_Start() call into src/gtest/main.cpp TransactionBuilder: Add support for transparent inputs and outputs TransactionBuilder: Add change output to transaction TransactionBuilder: Make fee configurable Rename xsk to expsk Implement CKeyStore::GetSaplingPaymentAddresses() Raise the 90-character limit on Bech32 encodings Add Sapling support to z_getnewaddress and z_listaddresses Fix block hash for checkpoint at height 270000 Formatting test: Deduplicate logic in wallet_addresses RPC test test: Another assert in wallet_zkeys_tests.store_and_load_sapling_zkeys test: Fix permissions of wallet_addresses test: Update rpc_wallet_z_importexport to account for Sapling changes Rename DecryptSpendingKey -> DecryptSproutSpendingKey Rename CryptedSpendingKeyMap -> CryptedSproutSpendingKeyMap Add Sapling decryption check to CCryptoKeyStore::Unlock() Check for unencrypted Sapling keys in CCryptoKeyStore::SetCrypted() Remove outdated comment Add CWallet::AddCryptedSaplingSpendingKey() hook Pass SaplingPaymentAddress to store through the CKeyStore Rename SpendingKeyMap -> SproutSpendingKeyMap Rename SerializedSize -> SerializedSproutSize Rename ViewingKey -> SproutViewingKey Formatting nits Rename *SpendingKey -> *SproutSpendingKey chainparams: Add BIP 44 coin type (as registered in SLIP 44) Upgrade Rust to 1.28.0 stable Adjust Makefile so that common can be used by the wallet Move RewindBlockIndex log message inside rewindLength check test: gtest for Sapling encoding and decoding test: Use regtest in key_tests/zs_address_test Disable Sapling features on mainnet

Jay Graber (13): Add Sapling Add/Have/Get to keystore Add SaplingIncomingViewingKeys map, SaplingFullViewingKey methods Add StoreAndRetrieveSaplingSpendingKey test Change default_address to return SaplingPaymentAddr and not boost::optional Add crypted keystore sapling add key Discard sk if ivk == 0 Add Sapling support to z_exportkey Add Sapling support to z_importkey Add Sapling to rpc_wallet_z_importexport test Refactor into visitors and throw errors for invalid key or address. Take expiryheight as param to createrawtransaction Add Sapling have/get sk crypter overrides Add Sapling keys to CCryptoKeyStore::EncryptKeys

Jonas Schnelli (2): [RPC, Wallet] Move RPC dispatch table registration to wallet/ code Fix test_bitcoin circular dependency issue

Kaz Wesley (1): IsInitialBlockDownload: usually avoid locking

Larry Ruane (4): Disable libsnark debug logging in Boost tests add extra help how to enable experimental features Add call to sync_all() after (z_sendmany, wait) don't ban peers when loading pre-overwinter blocks

Pejvan (2): Update README.md Update README.md

Richard Littauer (1): docs(LICENSE): update license year to 2018

Sean Bowe (21): Update librustzcash Implementation of Sapling in-band secret distribution. Swap types in OutputDescription to use new NoteEncryption interfaces. Prevent nonce reuse in Sapling note encryption API. Add get_esk() function to Sapling note encryption. Minor edits Decryption and tests of note/outgoing encryption. Update librustzcash and sapling-crypto. Fix bug in return value. Ensure sum of valueBalance and all vpub_new's does not exceed MAX_MONEY inside of CheckTransactionWithoutProofVerification. Move extern params to beginning of test_checktransaction. Relocate ECC_Start() to avoid test failures. Don't call ECC_Start/ECC_Stop outside the test harness. Make changes to gtest ECC behavior suggested by @str4d. Check the hash of the (Sapling+) zk-SNARK parameters during initialization. Switch to use the official Sapling parameters. make-release.py: Versioning changes for 2.0.0-rc1. make-release.py: Updated manpages for 2.0.0-rc1. make-release.py: Updated release notes and changelog for 2.0.0-rc1. Always write the empty root down as the best root, since we may roll back. Sapling mainnet activation height

Simon Liu (11): Add encryption of SaplingNotePlaintext and SaplingOutgoingPlaintext classes. Update and fix per review comments, the test for absurd fee. Minor update to address nits in review. Implement Sapling note decryption using full viewing key. Rename AttemptSaplingEncDecryptionUsingFullViewingKey and use function overloading. Only check for a valid Sapling anchor after Sapling activation. Clean up for rebase: rename mapNoteData to mapSproutNoteData. Clean up help messages for RPC createrawtransaction. Add tests for expiryheight parameter of RPC createrawtransaction. make-release.py: Versioning changes for 2.0.0. make-release.py: Updated manpages for 2.0.0.

Wladimir J. van der Laan (2): Make max tip age an option instead of chainparam rpc: Register calls where they are defined

kozyilmaz (1): Add -Wl,-pie linker option for macOS and use it instead of -pie

mdr0id (1): Fix minor references to auto-senescence in code
