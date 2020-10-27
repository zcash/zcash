Notable changes
===============

Migration to Clang and static libc++
------------------------------------

`zcashd` now builds its C++ (and C) dependencies entirely with a pinned version
of Clang, and statically links libc++ instead of dynamically linking libstdc++.
This migration enables us to reliably use newer C++ features while supporting
older LTS platforms, be more confident in the compiler's optimisations, and
leverage security features such as sanitisers and efficient fuzzing.

Additionally, because both Clang and rustc use LLVM as their backend, we can
optimise across the FFI boundary between them. This reduces the cost of moving
between C++ and Rust, making it easier to build more functionality in Rust
(though not making it costless, as we still need to work within the constraints
of the C ABI).

The system compiler is still used to compile a few native dependencies (used by
the build machine to then compile `zcashd` for the target machine). These will
likely also be migrated to use the pinned Clang in a future release.

Changelog
=========

Aditya Kulkarni (1):
      Add a config option to skip transaction verification in IBD mode

Ahmad Kazi (1):
      [Trivial] Add BITCOIN_FS_H endif footer in fs.h

Alfredo Garcia (9):
      add timestamp to warnings
      change order of returned pair, fix compatibility issue
      revert result key change, fix calls to getime
      add shielded balance to getwalletinfo
      Add null check to feof.
      allow wallet file to be outside datadir
      Apply suggestions from code review
      simplify TestBlockValidity
      update function comment

Ben Wilson (4):
      Added libtinfo5 to ci builder containers
      Added Arch and Centos to script, added libtinfo5 for arch
      Fixed Centos and Arch python requirements
      Build python for debian9 and ubuntu16.04

Carl Dong (3):
      depends: Build secondary deps statically.
      depends: Purge libtool archives
      scripted-diff: Run scripted-diff in subshell

Cory Fields (8):
      leveldb: integrate leveldb into our buildsystem
      build: No need to check for leveldb atomics
      build: out-of-tree fixups
      leveldb: enable runtime-detected crc32 instructions
      build: verify that the assembler can handle crc32 functions
      httpserver: use a future rather than relying on boost's try_join_for
      httpserver: replace boost threads with std
      devtools: add script to verify scriptable changes

Daira Hopwood (29):
      CBufferedFile: assert that Fill() is only called when nReadPos == nSrcPos, and simplify based on that assumption.
      CBufferedFile: use eof() method rather than feof(src) in error message.
      Make some conversions explicit to reduce sanitizer warnings.
      Rename z_*_balance fields of getwalletinfo output to shielded_*_balance
      Fix grammar in error messages.
      Ensure that the absolute path used in the test definitely does not exist.
      Line-wrap addition to README.md
      Minor additional OpenSSL scouring.
      Avoid undefined behaviour in scriptnum tests.
      Add assertions for CScriptNum[10] +/- int64_t to avoid the possibility of UB.
      It's unnecessary to pass int64_t by const reference.
      Cosmetics in CScriptNum code and tests.
      Add logging when we receive an invalid note plaintext (using the "receiveunsafe" log category).
      Fix a new warning about use of boost::bind placeholders after updating Boost. closes #4774
      Beef up the CoC to address use of dog-whistles.
      Fix warnings surfaced by compiling with clang++.
      Use the current time as the timestamp if we override a misc warning.
      qa/zcash/full_test_suite.py: changes needed for macOS. fixes #4785
      qa/zcash/full_test_suite.py: print immediately if a test fails.
      log(x)/log(2.0) can be written as log2(x).
      Fix integer types in DisplayDuration.
      Rename `time` to `duration` in `DisplayDuration`.
      Update contrib/devtools/symbol-check.py
      contrib/devtools/symbol-check.py: add info about Fedora-based distributions.
      Comment and error message cleanups for transaction checks.
      Add new copyright entries for build-aux/m4/ax_cxx_compile_stdcxx.m4
      Fix death gtests on macOS by switching to the threadsafe style.
      Fix an error reporting bug in "Checksum missing or mismatched ..."
      Rename the FS_ZIP214_ECC funding stream to FS_ZIP214_BP. See also https://github.com/zcash/zips/pull/412 .

Dimitris Apostolou (5):
      Remove reference to cargo-checksum.sh
      Fix typos
      Fix zeromq warning
      Remove deprecated init.md
      Remove Bitcoin release notes

Dimitris Tsapakidis (1):
      Fixed multiple typos

Hennadii Stepanov (1):
      Enable ShellCheck rules

