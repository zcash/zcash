Daira Hopwood (2):
      .clang-format: change standard to C++11
      Bucket -> note.

Jack Grigg (4):
      Collect all permutations of final solutions
      Add test case that requires the fix
      Reinstate previous testnet adjustment behaviour
      Hardfork to the previous testnet difficulty adjustment behaviour at block 43400

Nathan Wilcox (1):
      A script to remove "unofficial" tags from a remote, such as github.

Sean Bowe (36):
      Flush to disk more consistently by accounting memory usage of serials/anchors in cache.
      Always check valid joinsplits during performance tests, and avoid recomputing them every time we change the circuit.
      Remove the rest of libzerocash.
      Update tests with cache usage computations
      Reorder initialization routines to ensure verifying key log messages appear in debug.log.
      Remove zerocash tests from full-test-suite.
      Rename samplepour to samplejoinsplit
      Update libsnark to our fork.
      Initialize libsodium in this routine, which is now necessary because libsnark uses its PRNG.
      Pass our constraint system to libsnark, so that it doesn't need to (de)serialize it in the proving key.
      Rename CPourTx to JSDescription.
      Rename vpour to vjoinsplit.
      Rename JSDescription's `serials` to `nullifiers`.
      Test fixes.
      Rename GetPourValueIn to GetJoinSplitValueIn
      Rename HavePourRequirements to HaveJoinSplitRequirements.
      Rename GetSerial to GetNullifier.
      Renaming SetSerial to SetNullifier.
      Rename CSerialsMap to CNullifiersMap.
      Rename mapSerials to mapNullifiers.
      Rename some usage of 'pour'.
      Rename more usage of `serial`.
      Rename cacheSerials to cacheNullifiers and fix tests.
      Rename CSerialsCacheEntry.
      Change encryptedbucket1 to encryptednote1.
      Rename pour RPC tests
      Fix tests
      Remove more usage of `serial`.
      Fixes for indentation and local variable names.
      Change `serial` to `nf` in txdb.
      Rename `pour` in RPC tests.
      Remove the constraint system from the alpha proving key.
      Introduce `zcsamplejoinsplit` for creating a raw joinsplit description, and use it to construct the joinsplit for the performance tests that verify joinsplits.
      Bump the (minimum) protocol version to avoid invoking legacy behavior from upstream.
      Remove more from libsnark, and fix potential remote-DoS.
      Add test for non-intuitive merkle tree gadget witnessing behavior.

bitcartel (15):
      Disable USE_ASM when building libsnark (issue 932).
      Add getblocksubsidy RPC command to return the block reward for a given block, taking into account the mining slow start.
      Replace index with height in help message for getblocksubsidy RPC call.
      Narrow scope of lock.
      Add founders reward to output.
      Use new public/private key pairs for alert system.
      Add sendalert.cpp to repo.
      Fixes to integrate sendalert.cpp. Add sendalert.cpp to build process. Add alertkeys.h as a placeholder for private keys.
      Disable QT alert message.
      Update comments.
      Update alert ID start value and URL in comment.
      Update alert protocol version comment.
      Update URL for zcash alert IDs.
      Remove QT alert message box.
      New alert test data generated for new alert key pair. Added test fixture to create new test data. Added instructions for developer.
      Update tor.md for Zcash

Taylor Hornby (17):
      WIP: Add mock test coverage of CheckTransaction
      Split JoinSplit proof verification out of CheckTransaction.
      More testing of CheckTransaction
      Test non-canonical ed25519 signature check
      Rename zerocash to zcash in some places.
      Remove references to libzerocash in .gitignore
      Rename qa/zerocash to qa/zcash in Makefile.am
      Rename zerocash_packages to zcash_packages in packages.mk
      Add security warnings doc with warning about side channels.
      Add another security warning
      Add the results of #784 to security warnings.
      Fix bad_txns_oversize test for increased block size.
      Note that the actual secret spending key may be leaked.
      Mention physical access / close proximity
      Remove in-band error signalling from SignatureHash, fixing the SIGHASH_SINGLE bug.
      Fix the tests that the SIGHASH_SINGLE bugfix breaks.
      Remove insecurely-downloaded dependencies that we don't currently use.

aniemerg (1):
      Update GetDifficulty() to use consensus.powLimit from consensus parameters. Fixes #1032.
