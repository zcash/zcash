Daira Hopwood (1):
      README.md: simplify the title, drop "Core"

Jack Grigg (23):
      Make Equihash solvers cancellable
      Add tests that exercise the cancellation code branches
      Fix segfault by indirectly monitoring chainActive.Tip(), locking on mutex
      Move initialisations to simplify cancelled checks
      Use std::shared_ptr to deallocate partialSolns automatically
      Equihash: Pass each obtained solution to a callback for immediate checking
      Remove hardfork from special testnet difficulty rules
      Fix bug in 'generate' RPC method that caused it to fail with high probability
      Add thread parameter to solveequihash benchmark
      Eliminate some of the duplicates caused by truncating indices
      Use fixed-size array in IsProbablyDuplicate to avoid stack protector warning
      Eliminate probably duplicates in final round
      Simplify IsProbablyDuplicate()
      Add missing assert
      Simplify optional parameters
      Fix previous commit
      Remove the assumption that n/(k+1) is a multiple of 8.
      Add Equihash support for n = 200, k = 9
      Add test showing bug in IsProbablyDuplicate()
      Fix bug in IsProbablyDuplicate()
      Change Equihash parameters to n = 200, k = 9 (about 563-700 MiB)
      Update tests to account for new Equihash parameters
      Ignore duplicate entries after partial recreation

Simon (21):
      Inform user that zcraw... rpc calls are being deprecated.
      Add GetTxid() which returns a non-malleable txid.
      Update genesis blocks.
      Update precomputed equihash solutions used in test.
      Update block and tx data used in bloom filter tests.
      Updated test data for script_tests by uncommenting UPDATE_JSON_TESTS flag.
      Rename GetHash() method to GetSerializeHash().
      Replace calls to GetHash() with GetTxid() for transaction objects.
      Set nLockTime in CreateNewBlock() so coinbase txs do not have the same txid. Update test data in miner_tests.
      Refactor GetTxid() into UpdateTxid() to match coding style of hash member variable.
      Revert "Set nLockTime in CreateNewBlock() so coinbase txs do not have the same txid."
      Fix issue where a coinbase tx should have it's sigscript hashed to avoid duplicate txids, as discussed in BIP34 and BIP30.
      Update genesis block hashes and test data.
      Make txid const.
      Update deprecation message for zcraw api.
      Fix comment.
      Update comment.
      Extend try catch block around calls to libsnark, per discussion in #1126.
      Remove GetSerializeHash() method.
      Use -O1 opimitization flag when building libzcash. Continuation of #1064 and related to #1168.
      Add test for non-malleable txids.  To run just this test: ./zcash-gtest --gtest_filter="txid_tests*"

Taylor Hornby (8):
      Make the --enable-hardening flag explicit.
      Enable -O1 for better FORTIFY_SOURCE protections.
      Add checksec.sh from http://www.trapkit.de/tools/checksec.html
      Add tests for security hardening features
      Pull in upstream's make check-security, based on upstream PR #6854 and #7424.
      Make security options in configure.ac fail if unavailable.
      Put hardened stuff in libzcash CPPFLAGS.
      Add more commands to run unit tests under valgrind.

