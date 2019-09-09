Notable changes
===============

Shorter Block Times
-------------------
Shorter block times are coming to Zcash! In the v2.0.7 release we have implemented [ZIP208](https://github.com/zcash/zips/blob/master/zip-0208.rst) which will take effect when Blossom activates. Upon activation, the block times for Zcash will decrease from 150 seconds to 75 seconds, and the block reward will decrease accordingly. This affects at what block height halving events will occur, but should not affect the overall rate at which Zcash is mined. The total founders' reward stays the same, and the total supply of Zcash is decreased only microscopically due to rounding.

Blossom Activation on Testnet
-----------------------------
The v2.0.7 release includes Blossom activation on testnet, bringing shorter block times. The testnet Blossom activation height is 584000.

Insight Explorer
----------------
Changes needed for the Insight explorer have been incorporated into Zcash. *This is an experimental feature* and therefore is subject to change. To enable, add the `experimentalfeatures=1`, `txindex=1`, and `insightexplorer=1` flags to `zcash.conf`. This feature adds several RPCs to `zcashd` which allow the user to run an Insight explorer.

Changelog
=========

Daira Hopwood (11):
      Add MIT license to Makefiles
      Add MIT license to build-aux/m4/bitcoin_* scripts, and a permissive license to build-aux/m4/l_atomic.m4 .
      Update copyright information for Zcash, leveldb, and libsnark.
      Add license information for Autoconf macros. refs #2827
      Update contrib/debian/copyright for ax_boost_thread.m4
      Replace http with https: in links to the MIT license. Also change MIT/X11 to just MIT, since no distinction was intended.
      qa/zcash/checksec.sh is under a BSD license, with a specialized non-endorsement clause.
      Link to ticket #2827 using URL
      Release process doc: add step to set the gpg key id.
      Release process doc: mention the commit message.
      Add RPC test and test framework constants for Sapling->Blossom activation.

Dimitris Apostolou (5):
      Rename vjoinsplit to vJoinSplit
      Fix naming inconsistency
      Rename joinsplit to shielded
      Rename FindWalletTx to FindWalletTxToZap
      Fix RPC undefined behavior.

Eirik Ogilvie-Wigley (56):
      Make nextHeight required in CalculateNextWorkRequired
      Fix nondeterministic failure in sapling migration test
      Clean up and fix typo
      Apply suggestions from code review
      Shorter block times rpc test
      Update pow_tests for shorter block times
      Update test_pow for shorter block times
      Update block subsidy halving for zip208
      Make NetworkUpgradeAvailable a method of Params
      Temporarily disable test
      Simplify PartitionCheck
      Use static_assert
      Add missing new line at end of file
      pow test cleanup
      Add message to static_assert
      Update expiry height for shorter block times
      Fix zip208 founders reward calculation and update test
      PartitionCheck tests for shorter block times
      Add test for Blossom default tx expiry delta
      Update metrics block height estimation for shorter block times
      Do not create transactions that will expire after the next epoch
      Do not send migration transactions that would expire after a network upgrade
      Fix integer truncation in Blossom halving calculation
      Update main_tests for shorter block times
      Use pre-Blossom max FR height when calculating address change interval
      Make founders reward tests pass before and after Blossom activation height is set
      Extract Halvings method and add tests
      Add comments and fix typos
      Improve EstimateNetHeight calculation
      Fix check transaction tests
      Make sure to deactivate blossom in test case
      Fix parsing txexpirydelta argument
      Do not add expiring soon threshold to expiry height of txs near NU activation
      Fix/update comments
      Make sure that expiry height is not less than height
      Clarify documentation
      Update PoW related assertions
      Remove DefaultExpiryDelta method
      Algebraic improvements related to halving
      Distinguish between height and current header height on metrics screen
      Test clean up and fixes
      Add copyright info
      NPE defense in metrics screen
      Do not estimate height if there is no best header
      Rename method and use int64_t
      make-release.py: Versioning changes for 2.0.7-rc1.
      make-release.py: Updated manpages for 2.0.7-rc1.
      make-release.py: Updated release notes and changelog for 2.0.7-rc1.
      Update download path
      Set testnet Blossom activation height
      Notable changes for v2.0.7
      Enable shorter block times rpc test
      Grammatical fixes and improvements
      Remove constant
      make-release.py: Versioning changes for 2.0.7.
      make-release.py: Updated manpages for 2.0.7.

Eirik Ogilvie-Wigley (8):
      Use CommitTransaction() rather than sendrawtransaction()
      Move reused async rpc send logic to separate file
      Move reused sign and send logic
      Do not shadow the return value when testmode is true
      Inline sign_send_raw_transaction
      Allow passing optional reserve key as a parameter to SendTransaction
      Use reserve key for transparent change when sending to Sapling
      Fix comment in mergetoaddress RPC test

Jack Grigg (5):
      test: Check for change t-addr reuse in z_sendmany
      Use reserve key for transparent change when sending to Sprout
      test: Fix permissions on wallet_changeaddresses RPC test
      test: Fix pyflakes warnings
      test: Fix AuthServiceProxy closed conn detection

Larry Ruane (6):
      add addressindex related RPCs
      add spentindex RPC for bitcore block explorer
      add timestampindex related RPC getblockhashes
      fix getblockdeltas documentation formatting
      insightexplorer minor bug fixes
      insightexplorer fix LogPrintf

Luke Dashjr (2):
      Add MIT license to autogen.sh and share/genbuild.sh
      Trivial: build-aux/m4/l_atomic: Fix typo

Simon Liu (8):
      Redefine PoW functions to accept height parameter.
      Remove use of redundant member nPowTargetSpacing.
      Replace nPoWTargetSpacing -> PoWTargetSpacing()
      Update PoW function calls to pass in height.
      Update GetBlockTimeout() to take height parameter.
      Replace nPoWTargetSpacing -> PoWTargetSpacing() in ProcessMessage()
      Replace nPoWTargetSpacing -> PoWTargetSpacing() in tests
      Modify PartitionCheck to be aware of pre & post Blossom target spacing.

William M Peaster (1):
      Handful of copyedits to README.md

codetriage-readme-bot (1):
      Link to development guidelines in CONTRIBUTING.md

Jack Grigg (1):
      Update README.md

Benjamin Winston (3):
      Updated location to new download server
      Fixes 4097, improves caching on parameter downloads
      Updated location to new download server, fixing #4100

