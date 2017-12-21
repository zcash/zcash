Changelog
=========

Ariel Gabizon (1):
      boost::format -> tinyformat

Bruno Arueira (1):
      Removes out bitcoin mention in favor for zcash

Cory Fields (1):
      httpserver: explicitly detach worker threads

Duke Leto (1):
      Update performance-measurements.sh

Jack Grigg (37):
      Squashed 'src/snark/' content from commit 9ada3f8
      Add libsnark compile flag to not copy DEPINST to PREFIX
      Add Ansible playbook for grind workers
      Add connections in BIP65 and BIP66 tests to the test manager
      Add benchmark for listunspent
      [Test] MiniNode: Implement JSDescription parsing
      [Test] MiniNode: Implement v2 CTransaction parsing
      [Test] MiniNode: Implement Zcash block parsing
      [Test] MiniNode: Update protocol version and network magics
      [Test] MiniNode: Use Zcash PoW
      [Test] MiniNode: Fix coinbase creation
      [Test] MiniNode: Coerce OP_PUSHDATA bytearrays to bytes
      [Test] MiniNode: Implement Zcash coinbase
      Fix BIP65 and BIP66 tests
      Un-indent RPC test output in test runner
      Replace full-test-suite.sh with a new test suite driver script
      Move ensure-no-dot-so-in-depends.py into full_test_suite.py
      Move check-security-hardening.sh into full_test_suite.py
      Add memory benchmark for validatelargetx
      Migrate libsnark test code to Google Test
      Remove test code corresponding to removed code
      Add alt_bn128 to QAP and Merkle tree gadget tests
      Update libsnark LDLIBS
      Add "make check" to libsnark that runs the Google Tests
      Add "make libsnark-tests" that runs libsnark's "make check"
      Changes to get test_r1cs_ppzksnark passing
      Add bitcoin-util-test.py to full_test_suite.py
      Add stdout notice if any stage fails
      Add libsnark to "make clean"
      Ensure that libsnark is built first, so its headers are available
      Remove OpenSSL libraries from libsnark LDLIBS
      Add libsnark tests to full_test_suite.py
      Add --list-stages argument to full_test_suite.py
      Fix NPE in rpc_wallet_tests
      make-release.py: Versioning changes for 1.0.13-rc1.
      make-release.py: Updated manpages for 1.0.13-rc1.
      Change auto-senescence cycle to 16 weeks

Jason Davies (1):
      Replace "bitcoin" with "Zcash".

Jay Graber (1):
      s/zcash/Zcash

Jonathan "Duke" Leto (1):
      Fix bug where performance-measurements.sh fails hards when given no args

João Barbosa (1):
      Improve shutdown process

Sean Bowe (5):
      Remove libsnark from depends system and integrate it into build system.
      Remove crusty old "loadVerifyingKey"/"loadProvingKey" APIs and associated invariants.
      Refactor proof generation function.
      Add streaming prover.
      Integrate low memory prover.

Simon Liu (7):
      Replace 'bitcoin address' with 'zcash address'.
      Closes #2639. z_shieldcoinbase is now supported, no longer experimental.
      Closes #2263 fixing broken pipe error.
      Closes #2576. Update link to security info on z.cash website.
      Closes #2639. Adds optional limit parameter with a default value of 50.
      Fix an issue where qa test wallet_shieldcoinbase could hang.
      Add payment disclosure as experimental feature.

Wladimir J. van der Laan (4):
      Make HTTP server shutdown more graceful
      http: Wait for worker threads to exit
      http: Force-exit event loop after predefined time
      http: speed up shutdown

