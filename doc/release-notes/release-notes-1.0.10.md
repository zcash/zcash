Notable changes
===============

Signature validation using libsecp256k1
---------------------------------------

ECDSA signatures inside Zcash transactions now use validation using
[https://github.com/bitcoin/secp256k1](libsecp256k1) instead of OpenSSL.

Depending on the platform, this means a significant speedup for raw signature
validation speed. The advantage is largest on x86_64, where validation is over
five times faster. In practice, this translates to a raw reindexing and new
block validation times that are less than half of what it was before.

Libsecp256k1 has undergone very extensive testing and validation upstream.

A side effect of this change is that libconsensus no longer depends on OpenSSL.

Changelog
=========

Boris Hajduk (1):
      documentatin z_validateaddress was missing param

Daira Hopwood (8):
      Delete old protocol version constants and simplify code that used them.     fixes #2244
      Remove an unneeded version workaround as per @str4d's review comment.
      Remove unneeded lax ECDSA signature verification.
      Strict DER signatures are always enforced; remove the flag and code that used it.
      Repair tests for strict DER signatures.     While we're at it, repair a similar test for CLTV, and make the repaired RPC tests run by default.
      Make transaction test failures print the comments preceding the test JSON.
      Fix a comment that was made stale before launch by #1016 (commit 542da61).
      Delete test that is redundant and inapplicable to Zcash.

Jack Grigg (20):
      Fix incorrect locking in CCryptoKeyStore
      Use AtomicTimer for metrics screen thread count
      Revert "Fix secp256k1 test compilation"
      Squashed 'src/secp256k1/' changes from 22f60a6..84973d3
      Fix potential overflows in ECDSA DER parsers
      Rename FALLBACK_DOWNLOAD_PATH to PRIORITY_DOWNLOAD_PATH
      Add test for incorrect consensus logic
      Correct consensus logic in ContextualCheckInputs
      Add comments
      Update Debian copyright list
      Specify ECDSA constant sizes as constants
      Remove redundant `= 0` initialisations
      Ensure that ECDSA constant sizes are correctly-sized
      Add test for -mempooltxinputlimit
      Hold an ECCVerifyHandle in zcash-gtest
      Additional testing of -mempooltxinputlimit
      Fix comment
      Use sendfrom for both t-addr calls
      make-release.py: Versioning changes for 1.0.10.
      make-release.py: Updated manpages for 1.0.10.

Kevin Pan (1):
      "getblocktemplate" could work without wallet

Pieter Wuille (2):
      Update key.cpp to new secp256k1 API
      Switch to libsecp256k1-based validation for ECDSA

Simon Liu (5):
      Fix intermediate vpub_new leakage in multi joinsplit tx (#1360)
      Add option 'mempooltxinputlimit' so the mempool can reject a transaction     based on the number of transparent inputs.
      Check mempooltxinputlimit when creating a transaction to avoid local     mempool rejection.
      Partial revert & fix for commit 9e84b5a ; code block in wrong location.
      Fix #b1eb4f2 so test checks sendfrom as originally intended.

Wladimir J. van der Laan (2):
      Use real number of cores for default -par, ignore virtual cores
      Remove ChainParams::DefaultMinerThreads

kozyilmaz (3):
      [macOS] system linker does not support “--version” option but only “-v”
      option to disable building libraries (zcutil/build.sh)
      support per platform filename and hash setting for dependencies

