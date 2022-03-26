Notable changes
===============

Mnemonic Recovery Phrases
-------------------------

The zcashd wallet has been modified to support BIP 39, which describes how to
derive the wallet's HD seed from a mnemonic phrase.  The mnemonic phrase will
be generated on load of the wallet, or the first time the wallet is unlocked,
and is available via the `z_exportwallet` RPC call. All new addresses produced
by the wallet are now derived from this seed using the HD wallet functionality
described in ZIP 32 and ZIP 316. For users upgrading an existing Zcashd wallet,
it is recommended that the wallet be backed up prior to upgrading to the 4.7.0
Zcashd release.

Following the upgrade to 4.7.0, Zcashd will require that the user confirm that
they have backed up their new emergency recovery phrase, which may be obtained
from the output of the `z_exportwallet` RPC call. This confirmation can be
performed manually using the `zcashd-wallet-tool` utility that is supplied
with this release (built or installed in the same directory as `zcashd`).
The wallet will not allow the generation of new addresses until this
confirmation has been performed. It is recommended that after this upgrade,
that funds tied to preexisting addresses be migrated to newly generated
addresses so that all wallet funds are recoverable using the emergency
recovery phrase going forward. If you choose not to migrate funds in this
fashion, you will continue to need to securely back up the entire `wallet.dat`
file to ensure that you do not lose access to existing funds; EXISTING FUNDS
WILL NOT BE RECOVERABLE USING THE EMERGENCY RECOVERY PHRASE UNLESS THEY HAVE
BEEN MOVED TO A NEWLY GENERATED ADDRESS FOLLOWING THE 4.7.0 UPGRADE.

New RPC Methods
---------------

- 'walletconfirmbackup' This newly created API checks a provided emergency
  recovery phrase against the wallet's emergency recovery phrase; if the phrases
  match then it updates the wallet state to allow the generation of new addresses.
  This backup confirmation workflow can be disabled by starting zcashd with 
  `-requirewalletbackup=false` but this is not recommended unless you know what
  you're doing (and have otherwise backed up the wallet's recovery phrase anyway).
  For security reasons, this RPC method is not intended for use via zcash-cli 
  but is provided to enable `zcashd-wallet-tool` and other third-party wallet 
  interfaces to satisfy the backup confirmation requirement. Use of the 
  `walletconfirmbackup` API via zcash-cli would risk that the recovery phrase 
  being confirmed might be leaked via the user's shell history or the system
  process table; `zcashd-wallet-tool` is specifically provided to avoid this
  problem.
- 'z_getbalanceforviewingkey' This newly created API allows a user to obtain
  balance information for funds visible to a Sapling or Unified full
  viewing key; if a Sprout viewing key is provided, this method allows 
  retrieval of the balance only in the case that the wallet controls the
  corresponding spending key.

RPC Changes
-----------

- The results of the 'dumpwallet' and 'z_exportwallet' RPC methods have been modified
  to now include the wallet's newly generated emergency recovery phrase as part of the
  exported data.

- The results of the 'getwalletinfo' RPC have been modified to return two new fields:
  `mnemonic_seedfp` and `legacy_seedfp`, the latter of which replaces the field that
  was previously named `seedfp`. 

Wallet
------

'z_sendmany'
------------

- The 'z_sendmany' RPC call no longer permits Sprout recipients in the 
  list of recipient addresses. Transactions spending Sprout funds will
  still result in change being sent back into the Sprout pool, but no
  other `Sprout->Sprout` transactions will be constructed by the Zcashd
  wallet. 

- The restriction that prohibited `Sprout->Sapling` transactions has been 
  lifted; however, since such transactions reveal the amount crossing 
  pool boundaries, they must be explicitly enabled via a parameter to
  the 'z_sendmany' call.

- A new string parameter, `privacyPolicy`, has been added to the list of
  arguments accepted by `z_sendmany`. This parameter enables the caller to
  control what kind of information they permit `zcashd` to reveal when creating
  the transaction. If the transaction can only be created by revealing more
  information than the given strategy permits, `z_sendmany` will return an
  error. The parameter defaults to `LegacyCompat`, which applies the most
  restrictive strategy `FullPrivacy` when a Unified Address is present as the
  sender or a recipient, and otherwise preserves existing behaviour (which
  corresponds to the `AllowFullyTransparent` policy).

