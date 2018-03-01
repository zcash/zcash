Ethan Heilman (1):
      Increase test coverage for addrman and addrinfo

EthanHeilman (1):
      Creates unittests for addrman, makes addrman testable. Adds several unittests for addrman to verify it works as expected. Makes small modifications to addrman to allow deterministic and targeted tests.

Jack Grigg (24):
      Use depth-first scan for eliminating partial solutions instead of breadth-first
      Add a 256-bit reserved field to the block header
      Set -relaypriority default to false
      Regenerate genesis blocks
      Update tests to account for reserved field
      Update RPC tests to account for reserved field
      Decrease block interval to 2.5 minutes
      Update tests to account for decreased block interval
      Update RPC tests to account for decreased block interval
      Updated a hard-coded number of blocks to account for decreased block interval
      Fix failing tests
      Increase Equihash parameters to n = 96, k = 3 (about 430 MiB)
      Update tests to account for new Equihash parameters
      Speed up FullStepRow index comparison by leveraging big-endian byte layout
      Use little-endian for hash personalisation and hashing indices
      Use htole32 and htobe32 for endian conversions
      Regenerate genesis blocks
      Update miner tests for platform-independent Equihash
      Tweaks after review
      Implement new difficulty algorithm (#931)
      Update tests for new difficulty algorithm
      Improve comments per review
      Handle full Zcash version string in AC_INIT
      Fix bug in network hashrate lookup window configuration

Patrick Strateman (1):
      CAddrMan::Deserialize handle corrupt serializations better.

Philip Kaufmann (1):
      remove using namespace std from addrman.cpp

Sean Bowe (28):
      Move new coins tests to within coins_tests test suite.
      Ensure merkle tree fixed point removal is tested against inside coins_tests.
      Allow pours to be anchored to intermediate treestates of a transaction.
      Test behavior of chained pour consensus rules.
      Remove redundant constraints.
      Change merkle tree depth to 29.
      Update the zkSNARK parameters.
      Add test to ensure parent treestates only can appear earlier in the transaction or in the global state, not later.
      Minor changes to coins_tests.
      Rename `CheckInputs` to `ContextualCheckInputs` since it relies on a global variable and assumes calling conditions.
      Refactor contextual and noncontextual input checks.
      Prevent coinbases from being spent to transparent outputs.
      Disable coinbase-must-be-protected rule on regtest.
      Ensure mempool integrity checks don't trip on chained joinsplits.
      Enforce BIP16 and BIP30 unconditionally to all blocks.
      Enforce remaining softfork activation rules unconditionally.
      Ensure NonContextualCheckInputs runs before routines in ContextualCheckInputs.
      Rename to `fCoinbaseMustBeProtected`.
      Disable enforced coinbase protection in miner_tests.
      Do not encode leading bytes in `PaymentAddress` serialization; this is a task for a higher-level API.
      Use base58check to encode Zcash payment addresses, such that the first two bytes are "zc".
      Add tests for `CZCPaymentAddress`.
      Fix test against merkle tree root.
      Added encoding for Zcash spending keys.
      Guarantee first two bytes of spending key are SK
      Make testnet addresses always start with 'tn'.
      Add test to ensure spending keys always encode with 'SK' at beginning.
      Testnet spending keys should start with 'TK'.

Simon (5):
      Fix issue #717 where if addrman is starved of addresses (e.g. on testnet) the Select_() function will loop endlessly trying to find an address, and therefore eat up 100% cpu time on the 'opencon' thread.
      Declare constants for the maximum number of retries, when to sleep between retries and how long for.
      Implement issue #997 to reduce time for test_bitcoin due to sleeps in addrman.  Related to issue #717.
      Update to DistinctIndices function (for issue #857). Replaces pull request #974.
      Update variable name.

Taylor Hornby (1):
      Enable -alertnotify for hard fork detection. Test it.

