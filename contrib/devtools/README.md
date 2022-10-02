Contents
========

This directory contains tools for developers working on this repository.

check-doc.py
============

Check if all command line args are documented. The return value indicates the
number of undocumented args.

github-merge.sh
===============

A small script to automate merging pull-requests securely and sign them with GPG.

For example:

  ./github-merge.sh bitcoin/bitcoin 3077

(in any git repository) will help you merge pull request #3077 for the
bitcoin/bitcoin repository.

What it does:
* Fetch master and the pull request.
* Locally construct a merge commit.
* Show the diff that merge results in.
* Ask you to verify the resulting source tree (so you can do a make
check or whatever).
* Ask you whether to GPG sign the merge commit.
* Ask you whether to push the result upstream.

This means that there are no potential race conditions (where a
pullreq gets updated while you're reviewing it, but before you click
merge), and when using GPG signatures, that even a compromised github
couldn't mess with the sources.

Setup
-----
Configuring the github-merge tool for the bitcoin repository is done in the following way:

    git config githubmerge.repository zcash/zcash
    git config githubmerge.testcmd "make -j4 check" (adapt to whatever you want to use for testing)
    git config --global user.signingkey mykeyid (if you want to GPG sign)

fix-copyright-headers.py
========================

Every year newly updated files need to have its copyright headers updated to reflect the current year.
If you run this script from src/ it will automatically update the year on the copyright header for all
.cpp and .h files if these have a git commit from the current year.

For example a file changed in 2014 (with 2014 being the current year):
```// Copyright (c) 2009-2013 The Bitcoin Core developers```

would be changed to:
```// Copyright (c) 2009-2014 The Bitcoin Core developers```

This script has *not* been updated for Zcash.

rust-deps-graph.sh
==================

A utility that outputs an image `rust-dependency-graph.png` showing the graph of dependencies
between Rust crates used by zcashd.

This depends on `cargo deps` which can be installed using `cargo install cargo-deps`.
The arguments to `cargo deps` can be overridden by specifying them as arguments to the script
(default: `--no-transitive-deps`).

security-check.py
=================

Perform security hardening checks on the list of executables given on the command line:

- check for a position-independent executable, allowing address space randomization (ASLR);
  - for Windows executables, check that high-entropy ASLR is enabled;
- check that no sections are writable and executable, including the stack;
  - for Windows executables, check that Data Execution Prevention is enabled;
- for ELF executables, check for read-only relocations (RELRO and BIND_NOW);
- for ELF executables, check that stack canaries are enabled.

The stack canary check will report a false positive in the case of Rust ELF executables (e.g.
`zcash-inspect` and `zcashd-wallet-tool` on non-Windows platforms). The `--allow-no-canary`
flag suppresses this warning.

symbol-check.py
===============

A script to check that the (Linux) executables produced by gitian only contain
allowed gcc, glibc and libc++ version symbols.  This makes sure they are
still compatible with the minimum supported Linux distribution versions.

Example usage after a gitian build:

    find ../gitian-builder/build -type f -executable | xargs python contrib/devtools/symbol-check.py 

If only supported symbols are used the return value will be 0 and the output will be empty.

If there are 'unsupported' symbols, the return value will be 1 a list like this will be printed:

    .../64/test_bitcoin: symbol memcpy from unsupported version GLIBC_2.14
    .../64/test_bitcoin: symbol __fdelt_chk from unsupported version GLIBC_2.15
    .../64/test_bitcoin: symbol std::out_of_range::~out_of_range() from unsupported version GLIBCXX_3.4.15
    .../64/test_bitcoin: symbol _ZNSt8__detail15_List_nod from unsupported version GLIBCXX_3.4.15

gen-manpages.sh
===============

A small script to automatically create manpages in `doc/man` by running the release binaries
with the `-help` option. This script is normally run by `zcutil/make-release.py`, not directly.
It requires `help2man` which can be found at: https://www.gnu.org/software/help2man/

test-security-check.py
======================

Tests for `security-check.py`.

update-clang-hashes.sh
======================

A script to update the hashes for clang and libc++ tarballs in `depends/packages/native_clang.mk`
and `depends/packages/libcxx.mk`. See the comments at the top of `depends/packages/native_clang.mk`
for intended usage.

update-rust-hashes.sh
=====================

A script to update the hashes for the Rust toolchain in `depends/packages/native_rust.mk`.
See the comments at the top of that file for intended usage.
