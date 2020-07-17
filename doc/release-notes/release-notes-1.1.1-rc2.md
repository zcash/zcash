Notable changes
===============

zcash-cli: arguments privacy
----------------------------

The RPC command line client gained a new argument, `-stdin`
to read extra arguments from standard input, one per line until EOF/Ctrl-D.
For example:

    $ src/zcash-cli -stdin walletpassphrase
    mysecretcode
    120
    ^D (Ctrl-D)

It is recommended to use this for sensitive information such as private keys, as
command-line arguments can usually be read from the process table by any user on
the system.

Asm representations of scriptSig signatures now contain SIGHASH type decodes
----------------------------------------------------------------------------

The `asm` property of each scriptSig now contains the decoded signature hash
type for each signature that provides a valid defined hash type.

The following items contain assembly representations of scriptSig signatures
and are affected by this change:

- RPC `getrawtransaction`
- RPC `decoderawtransaction`
- REST `/rest/tx/` (JSON format)
- REST `/rest/block/` (JSON format when including extended tx details)
- `zcash-tx -json`

For example, the `scriptSig.asm` property of a transaction input that
previously showed an assembly representation of:

    304502207fa7a6d1e0ee81132a269ad84e68d695483745cde8b541e3bf630749894e342a022100c1f7ab20e13e22fb95281a870f3dcf38d782e53023ee313d741ad0cfbc0c509001

now shows as:

    304502207fa7a6d1e0ee81132a269ad84e68d695483745cde8b541e3bf630749894e342a022100c1f7ab20e13e22fb95281a870f3dcf38d782e53023ee313d741ad0cfbc0c5090[ALL]

Note that the output of the RPC `decodescript` did not change because it is
configured specifically to process scriptPubKey and not scriptSig scripts.

Changelog
=========

Cory Fields (4):
      serialization: teach serializers variadics
      build: univalue subdir build fixups
      don't throw std::bad_alloc when out of memory. Instead, terminate immediately
      prevector: assert successful allocation

Daira Hopwood (1):
      Use https: for BDB backup download URL.

David Llop (1):
      Update Payment API

Eirik Ogilvie-Wigley (7):
      Clarify help text of dumpprivkey
      Add sapling nullifier set
      Add enum for nullifier type
      Add sapling nullifiers to db and mempool
      Rename nullifier caches and maps to indicate sprout nullifiers
      Make sure transactions have non-empty outputs
      Coinbase transactions can not have shielded spend or output

Jack Grigg (52):
      Disable building libzcashconsensus by default
      depends: Upgrade Rust to 1.26.0-beta.3
      depends: Add support for unpackaged Rust crates
      depends: Update to latest librustzcash with sapling-crypto dependencies
      Add Sapling to upgrade list
      Add static asserts to ensure CONTINUE_EXECUTION doesn't collide
      [Bitcoin-Tx] Adjust util-test test cases for Zcash
      Handle usage of prevector for CScript in Zcash-specific code
      GetSerializeSize changes in Zcash-specific code
      Remove nType and nVersion from Zcash-specific code
      Adjust consensus rules to require v4 transactions from Sapling activation
      Implement basic Sapling v4 transaction parser
      Add Sapling v4 transactions to IsStandard
      Pass transaction header into correct SignatureHash serialization level
      Remove now-unshadowed serialization lines that do nothing
      Implement SpendDescription and OutputDescription datastructures
      Add a constant for Overwinter's transaction version
      Return result of boost::apply_visitor
      Improve best-effort logging before termination on OOM
      Attempt to log before terminating if prevector allocation fails
      Fix -Wstring-plus-int warning on clang
      Update mempool_nu_activation RPC test to exercise both Overwinter and Sapling
      Use CBitcoinAddress wrappers in Zcash-specific code
      Change JSOutPoint constructor to have js argument be uint64_t
      Update CreateNewContextualCMutableTransaction to create Sapling transactions
      Expire Overwinter transactions before the Sapling activation height
      Remove obsolete CreateJoinSplit and GenerateParams binaries
      Add missing include guard
      Raise 100kB transaction size limit from Sapling activation
      Benchmark the largest valid Sapling transaction in validatelargetx
      Rename MAX_TX_SIZE to MAX_TX_SIZE_AFTER_SAPLING
      Rework z_sendmany z-address recipient limit
      Add test of Sapling transaction size boundary
      Update tests for CreateNewContextualCMutableTransaction changes
      wallet: Change IsLockedNote to take a JSOutPoint
      wallet: Make some arguments const that can be
      Implement Sapling signature hash (ZIP 243)
      Update sighash tests
      Introduce wrappers around CZCPaymentAddress
      Introduce wrappers around CZCSpendingKey
      Introduce wrappers around CZCViewingKey
      Implement {Encode,Decode}PaymentAddress etc. without CZCEncoding
      Add key_io includes to Zcash-specific code
      Add valueBalance to value balances, and enforce its consensus rules
      Track net value entering and exiting the Sapling circuit
      Add contextual comment for GetValueOut() and GetShieldedValueIn()
      Use boost::variant to represent shielded addresses and keys
      Correctly serialize Groth16 JSDescription for verifyjoinsplit benchmark
      make-release.py: Versioning changes for 1.1.1-rc1.
      make-release.py: Updated manpages for 1.1.1-rc1.
      make-release.py: Updated release notes and changelog for 1.1.1-rc1.
      Comment out Gitian library handling while we don't build any libraries

