Notable changes
===============

Blossom network upgrade
-----------------------

The mainnet activation of the Blossom network upgrade is supported by this
release, with an activation height of 653600, which should occur in early
December — roughly one day following the targeted EOS halt of our 2.0.7-3
release. Please upgrade to this release, or any subsequent release, in order to
follow the Blossom network upgrade.

The Blossom network upgrade implements
[ZIP208](https://github.com/zcash/zips/blob/master/zip-0208.rst) which shortens
block times from 150s to 75s.

DoS Mitigation: Mempool Size Limit and Random Drop
--------------------------------------------------

This release adds a mechanism for preventing nodes from running out of memory
in the situation where an attacker is trying to overwhelm the network with
transactions. This is achieved by keeping track of and limiting the total
`cost` and `evictionWeight` of all transactions in the mempool. The `cost` of a
transaction is determined by its size in bytes, and its `evictionWeight` is a
function of the transaction's `cost` and its fee. The maximum total cost is 
configurable via the parameter `mempooltxcostlimit` which defaults to
80,000,000 (up to 20,000 txs). If a node's total mempool `cost` exceeds this
limit the node will evict a random transaction, preferentially picking larger
transactions and ones with below the standard fee. To prevent a node from
re-accepting evicted transactions, it keeps track of ones that it has evicted
recently. By default, a transaction will be considered recently evicted for 60
minutes, but this can be configured with the parameter
`mempoolevictionmemoryminutes`.

For full details see ZIP 401.

Asynchronous Operations Incorrectly Reporting Success
-----------------------------------------------------
We fixed an issue where asynchronous operations were sometimes reporting sucess
when they had actually failed. One way this could occur was when trying to use
`z_sendmany` to create a transaction spending coinbase funds in a way where
change would be generated (not a valid use of `z_sendmany`). In this case the
operation would erroneously report success, and the only way to see that the 
transaction had actually failed was to look in the `debug.log` file. Such
operations will now correctly report that they have failed.

Fake chain detection during initial block download
--------------------------------------------------

One of the mechanisms that `zcashd` uses to detect whether it is in "initial
block download" (IBD) mode is to compare the active chain's cumulative work
against a hard-coded "minimum chain work" value. This mechanism (inherited from
Bitcoin Core) means that once a node exits IBD mode, it is either on the main
chain, or a fake alternate chain with similar amounts of work. In the latter
case, the node has most likely become the victim of a 50% + 1 adversary.

Starting from this release, `zcashd` additionally hard-codes the block hashes
for the activation blocks of each past network upgrade (NU). During initial
chain synchronization, and after the active chain has reached "minimum chain
work", the node checks the blocks at each NU activation height against the
hard-coded hashes. If any of them do not match, the node will immediately alert
the user and **shut down for safety**.

Disabling old Sprout proofs
---------------------------

As part of our ongoing work to clean up the codebase and minimise the security
surface of `zcashd`, we are removing `libsnark` from the codebase, and dropping
support for creating and verifying old Sprout proofs. Funds stored in Sprout
addresses are not affected, as they are spent using the hybrid Sprout circuit
(built using `bellman`) that was deployed during the Sapling network upgrade.

This change has several implications:

- `zcashd` no longer verifies old Sprout proofs, and will instead assume they
  are valid. This has a minor implication for nodes: during initial block
  download, an adversary could feed the node fake blocks containing invalid old
  Sprout proofs, and the node would accept the fake chain as valid. However,
  as soon as the active chain contains at least as much work as the hard-coded
  "minimum chain work" value, the node will detect this situation and shut down.

- Shielded transactions can no longer be created before Sapling has activated.
  This does not affect Zcash itself, but will affect downstream codebases that
  have not yet activated Sapling (or that start a new chain after this point and
  do not activate Sapling from launch). Note that the old Sprout circuit is
  [vulnerable to counterfeiting](https://z.cash/support/security/announcements/security-announcement-2019-02-05-cve-2019-7167/)
  and should not be used in current deployments.

- Starting from this release, the circuit parameters from the original Sprout
  MPC are no longer required to start `zcashd`, and will not be downloaded by
  `fetch-params.sh`. They are not being automatically deleted at this time.

We would like to take a moment to thank the `libsnark` authors and contributors.
It was vital to the success of Zcash, and the development of zero-knowledge
proofs in general, to have this code available and usable.

Changelog
=========

Bryant Eisenbach (2):
      doc: Change Debian package description
      doc: Move text prior to "This package provides..."

Daira Hopwood (4):
      Remove copyright entries for Autoconf macros that have been deleted.
      Remove copyright entry for libsnark.
      Test setting an expiry height of 0.
      Fix setting an expiry height of 0. fixes #4132

Dimitris Apostolou (1):
      depends macOS: point --sysroot to SDK

Eirik Ogilvie-Wigley (41):
      Wrap metrics message in strprintf
      DoS protection: Weighted random drop of txs if mempool full
      Rebuild weighted list on removal and fix size calculation
      Grammatical fixes
      Remove transactions when ensuring size limit
      Help message cleanup and add lock
      Performance: Store weighted transactions in a tree
      Fix naming conventions
      No need to activate Overwinter/Sapling in rpc test
      Fix recently evicted list size
      Put size increment and decrement on their own lines
      Prevent adding duplicate transactions
      Move duplicate macro to reusable location
      mempool_limit rpc test cleanup
      Represent tx costs as signed integers
      Fix comments
      Rename variables for consistency
      Use tx cost rather than evictionWeight when checking mempool capacity
      Log rather than return error if a transaction is recently evicted
      Represent recently evicted list as a deque
      Rename files
      Update release notes
      Add test
      minor rpc test clean up
      Add explanatory comments
      Wording and grammatical fixes
      Update parameter names to match ZIP
      Clarify the difference between cost and evictionWeight
      Fix test cases: default mempool limiters
      Remove dots and underscores from parameter names
      Use same type when calling max
      make-release.py: Versioning changes for 2.1.0-rc1.
      make-release.py: Updated manpages for 2.1.0-rc1.
      make-release.py: Updated release notes and changelog for 2.1.0-rc1.
      Release notes for bug fix
      Better wording in release notes
      Fix wording
      Fix getblocksubsidy tests
      fix comment
      make-release.py: Versioning changes for 2.1.0.
      make-release.py: Updated manpages for 2.1.0.

Gareth Davies (1):
      Updating IPFS link for chunking

Jack Grigg (39):
      depends: Add FreeBSD to hosts and builders
      depends: Explicitly set Boost toolchain during configuration
      depends: Add FreeBSD support to OpenSSL
      depends: Patch libevent to detect arch4random_addrandom
      depends: Add FreeBSD Rust binaries
      depends: Explicitly call Rust install script using bash
      depends: Use project-config.jam to configure Boost instead of user-config.jam
      depends: Set PIC flags for FreeBSD
      Always skip verification for old Sprout proofs
      Remove ability to create non-Groth16 Sprout JSDescriptions
      Use Sapling JSDescriptions in Boost tests
      Remove non-Groth16 Sprout proofs from joinsplit gtests
      Migrate test utilities to generate Sapling-type Sprout proofs
      Use Sapling JSDescriptions in gtests
      Revert "Allow user to ask server to save the Sprout R1CS out during startup."
      Remove libsnark code for pre-Sapling Sprout proofs
      Remove pre-Sapling Sprout circuit
      Revert "configure: Guess -march for libsnark OPTFLAGS instead of hard-coding"
      Revert "Check if OpenMP is available before using it"
      Remove libsnark from build system
      Remove libsnark
      Remove libgmp
      Remove libsnark unit tests from full test suite
      test: Require minimum of Sapling for all RPC tests
      test: Add Sapling v4 transactions to mininode framework
      test: Add hashFinalSaplingProxy to create_block
      test: Update RPC tests to use a minimum of Sapling
      rpc: Use Sapling transactions in zc_raw_joinsplit
      depends: Fix crate vendoring path
      depends: Helper for vendoring new crates
      depends: Add flag for building with a local librustzcash repo
      tests: Clean up use of repr() in mininode
      Remove makeGrothProof argument from JoinSplit::prove
      Stop fetching old Sprout parameters
      Add libsnark removal to notable changes
      Move AbortNode to the top of main.cpp
      Abort node if NU activations have unexpected hashes
      Add block hashes for Overwinter, Sapling, and testnet Blossom
      Update release notes with node abort behaviour

Larry Ruane (5):
      insightexplorer: formatting, pyflakes cleanups
      precompute empty merkle roots
      update unit tests to compute empty roots
      access array element using at()
      #4175 z_sendmany coinbase to zaddr with change: report error status

Sean Bowe (1):
      Set mainnet activation of Blossom

jeff-liang (1):
      Display which network the node is running on.

Benjamin Winston (1):
      Removed stale seeder, fixing #4153