- Since Sprout outputs are no longer created (with the exception of change)
  'z_sendmany' no longer generates payment disclosures (which were only 
  available for Sprout outputs) when the `-paymentdisclosure` experimental
  feature flag is set.

Changelog
=========

Charlie O'Keefe (2):
      Update base image used by Dockerfile from debian 10 to debian 11
      Remove stretch (debian 9), add bullseye (debian 11) in gitian descriptor

Conrado Gouvea (1):
      Add comment to zcash_script_new_precomputed_tx about references to input buffers

Daira Hopwood (25):
      Avoid a warning by explicitly calling drop.
      Replace call to drop with zeroization.
      contrib/devtools/rust-deps-graph.sh: allow overriding the arguments to `cargo deps`.
      Cosmetic whitespace change
      Make a zcashd-wallet-tool executable.
      Attempt to fix linking problem on ARM.
      Add some text about choosing location of the physical backup. Add TODO for better handling of file not found and permission errors.
      Move `wallet_tool.rs` from `src/rust/src` into `src/rust/bin`. Also add a brief description of `zcashd-wallet-tool` to `src/rust/README.md`.
      Improved error handling.
      The recovery phrase confirmation and `zcashd-wallet-tool` are being introduced in zcashd v4.7.0.
      Refactor use of `export_path` as suggested.
      Tweak the wording of the fallback messages when the terminal cannot be automatically cleared.
      Simplify extraction of recovery phrase.
      Improve memory hygiene, and use -stdin to pass the recovery phrase to the child zcash-cli process.
      Cleanups to error handling for the first invocation of zcash-cli.
      Use the tracing crate for debugging.
      Improve error message when the config file cannot be found, taking into account -conf and -datadir.
      Ensure the buffer used in `prompt` is zeroized even on error.
      Document that '~' cannot be used in `-datadir` (see #5661).
      Set `meta` for `-datadir` option.
      Simplify debug tracing.
      Correct the fallback instruction for how to clear the terminal on macOS: pressing Command + K also clears scrollback.
      Include $(bin_SCRIPTS) in `check-symbols`, `check-security`, and `clean` targets. Checking for stack canaries in `check-security` is disabled for Rust executables (Rust does support `-Z stack-protector=all` but only for the nightly compiler).
      qa/zcash/full_test_suite.py: enable `test_rpath_runpath` for Rust binaries, and reenable `test_fortify_source` for C++ binaries.
      Tweaks to message text.

Dimitris Apostolou (1):
      Fix typos

Jack Grigg (65):
      wallet: Implement `z_getnewaccount`
      wallet: Implement `z_getaddressforaccount`
      wallet: Implement `z_listunifiedreceivers`
      wallet: Show UAs owned by the wallet in `z_viewtransaction`
      wallet: Reverse order of arguments to z_getaddressforaccount
      rust: Add missing Orchard receiver check in UA parsing
      rust: Add missing checks for shielded UFVK items
      rust: Add missing checks for transparent UFVK items
      wallet: Implement `z_getbalanceforaccount`
      wallet: Fix account generation bug
      wallet: Implement `z_getbalanceforaddress`
      wallet: Don't show Sapling receivers from UAs in `z_listaddresses`
      wallet: Show UAs instead of Sapling receivers in `z_listunspent`
      wallet: Remove `CWallet::GetKeyFromPool`
      wallet: Store internal transparent keys in the keypool
      wallet: Separate counters for external and internal transparent keys
      Add Orchard recipient support to the transaction builder
      Make `TransactionBuilder::AddOrchardOutput` memo optional
      Return UFVK from `CWallet::GenerateNewUnifiedSpendingKey`
      Rename to `ZcashdUnifiedSpendingKey::GetSaplingKey` for consistency
      Trial-decrypt candidate Sapling receivers with all wallet UFVKs
      Add mappings from Orchard receivers to IVKs to the wallet
      Add Orchard to default UA receiver types
      Fix semantic merge conflicts
      Select Orchard receivers preferentially from UAs
      Regenerate `TxDigests` after rebuilding tx with Orchard bundle
      qa: Bump all postponed dependencies
      qa: Postpone recent CCache release
      depends: Update Rust to 1.59.0
      depends: Update Clang / libcxx to LLVM 13.0.1
      cargo update
      rust: Fix clippy lint
      Ensure the view's best Orchard anchor matches the previous block
      Add missing `view.PopAnchor(_, ORCHARD)` in `DisconnectBlock`
      Add RPC test for the Orchard commitment tree bug on first NU5 testnet
      Use `std::optional` in `CValidationInterface::GetAddressForMining`
      Select Orchard receivers from UA miner addresses once NU5 activates
      miner: Manually add dummy Orchard output with Orchard miner address
      rpc: Handle keypool exhaustion separately in `generate` RPC
      depends: Revert to `libc++ 13.0.0-3` for Windows cross-compile
      Add Orchard spend support to the transaction builder
      wallet: Alter `OrchardWallet::GetSpendInfo` to return the spending key
      Improve FFI interface documentation
      Refactor `CWallet::GenerateChangeAddressForAccount`
      Add unit tests for `SpendableInputs::LimitToAmount`
      Fix bug in `SpendableInputs::LimitToAmount`
      Select spendable inputs based on recipient and change pool types
      Implement opportunistic shielding in `SpendableInputs::LimitToAmount`
      Add Orchard cases to note selection logic
      Add Orchard to change address generation
      Add support for sending Orchard funds in `z_sendmany`
      Set default Orchard anchor confirmations to 1
      build: Fix `zcash/address/orchard.hpp` filename in `src/Makefile.am`
      z_sendmany: Replace `allowRevealedAmount` with `privacyStrategy`
      z_sendmany: Expand `privacyPolicy` cases
      build: Add missing `util/match.h` to `src/Makefile.am`
      wallet: Add seedfp to transparent keys in dumpwallet / z_exportwallet
      Fix bugs in wallet_addresses RPC test
      wallet: Fix bugs in `listaddresses`
      wallet: Fix Sapling address bug in `listaddresses`
      wallet: Fix expected `listaddresses` sources in `rpc_wallet_tests`
      qa: Exclude `native_libtinfo` from dependency update checks
      cargo update
      make-release.py: Versioning changes for 4.7.0-rc1.
      make-release.py: Updated manpages for 4.7.0-rc1.

John Newbery (1):
      Log calls to getblocktemplate

Jonas Schnelli (1):
      [Wallet] add HD xpriv to dumpwallet

Kris Nuttycombe (243):
      Derive random HD seeds from ZIP-339 seed phrases.
      Add support for externally searching for valid Sapling diversifiers.
      Adds basic unified spending key derivation.
      Add unified full viewing keys and Zcash-internal unified addresses.
      Use the default UA-based Sapling address for the saplingmigration tool.
      Fix tests for wallet operations on legacy Sapling keys.
      Remove unused forward declaration.
      Update librustzcash dependency version.
      Apply suggestions from code review
      Derive transparent keys from mnemonic seed.
      Generate legacy Sapling addresses from the mnemonic seed.
      Replace account ID with ZIP-0032 key path in listaddresses output.
      Use legacy address for Sapling migration until UA functionality is available to the RPC tests.
      Revert change to the type of GenerateNewKey
      Fix wallet import/export test
      Use 0x7FFFFFFF for the legacy account ID.
      Require backup of the emergency recovery phrase.
      Use hardened derivation for the legacy Sapling key at the address index level.
      Address comments from code review.
      Restore legacy HD seed storage & retrieval tests.
      Fix spurious test passage.
      Move CKeyMetadata back to wallet.h
      Clean up format of recovery information in the wallet dump.
      Use SecureString for mnemonic phrase.
      Apply suggestions from code review
      Fix diversifier_index_t less than operator.
      Restore FindAddress comment
      Fix transparent BIP-44 keypaths.
      Fix naming of unified spending & full viewing keys
      Fix max transparent diversifier index.
      Clean up handling of mnemonic seed metadata.
      Restore legacy HDSeed encryption tests.
      Style fix in BIP 32 path account parsing test.
      Do not strip quotes when verifying mnemonic seed.
      Apply suggestions from code review
      Fix name of menmonic entropy length constant.
      Fix polymorphism of string serialization.
      Document mnemonic-seed-related RPC method changes & update changelog.
      Minor cleanup of the code that searches for a valid transparent key.
      Generalize keypath parsing over account id.
      Update documentation for GenerateNewSeed.
      Use MnemonicSeed instead of HDSeed where appropriate.
      Add diversifier_index_t::ToTransparentChildIndex
      Only maintain CKeyID for the transparent part of ZcashdUnifiedAddress
      Minor naming fixes
      Parameterize HD keypath parsing by coin type.
      Fix error message text to refer to zcashd-wallet-tool rather than the RPC method.
      Apply suggestions from code review
      Minor textual fixes to release notes.
      Improve documentation of the `-walletrequirebackup` zcashd wallet configuration option.
      Add libzcash::AccountId type.
      Adds Orchard Address, IncomingViewingKey, FullViewingKey, and SpendingKey types.
      Apply suggestions from code review
      Update orchard & librustzcash dependency versions.
      Remove incorrect FFI method documentation.
      Remove Orchard spending key equality implementation.
      Refine structure of Zcash address generation.
      Use CKeyID and CScriptID instead of new P2PKH/P2SHAddress classes.
      Remove ZcashdUnifiedAddress in favor of UnifiedAddress
      Update to ufvk zcash_address build.
      Adds SaplingDiversifiableFullViewingKey
      Add Rust FFI components for unified full viewing keys.
      Add UnifiedFullViewingKey type.
      Apply suggestions from code review
      Add tests for ufvk roundtrip serialization.
      Apply suggestions from code review
      Apply suggestions from code review
      Apply suggestions from code review
      Add functions for generating BIP-44 and ZIP-32 keypaths
      Check the output of zip339_phrase_to_seed in MnemonicSeed initialization.
      Compute key id for UFVKs.
      Add ZcashdUnifiedKeyMetadata and libzcash::ReceiverType
      Add unified key components to the transparent & Sapling wallet parts.
      Store ufvks to the wallet database.
      Add unified address tracking to KeyStore
      Load unified full viewing keys from the walletdb.
      Add key metadata to walletdb.
      Add unified address generation.
      AddTransparentSecretKey does not need to take a CExtKey
      Add newly generated transparent UA receivers to the wallet.
      Add CWallet::GetUnifiedForReceiver
      Add tests for keystore storage and retrieval of UFVKs.
      Add test for wallet UA generation & detection.
      Add test for CKeyStore::AddUnifiedAddress
      Fix handling of unified full viewing key metadata.
      Apply suggestions from code review
      Only derive ZcashdUnifiedFullViewingKey from UnifiedFullViewingKey
      Rename `ZcashdUnifiedSpendingKeyMetadata` -> `ZcashdUnifiedAccount`
      Remove unused ufvkid argument from AddTransparentSecretKey
      Ensure that unified address metadata is always correctly populated.
      Apply suggestions from code review
      Make `FindAddress` correctly check for maximum transparent child index.
      Use Bip44TransparentAccountKeyPath() for Bip44AccountChains keypath construction.
      Improve documentation of UFVK/UA metadata storage semantics.
      Apply suggestions from code review
      Fix encoding order of unified addresses.
      Remove the `InvalidEncoding` type from key & address variants.
      Use CKeyID and CScriptID instead of new P2PKH/P2SHAddress classes.
      Remove spurious uses of HaveSpendingKeyForPaymentAddress
      Add raw transparent address types to PaymentAddress
      Remove spurious variant from asyncrpcoperation_sendmany
      Remove `RawAddress`
      Replace `DecodeDestination` in `GetMinerAddress` with `DecodePaymentAddress`
      Remove uses of KeyIO::DecodeDestination
      Remove a use of KeyIO::DecodeDestination in z_shieldcoinbase
      Use libzcash::PaymentAddress instead of std::string in mergetoaddress
      Improve error messages in the case of taddr decoding failure.
      Apply suggestions from code review
      Apply suggestions from code review
      Use transaction builder for asyncrpcoperation_sendmany.
      Transaction builder must not set consensus branch ID for V4 transactions.
      Fix conditions around dust thresholds.
      Require an explicit flag to allow cross-pool transfers in z_sendmany.
      Apply suggestions from code review
      Return z_sendmany errors synchronously when possible.
      Update release notes to reflect z_sendmany changes
      Move FindSpendableInputs to CWallet
      Replace the badly-named `PaymentSource` with `ZTXOSelector`
      Add support for unified addresses to CWallet::ToZTXOSelector
      Replace `HaveSpendingKeyForAddress` with checks at ZTXOSelector construction.
      Modify CWallet::FindSpendableInputs to use ZTXOSelector
      Add CWallet::FindAccountForSelector
      Add RecipientAddress type.
      Use libzcash::RecipientAddress for z_sendmany recipients.
      Apply suggestions from code review
      Rename ZTXOSelector::SpendingKeysAvailable -> RequireSpendingKeys
      Make `FindSpendableInputs` respect `ZTXOSelector::RequireSpendingKeys()`
      Clarify documentation of CWallet::FindAccountForSelector
      Use `ZTXOSelector::IncludesSapling` rather than selector internals.
      Add documentation for `UnifiedAddress::GetPreferredRecipientAddress`
      Add correct selection of change addresses to z_sendmany
      Add a first-class type for transparent full viewing keys.
      Implement OVK selection for z_sendmany.
      Implement derivation of the internal Sapling spending key.
      Use a BIP 44 change address for change when sending from legacy t-addrs.
      Add a check for internal vs. external outputs to wallet_listreceived test.
      Fix z_sendmany handling of transparent OVK derivation in the ANY_TADDR case.
      Simplify determination of valid change types.
      Add failing tests for z_sendmany ANY_TADDR -> UA and UA -> <legacy_taddr>
      Add gtest for change address derivation.
      Fix variable shadowing in change address derivation & add change IVK to the keystore.
      GenerateLegacySaplingZKey only needs to return an address, not an extfvk.
      Rename AddSaplingIncomingViewingKey -> AddSaplingPaymentAddress
      Do not add Sapling addresses to the wallet by default when adding extfvks.
      Fix a bug in the generation of addresses from UFVKs
      Add accessor method for Sapling IVKs to SaplingDiversifiableFullViewingKey
      Address TODOs in rpc-tests/wallet-accounts.py
      Add a few additional cases to z_sendmany RPC tests.
      Update librustzcash dependency.
      Fix nondeterministic test error (checking for the wrong error case).
      Use z_shieldcoinbase for Sprout funds in wallet_listreceived tests.
      Apply suggestions from code review
      Apply suggestions from code review.
      Fix nondeterministic test failure in wallet_listreceivedby.py
      Fix locking in z_getbalanceforaddress and z_getbalanceforaccount
      Replace z_getbalanceforaddress with z_getbalanceforviewingkey
      Clarify documentation of z_getbalanceforviewingkey for Sprout viewing keys.
      Implement PaymentAddressBelongsToWallet for unified addresses.
      Add unified address support to GetSourceForPaymentAddress
      Address comments from review.
      Add change field to z_listreceivedbyaddress for transparent addrs.
      Fix missing std::variant header that was breaking Darwin builds.
      Add test vectors for UFVK derivation
      Rename sapling-specific zip32 FFI methods.
      Make SaveRecipientMappings polymorphic in RecipientMapping type.
      Add Rust backend for Orchard components of the wallet.
      Add GetFilteredNotes to Orchard wallet.
      Add test for Orchard wallet note detection.
      Move parsing of unified addresses to UnifiedAddress.
      Remove txid field from TxNotes
      Apply suggestions from code review
      Add various bits of documentation
      Add Orchard components to unified address
      Add Orchard components to unified full viewing keys
      Add Orchard components to unified spending keys
      Remove OrchardSpendingKey serialization code
      Select Orchard notes in FindSpendableInputs
      GenerateNewKey must be guarded by a cs_wallet lock
      Filter returned Orchard notes by minimum confirmations.
      Log outpoint for failed Sapling witness lookup.
      Add a roundtrip test for Orchard merkle frontier serialization from the C++ side.
      Add test for Orchard contribution to z_gettotalbalance
      Respect minDepth argument for Orchard notes in GetFilteredNotes
      Update MSRV for lints.
      Update incrementalmerkletree version
      Split LoadWalletTx from AddToWallet
      Fix missing locks for GenerateNewUnifiedSpendingKey tests.
      Record when notes are detected as being spent in the Orchard wallet.
      Reset Orchard wallet state when rescanning from below NU5 activation.
      Check wallet latest anchor against hashFinalOrchardRoot in ChainTip.
      Remove assertions that require Orchard wallet persistence to satisfy.
      Add a test for Orchard note detection.
      Assert we never attempt to checkpoint the Orchard wallet at a negative block height.
      Apply suggestions from code review
      Add an `InPoint` type to the Orchard wallet to fix incorrect conflict data.
      Apply suggestions from code review
      Fix missing update to `last_checkpoint` on rewind.
      Track mined-ness instead of spent-ness of notes in the Orchard wallet.
      Apply suggestions from code review
      Respect maxDepth for Orchard notes in GetFilteredNotes
      Set number of confirmations for Orchard notes returned by FindSpendableInputs
      Update walletTx with decrypted Orchard action metadata.
      Persist Orchard action index/IVK mappings in CWalletTx
      Restore decrypted notes to the wallet.
      Update tests with rollback checks.
      Apply suggestions from code review
      Restore mined block heights when restoring decrypted notes.
      Apply suggestions from code review
      Return std::optional<CExtKey> from CExtKey::Master
      Ensure that Orchard spentness information is repopulated by LoadUnifiedCaches.
      Modify `join_network` in tests to skip mempool resync.
      Address suggestions from code review on #5637
      Apply suggestions from code review
      Serialize the Orchard note commitment tree to the wallet.
      Update orchard_wallet_add_notes_from_bundle documentation.
      Derive the new mnemonic seed from the legacy HD seed, if one is available.
      Fix indentation.
      Apply suggestions from code review
      Explicitly specify the change address in the Sapling migration.
      Add TODO for Orchard z_listunspent integration.
      Document that z_getnewaccount and z_getaddressforaccount replace z_getnewaddress.
      Add orchard support to z_getnotescount
      Document that Orchard keys are not supported in z_importkey help
      Return the correct error for `z_getbalance` if a UA does not correspond to an account in the wallet..
      Update documentation of z_importviewingkey and z_exportviewingkey to address unified keys.
      Add UnifiedAddress variant to ZTXOSelector
      Eliminate redundancy between the wallet and the keystore.
      Simplify retrieval of unified account by address.
      Add OrchardWallet::GetTxActions
      Apply suggestions from code review
      Update z_viewtransaction documentation to correctly represent field names.
      Add debug printing for receivers and recipient addresses.
      Correctly report change outputs in z_viewtransaction.
      Return the default unified address if we have the UFVK but no address metadata.
      Fix missing AllowRevealedSenders flag in test.
      Uncomment addtional tests that depend on z_viewtransaction.
      Minor rename & documentation improvement.
      Add RecipientType to GetPaymentAddressForRecipient result.
      Make CWallet::DefaultReceiverTypes height-dependent.
      Return failure rather than asserting on WriteRecipientMapping failure.
      Lock cs_main for accesses to chainActive in GetPaymentAddressForRecipient.
      Fix legacy address handling in CWallet::GetPaymentAddressForRecipient
      Documentation fix for UnifiedAddressForReciever (+ spelling corrections.)

Larry Ruane (12):
      add ParseArbitraryInt() for diversifier index
      add -orchardwallet experimental feature flag
      Add new and modify existing Orchard RPCs, non-functional
      mining: submitblock: log detailed equihash solution error
      allow UA as z_shieldcoinbase destination
      fix minconf parsing for z_getbalanceforaccount and z_getbalanceforaddress
      Update z_listreceivedbyaddress to support unified addresses (5467)
      fix wallet_listreceived.py, add blockdata to taddr output
      z_listreceivedbyaddress: reject UA component addr (#5537)
      add functional test
      document global variables
      update listaddresses RPC for UAs, Orchard

Marius Kjærstad (1):
      Update copyright year to 2022

Pieter Wuille (2):
      Fix csBestBlock/cvBlockChange waiting in rpc/mining
      Modernize best block mutex/cv/hash variable naming

Sean Bowe (5):
      wallet: consolidate unified key/address/account map reconstruction
      wallet: restore Orchard secret keys from mnemonic seed
      wallet: restore orchard address to IVK mappings during wallet loading
      wallet: rather than assert, error in case of inconsistency between FVK and address
      wallet: add logging for failure cases in unified cache loading

Steven Smith (8):
      Lock cs_main prior to calling blockToJSON
      Mark z_gettotalbalance and dumpwallet as deprecated
      Add Orchard support to the z_gettreestate RPC
      Update transaction size estimation to include V5 transactions
      Extend uniqueness check in z_sendmany to UA receivers
      Load previously persisted sent transaction recipient metadata back into the wallet.
      Add Orchard & unified address support to z_viewtransaction.
      Ensure z_viewtransaction returns Orchard details

Taylor Hornby (1):
      Untested, not working yet, use libtinfo from the debian packages

sasha (12):
      on Arch only, use Debian's libtinfo5_6.0 to satisfy clang
      explain the 0x0f0f[..]0f0f powLimit constant for regtest
      remove superfluous space at end of native_packages line
      gtests ordering: change wallet filename in WriteZkeyDirectToDb
      gtests ordering: ContextualCheckBlockTest's TearDown deactivates Blossom
      gtests ordering: CheckBlock.VersionTooLow calls SelectParams(MAIN)
      implement AtomicTimer::zeroize() that resets start_time and total_time
      gtests ordering: make Metrics.GetLocalSolPS idempotent
      gtests ordering: clean up wallet files before each WalletZkeysTest
      make librustzcash_init_zksnark_params idempotent
      move proof parameter loading out of gtest/main.cpp and into utiltest.cpp
      Call LoadProofParameters() in gtests that need proofs

Ying Tong Lai (18):
      Move SendManyRecipient to wallet.h and introduce optional ua field.
      SendTransaction: Introduce recipients argument.
      Implement read and write for (txid, recipient) -> ua mapping.
      z_sendmany: Only get ua if decoded is ua variant.
      ShieldToAddress: Factor out static shieldToAddress() helper.
      Docfixes.
      CSerializeRecipientAddress: add Read method and make constructor private.
      WriteRecipientMapping: Check that receiver exists in UA.
      wallet_sendmany_any_taddr.py: Test sending from a change taddr.
      wallet_sendmany_any_taddr.py: Test sending output from expired tx.
      FindSpendableInputs: Add nDepth < 0 check.
      wallet_sendmany_any_taddr.py: Expect expired tx to be ignored.
      Orchard: invalidate mempool transactions that use orphaned anchors.
      coins_tests.cpp: Add Orchard nullifier to TxWithNullifiers().
      coins_tests: Update tests to include Orchard case.
      CWallet::GetConflictS: Handle conflicting Orchard spends.
      z_getbalance: Handle Unified Address case.
      Adapt RPC tests to use z_getbalance for UAs.

ying tong (2):
      Apply docfixes from code review
      Style improvements in RPC tests.

Zancas Wilcox (4):
      blake2b/s is integrated into hashlib, drop external python package dependency
      update doctest in gtest suite to prefer hashlib
      enforce usage of the get_tests comptool interface as ComparisonTestFramework method
      All implementations of ComparisonTestFramework were overriding num_nodes

