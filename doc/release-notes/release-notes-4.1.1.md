Notable changes
===============

Optimize release build
----------------------
The release build now sets CLAGS/CXXFLAGS to use the -O3 optimization
option, which turns on more optimization flags than the previously used
-O1. This produces a faster build, addressing a performance regression in 
v4.1.0.

Correctly report Founders' Reward amount in `getblocktemplate`
--------------------------------------------------------------
This release correctly returns the `foundersreward` field from `getblocktemplate`
output pre-Canopy and removes the field post-Canopy. (The Founders' Reward will
expire exactly as Canopy activates, as specified in [ZIP 207](https://zips.z.cash/zip-0207).)
To obtain information about funding stream amounts, use `getblocksubsidy HEIGHT`,
passing in the height returned by the `getblocktemplate` API.

Changelog
=========

Akio Nakamura (1):
      [script] lint-whitespace: improve print linenumber

Alfredo Garcia (1):
      add myblockhash parameter to getrawtransaction

Daira Hopwood (3):
      Wording improvements to getrawtransaction RPC documentation
      GetNextWorkRequired: clarify why this computation is equivalent to that in the protocol spec. refs https://github.com/zcash/zips/pull/418
      Set release CFLAGS/CXXFLAGS to use -O3.

Dan Raviv (1):
      Fix header guards using reserved identifiers

DesWurstes (1):
      Obsolete #!/bin/bash shebang

Evan Klitzke (1):
      Add a lint check for trailing whitespace.

Jack Grigg (14):
      lints: Use Zcash-specific include guards for new files
      lints: Update expected Boost imports
      lints: Match `export LC_ALL="C"` in lint-shell-locale
      test: Fix pyflakes warning in bitcoin-util-test.py
      lint: Fix missing or inconsistent include guards
      lint: Fix duplicate includes
      python: Explicitly set encoding to utf8 when opening text files
      lint: Use consistent shebangs
      lint: Opt out of locale dependence in Zcash shell scripts
      lint: Re-exclude subtrees from lint-include-guards.sh
      lint: Apply include guard style to src/rust/include
      lint: s/trim/lenTrim in src/crypto/equihash.[cpp,h]
      lint: Fix minor shellcheck lints
      cargo update

John Newbery (4):
      [contrib] convert test-security-check to python3
      Clean up bctest.py and bitcoin-util-test.py
      Improve logging in bctest.py if there is a formatting mismatch
      [linter] Strip trailing / in path for git-subtree-check

João Barbosa (1):
      qa: Ignore shellcheck warning SC2236

Julian Fleischer (3):
      fix locale for lint-shell
      use export LC_ALL=C.UTF-8
      Run all lint scripts

Kris Nuttycombe (6):
      Change order of checks to skip IsInitialBlockDownload check if flag is unset.
      Correctly report founder's reward amount in getblocktemplate prior to Canopy
      Document how to get block subsidy information in getblocktemplate.
      Update getblocktemplate documentation.
      make-release.py: Versioning changes for 4.1.1.
      make-release.py: Updated manpages for 4.1.1.

Kristaps Kaupe (1):
      Make lint-includes.sh work from any directory

Marco Falke (4):
      devtools: Exclude patches from lint-whitespace
      Refine travis check for duplicate includes
      test: Move linters to test/lint, add readme
      Revert "Remove unused variable in shell script"

MeshCollider (1):
      Add tab char lint check and exclude imported dependencies

Philip Kaufmann (1):
      [Trivial] ensure minimal header conventions

Pieter Wuille (1):
      Improve git-subtree-check.sh

Sjors Provoost (3):
      [scripts] lint-whitespace: use perl instead of grep -P
      [scripts] lint-whitespace: check last N commits or unstaged changes
      doc: improve subtree check instructions

Vidar Holen (1):
      refactor/lint: Add ignored suggestions to an array

Wladimir J. van der Laan (4):
      contrib: Ignore historical release notes for whitespace check
      test: Add format-dependent comparison to bctest
      test: Explicitly set encoding to utf8 when opening text files
      uint256: replace sprintf with HexStr and reverse-iterator

adityapk00 (1):
      Don't compile ehHashState::* if mining is disabled

isle2983 (1):
      [copyright] add MIT license headers to .sh scripts where missing

jnewbery (5):
      Add bitcoin-tx JSON tests
      Add option to run bitcoin-util-test.py manually
      bitcoin-util-test.py should fail if the output file is empty
      add verbose mode to bitcoin-util-test.py
      Add logging to bitcoin-util-test.py

practicalswift (20):
      Document include guard convention
      Fix missing or inconsistent include guards
      Add lint-include-guards.sh which checks include guard consistency
      Add Travis check for duplicate includes
      Add shell script linting: Check for shellcheck warnings in shell scripts
      add lint tool to check python3 shebang
      build: Guard against accidental introduction of new Boost dependencies
      build: Add linter for checking accidental locale dependence
      docs: Mention lint-locale-dependence.sh in developer-notes.md
      Add "export LC_ALL=C" to all shell scripts
      Add linter: Make sure all shell scripts opt out of locale dependence using "export LC_ALL=C"
      Explicitly specify encoding when opening text files in Python code
      Add linter: Make sure we explicitly open all text files using UTF-8 or ASCII encoding in Python
      macOS fix: Work around empty (sub)expression error when using BSD grep
      macOS fix: Add excludes for checks added in the newer shellcheck version installed by brew
      Remove repeated suppression. Fix indentation.
      Fix warnings introduced in shellcheck v0.6.0
      Remove no longer needed shellcheck suppressions
      Follow-up to #13454: Fix broken build by exporting LC_ALL=C
      Remove unused variables in shell scripts.

Jack Grigg (5):
      Small documentation fixes
      lints: Add a missing copyright header
      lint: Allow stoi in src/rpc/blockchain.cpp
      lint: Remove some subtrees from exclusion
      doc: Adjust subtree developer notes to refer to Zcash

vim88 (1):
      Scripts and tools & Docs: Used #!/usr/bin/env bash instead of obsolete #!/bin/bash, added linting for .sh files shebang and updated the Developer Notes.

