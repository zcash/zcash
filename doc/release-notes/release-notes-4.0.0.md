Notable changes
===============

The mainnet activation of the Canopy network upgrade is supported by the 4.0.0
release, with an activation height of 1046400, which should occur roughly in the
middle of November — following the targeted EOS halt of our 3.1.0 release.
Please upgrade to this release, or any subsequent release, in order to follow
the Canopy network upgrade.

The following ZIPs are being deployed as part of this upgrade:

* [ZIP 207: Funding Streams](https://zips.z.cash/zip-0207) in conjunction with [ZIP 214: Consensus rules for a Zcash Development Fund](https://zips.z.cash/zip-0214)
* [ZIP 211: Disabling Addition of New Value to the Sprout Value Pool](https://zips.z.cash/zip-0211)
* [ZIP 212: Allow Recipient to Derive Sapling Ephemeral Secret from Note Plaintext](https://zips.z.cash/zip-0212)
* [ZIP 215: Explicitly Defining and Modifying Ed25519 Validation Rules](https://zips.z.cash/zip-0215)

In order to help the ecosystem prepare for the mainnet activiation, Canopy has
already been activated on the Zcash testnet. Any node version 3.1.0 or higher,
including this release, supports the Canopy activation on testnet.

Disabling new value in the Sprout value pool
--------------------------------------------

After the mainnet activation of Canopy, it will not be possible to send funds to
Sprout z-addresses from any _other_ kind of address, as described in [ZIP 211](https://zips.z.cash/zip-0211).
It will still be possible to send funds _from_ a Sprout z-address and to send
funds between Sprout addresses. Users of Sprout z-addresses are encouraged to
use Sapling z-addresses instead, and to migrate their remaining Sprout funds
into a Sapling z-address using the migration utility in zcashd: set `migrate=1`
in your `zcash.conf` file, or use the `z_setmigration` RPC.

New logging system
------------------

The `zcashd` logging system is now powered by the Rust `tracing` crate. This
has two main benefits:

- `tracing` supports the concept of "spans", which represent periods of time
  with a beginning and end. These enable logging additional information about
  temporality and causality of events. (Regular log lines, which represent
  moments in time, are called `events` in `tracing`.)
- Spans and events are structured, and can record typed data in addition to text
  messages. This structure can then be filtered dynamically.

The existing `-debug=target` config flags are mapped to `tracing` log filters,
and will continue to correctly enable additional logging when starting `zcashd`.
A new `setlogfilter` RPC method has been introduced that enables reconfiguring
the log filter at runtime. See `zcash-cli help setlogfilter` for its syntax.

As a minor note, `zcashd` no longer reopens the `debug.log` file on `SIGHUP`.
This behaviour was originally introduced in upstream Bitcoin Core to support log
rotation using external tools. `tracing` supports log rotation internally (which
is currently disabled), as well as a variety of interesting backends (such as
`journald` and OpenTelemetry integration); we are investigating how these might
be exposed in future releases.

Compatibility
-------------
macOS versions earlier than 10.12 (Sierra) are no longer supported.

Changelog
=========

Alfredo Garcia (3):
      only allow duplicates for certain options of the config
      install bdb binaries
      add more allowed duplicates

Andrew Chow (1):
      Fix naming of macOS SDK and clarify version

Carl Dong (8):
      contrib: macdeploy: Correctly generate macOS SDK
      Adapt rest of tooling to new SDK naming scheme
      native_cctools: Don't use libc++ from pinned clang
      contrib: macdeploy: Use apple-sdk-tools instead of xar+pbzx
      contrib: macdeploy: Remove historical extraction notes
      depends: Decouple toolchain + binutils
      depends: Specify path to native binaries as clang argument
      depends: Add justifications for macOS clang flags

Charlie O'Keefe (1):
      Remove 'jessie' (debian 8) from suites list in linux gitian descriptors

Cory Fields (14):
      crypto: add AES 128/256 CBC classes
      crypto: add aes cbc tests
      crypter: fix the stored initialization vector size
      crypter: constify encrypt/decrypt
      crypter: hook up the new aes cbc classes
      crypter: add a BytesToKey clone to replace the use of openssl
      crypter: add tests for crypter
      build: Enumerate ctaes rather than globbing
      depends: bump MacOS toolchain
      macos: Bump to xcode 11.3.1 and 10.15 SDK
      darwin: pass mlinker-version so that clang enables new features
      depends: specify libc++ header location for darwin
      depends: enable lto support for Apple's ld64
      depends: bump native_cctools for fixed lto with external clang

Daira Hopwood (7):
      zcutil/distclean.sh: remove BDB utility programs.
      Update .gitignore.
      Fix a return status issue.
      Update Makefile.am
      Newer version of checksec.sh from https://github.com/slimm609/checksec.sh/commit/a6df608ac077689b2160e521db6601abc7b9e26e
      Repair full_test_suite.py for new checksec.sh.
      Clarify a comment about the ZF and MG addresses

Dimitris Apostolou (1):
      Remove deprecated contrib utilities

Jack Grigg (65):
      Move GrothProof and SproutProof definitions into zcash/Proof.hpp
      Remove unused declarations left over from libsnark verification
      Make ZCJoinSplit::prove static and remove ZCJoinSplit globals
      Move ProofVerifier out of the libzcash namespace
      Move JSDescription::Verify to ProofVerifier::VerifySprout
      Skip Sprout proof verification for ProofVerifier::Disabled
      Send alert to put pre-Heartwood nodes into safe mode.
      Squashed 'src/crypto/ctaes/' content from commit 003a4acfc
      build: shuffle gtest Makefile so that crypto can be used by the wallet
      metrics: Collect general stats before clearing screen
      Debian: Add copyright entries for ctaes and secp256k1
      Revert "Rename FALLBACK_DOWNLOAD_PATH to PRIORITY_DOWNLOAD_PATH"
      Revert "Try downloading from our mirror first to avoid headaches."
      depends: Use FALLBACK_DOWNLOAD_PATH if the primary's hash doesn't match
      test: Remove obsolete TransactionBuilder test
      Add tracing to librustzcash dependencies
      FFI wrapper around tracing crate
      Replace C++ logging with tracing logging
      Use a tracing EnvFilter directive for -debug flags
      Add support for reloading the tracing filter
      Add an RPC method for setting the tracing filter directives
      Add support for tracing spans
      Add some spans to the Zcash codebase
      FFI: Extract common codeunit types into a rust/types.h header
      tracing: Use 'static constexpr' hack in macros
      wallet: Fix logging to satisfy constexpr requirements
      FFI: Add missing <stddef.h> includes
      init: Place additional constraints on pathDebug
      rpc: Throw error in setlogfilter if filter reloading fails
      tracing: Log field values that aren't valid UTF-8
      tracing: Document macro arguments that MUST be static constant UTF-8 strings
      doc: Update release notes for tracing backend
      qa: Add tracing dependencies to updatecheck.py
      depends: tracing-core 0.1.13
      Revert "Add check-depends step to STAGE_COMMANDS list"
      contrib: Update macdeploy README
      depends: Rework Rust integration
      depends: Add platform-specific overrides for download files
      depends: Split check-packages and check-sources across categories
      FFI: Fix tracing log path handling on Windows
      tracing: Add MAP macro
      tracing: Add support for event fields
      tracing: Add support for span fields
      tracing: Format field values with Display
      Add fields to logging in CNode and UpdateTip
      util: Use DEBUG level for LogPrint(), leaving INFO for LogPrintf()
      tracing: Parse log_path into an Option<Path>
      tracing: Rework tracing_init into a single function
      init: Rework tracing_init call
      init: Add spans for initialization and shutdown
      Replace libsodium's randombytes_buf with rand_core::OsRng::fill_bytes
      consensus: Add assertions for Params::HalvingHeight parameters
      consensus: Document the empty conditional branch in ContextualCheckBlock
      consensus: Statically check funding stream numerators and denominators
      consensus: Clearly gate active funding stream elements on Canopy
      Replace libsodium's crypto_sign with ed25519-zebra
      ed25519: Panic (triggering abort) if nullptr passed into APIs
      test: Update ZIP 215 test cases from ed25519-zebra
      depends: Migrate to zcash_* 0.3.0 Rust crates
      FFI: Remove circuit parameter hashes from librustzcash_init_zksnark_params
      FFI: Migrate to bls12_381 and jubjub crates
      depends: cargo update
      qa: Update list of postponed crate versions
      FFI: Rename to librustzcash_sapling_compute_cmu
      FFI: Rename r to rcm

Kris Nuttycombe (13):
      Remove amqp code and Proton library depenencies & flags.
      Remove Proton license from contrib/debian/copyright
      consensus: Clean up some whitespace and variable names
      consensus: Refactor Sprout contextual rules to match the rest
      consensus: Remove canopyActive gate around GetActiveFundingStreamElements
      consensus: Combine heartwoodActive conditionals
      consensus: Add a placeholder for !canopyActive
      consensus: Move overwinterActive rules ahead of saplingActive rules
      consensus: Combine saplingActive conditionals
      consensus: Move Sapling-disabled Overwinter rules above Sapling rules
      consensus: Reorder Overwinter+!Sapling rules
      consensus: Remove redundant contextual consensus rules
      Add comment in lieu of redundant overwinter version check & fix tests.

Larry Ruane (2):
      flush wallet db (SetBestChain()) on clean shutdown
      wallet: lock cs_main while accessing chainActive

LongShao007 (1):
      fix bug of bdb.mk

Per Grön (11):
      Get rid of implicit hidden dependencies between test .cpp files
      Add missing #includes to test_block.cpp
      Add actual header file for utilities in gtest/utils.cpp
      Fix linkage issue with consts in primitives/block.h
      Remove Checkpoints_tests.cpp
      libsnark: Don't (implicitly) rely on other tests initializing the public params
      Add missing libsnark initialization call
      Don't clobber cwd in rpc_wallet_tests.cpp
      Include header files within the source tree using "" instead of <>
      Be consistent about what path to include bitcoin-config.h with
      Be consistent with how to #include test data headers

Pieter Wuille (1):
      Add ctaes-based constant time AES implementation

Sean Bowe (13):
      Postpone boost 1.74.0 update
      Postpone rust updates
      make-release.py: Versioning changes for 4.0.0-rc1.
      make-release.py: Updated manpages for 4.0.0-rc1.
      make-release.py: Updated release notes and changelog for 4.0.0-rc1.
      Add release notes to describe the Canopy network upgrade.
      Update names of contributors in release notes.
      Specify 4.0.0 in release notes
      Add dev fund addresses for mainnet.
      Set activation height for Canopy on mainnet.
      Postpone updates for dependencies until after 4.0.0 release.
      make-release.py: Versioning changes for 4.0.0.
      make-release.py: Updated manpages for 4.0.0.

Taylor Hornby (5):
      Implement system for postponing dependency updates.
      Change release instructions to block the release when dependencies are not updated and not postponed.
      Enforce pre-release dependency update check in make-release.py
      Extend deadline for postponing dependency updates
      Add new dependencies to updatecheck.py, add a flag we can use to have our CI test it.

Ariel Gabizon (1):
      explain expiry error

Benjamin Winston (2):
      Added support for afl-clang-fast.
      Added libfuzzer support.

elbandi (3):
      Allow configure params directory
      Add paramsdir option for manpage
      Throw error if -paramsdir not a valid directory

fanquake (8):
      depends: set OSX_MIN_VERSION to 10.10
      doc: mention that macOS 10.10 is now required
      scripted-diff: prefer MAC_OSX over `__APPLE__`
      build: set minimum supported macOS to 10.12
      depends: clang 6.0.1
      depends: native_cctools 921, ld64 409.12, libtapi 1000.10.8
      build: use macOS 10.14 SDK
      doc: explain why passing -mlinker-version is required

noname45688@gmail.com (2):
      Updating to Python 3
      Update to Python 3

Jack Grigg (2):
      debian: Rename X11 to Expat-with-advertising-clause in copyright
      Adjust GetActiveFundingStream* comments

teor (1):
      Fix a comment typo in pow.cpp

Ying Tong Lai (7):
      Add Debian8 deprecation to release notes
      Add missing curly braces after if statement
      Add test for garbage memory in history nodes
      Add documentation specific to ZIP 212
      Move esk derivation check to beginning of plaintext_checks_without_height()
      Define PRF diversifiers in prf.h
      assert(leadbyte == 0x02) after every if(leadbyte != 0x01)

ying tong (4):
      Update doc/release-notes/release-notes-3.1.0.md
      Make sure garbage bytes are different
      Rename PRV_DIVERSIFIER to PRF_TAG
      Add link to ZIP212 in coinbase comment

Zancas Wilcox (1):
      make deprecation.h include consensus/params.h