Jay Graber (1):
      Add test for dependent txs to mempool_tx_expiry.py

Jeremy Rubin (1):
      Fix subscript[0] in base58.cpp

Jonas Schnelli (4):
      [RPC] createrawtransaction: add option to set the sequence number per input
      [bitcoin-tx] allow to set nSequence number over the in= command
      [Bitcoin-Tx] Add tests for sequence number support
      add bip32 pubkey serialization

João Barbosa (1):
      Remove unused GetKeyID and IsScript methods from CBitcoinAddress

Karl-Johan Alm (1):
      Removed using namespace std from bitcoin-cli/-tx and added std:: in appropriate places.

Kaz Wesley (1):
      CBase58Data::SetString: cleanse the full vector

Larry Ruane (1):
      fix qa/zcash/full_test_suite.py pathname

MarcoFalke (3):
      [uacomment] Sanitize per BIP-0014
      [rpcwallet] Don't use floating point
      [test] Remove unused code

Matt Corallo (1):
      Add COMPACTSIZE wrapper similar to VARINT for serialization

Pavel Janík (1):
      [WIP] Remove unused statement in serialization

Pavol Rusnak (2):
      implement uacomment config parameter     which can add comments to user agent as per BIP-0014
      limit total length of user agent comments

Pedro Branco (1):
      Prevent multiple calls to ExtractDestination

Per Grön (1):
      Make some globals static that can be

Peter Pratscher (1):
      Backported Bitcoin PR #8704 to optionally return full tx details in the getblock rpc call

Pieter Wuille (22):
      Prevector type
      Remove unused ReadVersion and WriteVersion
      Make streams' read and write return void
      Make nType and nVersion private and sometimes const
      Make GetSerializeSize a wrapper on top of CSizeComputer
      Get rid of nType and nVersion
      Avoid -Wshadow errors
      Make CSerAction's ForRead() constexpr
      Add optimized CSizeComputer serializers
      Use fixed preallocation instead of costly GetSerializeSize
      Add serialization for unique_ptr and shared_ptr
      Add deserializing constructors to CTransaction and CMutableTransaction
      Avoid unaligned access in crypto i/o
      Fix some empty vector references
      Introduce wrappers around CBitcoinAddress
      Move CBitcoinAddress to base58.cpp
      Implement {Encode,Decode}Destination without CBitcoinAddress
      Import Bech32 C++ reference code & tests
      Convert base58_tests from type/payload to scriptPubKey comparison
      Replace CBitcoinSecret with {Encode,Decode}Secret
      Stop using CBase58Data for ext keys
      Split key_io (address/key encodings) off from base58

Puru (1):
      bitcoin-cli.cpp: Use symbolic constant for exit code

