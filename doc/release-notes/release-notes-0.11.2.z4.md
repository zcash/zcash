Alex (1):
      add sha256sum support for Mac OS X

Alfie John (1):
      Rename libzerocash to libzcash

Jack Grigg (21):
      Implement mining slow start with a linear ramp
      Update subsidy tests to account for mining slow start
      Update miner tests to account for mining slow start
      Disable mining slow start in regtest mode
      Fix failing miner test
      Add Zcash revision to version strings
      Bitcoin -> Zcash in version and help text
      Add Zcash Developers to CLI copyright notice
      Minor error message tweak
      Refactor StepRow to make optimisation easier
      Cleanups
      Implement index-truncation Equihash optimisation
      Store truncated indices in the same char* as the hash (H/T tromp for the idea!)
      Use template parameters to statically initialise Equihash
      Merge *StepRow XOR and trimming operations
      Use comparator object for sorting StepRows
      Store full indices in the same char* as the hash
      Use fixed-width array for storing hash and indices
      Use optimised Equihash solver for miner and benchmarks
      Fix comment
      Fix nits after review

Nathan Wilcox (1):
      Fix a test name bug so that ``make cov-zcash`` correctly runs the ``zcash-gtest`` binary. Fixes #946.

Sean Bowe (14):
      Refactor PRF_gadget to hand responsibility to PRF_addr_a_pk_gadget for creating the '0' argument to the PRF.
      Enforce first four bits are zero for all spending keys and phi.
      Enable binary serializations of proofs and r1cs keys, and make the `CPourTx` proof field fixed-size.
      Reorder fields of CPourTx to reflect the spec.
      Update proving key and tests that depend on transaction structure changes
      Enable MULTICORE proving behavior with omp.
      Pass `-fopenmp` at compile-time to enable MULTICORE.
      Switch to Ed25519 for cryptographic binding of joinsplits to transactions.
      Enforce that the `S` value of the ed25519 signature is smaller than the group order to prevent malleability attacks.
      Use joinsplit_sig_t in more places.
      Wrap lines in *CTransaction constructors.
      Change error for invalid joinsplit signature for consistency.
      Add additional assertions.
      Update performance measurement transaction.

Simon (2):
      Remove Bitcoin testnet seeds.
      Remove Bitcoin mainnet seeds.

Taylor Hornby (16):
      Fix build warnings in sighash tests.
      Fix FORTIFY_SOURCE build errors.
      Use HARDENED_CPPFLAGS in the Makefile consistently.
      Use left shift instead of floating-point pow() in equihash.
      Ignore deprecated declaration warnings.
      Remove unused code in libzerocash util.cpp
      Turn on -Werror for the Zcash build.
      Patch libsnark to build with my compiler. Upstream PR #35.
      Hide new Boost warnings on GCC 6.
      Add ability to run things under valgrind.
      Pass -DPURIFY to OpenSSL so it doesn't clutter valgrind output.
      Enable -v for valgrind so we can see counts for each error.
      Sign JoinSplit transactions
      We don't want to benchmark signature creation / verification.
      Implement signature verification in CheckTransaction
      Fix tests for JoinSplit signatures

