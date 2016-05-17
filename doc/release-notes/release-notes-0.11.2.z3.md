Daira Hopwood (1):
      zkSNARK: Add constraint that the total value in a JoinSplit is a 64-bit integer.

Nathan Wilcox (4):
      Add a depends description for googletest.
      Add a zcash-gtest binary to our build with a single tautological test.
      Add coverage support scoped to only the zcash-gtest run; invoke with make zcash-cov; make cov is a superset.
      Add googlemock 1.7.0 dependency.

Sean Bowe (49):
      Add serialization for primitive boost::optional<T>.
      New implementation of incremental merkle tree
      Integrate new incremental merkle tree implementation into consensus.
      Test old tree along with new tree as much as possible.
      Deprecate the old tree and remove old tree tests from the test suite.
      Initialize curve/field parameters in case another test hasn't done so.
      Improve well-formedness checks and add additional serialization/deserialization tests.
      Add more well-formedness checks/tests to tree.
      Make appending algorithm more succinct.
      Move incremental merkle tree tests to zcash-gtest.
      NoteEncryption implementation and integration, removal of ECIES and crypto++ dependencies.
      Move NoteEncryption tests to gtest suite.
      Add additional tests for ephemeral key behavior.
      Clarify the usage of decryption API.
      Check exception has specific string message.
      Small nit fixes
      Run `zcash-gtest` in `make check` and fix performance tests.
      Perform zerocash tests as part of full-test-suite, in preparation for removal of zerocash waterfall.
      Distinguish the failure cases of wfcheck in tree.
      Change ciphertext length to match protocol spec, and refactor the use of constants.
      Initialize libsodium in the gtest suite.
      Introduce new `libzcash` Zcash protocol API and crypto constructions surrounding the zkSNARK circuit.
      zkSNARK: Foundations of circuit design and verification logic.
      zkSNARK: Add "zero" constant variable.
      zkSNARK: Enforce spend-authority of input notes.
      zkSNARK: Enforce disclosure of input note nullifiers
      zkSNARK: Authenticate h_sig with a_sk
      zkSNARK: Enforce that new output notes have unique `rho` to prevent faerie gold attack.
      zkSNARK: Enforce disclosure of commitments to output notes.
      zkSNARK: Ensure that values balance correctly.
      zkSNARK: Witness commitments to input notes.
      zkSNARK: Enforce merkle authentication path from nonzero-valued public inputs to root.
      libzcash: Add tests for API
      Remove scriptPubKey/scriptSig from CPourTx, and add randomSeed.
      Transplant of libzcash.
      Added public zkSNARK parameter generation utility.
      Stop testing old tree against new tree.
      Remove nearly all of libzerocash.
      Update public zkSNARK parameters for new circuit.
      Fix performance measurements due to modified transaction structure.
      Remove the zerocash tests from the full test suite.
      Protect-style joinsplits should anchor to the latest root for now, until #604 is resolved.
      Use inheritance for PRF gadgets.
      Rename ZCASH_ constants to ZC_.
      Rename hmac -> mac in circuit.
      `Note` values should be little-endian byte order.
      Update zkSNARK proving/verifying keys.
      Add h_sig test vectors.
      Change testnet network magics.

Taylor Hornby (7):
      Add check that vpubs are not both nonzero and test it.
      Fix sighash tests
      Add empty merkle/noteencryption tests so Sean can rebase.
      Fix RPC tests
      Rename bitcoin.conf and bitcoind.pid to zcash.conf and zcashd.pid in qa/ and src/
      Trivial change: Capitalize the Z in Zerocash
      Remove the Merkle tree hash function's fixed point.

