Notable changes
===============

Overwinter network upgrade
--------------------------

The code preparations for the Overwinter network upgrade, as described in [ZIP
200](https://github.com/zcash/zips/blob/master/zip-0200.rst), [ZIP
201](https://github.com/zcash/zips/blob/master/zip-0201.rst), [ZIP
202](https://github.com/zcash/zips/blob/master/zip-0202.rst), [ZIP
203](https://github.com/zcash/zips/blob/master/zip-0203.rst), and [ZIP
143](https://github.com/zcash/zips/blob/master/zip-0143.rst) are
finished and included in this release. Overwinter will activate on testnet at
height 207500, and can also be activated at a specific height in regtest mode
by setting the config option `-nuparams=5ba81b19:HEIGHT`.

However, because the Overwinter activation height is not yet specified for
mainnet, version 1.0.15 will behave similarly as other pre-Overwinter releases
even after a future activation of Overwinter on the network. Upgrading from
1.0.15 will be required in order to follow the Overwinter network upgrade on
mainnet.

Overwinter transaction format
-----------------------------

Once Overwinter has activated, transactions must use the new v3 format
(including coinbase transactions). All RPC methods that create new transactions
(such as `createrawtransaction` and `getblocktemplate`) will create v3
transactions once the Overwinter activation height has been reached.

Overwinter transaction expiry
-----------------------------

Overwinter transactions created by `zcashd` will also have a default expiry
height set (the block height after which the transaction becomes invalid) of 20
blocks after the height of the next block. This can be configured with the
config option `-txexpirydelta`.

UTXO and note merging
---------------------

In order to simplify the process of combining many small UTXOs and notes into a
few larger ones, a new RPC method `z_mergetoaddress` has been added. It merges
funds from t-addresses, z-addresses, or both, and sends them to a single
t-address or z-address.

Unlike most other RPC methods, `z_mergetoaddress` operates over a particular
quantity of UTXOs and notes, instead of a particular amount of ZEC. By default,
it will merge 50 UTXOs and 10 notes at a time; these limits can be adjusted with
the parameters `transparent_limit` and `shielded_limit`.

`z_mergetoaddress` also returns the number of UTXOs and notes remaining in the
given addresses, which can be used to automate the merging process (for example,
merging until the number of UTXOs falls below some value).

UTXO memory accounting
----------------------

The default `-dbcache` has been changed in this release to 450MiB. Users can set
`-dbcache` to a higher value (e.g. to keep the UTXO set more fully cached in
memory). Users on low-memory systems (such as systems with 1GB or less) should
consider specifying a lower value for this parameter.

Additional information relating to running on low-memory systems can be found
here: [reducing-memory-usage.md](https://github.com/zcash/zcash/blob/master/doc/reducing-memory-usage.md).

Changelog
=========

21E14 (1):
      Remove obsolete reference to CValidationState from UpdateCoins.

Alex Morcos (1):
      Implement helper class for CTxMemPoolEntry constructor

Ariel (2):
      add blake2b writer
      update SignatureHash according to Overwinter spec

Ashley Holman (1):
      TxMemPool: Change mapTx to a boost::multi_index_container

Cory Fields (2):
      chainparams: move CCheckpointData into chainparams.h
      chainparams: don't use std namespace

Daniel Kraft (1):
      Clean up chainparams some more.

Jack Grigg (43):
      Scope the ECDSA constant sizes to CPubKey / CKey classes
      Enable Bash completion for -exportdir
      Check chainValueZat when checking value pool monitoring
      Add missing namespace for boost::get
      Add viewing key prefix to regtest parameters
      zkey_import_export: Synchronize mempools before mining
      Use JoinSplitTestingSetup for Boost sighash tests
      Network upgrade activation mechanism
      Allow changing network upgrade parameters on regtest
      Test network upgrade logic
      Adjust rewind logic to use the network upgrade mechanism
      Add Overwinter to upgrade list
      Add method for fetching the next activation height after a given block height
      Use a boost::optional for nCachedBranchId
      Change UI/log status message for block rewinding
      Update quote from ZIP 200
      Update SignatureHash tests for transaction format changes
      Implement roll-back limit for reorganisation
      Add rollback limit to block index rewinding
      Remove mempool transactions which commit to an unmineable branch ID
      Remove P2WPKH and P2WSH from signing logic
      Add consensus branch ID parameter to SignatureHash, remove SigVersion parameter
      Cleanup: Wrap function arguments
      Regenerate SignatureHash tests
      Make number of inputs configurable in validatelargetx test
      Use v3 transactions with caching for validatelargetx benchmark
      Extend CWallet::GetFilteredNotes to enable filtering on a set of addresses
      Add branch IDs for current and next block to getblockchaininfo
      Check Equihash solution when loading block index
      Implement z_mergetoaddress for combining UTXOs and notes
      Gate z_mergetoaddress as an experimental feature
      Add z_mergetoaddress to release notes
      Check upgrade status in wallet_overwintertx RPC test
      Document that consensus.chaintip != consensus.nextblock just before an upgrade
      Regenerate sighash tests
      wallet_mergetoaddress: Add additional syncs to prevent race conditions
      make-release.py: Versioning changes for 1.0.15-rc1.
      make-release.py: Updated manpages for 1.0.15-rc1.
      make-release.py: Updated release notes and changelog for 1.0.15-rc1.
      Use block hash comparison for consistency check when loading block index
      Overwinter release notes and testnet activation height
      make-release.py: Versioning changes for 1.0.15.
      make-release.py: Updated manpages for 1.0.15.

Jay Graber (8):
      Add getdeprecationinfo rpc call to return current version and deprecation block height.
      Make applicable only on mainnet
      Add upgrades field to RPC call getblockchaininfo
      Implement transaction expiry for Overwinter
      Add -txexpirydelta cli option
      Add mempool_tx_expiry.py test
      Add expiry to z_mergetoaddress
      Change rpc_tests to 21

Jonas Nick (1):
      Reduce unnecessary hashing in signrawtransaction

Jorge Timón (3):
      Chainparams: Introduce CreateGenesisBlock() static function
      Chainparams: CTestNetParams and CRegTestParams extend directly from CChainParams
      Mempool: Use Consensus::CheckTxInputs direclty over main::CheckInputs

Marius Kjærstad (1):
      Changed http:// to https:// on some links

Mark Friedenbach (1):
      Explicitly set tx.nVersion for the genesis block and mining tests

Matt Corallo (5):
      Add failing test checking timelocked-txn removal during reorg
      Fix removal of time-locked transactions during reorg
      Fix comment in removeForReorg
      Make indentation in ActivateBestChainStep readable
      removeForReorg calls once-per-disconnect-> once-per-reorg

Maxwell Gubler (1):
      Fix syntax examples for z_importwallet and export

Nicolas DORIER (1):
      Unit test for sighash caching

Pavel Vasin (1):
      remove unused NOBLKS_VERSION_{START,END} constants

Pieter Wuille (8):
      Add rewind logic to deal with post-fork software updates
      Support -checkmempool=N, which runs checks on average once every N transactions
      Report non-mandatory script failures correctly
      Refactor script validation to observe amounts
      BIP143: Verification logic
      BIP143: Signing logic
      Precompute sighashes
      Rename to PrecomputedTransactionData

Simon Liu (11):
      Fixes #2793. Backport commit f33afd3 to increase dbcache default.
      Add documentation about dbcache.
      Add note about dbcache to 1.0.15 release notes.
      Remove redundant service flag NODE_GETUTXO meant for Bitcoin XT.
      Implementation of Overwinter transaction format ZIP 202.
      Add test to check malformed v1 transaction against Overwinter tx parser
      Closes #2964. z_sendmany once again makes v1 tx for taddr to taddr.
      Closes #2954 and #2959.  Fixes Overwinter issues in sighash_tests.
      Add field nProtocolVersion to struct NetworkUpgrade.
      Overwinter peer management and network handshaking.
      Add python qa test overwinter_peer_management.

Suhas Daftuar (3):
      Track coinbase spends in CTxMemPoolEntry
      Don't call removeForReorg if DisconnectTip fails
      Fix removeForReorg to use MedianTimePast

jc (1):
      read hashReserved from disk block index

syd (2):
      Fix libsnark dependency build.
      Remove OSX and Windows files from Makefile + share directory.

