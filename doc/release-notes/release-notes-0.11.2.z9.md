
Sean Bowe:
      Change memo field size and relocate `ciphertexts` field of JoinSplit description.
      Implement zkSNARK compression.
      Perform curve parameter initialization at start of gtest suite.
      Update libsnark dependency.
      Enable MONTGOMERY_OUTPUT everywhere.
      Update proving/verifying keys.

Jack Grigg:
      Add support for spending keys to the basic key store.
      Merge AddSpendingKeyPaymentAddress into AddSpendingKey to simplify API.
      Add methods for byte array expansion and compression.
      Update Equihash hash generation to match the Zcash spec.
      Extend byte array expansion and compression methods with optional padding.
      Store the Equihash solution in minimal representation in the block header.
      Enable branch coverage in coverage reports.
      Add gtest coverage and intermediates to files deleted by "make clean".
      Remove non-libsnark dependencies and test harness code from coverage reports.
      Add separate lock for SpendingKey key store operations.
      Test conversion between solution indices and minimal representation.

Daira Hopwood:
      Move bigint arithmetic implementations to libsnark.
      Add mostly-static checks on consistency of Equihash parameters, MAX_HEADERS_RESULTS, and MAX_PROTOCOL_MESSAGE_LENGTH.
      Change some asserts in equihash.cpp to be static.
      Decrease MAX_HEADERS_RESULTS to 160. fixes #1289
      Increment version numbers for z9 release.
      Add these release notes for z9.

Taylor Hornby:
      Disable hardening when building for coverage reports.
      Upgrade libsodium for AVX2-detection bugfix.
      Fix inconsistent optimization flags; single source of truth.
      Add -fwrapv -fno-strict-aliasing; fix libdwcash flags.
      Use libsodium's s < L check, instead checking that libsodium checks that.

Simon Liu:
      Fixes #1193 so that during verification benchmarking it does not unncessarily create thousands of CTransaction objects.
      Closes #701 by adding documentation about the Payment RPC interface.
      Add note about zkey and encrypted wallets.

Gaurav Rana:
      Update dwcash-cli stop message.

Tom Ritter:
      Clarify comment about nonce space for Note Encryption.

Robert C. Seacord:
      Memory safety and correctness fixes found in NCC audit.

Patrick Strateman (merged by Taylor Hornby):
      Pull in some DoS mitigations from upstream. (#1258)

Wladimir J. van der Laan:
      net: correctly initialize nMinPingUsecTime.

