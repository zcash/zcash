Notable changes
===============

Changes to Testnet NU5 Consensus Rules
--------------------------------------

NOTE: All testnet nodes that have been running on testnet above height 1,599,200 will need
to upgrade to `v4.7.0` and then run with `-reindex` and `-rescan`.

- In order to better support hardware wallets, transparent signature hash construction as
  defined in [ZIP 244](https://zips.z.cash/zip-0244) has been modified to include a hash
  of the serialization of the amounts of all outputs being spent, along with a hash of all
  spent outputs `scriptPubKeys` values, except in the case that the `ANYONECANPAY` flag is
  set. This allows hardware wallet devices to verify the UTXO amounts without having to
  stream all the previous transactions containing the outputs being spent to the device.
  Also as part of these changes, the transparent signature hash digest now commits
  directly, rather than implicitly, to the sighash type, and the sighash type is
  restricted to a fixed set of valid values. The change to ZIP 244 can be seen 
  [here](https://github.com/zcash/zips/commit/ac9dd97f77869b9191dc9592faa3ebf45475c341).
- This release fixes a bug in `v4.6.0` that caused a consensus failure on the Zcash
  testnet at height `1,779,200`. For details see
  <https://github.com/zcash/zcash/commit/5990853de3e9f04f28b7a4d59ef9e549eb4b4cca>.
- There have been changes to the Halo2 proving system to improve consistency between the
  specification and the implementation, and these may break compatibility. See for example
  <https://github.com/zcash/halo2/commit/247cd620eeb434e7f86dc8579774b30384ae02b2>.
- There have been numerous changes to the Orchard circuit implementation since `v4.6.0`.
  See <https://github.com/zcash/orchard/issues?q=label%3AM-consensus-change-since-0.1.0-beta-1>
  for a complete list.
- A potential Faerie Gold vulnerability affecting the previous activation of NU5 on
  testnet and existing since `v4.6.0` has been mitigated.

NU5 Testnet Reactivation
------------------------

To support the aforementioned testnet consensus changes, the following changes are made in
`zcashd v4.7.0`:

- The consensus branch ID for NU5 is changed to `0xC2D6D0B4`.
- The protocol version indicating NU5-aware testnet nodes is set to `170050`.
- The testnet reactivation height for NU5 is set to **1,842,420**.

As mentioned above, all testnet nodes that have been running on testnet above height
1,599,200 will need to upgrade to `v4.7.0` and then run with `-reindex` and `-rescan`.

Emergency Recovery Phrases
--------------------------

The zcashd wallet has been modified to support BIP 39, which describes how to derive the
wallet's HD seed from a mnemonic phrase, hereafter known as the wallet's "emergency
recovery phrase". The emergency recovery phrase will be generated on load of the wallet,
or the first time the wallet is unlocked, and is available via the `z_exportwallet` RPC
call. All new addresses produced by the wallet are now derived from this seed using the HD
wallet functionality described in ZIP 32 and ZIP 316. For users upgrading an existing
Zcashd wallet, it is recommended that the wallet be backed up prior to upgrading to the
4.7.0 Zcashd release. In the remainder of this document, the HD seed derived from the 
emergency recovery phrase will be termed the wallet's "mnemonic seed".

Following the upgrade to 4.7.0, Zcashd will require that the user confirm that they have
backed up their new emergency recovery phrase, which may be obtained from the output of
the `z_exportwallet` RPC call. This confirmation can be performed manually using the
`zcashd-wallet-tool` utility that is supplied with this release (built or installed in the
same directory as `zcashd`).  The wallet will not allow the generation of new addresses
until this confirmation has been performed. It is recommended that after this upgrade,
funds tied to preexisting addresses be migrated to newly generated addresses so that all
wallet funds are recoverable using the emergency recovery phrase going forward. If you
choose not to migrate funds in this fashion, you will continue to need to securely back up
the entire `wallet.dat` file to ensure that you do not lose access to existing funds;
EXISTING FUNDS WILL NOT BE RECOVERABLE USING THE EMERGENCY RECOVERY PHRASE UNLESS THEY
HAVE BEEN MOVED TO A NEWLY GENERATED ADDRESS FOLLOWING THE 4.7.0 UPGRADE.

In the case that your wallet previously contained a Sapling HD seed, the emergency
recovery phrase is constructed using the bytes of that seed, such that it is possible to
reconstruct keys generated using that legacy seed if you know the emergency recovery
phrase. HOWEVER, THIS RECONSTRUCTION DOES NOT FOLLOW THE NORMAL PROCESS OF DERIVATION FROM
THE EMERGENCY RECOVERY PHRASE. Instead, to recover a legacy Sapling key from the emergency
recovery phrase, it is necessary to reconstruct the bytes of the legacy seed by conversion
of the phrase back to its source randomness instead of by hashing as is specified in BIP
39. Only keys and addresses produced after the upgrade can be obtained by normal
derivation of a ZIP 32 or BIP 32 master seed using BIP 39.

Wallet Updates
--------------

The zcashd wallet now supports the Orchard shielded protocol.

The zcashd wallet has been modified to alter the way that change is handled. In the case
that funds are being spent from a unified account, change is sent to a wallet-internal
change address for that account instead of sending change amounts back to the original
address where a note being spent was received.  The rationale for this change is that it
improves the security that is provided to the user of the wallet when supplying incoming
viewing keys to third parties; previously, an incoming viewing key could effectively be
used to detect when a note was spent (hence violating the "incoming" restriction) by
observing change outputs that were sent back to the address where the spent note was
originally received.

New RPC Methods
---------------

- `walletconfirmbackup` This newly created API checks a provided emergency recovery phrase 
  against the wallet's emergency recovery phrase; if the phrases match then it updates the
  wallet state to allow the generation of new addresses.  This backup confirmation
  workflow can be disabled by starting zcashd with `-walletrequirebackup=false` but this
  is not recommended unless you know what you're doing (and have otherwise backed up the
  wallet's emergency recovery phrase anyway).  For security reasons, this RPC method is
  not intended for use via `zcash-cli` but is provided to enable `zcashd-wallet-tool` and
  other third-party wallet interfaces to satisfy the backup confirmation requirement. Use
  of the `walletconfirmbackup` API via `zcash-cli` would risk that the emergency recovery
  phrase being confirmed might be leaked via the user's shell history or the system
  process table; `zcashd-wallet-tool` is provided specifically to avoid this problem.
- `z_getnewaccount` This API allows for creation of new BIP 44 / ZIP 32 accounts using HD
  derivation from the wallet's mnemonic seed. Each account represents a separate spending
  authority and source of funds. A single account may contain funds in the Sapling and
  Orchard shielded pools, as well as funds held in transparent addresses.
- `z_listaccounts` This API returns the list of BIP 44 / ZIP 32 accounts that are being
  tracked by the wallet.
- `z_getaddressforaccount` This API allows for creation of diversified unified addresses 
  under a single account. Each call to this API will, by default, create a new diversified
  unified address containing transparent p2pkh, Sapling, and Orchard receivers. Additional
  arguments to this API may be provided to request the address to be created with a
  user-specified set of receiver types and diversifier index.
- `z_getbalanceforaccount` This API makes it possible to obtain balance information on a 
  per-account basis.
- `z_getbalanceforviewingkey` This API allows a user to obtain balance
  information for funds visible to a Sapling or Unified full viewing key; if a Sprout
  viewing key is provided, this method allows retrieval of the balance only in the case
  that the wallet controls the corresponding spending key.  This API has been added to
  supplement (and largely supplant) `z_getbalance`. Querying for balance by a single
  address returns only the amount received by that address, and omits value sent to other
  diversified addresses derived from the same full viewing key; by using
  `z_getbalanceforviewingkey` it is possible to obtain a correct balance that includes all
  amounts controlled by a single spending key, including both those sent to external
  diversified addresses and to wallet-internal change addresses.
- `z_listunifiedreceivers` This API allows the caller to extract the individual component
  receivers from a unified address. This is useful if one needs to provide a bare Sapling
  or transparent p2pkh address to a service that does not yet support unified addresses.

RPC Changes
-----------

- The result type for the `listaddresses` endpoint has been modified:
  - The `keypool` source type has been removed; it was reserved but not used.
  - In the `sapling` address results, the `zip32AccountId` attribute has been removed in
    favor of `zip32KeyPath`. This is to allow distinct key paths to be reported for
    addresses derived from the legacy account under different child spending authorities,
    as are produced by `z_getnewaddress`.
  - Addresses derived from the wallet's mnemonic seed are now included in
    `listaddresses` output.
- The results of the `dumpwallet` and `z_exportwallet` RPC methods have been modified
  to now include the wallet's newly generated emergency recovery phrase as part of the
  exported data. Also, the seed fingerprint and HD keypath information are now included in
  the output of these methods for all HD-derived keys.
- The results of the `getwalletinfo` RPC have been modified to return two new fields:
  `mnemonic_seedfp` and `legacy_seedfp`, the latter of which replaces the field that
  was previously named `seedfp`.
- A new `pool` attribute has been added to each element returned by `z_listunspent` to
  indicate which value pool the unspent note controls funds in.
- `z_listreceivedbyaddress` 
  - A `pool` attribute has been added to each result to indicate what pool the received
    funds are held in. 
  - A boolean-valued `change` attribute has been added to indicate whether the output is
    change.
  - Block metadata attributes `blockheight`, `blockindex`, and `blocktime` have been
    added to the result.
- `z_viewtransaction` has been updated to include attributes that provide information
  about Orchard components of the transaction. Also, the `type` attribute for spend and
  output values has been deprecated and replaced by the `pool` attribute.
- `z_getnotescount` now also returns information for Orchard notes.
- The output format of `z_exportwallet` has been changed. The exported file now includes
  the mnemonic seed for the wallet, and HD keypaths are now exported for transparent
  addresses when available.
- The result value for `z_importviewingkey` now includes an `address_type` field that
  replaces the now-deprecated `type` key.
- `z_listunspent` has been updated to render unified addresses for Sapling and 
  Orchard outputs when those outputs are controlled by unified spending keys.  Outputs
  received by unified internal addresses do not include the `address` field.
- Legacy transparent address generation using `getnewaddress` no longer uses a
  preallocated keypool, but instead performs HD derivation from the wallet's mnemonic seed
  according to BIP 39 and BIP 44 under account ID `0x7FFFFFFF`.
- `z_gettreestate` has been updated to include information about the Orchard note
  commitment tree.

### 'z_sendmany'

- The `z_sendmany` RPC call no longer permits Sprout recipients in the list of recipient
  addresses. Transactions spending Sprout funds will still result in change being sent
  back into the Sprout pool, but no other `Sprout->Sprout` transactions will be
  constructed by the Zcashd wallet.
- The restriction that prohibited `Sprout->Sapling` transactions has been lifted; however, 
  since such transactions reveal the amount crossing pool boundaries, they must be
  explicitly enabled via a parameter to the `z_sendmany` call.
- A new string parameter, `privacyPolicy`, has been added to the list of arguments
  accepted by `z_sendmany`. This parameter enables the caller to control what kind of
  information they permit `zcashd` to reveal on-chain when creating the transaction. If the
  transaction can only be created by revealing more information than the given strategy
  permits, `z_sendmany` will return an error. The parameter defaults to `LegacyCompat`,
  which applies the most restrictive strategy `FullPrivacy` when a Unified Address is
  present as the sender or a recipient, and otherwise preserves existing behaviour (which
  corresponds to the `AllowFullyTransparent` policy). In cases where it is possible to do
  so without revealing additional information, and where it is permitted by the privacy
  policy, the wallet will now opportunistically shield funds to the most current pool.
- Since Sprout outputs are no longer created (with the exception of change)
  `z_sendmany` no longer generates payment disclosures (which were only available for
  Sprout outputs) when the `-paymentdisclosure` experimental feature flag is set.
- Outgoing viewing keys used for shielded outputs are now produced as 
  described in [ZIP 316](https://zips.z.cash/zip-0316#usage-of-outgoing-viewing-keys)
- When sending from or to one or more unified addresses, change outputs
  are now always sent to addresses controlled by the wallet's internal spending keys, as
  described in [ZIP 316](https://zips.z.cash/zip-0316#deriving-internal-keys).  These
  addresses are not returned by any RPC API, as they are intended to never be shared with
  any third party, and are for wallet-internal use only. This change improves the privacy
  properties that may be maintained when sharing a unified internal viewing key for an
  account in the wallet.
- In cases where `z_sendmany` might produce transparent change UTXOs, those UTXOs are
  sent to addresses derived from the wallet's mnemonic seed via the BIP 44 `change`
  derivation path.

RPC Deprecations
----------------

- `z_getnewaddress` has been deprecated in favor of `z_getnewaccount` and
  `z_getaddressforaccount`.
- `z_listaddresses` has been deprecated. Use `listaddresses` instead.
- `z_getbalance` has been deprecated. Use `z_getbalanceforviewingkey` instead.
  See the discussion of how change is now handled under the `Wallet` heading for
  additional background.
- `z_gettotalbalance` has been deprecated. Use `z_getbalanceforaccount` instead.
- `dumpwallet` has been deprecated. Use `z_exportwallet` instead.

Build System
------------

- Clang has been updated to use LLVM 13.0.1.
- libc++ has been updated to use LLVM 13.0.1, except on Windows where it uses 13.0.0-3.
- The Rust toolchain dependency has been updated to version 1.59.0.

Platform Support
----------------

- Debian 9 has been removed from the list of supported platforms.
- Debian 11 (Bullseye) has been added to the list of supported platforms.
- A build issue (a missing header file) has been fixed for macOS targets.
- On Arch Linux only, a copy of Debian's libtinfo5_6.0 is used to fix a build
  regression.

Mining
------

- Mining to Orchard recipients is now supported on testnet. 
- It is now possible to mine to a Sapling receiver of a unified address.
- Concurrency bugs related to `getblocktemplate` have been fixed via backports from
  Bitcoin Core.

Licenses
--------

- License information in `contrib/debian/copyright` has been updated to be more accurate.

Changelog
=========

Alfredo Garcia (3):
      change error message
      fix grammar in message
      remove final dot from error msg

Charlie O'Keefe (2):
      Update base image used by Dockerfile from debian 10 to debian 11
      Remove stretch (debian 9), add bullseye (debian 11) in gitian descriptor

Conrado Gouvea (2):
      Add comment to zcash_script_new_precomputed_tx about references to input buffers
      Add zcash_script V5 functions

Daira Hopwood (42):
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
      zcashd-wallet-tool: add man page generation. fixes #5729
      zcashd-wallet-tool: warn that wallet.dat needs to be backed up. fixes #5704
      zcashd-wallet-tool: adjust man page to correct copyright information.
      Start with an empty banlist if -reindex is set. fixes #5739
      contrib/debian/copyright: add license entries for Libtool macros.
      contrib/debian/copyright: minor corrections and formatting
      contrib/debian/copyright: add licenses of Rust dependencies that do not have Expat/MIT as an option.
      contrib/debian/copyright: add license for Apache-2.0.
      Switch Jack Grigg's copyright on src/rust/{include/tracing.h, src/tracing_ffi.rs} to the Zcash developers (with his permission).
      Repair `feature_zip239` RPC test by checking the debug log of node 0 rather than its stderr.
      qa/zcash/updatecheck.py: print status code and response of failed http requests.
      Postpone native_clang and libcxx 14.0.0.
      Fix an incorrect preprocessor symbol. Also repair the lint-includes-guards.sh script that was checking for the incorrect symbol.
      .gitignore: add files temporarily created by autoconf.
      This check done for Sprout and Sapling (which is separate from consensus nullifier checks) was not being done for Orchard.
      AcceptToMemoryPool: Remove initial nullifier checks for Sprout and Sapling (within the mempool only), that are redundant with checks in HaveShieldedRequirements. The latter take into account nullifiers in the chain (using CCoinsViewMemPool::GetNullifier) and include Orchard.
      Revert "rpc: Reload CNode spans after reloading the log filter". fixes #5863

Dimitris Apostolou (2):
      Fix typos
      Fix typos

Jack Grigg (111):
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
      Move shielded signature checks out of `ContextualCheckTransaction`
      Make `PrecomputedTransactionData` a required argument of `SignatureHash`
      Update ZIP 244 test vectors
      Add Orchard recipient support to the transaction builder
      Update ZIP 244 implementation
      Enforce ZIP 244 consensus rules on sighash type
      Make `TransactionBuilder::AddOrchardOutput` memo optional
      Enforce length constraints on allPrevOutputs
      ZIP 244: Address review comments
      Return UFVK from `CWallet::GenerateNewUnifiedSpendingKey`
      Rename to `ZcashdUnifiedSpendingKey::GetSaplingKey` for consistency
      Trial-decrypt candidate Sapling receivers with all wallet UFVKs
      Add mappings from Orchard receivers to IVKs to the wallet
      Add Orchard to default UA receiver types
      Fix semantic merge conflicts
      Select Orchard receivers preferentially from UAs
      Regenerate `TxDigests` after rebuilding tx with Orchard bundle
      Migrate to `orchard` crate revision with circuit changes
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
      libzcash_script: Add more granular errors to the new APIs
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
      make-release.py: Updated release notes and changelog for 4.7.0-rc1.
      Migrate to latest `zcash/librustzcash` revision
      Fix logical merge conflicts after merging 4.7.0-rc1
      builder: Use correct `PrecomputedTransactionData` for transparent sigs
      Set NU5 protocol version for regtest to 170040, bump protocol version
      qa: Remove hard-coded consensus branch IDs from RPC tests
      qa: Remove pinned action indices from wallet_listreceived
      Test Orchard shielded coinbase rules
      test: Print all logged errors to stdout during gtests
      builder: Handle `std::ios_base::failure` exceptions during sighash
      test: Improve gtest handling of `TransactionBuilderResult::GetTxOrThrow`
      rust: Improve `PrecomputedTransactionData` construction errors
      test: Fix `WalletTests.GetConflictedOrchardNotes` gtest
      qa: Add RPC test testing Orchard note position persistence
      wallet: Move Orchard note position data into a separate map
      wallet: Persist Orchard note positions with the witness tree
      wallet: Add version information to Orchard commitment tree data
      wallet: Persist Orchard tx height alongside note positions
      qa: Add test for Orchard support in `z_listunspent`
      wallet: Treat `mnemonichdchain` records as key material
      wallet: Initialise ThreadNotifyWallets with wallet's best block
      Apply `HaveShieldedRequirements` to coinbase transactions
      mempool: Remove duplicated anchor and nullifier assertions
      AcceptToMemoryPool: Re-add missing code comment
      wallet: Rename `CWallet::GetBestBlock` to `GetPersistedBestBlock`
      Add trace-level logging to the Orchard wallet
      wallet: Add assertions during Orchard wallet bundle appends
      wallet: Remove duplicate nullifier insertion into Orchard wallet
      lint: Add check that every Cargo patch has a matching replacement
      qa: Postpone update to Rust 1.60
      wallet: Bump a trace log message to error in `Wallet::checkpoint`
      rpc: Document that enabling trace-level logging is unsafe
      test: Capture gtest log output and only print on error
      qa: Update Berkeley DB release listener regex
      cargo update
      depends: Remove `hyper 0.14.2` pin
      depends: Remove direct `tokio` dependency

John Newbery (1):
      Log calls to getblocktemplate

Jonas Schnelli (1):
      [Wallet] add HD xpriv to dumpwallet

Kris Nuttycombe (303):
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
      Restore legacy default Sapling addresses to the keystore.
      Assert that pindexStart cannot be null in CWallet::ScanForWalletTransactions
      Rename unifiedaddress->address in z_getaddressforaccount results
      Add undocumented components to getinfo API.
      Improve consistency of RPC parameter and result attribute naming.
      Correctly return p2pkh/p2sh labels for z_listunifiedreceivers
      Add test for rollback of an Orchard spend.
      Add missing filter to ensure only fully-transparent transactions end up in mapOrphans.
      Apply suggestions from code review
      Improve error message for when a UA is only usable after a future NU.
      Improve error logging in walletdb.cpp
      Add an RPC test that attempts a double-spend.
      Fix overzealous matching of Orchard FVKs to addresses.
      Guard against invalid coercion of int to u32 in FindSpendableInputs
      Apply suggestions from code review
      Ensure that legacy imported addresses are properly restored to the wallet.
      Add a test verifying the default addr is added when importing a Sapling key.
      Update release notes to include all RPC changes since 4.6.0
      Note that Debian 9 has been removed from the list of supported platforms.
      Add independent wallet persistence tests.
      Apply suggestions from code review
      Add Orchard support to z_listunspent.
      Update incrementalmerkletree version to fix GC bug & use updated API.
      Add account ID to z_listunspent results.
      Add a test demonstrating that z_listaddresses reveals internal addrs.
      Do not display internal addresses in z_listaddresses.
      Adds a test demonstrating an Orchard wallet initialization bug.
      Refactor ChainTip to take a struct of Merkle trees instead of a pair.
      Fix a bug in initialization of the Orchard wallet after NU5 activation.
      Improve error output from OrchardWallet::get_spend_info
      Update test to verify rewind behavior.
      Omit check of Orchard commitment root after rewind past first checkpoint.
      Only check nWitnessCacheSize on rewind if we've ever witnessed a Sprout or Sapling note.
      Add a test demonstrating that change may cross the pool boundary without permission.
      Add logging to the miner to provide more detail on tx selection.
      Check privacy strategy when setting allowed change types.
      Update to orchard-0.1.0-beta.3, incrementalmerkletree 0.3.0-beta.2, secp256k1-0.21
      Make all privacy policy checks after note selection.
      Add 'AllowRevealedAmounts' in tests where it is now necessary.
      Return MAX_PRIORITY when transactions contain an Orchard bundle.
      Use DEFAULT_FEE consistently in wallet_unified_change test
      Never consider transactions that pay the default fee to be free.
      Fix assertion in wallet initialization when wallet best block is ahead of the main chain.
      Apply suggestions from code review
      Apply suggestions from code review
      Fix error in z_listaccounts help text.
      Ensure pindexPrev is not null before mining against it.
      Make Orchard `finalState` serialization format match Sapling.
      Fix missing null initialization of `pindexLastTip` pointer in ThreadStartWalletNotifier
      Fix uninitialized sleep variable.
      Don't advance the wallet init timer until past reindex.
      Defensively check for a null pindex in `FindFork`
      Set NU5 testnet reactivation height.
      Update protocol version to 170050 for v4.7.0 release.
      Update the release notes to describe consensus changes since v4.6.0
      Apply suggestions from code review
      Push back NU5 testnet release height to 1842420
      qa: postpone native_clang, libcxx, boost, and native_b2 upgrades before 4.7.0 release
      make-release.py: Versioning changes for 4.7.0.
      make-release.py: Updated manpages for 4.7.0.

Larry Ruane (13):
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
      Add regtest documentation to book

Marius Kjærstad (1):
      Update copyright year to 2022

Marshall Gaucher (1):
      Update Dockerfile

Pieter Wuille (2):
      Fix csBestBlock/cvBlockChange waiting in rpc/mining
      Modernize best block mutex/cv/hash variable naming

Sean Bowe (12):
      wallet: consolidate unified key/address/account map reconstruction
      wallet: restore Orchard secret keys from mnemonic seed
      wallet: restore orchard address to IVK mappings during wallet loading
      wallet: rather than assert, error in case of inconsistency between FVK and address
      wallet: add logging for failure cases in unified cache loading
      AddBogusOrchardSpends
      Add test that checks if a bundle containing duplicate Orchard nullifiers is rejected.
      Hardcode transaction data for DuplicateOrchardNullifier test.
      Revert "AddBogusOrchardSpends"
      Activate NU5 at start of DuplicateOrchardNullifier test.
      Rename constant in include file to avoid conflicts with btest
      Fix logic for deciding whether to perform Orchard updates during rescan

Steven Smith (9):
      Lock cs_main prior to calling blockToJSON
      Mark z_gettotalbalance and dumpwallet as deprecated
      Add Orchard support to the z_gettreestate RPC
      Update transaction size estimation to include V5 transactions
      Extend uniqueness check in z_sendmany to UA receivers
      Load previously persisted sent transaction recipient metadata back into the wallet.
      Add Orchard & unified address support to z_viewtransaction.
      Ensure z_viewtransaction returns Orchard details
      Remove the fExperimentalOrchardWallet flag and related logic

Taylor Hornby (2):
      Untested, not working yet, use libtinfo from the debian packages
      Only send exceptions to the log, not stderr

sasha (21):
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
      Add missing wallet/orchard.h to src/Makefile.am
      Add missing gtest/test_transaction_builder.h to Makefile.gtest.include
      Prevent a pindex == NULL case in the ScanForWalletTransactions traversal
      Disallow testnet peers with a protocol version older than 170040
      Fix test_wallet_zkeys failures by increasing diversifier search offset
      Fix sporadic failures in StoreAndRetrieveMnemonicSeedInEncryptedStore
      Fix the exception message for SetSeedFromMnemonic failure
      add walletrequirebackup=false to the zcash argument list in smoke_tests.py
      fix smoke_tests.py accounting math to remove the warning before test 4r

Jack Grigg (1):
      Apply suggestions from code review

Ying Tong Lai (31):
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
      test_wallet: Test GetConflictedOrchardNotes.
      Fix GetConflictedOrchardNotes test.
      Introduce OrchardWallet::GetPotentialSpendsFromNullifier method.
      Update FFI to use scoped APIs for viewing keys and addresses
      Introduce CWallet::HaveOrchardSpendingKeyForAddress.
      Introduce push_orchard_result in z_listreceivedbyaddress.
      IsNoteSaplingChange: Inline internal recipient check.
      Correctly derive UAs for unknown Orchard receivers.
      Add z_listaccounts RPC.
      Inline z_listaccounts check in wallet_accounts.py
      Use height of latest network upgrade in -mineraddress validation.
      Fix >= bound when iterating over network upgrades.
      Check for overflow in IncrementAccountCounter().

ying tong (3):
      Apply docfixes from code review
      Style improvements in RPC tests.
      orchard_wallet_get_potential_spends: Do not use uint256 type.

Zancas Wilcox (4):
      blake2b/s is integrated into hashlib, drop external python package dependency
      update doctest in gtest suite to prefer hashlib
      enforce usage of the get_tests comptool interface as ComparisonTestFramework method
      All implementations of ComparisonTestFramework were overriding num_nodes

