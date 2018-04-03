Notable changes
===============

Incoming viewing keys
---------------------

Support for incoming viewing keys, as described in
[the Zcash protocol spec](https://github.com/zcash/zips/blob/master/protocol/protocol.pdf),
has been added to the wallet.

Use the `z_exportviewingkey` RPC method to obtain the incoming viewing key for a
z-address in a node's wallet. For Sprout z-addresses, these always begin with
"ZiVK" (or "ZiVt" for testnet z-addresses). Use `z_importviewingkey` to import
these into another node.

A node that possesses an incoming viewing key for a z-address can view all past
transactions received by that address, as well as all future transactions sent
to it, by using `z_listreceivedbyaddress`. They cannot spend any funds from the
address. This is similar to the behaviour of "watch-only" t-addresses.

`z_gettotalbalance` now has an additional boolean parameter for including the
balance of "watch-only" addresses (both transparent and shielded), which is set
to `false` by default. `z_getbalance` has also been updated to work with
watch-only addresses.

- **Caution:** for z-addresses, these balances will **not** be accurate if any
  funds have been sent from the address. This is because incoming viewing keys
  cannot detect spends, and so the "balance" is just the sum of all received
  notes, including ones that have been spent. Some future use-cases for incoming
  viewing keys will include synchronization data to keep their balances accurate
  (e.g. [#2542](https://github.com/zcash/zcash/issues/2542)).

Changelog
=========

Anthony Towns (1):
      Add configure check for -latomic

Cory Fields (12):
      c++11: don't throw from the reverselock destructor
      c++11: CAccountingEntry must be defined before use in a list
      c++11: fix libbdb build against libc++ in c++11 mode
      depends: use c++11
      depends: bump OSX toolchain
      build: Split hardening/fPIE options out
      build: define base filenames for use elsewhere in the buildsystem
      build: quiet annoying warnings without adding new ones
      build: fix Windows builds without pkg-config
      build: force a c++ standard to be specified
      build: warn about variable length arrays
      build: add --enable-werror option

Jack Grigg (40):
      Squashed 'src/secp256k1/' changes from 84973d3..6ad5cdb
      Use g-prefixed coreutils commands if they are available
      Replace hard-coded defaults for HOST and BUILD with config.guess
      Remove manual -std=c++11 flag
      Replace "install -D" with "mkdir -p && install"
      Check if OpenMP is available before using it
      [libsnark] Use POSIX-compliant ar arguments
      Include endian-ness compatibility layer in Equihash implementation
      build: Split hardening/fPIE options out in Zcash-specific binaries
      Change --enable-werror to apply to all warnings, use it in build.sh
      Move Zcash flags into configure.ac
      ViewingKey -> ReceivingKey per zcash/zips#117
      Implement viewing key storage in the keystore
      Factor out common logic from CZCPaymentAddress and CZCSpendingKey
      Track net value entering and exiting the Sprout circuit
      Add Sprout value pool to getblock and getblockchaininfo
      Apply -fstack-protector-all to libsnark
      Add Rust and Proton to configure options printout
      Clarify operator precedence in serialization of nSproutValue
      Remove nSproutValue TODO from CDiskBlockIndex
      Add Base58 encoding of viewing keys
      Implement viewing key storage in the wallet
      Add RPC methods for exporting/importing viewing keys
      Update wallet logic to account for viewing keys
      Add watch-only support to Zcash RPC methods
      Modify zcrawkeygen RPC method to set "zcviewingkey" to the viewing key
      Cleanup: Add braces for clarity
      Add cautions to z_getbalance and z_gettotalbalance help text about viewing keys
      Add release notes for incoming viewing keys
      Create release notes starting from the previous non-beta non-RC release
      release-notes.py: Remove unnecessary parameter
      Regenerate previous release notes to conform to new format
      Exclude beta and RC release notes from author tallies
      Fix pyflakes warnings in zkey_import_export RPC test
      make-release.py: Versioning changes for 1.0.14-rc1.
      make-release.py: Updated manpages for 1.0.14-rc1.
      make-release.py: Updated release notes and changelog for 1.0.14-rc1.
      Update release process
      make-release.py: Versioning changes for 1.0.14.
      make-release.py: Updated manpages for 1.0.14.

Jay Graber (3):
      Add cli and rpc examples for z_sendmany
      Fix cli help result for z_shieldcoinbase
      Add rpc test that exercises z_importkey

Jonas Schnelli (1):
      Add compile and link options echo to configure

Luke Dashjr (4):
      depends: Use curl for fetching on Linux
      Travis: Use curl rather than wget for Mac SDK
      Bugfix: depends/Travis: Use --location (follow redirects) and --fail [on HTTP error response] with curl
      Travis: Use Blue Box VMs for IPv6 loopback support

MarcoFalke (2):
      Fix url in .travis.yml
      [depends] builders: No need to set -L and --location for curl

Per Grön (2):
      Deduplicate test utility method wait_and_assert_operationid_status
      Print result of RPC call in test only when PYTHON_DEBUG is set

René Nyffenegger (1):
      Use AC_ARG_VAR to set ARFLAGS.

Simon Liu (5):
      RPC dumpwallet and z_exportwallet updated to no longer allow     overwriting an existing file.
      Add documentation for shielding coinbase utxos.
      Add documentation for payment disclosure.
      Closes #2759. Fixes broken pipe error with QA test wallet.py.
      Closes #2746. Payment disclosure blobs now use 'zpd:' prefix.

Wladimir J. van der Laan (6):
      build: Enable C++11 build, require C++11 compiler
      build: update ax_cxx_compile_stdcxx to serial 4
      test: Remove java comparison tool
      build: Remove check for `openssl/ec.h`
      devtools: Check for high-entropy ASLR in 64-bit PE executables
      build: supply `-Wl,--high-entropy-va`

daniel (1):
      add powerpc build support for openssl lib

fanquake (3):
      [build-aux] Update Boost & check macros to latest serials
      [depends] Add -stdlib=libc++ to darwin CXX flags
      [depends] Set OSX_MIN_VERSION to 10.8

kozyilmaz (1):
      empty spaces in PATH variable cause build failure

syd (13):
      Upgrade googletest to 1.8.0
      Get the sec-hard tests to run correctly.
      Update libsodium from 1.0.11 to 1.0.15
      Remove Boost conditional compilation.
      Update to address @daira comments wrt fixing configure.ac
      Get rid of consensus.fPowAllowMinDifficultyBlocks.
      Don't compile libgtest.a when building libsnark.
      Add gtests to .gitignore
      Get rid of fp3 from libsnark, it is not used.
      InitGoogleMock instead of InitGoogleTest per CR
      Get rid of underscore prefixes for include guards.
      Rename bash completion files so that they refer to zcash and not bitcoin.
      Fix libsnark test failure.