Jack Grigg (79):
      Assorted small changes to the locked pool manager
      wallet: Add ANY_TADDR special string to z_sendmany
      Allow multiple nuparams options in config file
      depends: Switch to `cargo vendor` for Rust dependencies
      QA: Comment out Rust crate checks in updatecheck.py
      depends: Ensure that SOURCES_PATH exists before vendoring crates
      wallet: Ignore coinbase UTXOs with z_sendmany ANY_TADDR
      rpc: Fix comma spacing in example z_sendmany commands
      Squashed 'src/leveldb/' changes from 20ca81f08..a31c8aa40
      Squashed 'src/leveldb/' changes from a31c8aa40..196962ff0
      Squashed 'src/leveldb/' changes from 196962ff0..c521b3ac6
      Squashed 'src/leveldb/' changes from c521b3ac6..64052c76c
      Squashed 'src/leveldb/' changes from 64052c76c..524b7e36a
      Squashed 'src/leveldb/' changes from 524b7e36a..f545dfabf
      depends: Remove cargo-checksum.sh
      Replace libsodium's crypto_generichash_blake2b with blake2b_simd
      blake2b: Allow consuming partial BLAKE2b output
      tracing: Correctly override tracing::Span move constructors
      build: Remove Rust staticlib naming workaround
      depends: Update to latest config.guess & config.sub
      build: out-of-tree fixups
      leveldb: Assert that ssize_t is the same size as size_t on Windows
      LockedPool: Fix LockedPool::free(nullptr) to be a no-op
      LockedPool: Make Arena::free and LockedPool::free noexcept
      allocators: Apply Allocator named requirements to secure_allocator::deallocate
      depends: Update map of GCC canonical hosts to Rust targets
      QA: Switch to x86_64-pc-linux-gnu for hard-coded Linux HOST
      build: Switch to x86_64-pc-linux-gnu for codecov filtering
      gitian: Switch from x86_64-unknown-linux-gnu to x86_64-linux-gnu
      util: Remove OpenSSL multithreading infrastructure
      Remove remaining OpenSSL references
      QA: Remove OpenSSL from updatecheck.py
      build: Remove a stray -lcrypto
      Squashed 'src/secp256k1/' changes from 6ad5cdb42..8ab24e8da
      build: Use the endomorphism optimization for secp256k1
      depends: libevent 2.1.12
      depends: ccache 3.7.11
      depends: googletest 1.8.1
      depends: utfcpp 3.1.2
      depends: Use correct HOST for download-linux target
      QA: Fix backporting bugs in httpbasics.py
      depends: Boost 1.74.0
      depends: ccache 3.7.12
      cargo update
      depends: ZeroMQ 4.3.3
      FFI: Merge librustzcash_init_zksnark_params variants into one function
      depends: Postpone current and scheduled Rust releases until 2021
      MOVEONLY: Move logging code from util.{h,cpp} to new files
      depends: Add Clang 8.0.0
      depends: Use vendored Clang for native compilation
      depends: Use vendored Clang for macOS cross-compilation
      depends: Vendor LLD and use it for linking
      depends: Add libc++ as a dependency
      depends: Don't replace default CXXFLAGS in C++ dependencies
      depends: Add multilib paths for Linux cross-compile
      build: Statically link libc++
      build: Add missing LIBUNIVALUE to Makefile.bench.include LDADD
      depends: Fix "unused variables" warning when compiling zeromq for Windows
      depends: Rename Boost libraries to follow MinGW/GCC convention
      depends: Fix boost::iostreams usage on Windows with libc++
      build: Compile secp256k1 with C99
      build: Add -lpthread to univalue test LDFLAGS
      qa: Disable FORTIFY_SOURCE checks
      QA: Add native_clang and libcxx to updatecheck.py
      test: Fix various pyflakes warnings
      doc: Add Clang and libc++ migration to release notes
      build: Update AX_CXX_COMPILE_STDCXX macro
      build: Require and build with C++ 17
      depends: Build C++ dependencies with C++ 17
      Switch from std::random_shuffle to std::shuffle
      Squashed 'src/secp256k1/' changes from 8ab24e8da..c6b6b8f1b
      build: Update secp256k1 configure flags
      Improve reject reasons for unmet shielded requirements
      Add logging to CCoinsViewCache::HaveShieldedRequirements
      utils: Remove unnecessary GetTempPath()
      Add txid to "shielded requirements not met" messages
      test/lint: Check for working changes before checking scripted diffs
      tests: Update chained_joinsplits test for HaveShieldedRequirements API change
      scripted-diff: Remove BOOST_STATIC_ASSERT

Jeffrey Czyz (2):
      Fix compilation errors in support/lockedpool.cpp
      Fix segfault in allocator_tests/arena_tests

Jeremy Rubin (1):
      Fix subscript[0] potential bugs in key.cpp

John Newbery (1):
      [docs] document scripted-diff

Jonas Schnelli (2):
      NotifyBlockTip signal: switch from hash (uint256) to CBlockIndex*
      Move uiInterface.NotifyBlockTip signal above the core/wallet signal

Kaz Wesley (3):
      LockedPool: test handling of invalid allocations
      LockedPool: fix explosion for illegal-sized alloc
      LockedPool: avoid quadratic-time allocation

Kris Nuttycombe (14):
      Prevent creation of shielded transactions in initial block download.
      Revert the move of the `getBalanceZaddr` block for ease of review.
      Fix forward declaration.
      Remove redundant CheckBlock calls.
      Reduce diff complexity.
      Apply style suggestions from code review
      -ibdskiptxverification must imply -checkpoints
      Apply suggestions from code review
      Ensure conflicting flags are reported as an error.
      Reject incompatible flags in "Step 2"
      Rename IBDSkipTxVerification back to ShouldCheckTransaction
      Fix command-line help for -ibdskiptxverification
      Fix invocation of updatecheck.py in make-release.py
      Replace invalid characters in log message decoding.

