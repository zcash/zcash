Notable changes
===============

Overwinter network upgrade
--------------------------

The activation height for the Overwinter network upgrade on mainnet is included
in this release. Overwinter will activate on mainnet at height 347500, which is
expected to be mined on the 25th of June 2018. Please upgrade to this release,
or any subsequent release, in order to follow the Overwinter network upgrade.

`-mempooltxinputlimit` deprecation
----------------------------------

The configuration option `-mempooltxinputlimit` was added in release 1.0.10 as a
short-term fix for the quadratic hashing problem inherited from Bitcoin. At the
time, transactions with many inputs were causing performance issues for miners.
Since then, several performance improvements have been merged from the Bitcoin
Core codebase that significantly reduce these issues.

The Overwinter network upgrade includes changes that solve the quadratic hashing
problem, and so `-mempooltxinputlimit` will no longer be needed - a transaction
with 1000 inputs will take just as long to validate as 10 transactions with 100
inputs each. Starting from this release, `-mempooltxinputlimit` will be enforced
before the Overwinter activation height is reached, but will be ignored once
Overwinter activates. The option will be removed entirely in a future release
after Overwinter has activated.

`NODE_BLOOM` service bit
------------------------

Support for the `NODE_BLOOM` service bit, as described in [BIP
111](https://github.com/bitcoin/bips/blob/master/bip-0111.mediawiki), has been
added to the P2P protocol code.

BIP 111 defines a service bit to allow peers to advertise that they support
Bloom filters (such as used by SPV clients) explicitly. It also bumps the protocol
version to allow peers to identify old nodes which allow Bloom filtering of the
connection despite lacking the new service bit.

In this version, it is only enforced for peers that send protocol versions
`>=170004`. For the next major version it is planned that this restriction will be
removed. It is recommended to update SPV clients to check for the `NODE_BLOOM`
service bit for nodes that report version 170004 or newer.

Changelog
=========

Brad Miller (2):
      Clean up
      Implement note locking for z_mergetoaddress

Charlie O'Keefe (1):
      Add filename and sha256 hash for windows rust package

Daira Hopwood (5):
      Squashed commit of the following:
      pyflakes cleanups to RPC tests after Overwinter PRs.
      Add support for Overwinter v3 transactions to mininode framework.
      Test that receiving an expired transaction does not increase the peer's ban score.
      Don't increase banscore if the transaction only just expired.

Daniel Kraft (1):
      trivial: use constants for db keys

Jack Grigg (47):
      Add environment variable for setting ./configure flags in zcutil/build.sh
      Add configure flags for enabling ASan/UBSan and TSan
      Split declaration and definition of SPROUT_BRANCH_ID constant
      Add link to Overwinter info page
      Notify users about auto-senescence via -alertnotify
      test: Move wait_and_assert_operationid_status debug output before asserts
      Don't require RELRO and BIND_NOW for Darwin
      Only set multicore flags if OpenMP is available
      Revert "remove -mt suffix from boost libraries built by depends"
      Use correct Boost::System linker flag for libzcash
      depends: Remove -mt suffix from Boost libraries
      snark: Remove -mt suffix from Boost library
      cleanup: Ensure code is pyflakes-clean for CI
      Ignore -mempooltxinputlimit once Overwinter activates
      depends: Explicitly download and vendor Rust dependencies
      Make Rust compilation mandatory
      Optimise serialization of MerklePath, avoiding ambiguity of std::vector<bool>
      Use uint64_t instead of size_t for serialized indices into tx.vjoinsplit
      Move explicit instantiation of IncrementalMerkleTree::emptyroots into header
      libsnark: Don't set -static on Darwin
      Set PLATFORM flag when compiling libsnark
      Add base case to CurrentEpoch()
      Cast ZCIncrementalMerkleTree::size() to uint64_t before passing to UniValue
      rpcwallet.cpp: Cast size_t to uint64_t before passing to UniValue
      wallet: Cast size_t to uint64_t before passing to UniValue
      Test calling z_mergetoaddress to merge notes while a note merge is ongoing
      depends: Fix regex bugs in cargo-checksum.sh
      Fix z_importviewingkey startHeight parameter
      Add RPC test of RewindBlockIndex
      When rewinding, remove insufficiently-validated blocks
      Adjust deprecation message to work in both UI and -alertnotify
      Refactor Zcash changes to CCoinsViewDB
      Update blockchain.py RPC test for Zcash
      Update CBlockTreeDB::EraseBatchSync for dbwrapper refactor
      Fix typo
      test: Check return value of snprintf
      test: Add missing Overwinter fields to mininode's CTransaction
      Add RPC test for -enforcenodebloom
      Fix NODE_BLOOM documentation errors
      Move bloom filter filtering logic back into command "switch"
      Update -enforcenodebloom RPC test with filterclear vs filteradd
      make-release.py: Versioning changes for 1.1.0-rc1.
      make-release.py: Updated manpages for 1.1.0-rc1.
      make-release.py: Updated release notes and changelog for 1.1.0-rc1.
      Set Overwinter protocol version to 170005
      make-release.py: Versioning changes for 1.1.0.
      make-release.py: Updated manpages for 1.1.0.

James O'Beirne (3):
      Refactor leveldbwrapper
      Minor bugfixes
      Add tests for gettxoutsetinfo, CLevelDBBatch, CLevelDBIterator

Jason Davies (1):
      Fix typo in comment: should link to issue #1359.

Jay Graber (1):
      Set ban score for expired txs to 0

Jeff Garzik (3):
      leveldbwrapper: Remove unused .Prev(), .SeekToLast() methods
      leveldbwrapper symbol rename: Remove "Level" from class, etc. names
      leveldbwrapper file rename to dbwrapper.*

Jonathan "Duke" Leto (7):
      Fix references to Bitcoin in RPC tests readme
      This library seems to not be used at all and all comments mentioning it are ghosts
      Update awkward wording about blocks as per @daira
      Regtest mining does have a founders reward, a single address t2FwcEhFdNXuFMv1tcYwaBJtYVtMj8b1uTg
      Fix outdated comment about starting balance of nodes
      Return JoinSplit and JoinSplitOutput indexes in z_listreceivedbyaddress
      Add tests for new JoinSplit keys returned by z_listreceivedbyaddress

Lauda (1):
      [Trivial] Grammar and typo correction

Matt Corallo (3):
      Add test for dbwrapper iterators with same-prefix keys.
      Add NODE_BLOOM service bit and bump protocol version
      Don't do mempool lookups for "mempool" command without a filter

Patick Strateman (3):
      Move bloom filter filtering logic outside of command "switch" (giant if/else).
      Add enforcenodebloom option.
      Document both the peerbloomfilters and enforcenodebloom options.

Pavel Janík (1):
      Do not shadow members in dbwrapper

Pieter Wuille (2):
      Encapsulate CLevelDB iterators cleanly
      Fix chainstate serialized_size computation

R E Broadley (1):
      Allow filterclear messages for enabling TX relay only.

Simon Liu (14):
      Code clean up. Remove use of X macro.
      Enable mempool logging in tx expiry QA test.
      Closes #3084. Log txid when removing expired txs from mempool.
      Add qa test for cache invalidation bug found in v1.0.0 to v1.0.3.
      Remove local function wait_and_assert_operationid_status which is     now defined in the test framework for shared usage.
      Update boost to 1.66.0
      Part of #2966, extending Sprout tests to other epochs.
      Update boost package URL to match official download url on boost.org
      Closes #3110.  Ensure user can see error message about absurdly high fees.
      Closes #2910. Add z_listunspent RPC call.
      Upgrade OpenSSL to 1.1.0h
      Use range based for loop
      Bump MIT Licence copyright header.
      Fix test to check for sanitized string from alertnotify.

Wladimir J. van der Laan (6):
      dbwrapper: Pass parent CDBWrapper into CDBBatch and CDBIterator
      dbwrapper: Move `HandleError` to `dbwrapper_private`
      chain: Add assertion in case of missing records in index db
      test: Add more thorough test for dbwrapper iterators
      test: Replace remaining sprintf with snprintf
      doc: update release-notes and bips.md for BIP111

kozyilmaz (1):
      Fix test/gtest bugs caught by latest macOS clang

rofl0r (2):
      fix build error due to usage of obsolete boost_system-mt
      remove -mt suffix from boost libraries built by depends