Sean Bowe (49):
      Switch to latest librustzcash
      Invoke the merkle_hash API in librustzcash via test suite.
      Link with -ldl
      Update librustzcash hash
      Load Sapling testnet parameters into memory.
      Update librustzcash hash
      Check that duplicate Sapling nullifiers don't exist within a transaction.
      Abstract `uncommitted` and depth personalization for IncrementalMerkleTree.
      Add implementation of Sapling merkle tree
      Add regression tests and test vectors for Sapling merkle tree
      Rename NullifierType to ShieldedType.
      Specifically describe anchors as Sprout anchors.
      Rename hashAnchor to hashSproutAnchor.
      Rename hashReserved to hashSaplingAnchorEnd.
      Add primitive implementation of GetSaplingAnchorEnd.
      Rename DB_ANCHOR to DB_SPROUT_ANCHOR.
      Rename GetAnchorAt to GetSproutAnchorAt.
      Rename PushAnchor to PushSproutAnchor.
      Introduce support for GetBestAnchor(SAPLING).
      Generalize the PopAnchor implementation behavior.
      Generalize the PushAnchor implementation behavior.
      Remove underscores from gtest test names.
      Rename hashSaplingAnchorEnd to hashFinalSaplingRoot to match spec.
      Rename hashSproutAnchorEnd to hashFinalSproutRoot to be consistent.
      Add support for Sapling anchors in coins/txdb.
      Add support for PopAnchor(.., SPROUT/SAPLING)
      Add `PushSaplingAnchor`
      Add consensus support for Sapling merkle trees.
      Add support for Sapling anchor checks in mempool consistency checks.
      Calculate the correct hashFinalSaplingRoot in the miner.
      Adjust tests to handle Sapling anchor cache
      Evict transactions with obsolete anchors from the mempool
      Fix outdated comment
      Fix broken error messages.
      Fix miner tests
      Update sapling-crypto and librustzcash
      Swap bit endianness of test vectors
      Remove unnecessary IsCoinbase() check. Coinbases are guaranteed to have empty vjoinsplit.
      Refactor so that dataToBeSigned can be used later in the function for other purposes.
      Update to latest librustzcash
      Check Sapling Spend/Output proofs and signatures.
      Integrate Groth16 verification and proving.
      Update librustzcash again
      Adjust tests and benchmarks
      Switch Rust to 1.26 Stable.
      Update librustzcash
      Update Sapling testnet parameters
      Update merkle tree and pedersen hash tests to account for new encoding
      Change txdb prefixes for sapling and avoid writing unnecessary information.

Simon Liu (16):
      Part of #2966, extending Sprout tests to other epochs.
      Closes #3134 - Least Authority Issue E
      Refactoring: libzcash::Note is now a subclass of libzcash::BaseNote.
      Refactoring: Rename class libzcash::Note to libzcash::SproutNote.
      Refactoring: SproutNote member variable value moved to BaseNote.
      Add virtual destructor to SproutNote and BaseNote
      Remove unused SproutNote variables.
      Refactoring: rename NotePlaintext --> SproutNotePlaintext
      Create class hierarchy for SproutNotePlaintext.
      Move memo member varible from SproutNotePlaintext to BaseNotePlaintext.
      Tweaks to d0a1d83 to complete backport of Bitcoin PR #8704
      Closes #3178 by adding verbosity level improvements to getblock RPC.
      Fix undefined behaviour, calling memcpy with NULL pointer.
      Closes #3250. Memo getter should return by reference, not by value.
      make-release.py: Versioning changes for 1.1.1-rc2.
      make-release.py: Updated manpages for 1.1.1-rc2.

Tom Harding (1):
      Add optional locktime to createrawtransaction

UdjinM6 (2):
      Fix exit codes:
      Every main()/exit() should return/use one of EXIT_ codes instead of magic numbers

Wladimir J. van der Laan (2):
      rpc: Input-from-stdin mode for bitcoin-cli
      doc: mention bitcoin-cli -stdin in release notes

ca333 (2):
      [fix] proton download path
      update proton.mk

kozyilmaz (2):
      [macOS] added curl method for param download
      [macOS] use shlock instead of flock in fetch-params

mruddy (1):
      Resolve issue bitcoin/bitcoin#3166.

