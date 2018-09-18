Notable changes
===============

`-disabledeprecation` removal
-----------------------------

In release 1.0.9 we implemented automatic deprecation of `zcashd` software
versions made by the Zcash Company. The configuration option
`-disabledeprecation` was added as a way for users to specifically choose to
stay on a particular software version. However, it incorrectly implied that
deprecated releases would still be supported.

This release removes the `-disabledeprecation` option, so that `zcashd` software
versions made by the Zcash Company will always shut down in accordance with the
defined deprecation policy (currently 16 weeks after release). Users who wish to
use a different policy must now specifically choose to either:

- edit and compile the source code themselves, or
- obtain a software version from someone else who has done so (and obtain
  support from them).

Either way, it is much clearer that the software they are running is not
supported by the Zcash Company.

Changelog
=========

Ariel Gabizon (1):
      Improve/Fix variable names

Daira Hopwood (1):
      Update code_of_conduct.md

Eirik Ogilvie-Wigley (5):
      Add tests for sapling anchors
      Add hashFinalSaplingRoot to getblocktemplate
      Fix parsing parameters in getnetworksolps
      Add BOOST_TEST_CONTEXT to distinguish sprout v. sapling
      Rename typename

Jack Grigg (16):
      Replace boost::array with std::array
      Add MacOS support to no-dot-so test
      Whitespace cleanup
      chainparams: Add Sapling Bech32 HRPs
      ConvertBits() - convert from one power-of-2 number base to another.
      Fix bech32::Encode() error handling
      Implement encoding and decoding of Sapling keys and addresses
      Add Mach-O 64-bit detection to security-check.py
      Fix cached_witnesses_empty_chain test failure on MacOS
      Skip ELF-only sec-hard checks on non-ELF binaries
      Remove config option -disabledeprecation
      Add release notes for -disabledeprecation removal
      Add comment about size calculations for converted serialized keys
      Add examples of ConvertBits transformation
      Use CChainParams::Bech32HRP() in zs_address_test
      Add hashFinalSaplingRoot to getblockheader and getblock output

Jay Graber (8):
      Add Sapling key classes to wallet, with new librustzcash APIs
      Change librustzcash dependency hash to work for new Sapling classes
      Update librustzcash dependency, address comments
      Minimal sapling key test
      Fix default_address()
      s/SaplingInViewingKey/SaplingIncomingViewingKey
      Make diversifier functions return option
      Add json test vectors for Sapling key components.

Jonathan "Duke" Leto (1):
      Clarify help that signmessage only works on taddrs

Larry Ruane (1):
      (rpc-test) accurately account for fee without rounding error

Matthew King (2):
      Use portable #! in python scripts (/usr/bin/env)
      Favour python over python2 as per PR #7723

Paige Peterson (1):
      include note about volunteers in CoC

Pieter Wuille (2):
      Generalize ConvertBits
      Simplify Base32 and Base64 conversions

Simon Liu (12):
      Part of #3277. Add comment about deprecated txdb prefixes.
      Remove now redundant Rust call to librustzcash_xor.
      Add SaplingNote class and test_sapling_note unit test.
      Refactor and replace factory method random() with constructor.
      Return optional for Sapling commitments and nullifiers.
      Closes #3328. Send alert to put non-Overwinter nodes into safe mode.
      Fix pyflakes error in test zkey_import_export.
      make-release.py: Versioning changes for 1.1.2-rc1.
      make-release.py: Updated manpages for 1.1.2-rc1.
      make-release.py: Updated release notes and changelog for 1.1.2-rc1.
      make-release.py: Versioning changes for 1.1.2.
      make-release.py: Updated manpages for 1.1.2.

