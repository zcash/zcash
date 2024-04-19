This is a maintenance release that updates dependencies and sets a new
end-of-service height to help ensure continuity of services.

Notable changes
===============

License documentation has been updated to account for the `orchard` crate
now being licensed under "MIT OR Apache-2.0".

Platform Support
----------------

- Debian 11 (Bookworm) is now a Tier 1 platform.

- Intel macOS will be formally downgraded to a Tier 3 platform in the next
  release. Previously it has informally been at both Tier 1 (because builds and
  tests were run in CI) and Tier 3 (because no packages were provided). However,
  we are currently unable to update the Clang and Rust compilers due to there
  being no Clang release built for Intel macOS from LLVM 16 onwards. Combined
  with the fact that the Intel Macbook was discontinued in 2021, this decision
  means that we will no longer be building `zcashd` natively on Intel macOS in
  CI for testing purposes. We will not be removing build system support (so
  builds may still function, and community patches to fix issues are welcomed).

Changelog
=========

Conrado Gouvea (2):
      make cache.cpp compile with MSVC
      Update src/zcash/cache.h

Daira Emma Hopwood (3):
      Lead with "Zcash is HTTPS for money" in both the README and the Debian package description. (This also fixes a typo in the latter.)
      Change my name in Cargo.toml and remove old email addresses for Ying Tong Lai, Greg Pfeil, and Steven Smith.
      Remove references to the BOSL license, which will no longer be used by any dependency as of the next release.

Daira-Emma Hopwood (6):
      Update or postpone dependencies for zcashd 5.9.0.
      deny.toml: remove license exception for orchard.
      deny.toml: migrate to version 2 to avoid some warnings.
      Additional updated and postponed dependencies for zcashd 5.9.0.
      * cargo update * cargo update -p home@0.5.9 --precise 0.5.5
      Update audits.

Jack Grigg (14):
      CI: Migrate to `{upload, download}-artifact@v4`
      CI: Fix name for gtest job
      CI: Run bitrot build jobs in parallel
      CI: Run Rust tests
      rust: Migrate to `zcash_primitives 0.14.0`
      depends: Postpone LLVM 18.1.4
      docs: Document Debian 12 as a Tier 1 platform
      make-release.py: Versioning changes for 5.9.0-rc1.
      make-release.py: Updated manpages for 5.9.0-rc1.
      make-release.py: Updated release notes and changelog for 5.9.0-rc1.
      make-release.py: Updated book for 5.9.0-rc1.
      Document that Intel macOS will be formally moved to Tier 3 in 5.10.0
      make-release.py: Versioning changes for 5.9.0.
      make-release.py: Updated manpages for 5.9.0.

Yasser Isa (13):
      CI: Specify `HOST` for every build instead of just cross-compiles
      CI: Cache Sprout parameters during setup phase
      CI: Store build artifacts in GCS instead of GitHub
      CI: Add `no-dot-so` lint
      CI: Add `sec-hard` test
      CI: Add remaining unit tests
      add pull_request_target in the CI
      Revert "Update the CI to include `pull_request_target`"
      ADD support to Bookworm
      Create docker-release.yaml in Github Actions
      Allow running CI from a fork
      Debian CI in Github Actions
      fix CI - cache sprout params

shuoer86 (2):
      Fix typos
      Fix typos

