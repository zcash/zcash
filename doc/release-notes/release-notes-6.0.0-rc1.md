Notable changes
===============

- Windows builds have been fixed.

Changelog
=========

Daira-Emma Hopwood (2):
      Ensure out-reference parameters of `CWallet::CreateTransaction` are initialized.
      Rename ecc_addresses to bp_addresses in chainparams.cpp.

Jack Grigg (16):
      depends: Update Rust to 1.81.0
      depends: native_cmake 3.30.3
      depends: cxx 1.0.128
      cargo vet prune
      cargo update
      qa: Postpone Boost, LevelDB, and Clang updates
      Fix clippy lints for 1.81
      Remove `#[should_panic]` tests of `extern "C"` functions
      depends: Fix incompatibility between libsodium 1.0.20 and Clang 18
      depends: Downgrade libc++ for MinGW to 18.1.6-1
      Migrate to latest revision of Zcash Rust crates
      depends: native_cmake 3.30.4
      Update release notes
      Decrease support window to 6 weeks for 6.0.0-rc1
      make-release.py: Versioning changes for 6.0.0-rc1.
      make-release.py: Updated manpages for 6.0.0-rc1.

Kris Nuttycombe (1):
      Use scopes to make it more obvious that certain variables are never used.

y4ssi (2):
      fix gitian-descriptors
      Simplify Dockerfile (#6906)

