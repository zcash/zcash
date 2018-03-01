Daira Hopwood (2):
      Add Code of Conduct. fixes #802
      Specify Sean as the second contact for conduct issues.

Jack Grigg (6):
      Implement validator and basic solver for Equihash
      Add test vectors for Equihash
      Use Equihash for Proof-of-Work
      Adjust genesis blocks to have valid solutions and hashes
      Fix tests that depend on old block header format
      Fix pow_tests to work with Equihash

Nathan Wilcox (4):
      Log all failing rpc tests concisely.
      Apply a patch from Sean to update wallet to use our new founders-reward aware balances.
      Fix (most) rpc tests by updating balances. zcpour, zcpourdoublespend, and txn_doublespend currently fail.
      Update a bunch of docs by adding a banner, delete a bunch of known bitrot docs; does not update release-process.md.

Sean Bowe (5):
      Fix miner_tests to work with equihash
      Add missing synchronization that causes race condition in test.
      Implementation of Founders' Reward.
      Fix remaining RPC tests.
      Change pchMessageStart for new testnet.

Taylor Hornby (8):
      Add automated performance measurement system.
      Add equihash solving benchmarks
      Add JoinSplit verification benchmarks
      Add verify equihash benchmark
      Don't leave massif.out lying around after the benchmarks
      Use a separate datadir for the benchmarks
      Make benchmark specified by command-line arguments
      Benchmark a random equihash input.

