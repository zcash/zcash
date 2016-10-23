Daira Hopwood (22):
      Add link to protocol specification.
      Add tests for IsStandardTx applied to v2 transactions.
      Make v2 transactions standard. This also corrects a rule about admitting large orphan transactions into the mempool, to account for v2-specific fields.
      Changes to build on Alpine Linux.
      Add Tromp's implementation of Equihash solver (as of tromp/equihash commit 690fc5eff453bc0c1ec66b283395c9df87701e93).
      Integrate Tromp solver into miner code and remove its dependency on extra BLAKE2b implementation.
      Minor edits to dnsseed-policy.md.
      Avoid boost::posix_time functions that have potential out-of-bounds read bugs. ref #1459
      Add help for -equihashsolver= option.
      Assert that the Equihash solver is a supported option.
      Repair check-security-hardening.sh.
      Revert "Avoid boost::posix_time functions that have potential out-of-bounds read bugs. ref #1459"
      Fix race condition in rpc-tests/wallet_protectcoinbase.py. closes #1597
      Fix other potential race conditions similar to ref #1597 in RPC tests.
      Update the error message string for tx version too low. ref #1600
      Static assertion that standard and network min tx versions are consistent.
      Update comments in chainparams.cpp.
      Update unit-tests documentation. closes #1530
      Address @str4d's comments on unit-tests doc. ref #1530
      Remove copyright entries for some files we deleted.
      Update license text in README.md. closes #38
      Bump version numbers to 1.0.0-rc2.

David Mercer (4):
      explicitly pass HOST and BUILD to ./configure
      allow both HOST and BUILD to be passed in from the zcutil/build.sh
      pass in both HOST and BUILD to depends system, needed for deterministic builds
      explicitly pass HOST and BUILD to libgmp ./configure

Gregory Maxwell (1):
      Only send one GetAddr response per connection.

Jack Grigg (31):
      Implement MappedShuffle for tracking the permutation of an array
      Implement static method for creating a randomized JSDescription
      Randomize JoinSplits in z_sendmany
      Refactor test code to better test JSDescription::Randomized()
      Remove stale comment
      Rename libbitcoinconsensus to libzcashconsensus
      Rename bitcoin-tx to zcash-tx
      Remove the RC 1 block index error message
      Disable wallet encryption
      Add more assertions, throw if find_output params are invalid
      Clear witness cache when re-witnessing notes
      Add heights to log output
      Throw an error when encryptwallet is disabled
      Document that wallet encryption is disabled
      Document another wallet encryption concern
      Improve security documentation
      Fix RPC tests that require wallet encryption
      Add test that encryptwallet is disabled
      Revert "Revert "Avoid boost::posix_time functions that have potential out-of-bounds read bugs. ref #1459""
      GBT: Support coinbasetxn instead of coinbasevalue
      GBT: Add informational founders' reward value to coinbasetxn
      GBT: Correct block header in proposals RPC test
      GBT: Add RPC tests
      Disallow v0 transactions as a consensus rule
      Reject block versions lower than 4
      Regenerate genesis blocks with nVersion = 4
      Use tromp's solver to regenerate miner tests
      Update tests for new genesis blocks
      Enforce standard transaction rules on testnet
      Update sighash tests for new consensus rule
      Fix RPC test

Jay Graber (7):
      Rm bitcoin dev keys from gitian-downloader, add zcash dev keys
      Rm bips.md
      Update files.md for zcash
      Update dnsseed-policy.md
      Developer notes still relevant
      Document RPC interface security assumptions in security-warnings.md
      Update RPC interfaces warnings language

Patrick Strateman (1):
      CDataStream::ignore Throw exception instead of assert on negative nSize.

Pieter Wuille (4):
      Introduce constant for maximum CScript length
      Treat overly long scriptPubKeys as unspendable
      Fix OOM bug: UTXO entries with invalid script length
      Add tests for CCoins deserialization

Simon (17):
      Fixes CID 1147436 uninitialized scalar field.
      Fixes CID 1352706 uninitialized scalar field.
      Fixes CID 1352698 uninitialized scalar field.
      Fixes CID 1352687 uninitialized scalar field.
      Fixes CID 1352715 uninitialized scalar field.
      Fixes CID 1352686 uninitialized scalar variable.
      Fixes CID 1352599 unitialized scalar variable
      Fixes CID 1352727 uninitialized scalar variable.
      Fixes CID 1352714 uninitialized scalar variable.
      Add security warning about logging of z_* calls.
      Add debug option "zrpcunsafe" to be used when logging more sensitive information such as the memo field of a note.
      Closes #1583 by setting up the datadir for the wallet gtest.
      Fix issue where z_sendmany is too strict and does not allow integer based amount e.g. 1 which is the same as 1.0
      Update test to use integer amount as well as decimal amount when calling z_sendmany
      Fix build problem with coins_tests
      Workaround g++ 5.x bug with brace enclosed initializer.
      Patch backport of upstream 1588 as we don't (yet) use the NetMsgType namespace

Wladimir J. van der Laan (1):
      net: Ignore `notfound` P2P messages

