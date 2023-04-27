Notable changes
===============

RPC Changes
-----------

- `getdeprecationinfo` has several changes:
  - It now returns additional information about currently deprecated and
    disabled features.
  - A new `end_of_service` object that contains both the block height for
    end-of-service and the estimated time that the end-of-service halt is
    expected to occur. Note that this height is just an approximation and
    will change over time as the end-of-service block height approaches,
    due to the variability in block times. The
    `end_of_service` object is intended to replace the `deprecationheight`
    field; see the [Deprecations](#deprecations) section for additional detail.
  - This RPC method was previously only available for mainnet nodes; it is now
    also available for testnet and regtest nodes, in which case it does not
    return end-of-service halt information (as testnet and regtest nodes do not
    have an end-of-service halt feature.)
- Several `z_sendmany`, `z_shieldcoinbase` and `z_mergetoaddress` failures have
  moved from synchronous to asynchronous, so while you should already be
  checking the async operation status, there are now more cases that may trigger
  failure at that stage.
- The `AllowRevealedRecipients` privacy policy is now required in order to choose a
  transparent change address for a transaction. This will only occur when the wallet
  is unable to construct the transaction without selecting funds from the transparent
  pool, so the impact of this change is that for such transactions, the user must specify
  `AllowFullyTransparent`.
- The `z_shieldcoinbase` RPC method now supports an optional memo.
- The `z_shieldcoinbase` and `z_mergetoaddress` RPC methods now support an
  optional privacy policy.
- The `z_mergetoaddress` RPC method can now merge _to_ UAs and can also send
  between different shielded pools (when `AllowRevealedAmounts` is specified).
- The `estimatepriority` RPC call has been removed.
- The `priority_delta` argument to the `prioritisetransaction` RPC call now has
  no effect and must be set to a dummy value (0 or null).

Changes to Transaction Fee Selection
------------------------------------

- The zcashd wallet now uses the conventional transaction fee calculated according
  to [ZIP 317](https://zips.z.cash/zip-0317) by default. This conventional fee
  will be used unless a fee is explicitly specified in an RPC call, or for the
  wallet's legacy transaction creation APIs (`sendtoaddress`, `sendmany`, and
  `fundrawtransaction`) when the `-paytxfee` option is set.
- The `-mintxfee` and `-sendfreetransactions` options have been removed. These
  options previously instructed the legacy transaction creation APIs to increase
  fees to this limit and to use a zero fee for "small" transactions that spend
  "old" inputs, respectively. They will now cause a warning on node startup if
  used.


Changes to Block Template Construction
--------------------------------------

We now use a new block template construction algorithm documented in
[ZIP 317](https://zips.z.cash/zip-0317#recommended-algorithm-for-block-template-construction).

- This algorithm no longer favours transactions that were previously considered
  "high priority" because they spent older inputs. The `-blockprioritysize` config
  option, which configured the portion of the block reserved for these transactions,
  has been removed and will now cause a warning if used.
- The `-blockminsize` option, which configured the size of a portion of the block
  to be filled regardless of transaction fees or priority, has also been removed
  and will cause a warning if used.
- A `-blockunpaidactionlimit` option has been added to control the limit on
  "unpaid actions" that will be accepted in a block for transactions paying less
  than the ZIP 317 fee. This defaults to 50.

Change to Transaction Relay Policy
----------------------------------

The allowance for "free transactions" in mempool acceptance and relay has been
removed. All transactions must pay at least the minimum relay threshold, currently
100 zatoshis per 1000 bytes up to a maximum of 1000 zatoshis, in order to be
accepted and relayed. (Individual nodes can change this using `-minrelaytxfee`
but in practice the network default needs to be adhered to.) This policy is under
review and [might be made stricter](https://zips.z.cash/zip-0317#transaction-relaying);
if that happens then the ZIP 317 conventional fee will still be sufficient for
mempool acceptance and relay.

Removal of Priority Estimation
------------------------------

Estimation of "priority" needed for a transaction to be included within a target
number of blocks, and the associated `estimatepriority` RPC call, have been
removed. The format for `fee_estimates.dat` has also changed to no longer save
these priority estimates. It will automatically be converted to the new format
which is not readable by prior versions of the software. The `-txconfirmtarget`
config option is now obsolete and has also been removed. It will cause a
warning if used.

[Deprecations](https://zcash.github.io/zcash/user/deprecation.html)
--------------

The following features have been deprecated, but remain available by default.
These features may be disabled by setting `-allowdeprecated=none`. 18 weeks
after this release, these features will be disabled by default and the following
flags to `-allowdeprecated` will be required to permit their continued use:

- `deprecationinfo_deprecationheight`: The `deprecationheight` field of
  `getdeprecationinfo` has been deprecated and replaced by the `end_of_service`
  object.

Changelog
=========

ANISH M (3):
      use SOURCES_PATH instead of local git DEPENDS_SOURCES_DIR
      report the git-derived version in metrics screen
      Update src/metrics.cpp by removing v prefix.

Alex Morcos (14):
      Refactor CreateNewBlock to be a method of the BlockAssembler class
      FIX: Account for txs already added to block in addPriorityTxs
      FIX: correctly measure size of priority block
      [rpc] Remove estimatepriority.
      [mining] Remove -blockprioritysize.
      [debug] Change -printpriority option
      [cleanup] Remove estimatePriority
      [rpc] sendrawtransaction no longer bypasses minRelayTxFee
      [test] Remove priority from tests
      [rpc] Remove priority information from mempool RPC calls
      [rpc] Remove priorityDelta from prioritisetransaction
      [cleanup] Remove coin age priority completely.
      Allow setting minrelaytxfee to 0
      Update example zcash.conf

Charlie O'Keefe (1):
      Add reference in Makefile.am to zip317.h

Daira Emma Hopwood (38):
      Remove unnecessary #include.
      Adjust indentation to be consistent without changing existing code.
      Repair show_help RPC test.
      Fix bit-rotted code in miner tests.
      Implement ZIP 317 computations.
      `cargo update`
      Add audits for updates to futures-* 0.3.28 and redjubjub 0.7.0.
      Add `examine`, a wrapper around `std::visit` that reverses the arguments.
      Use the new `examine` macro to replace all instances of `std::visit(match {...}, specimen)`, improving code readability.
      Refactoring to avoid duplicated code.
      Refactoring to avoid an unnecessary temporary.
      Refactor that avoids using exceptions for local flow control and is simpler.
      Correct the documentation of `-rpcconnect` in the example `zcash.conf`. `-rpcconnect` is only used by `zcash-cli`.
      Change ZIP 401 mempool limiting to use conventional fee.
      Change ZIP 401 mempool limiting to use constants decided in zcash/zips#565.
      Warn on node startup if the removed `-blockprioritysize` option is set to a non-zero value.
      Log (at the mempool DEBUG level) when a transaction cannot be accepted to the mempool because its modified fee is below the minimum relay fee.
      Fix the dust threshold rate to three times 100 zats/1000 bytes. (We express it that way rather than 300 zats/1000 bytes, because the threshold is always rounded to an integer and then multiplied by 3.)
      Fix some messages, comments, and documentation that: * used "fee" to mean "fee rate", "kB" to mean 1000 bytes, "satoshis"   to mean zatoshis, or that incorrectly used "BTC" in place of "ZEC"; * used obsolete concepts such as "zero fee" or "free transaction"; or * did not accurately document their applicability.
      Fix tests that failed due to the minimum relay fee being enforced.
      Fix miner_tests btest.
      Fix mempool_packages and prioritisetransaction RPC tests.
      Implement `GetUnpaidActionCount` and `GetWeightRatio` for ZIP 317.
      ZIP 317 block construction algorithm. This breaks tests which are repaired in subsequent commits.
      Add assertions that `GetRandInt*` functions are called with `nMax >= 0`. All existing uses have been checked to ensure they are consistent with this assertion.
      Repair `miner_tests.py` btest.
      Repair some RPC tests.
      mergetoaddress_helper.py: Use `generate_and_check` helper to mine a block and make sure that it contains the expected number of transactions.
      mergetoaddress_helper.py: use Decimal for amounts and integers otherwise.
      Remove the implementation of score-based block template construction and the `-blockminsize` option.
      Add a `-blockunpaidactionlimit` config option to configure the per-block limit on unpaid actions for ZIP 317 block template construction.
      miner_tests.cpp improvements.
      random.h documentation improvements.
      Fix/suppress clippy warnings.
      Improve the `show_help.py` RPC test to include `-help-debug` (needed to test the help change in the next commit).
      Improve `-printpriority` output to log the modified fee, conventional fee, size, logical action count, and unpaid action count. This reflects the changes to use the ZIP 317 block construction algorithm and de-emphasise fee rate.
      Fix a build regression if `--disable-mining` is selected.
      Fix a build regression if both `--disable-mining` and `--disable-wallet` are selected.

Daira Hopwood (10):
      Use a more recent URL format for GitHub release archives.
      Clarify that patches to a dependency are under the same license as that dependency.
      Update copyright date and email for tl_expected.
      Use the same convention for the tl_expected download files as for native_cctools
      Refactoring to split the weighted tx tree out of mempool_limit.{cpp,h} and make it more reusable.
      Minor optimization to weighted_map::remove
      This PR doesn't bring in any ZIP 317 changes yet
      Another minor optimization
      Change spelling of prioritisation in an error message
      Correction "height" -> "time" in release notes

DeckerSU (1):
      InsertBlockIndex: pass const reference on hash, instead of hash

Dimitris Apostolou (3):
      Fix typo
      Update documentation link
      Fix typos

Evan Klitzke (1):
      Fix automake warnings when running autogen.sh

Greg Pfeil (89):
      Show in-progress tests when rpc-tests is interrupted
      Make extra newline more explicit
      Apply suggestions from code review
      Make pool selection order more flexible
      Simplify diversifier_index_t handling
      Update tests for async z_sendmany
      Limit UTXOs
      Some orchard fixes for wallet_tx_builder
      Return anchorHeight from ResolveInputsAndPayments
      Refactoring InsufficientFundsError
      Ensure we don’t make Orchard change pre-NU5
      Test updates for z_sendmany WalletTxBuilder changes
      Fix weakened privacy policy for transparent change
      Fix some overly-strict privacy policies in btest
      Apply suggestions from code review
      Unify requireTransparentCoinbase handling
      Rename `Get*Balance` to `Get*Total`
      Remove changes that aren’t needed by z_sendmany
      Improve GetRequiredPrivacyPolicy
      Add release notes for (a)sync z_sendmany changes
      Assert that we get a change addr for any selector
      Don’t pass PrivacyPolicy to selector constructor
      Address comments on WalletTxBuilder introduction
      Make RPC test output more deterministic
      Update WalletTxBuilder based on review
      Clarify `AddressResolutionError`
      Don’t permit user-provided “internal” payments
      Address WalletTxBuilder PR feedback
      Ensure that a WalletTxBuilder tx balances
      Additional z_sendmany test cases
      Address WalletTxBuilder review feedback
      Apply suggestions for WalletTxBuilder from code review
      Simplify SelectOVKs
      Have GetRecipientPools return a copy
      Remove CWallet member from WalletTxBuilder
      Improve taddr no-memo check
      Update src/wallet/rpcwallet.cpp
      Lock notes (except Orchard) in wallet_tx_builder
      Improve Doxygen for note locking
      Require `AllowRevealedRecipients` for t-change
      Update release-notes for transparent change restriction
      Correct EditorConfig for Makefiles
      Split C++ generated from Rust into own lib
      Simplify canResolveOrchard logic
      Support ZIP 317 fees in the zcashd wallet
      Correct change handling for ZIP 317 fees
      Address review feedback re: ZIP 317 in wallet
      Revert "Add `AllowRevealedSenders` to fix `mempool_nu_activation.py`"
      Eliminate LegacyCompat–ZTXOSelector cycle
      Simplify client.cpp
      Enrich zcash-cli arg conversion
      Better messages on client-side zcash-cli errors
      Fix accidental reversion of #6409
      Remove unnecessary explicit privacy policy
      Use examine instead of std::visit
      Simplify some vector initialization
      Fix edge case revealed by #6409
      Rename Arg* to Param
      Adjust wallet absurd fee check for ZIP 317
      Address review feedback for ZIP 317 fees in wallet
      Address more ZIP 317 fee feedback
      Fix zcash-cli crash when printing help message
      Use null as the ZIP 317 fee sentinel instead of -1
      Add z_sendmany RPC examples with fee field
      Have z_shieldcoinbase use WalletTxBuilder
      Address review feedback on z_shieldcoinbase
      Fix an incorrect error message in a test
      Minor improvements to z_shieldcoinbase
      Support ZIP 317 fees for legacy wallet operations
      Remove `-mintxfee` config option
      Address review comments re: legacy wallet ZIP 317
      Remove `-txconfirmtarget` config option
      Restore previous `-maxtxfee` bound
      Correct -maxtxfee lower bound diagnostic messages
      WalletTxBuilder support for net payments
      fixup! WalletTxBuilder support for net payments
      Don’t test “Insufficient funds” for `z_shieldcoinbase`
      Update z_mergetoaddress to use WalletTxBuilder
      Many z_mergetoaddress updates
      Avoid extra copy in ResolveInputsAndPayments
      Address z_mergetoaddress review feedback
      Allow explicit “no memo” in z_mergetoaddress
      Add `memo` parameter to `z_shieldcoinbase`
      Support UA destinations in `z_mergetoaddress`
      Include Orchard dest in z_mergetoaddress estimation
      Update release notes
      Support privacyPolicy parameters in zcash-cli
      Treat "F6" in RPC calls as if no memo were provided
      Support nullable strings in `zcash-cli`

Jack Grigg (92):
      rust: Add `cxx` version of `RustStream`
      rust: Migrate `OrchardMerkleFrontier` to `cxx`
      CreateNewBlock: Leave more space for Orchard shielded coinbase
      Retroactively enable ZIP 216 before NU5 activation
      rust: Compile with ThinLTO
      depends: Update Rust to 1.67.1
      depends: Update Clang / libcxx to LLVM 15.0.6
      Fix 1.67.1 clippy lints
      depends: Evaluate `native_packages` before `packages`
      qa: Fix year in postponement lines
      qa: Fix `google/leveldb` tag parsing in `updatecheck.py`
      qa: Handle commit IDs correctly to `updatecheck.py`
      depends: `cxx 1.0.91`
      depends: `native_zstd 1.5.4`
      `cargo vet regenerate imports`
      qa: Import Rust crate audits from ISRG
      `cargo update`
      qa: Postpone LevelDB 1.23
      book: Add page with release support details and EoS halt heights
      Update release support book page in release process
      depends: Update Rust to 1.68.0
      qa: Replace Firefox audits with aggregated Mozilla audits in registry
      qa: Import Rust crate audits from ChromeOS
      Move `fEnableAddrTypeField` outside `ENABLE_WALLET`
      `s/string/std::string` in `init.cpp`
      CI: Check that the PR branch has a sufficiently recent base for Tekton
      CI: Fix permissions for Checks workflow
      Add `CChainParams::RustNetwork`
      wallet: Consolidate `CWalletTx` Sapling output decryption methods
      wallet: Use `zcash_note_encryption` in `CWalletTx::DecryptSaplingNote`
      wallet: Use `CWalletTx::DecryptSaplingNote` in more places
      wallet: Use `zcash_note_encryption` in `CWallet::FindMySaplingNotes`
      wallet: Use `zcash_note_encryption` in `CWalletTx::RecoverSaplingNote`
      wallet: Remove recipient-side `SaplingNotePlaintext::decrypt`
      CI: Fetch all history for "recent base" check
      CI: Include explicit `failure()` condition in "recent base" check
      CI: Use `github.head_ref` instead of `HEAD` for "recent base" check
      book: Add End-of-Support heights for v5.3.3 and v5.4.2
      CI: Remove most usages of `actions-rs` actions
      CI: Migrate to `cargo-vet 0.5`
      cargo vet prune
      CI: Provide `write` permission for `pull-requests`
      CI: Check out both the base and PR branches for "recent base" check
      Migrate to `zcash_primitives 0.10`
      depends: `cxx 1.0.92`
      depends: CMake 3.26.0
      depends: Postpone CCache updates again
      cargo update
      Use `RandomInvalidOutputDescription()` everywhere it makes sense
      Merge most `cxx::bridge` definitions into a single bridge
      Expand `CppStream` to cover all `Stream`-like C++ types
      Migrate `OrchardMerkleFrontier` to use new `CppStream` APIs
      build: Tolerate split LLVM versions
      Use `cxx` bridge for all Orchard bundle inspection and validation
      gtest: Minor improvements to `CoinsTests`
      rust: Migrate Ed25519 FFI to `cxx`
      Tell `cargo-vet` to ignore patched dependencies
      cargo-vet: Regenerate imports
      cargo-vet: Switch to Google's aggregated audits
      More crate audits
      Migrate to published `orchard 0.4`
      qa: Fix update checker to handle `native_clang` version format
      depends: CMake 3.26.3
      depends: Rust 1.68.2
      depends: `native_zstd 1.5.5`
      depends: `cxx 1.0.94`
      qa: Postpone dependencies we aren't updating
      cargo update
      Use published `zcash_primitives 0.11` and `zcash_proofs 0.11`
      test: Avoid generating a chain fork in `mempool_packages` RPC test
      test: Sync blocks before invalidating them in `mempool_packages` RPC test
      depends: Boost 1.82.0
      qa: Postpone Clang 16.0.2
      cargo update
      depends: Rust 1.69.0
      make-release.py: Versioning changes for 5.5.0-rc1.
      make-release.py: Updated manpages for 5.5.0-rc1.
      make-release.py: Updated release notes and changelog for 5.5.0-rc1.
      make-release.py: Updated book for 5.5.0-rc1.
      CI: Add a GitHub Actions workflow that builds zcashd for platform tiers
      CI: Add caching to build workflow
      depends: Ensure `native_cxxbridge` source is fetched for `rustcxx`
      depends: Don't build BDB utilities on macOS
      depends: Remove Fortran and LLDB components from staged `native_clang`
      CI: Build with `--with-libs`, `--disable-wallet`, and `--disable-mining`
      Fix `make-release.py` to write correct halt height into book
      Update v5.5.0 release notes
      build: Fix MinGW cross-compilation with `--disable-wallet`
      make-release.py: Versioning changes for 5.5.0-rc2.
      make-release.py: Updated manpages for 5.5.0-rc2.
      make-release.py: Updated release notes and changelog for 5.5.0-rc2.
      make-release.py: Updated book for 5.5.0-rc2.

Kris Nuttycombe (33):
      Fix potential path or symlink traversal
      Add a docker-compose.yml for prometheus/grafana metrics collection.
      Apply suggestions from code review
      Make all CCoinsView methods pure-virtual.
      Remove `FakeCoinsViewDB` as it is identical to `CCoinsViewDummy`
      Postpone dependency updates.
      make-release.py: Versioning changes for 5.3.3.
      make-release.py: Updated manpages for 5.3.3.
      make-release.py: Updated release notes and changelog for 5.3.3.
      Set urgency to `high` in Debian changelog.
      Add information about deprecated features to `deprecationinfo` results.
      Apply suggestions from code review
      Add a wallet-aware transaction builder.
      Use WalletTxBuilder for z_sendmany
      Allow selectors to require transparent coinbase
      Fix a longstanding zcashd build warning
      Fix `make distclean` to recursively remove `rust/gen`
      Improve const-ness of CChainParams retrieval by network ID
      Explicitly provide CChainParams to `EnforceNodeDeprecation`
      revert broken "safe extract" functionality in golden tests.
      Refactor RPC privacyPolicy handling
      Update to use the `ff 0.13` dependency stack.
      Add `AllowRevealedSenders` to fix `mempool_nu_activation.py`
      Calculate convential fee in `CreateTransaction` before stripping sigs.
      Fix formating of money strings.
      Use the conventional fee for prioritisation in prioritisetransactions.py
      `z_sendmany` now accepts 6 parameters, not 5.
      make-release.py: Versioning changes for 5.5.0-rc3.
      make-release.py: Updated manpages for 5.5.0-rc3.
      make-release.py: Updated release notes and changelog for 5.5.0-rc3.
      make-release.py: Updated book for 5.5.0-rc3.
      make-release.py: Versioning changes for 5.5.0.
      make-release.py: Updated manpages for 5.5.0.

Luke Dashjr (1):
      RPC/Mining: Restore API compatibility for prioritisetransaction

Marco Falke (1):
      wallet: Remove sendfree

Marius Kjærstad (2):
      New checkpoint at block 2000000 for mainnet
      Update estimated number of transactions due to Blossom NU

Miodrag Popović (2):
      Fix for broken cross-build to Windows target on Ubuntu 22.04 and Debian 11
      Update depends/hosts/mingw32.mk to use posix variant library path

Sean Bowe (1):
      Add additional audits.

Suhas Daftuar (4):
      Add tags to mempool's mapTx indices
      Fix mempool limiting for PrioritiseTransaction
      Use fee deltas for determining mempool acceptance
      Remove GetMinRelayFee

TrellixVulnTeam (1):
      Adding tarfile member sanitization to extractall()

Wladimir J. van der Laan (1):
      Merge #7730: Remove priority estimation

Yasser Isa (1):
      DOWNLOAD_URL dynamic in fetch-params.sh

cronicc (1):
      Fix Horizen Security contact email

instagibbs (2):
      Gave miner test values constants for less error-prone values.
      Corrected values

sasha (13):
      Partially revert PR #6384, but only for URLs using a git commit hash
      Download `native_cctools` and its `libtapi` to meaningful filenames
      Better error messages if proof parameters aren't loaded
      Enable the (existing) custom error message for the invalid checksum case
      Re-download parameters if they already exist but don't have correct sums
      Alias Sasha->sasha in release-notes.py to avoid authors.md split
      Update `smoke_tests.py` to run against 5.5.0, using `allowdeprecated`
      Use more restrictive privacy policies in `smoke_tests.py`
      Use default values for `z_mergetoaddress` again
      Don't hardcode 0.00001 explicitly
      Change output format for `smoke_tests.py`
      `DEFAULT_FEE` -> `LEGACY_DEFAULT_FEE` in `smoke_tests.py`
      use `AllowRevealedRecipients` in `smoke_tests.py` case 4w

Jack Grigg (4):
      Improvements to code comments
      Adjust documentation
      z_mergetoaddress: Include Sapling output padding in size estimate
      test: Fix copyright years in new RPC tests

teor (2):
      Replace custom zcash_script TxInputStream with RustDataStream
      Change module comment in bridge.rs to doc comment