Larry Ruane (4):
      Flush witness data when consistent (part 2)
      performance: auto params = CChainParams::GetConsensus()
      allow getaddressutxos if -lightwalletd
      add z_gettreestate rpc

Luke Dashjr (2):
      lockedpool: When possible, use madvise to avoid including sensitive information in core dumps
      Add MIT license to Makefiles

Marco Falke (3):
      Limit scope of all global std::once_flag
      Add extra LevelDB source to Makefile
      test: Move linters to test/lint, add readme

Martin Ankerl (2):
      Use best-fit strategy in Arena, now O(log(n)) instead O(n)
      fix nits: variable naming, typos

Mustafa (2):
      Add a source file for unit test utils.
      Move GetTempPath() to testutil.

Nate Wilcox (3):
      Link the README.md to the specific readthedocs.io page for building on Debian/Ubuntu.
      Convert a sed command to a static patch file.
      depends: fix a logging bug for multi-archive packages.

Nick (1):
      [RPC] Add transaction size to JSON output

Pavel Janík (2):
      Do not shadow variable, use deprecated MAP_ANON if MAP_ANONYMOUS is not defined.
      Do not include env_win.cc on non-Windows systems

Pieter Wuille (2):
      Remove some unused functions and methods
      Fail on commit with VERIFY SCRIPT but no scripted-diff

Sean Bowe (3):
      Update Rust to 1.44.1.
      cargo update
      Update to latest zcash_* and zkcrypto crates.

Taylor Hornby (3):
      Fix buffer overflows in P2PKH tests
      Add a missing % to a string interpolation in rpc test framework
      Fix undefined behavior in the test_bitcoin tests

Thomas Snider (1):
      [trivial] Switched constants to sizeof()

Vasil Dimov (1):
      lockedpool: avoid sensitive data in core files (FreeBSD)

Wladimir J. van der Laan (19):
      wallet: Change CCrypter to use vectors with secure allocator
      wallet: Get rid of LockObject and UnlockObject calls in key.h
      support: Add LockedPool
      rpc: Add `getmemoryinfo` call
      bench: Add benchmark for lockedpool allocation/deallocation
      http: Restrict maximum size of request line + headers
      Replace scriptnum_test's normative ScriptNum implementation
      build: remove libcrypto as internal dependency in libbitcoinconsensus.pc
      http: Do a pending c++11 simplification
      http: Add log message when work queue is full
      http: Change boost::scoped_ptr to std::unique_ptr in HTTPRequest
      http: use std::move to move HTTPRequest into HTTPWorkItem
      Add fs.cpp/h
      Replace includes of boost/filesystem.h with fs.h
      Replace uses of boost::filesystem with fs
      Use fsbridge for fopen and freopen
      torcontrol: Use fs::path instead of std::string for private key path
      Remove `namespace fs=fs`
      test: Mention commit id in scripted diff error

fanquake (8):
      build: remove SSL lib detection
      build: remove OpenSSL detection and libs
      depends: remove OpenSSL package
      doc: remove OpenSSL from build instructions and licensing info
      depends: Disable unused ZeroMQ features
      depends: zeromq: disable draft classes and methods
      build: only pass --disable-dependency-tracking to packages that understand it
      build: pass --enable-option-checking to applicable packages

mruddy (1):
      [depends, zmq, doc] avoid deprecated zeromq api functions

practicalswift (7):
      Fix out-of-bounds write in case of failing mmap(...) in PosixLockedPageAllocator::AllocateLocked
      Improve readability by removing redundant casts to same type (on all platforms)
      tests: Remove OldSetKeyFromPassphrase/OldEncrypt/OldDecrypt
      Remove unused Boost includes
      Add "export LC_ALL=C" to all shell scripts
      Add error handling: exit if cd fails
      Use bash instead of POSIX sh. POSIX sh does not support arrays.

Jack Grigg (9):
      Update license headers
      leveldb: Fix typo
      LockedPool: Switch to HTTPS URLs in licenses and comments
      test: Fix LFSR period in comments
      httpserver: Code style cleanups
      depends: Update packages documentation for Zcash
      depends: Add untested note to FreeBSD host
      Update example scripted-diff comit in doc/developer-notes.md
      Use HTTPS in script license headers

syd (1):
      Add assert_raises_message to the python test framework.

Ying Tong Lai (9):
      Add funding stream addresses to getblocksubsidy RPC output
      Fix CScript encoding
      Handle shielded address case
      Minor cleanups
      Only return address instead of CScript
      Remove void declaration of ScriptPubKeyToJSON()
      Postpone native_ccache 4.0
      make-release.py: Versioning changes for 4.1.0-rc1.
      make-release.py: Updated manpages for 4.1.0-rc1.

ying tong (1):
      Apply suggestions from code review

Benjamin Winston (1):
      Postponed dependency updates, refer to core team sync meeting.

