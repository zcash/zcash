(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

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
