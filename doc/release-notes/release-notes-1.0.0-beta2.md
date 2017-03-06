Alfie John (2):
      Typo in params README
      Updating wording to match Beta Guide

Bryan Stitt (1):
      Link to beta guide

Daira Hopwood (9):
      Benchmark 50 iterations of solveequihash
      Remove FindAndDelete. refs #1386
      Update my email address in the Code of Conduct.
      Repair FormatSubVersion tests. refs #1138
      WIP: update address prefixes. refs #812
      Reencode keys in JSON test data. refs #812
      CBitcoinAddress should use nVersionBytes == 2.
      Repair bitcoin-util-test.
      Repair rpc-tests/signrawtransactions.py.

Gregory Maxwell (1):
      Limit setAskFor and retire requested entries only when a getdata returns.

Jack Grigg (43):
      Add support for encrypting spending keys
      Check we haven't trashed the first key entry with the second
      Move serialized Zcash address length constants into zcash/Address.hpp
      Measure multithreaded solveequihash time per-thread
      Add a make command for checking expected failures
      Enable high-priority alerts to put the RPC into safe mode
      Fix test
      Add wallet method to clear the note witness cache
      Clear note witness caches on reindex
      Write note witness cache atomically to disk to avoid corruption
      Test that invalid keys fail to unlock the keystore
      Implement CSecureDataStream for streaming CKeyingMaterial
      Cache note decryptors in encrypted keystore
      Use correct lock for spending keys
      Upgrade Boost to 1.62.0
      Upgrade libgmp to 6.1.1
      Upgrade OpenSSL to 1.1.0b
      Upgrade miniupnpc to 2.0
      Upgrade ccache to 3.3.1
      Release process: check dependencies for updates
      Fix auto_ptr deprecation warning in Boost
      Replace auto_ptr with unique_ptr
      Re-enable disabled compiler warnings
      Disable nearly everything in OpenSSL
      Add libsnark to pre-release dependency checks
      Assert that new OpenSSL allocators succeed
      Remove no-autoalginit and no-autoerrinit OpenSSL flags
      Use asserts to check allocation errors in CECKey::Recover
      Simplify ClearNoteWitnessCache()
      Add tests for alerts enabling RPC safe mode
      Ensure correctness if asserts are compiled out
      Disable OP_CODESEPARATOR
      Remove OP_CODESEPARATOR from tests
      Downgrade bdb to 5.3.28
      Use CLIENT_VERSION_BUILD to represent -beta and -rc in client version
      Update release process with version schema
      Formatting fix
      Mark previously-valid test data as invalid
      Re-encode hard-coded addresses in tests
      Re-encode Founders' Reward keys
      Fix secp256k1 test compilation
      Fix zkey test
      Update address in Founders' Reward gtest

Jay Graber (4):
      Link to z.cash on security-warnings.md
      Add section abt confs and reorgs to security-warnings.md
      Update wording
      Final edits

Kevin Gallagher (5):
      Lock to prevent parallel execution of fetch-params.sh
      Updates dns.testnet.z.cash -> dnsseed.testnet.z.cash
      Verify TLS certificates w/ wget in fetch-params.sh
      Inserts some notes related to testnet deployment
      Adds note about updating guide during testnet deployment

Pieter Wuille (1):
      Fix and improve relay from whitelisted peers

Robert C. Seacord (1):
      Changes to upgrade bdb to 6.2.23

Sean Bowe (1):
      Update to `beta2` public parameters, remove `regtest`/`testnet3` parameters     subdirectories.

Simon (20):
      Replace %i format specifier with more commonly used %d.
      Fix GetFilteredNotes to use int for minDepth like upstream and avoid casting problems. Don't use FindMyNotes as mapNoteData has already been set on wallet tx.
      Update test to filter and find notes.
      Add support for spending keys to the encrypted wallet.
      Update to use new API in CCryptoKeyStore and store a viewing key in walletdb.
      Fix comment and formatting per review
      Add founders reward to ChainParams.     Fix bug where subsidy slow shift was ignored.
      Founders reward: changed index computation, added new test and some refactoring.
      Founders reward: Refactor test and formatting per review.
      Refactor to add test to verify number of rewards each mainnet address will receive
      Refactor and fix per review
      Update comment per review
      Update founders reward test to output path of temporary wallet.dat file which contains keys which can be used for testing founders reward addresses.
      Update testnet founders reward addresses
      Add mainnet 2-of-3 multisig addresses for testing.
      Add field fMinerTestModeForFoundersRewardScript to chainparams
      Update mainnet addresses used for testing to have the correct number
      Fixes #1345 so that UTXO debit and credits are computed correctly for a transaction.
      Closes #1371 by updating signed message
      Modify message string so we don't need to backport commits which implement FormatStateMessage and GetDebugMessage and involve changes to consensus/validation.h

Wladimir J. van der Laan (1):
      build: remove libressl check

fanquake (1):
      [depends] OpenSSL 1.0.1k - update config_opts

kazcw (1):
      prevent peer flooding request queue for an inv

