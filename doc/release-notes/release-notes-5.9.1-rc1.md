Notable changes
===============

- Added a `z_converttex` RPC method to support conversion of transparent
  p2pkh addresses to the ZIP 320 (TEX) format.

- zcashd will now disconnect from a peer on receipt of a malformed `version`
  message, and also will reject duplicate `verack` messages from outbound
  connections. See https://github.com/zcash/zcash/pull/6781 for additional
  details.


Changelog
=========

Daira-Emma Hopwood (4):
      Remove code that is definitely dead, because pfrom->nVersion must be >= MIN_PEER_PROTO_VERSION (170100) at this point.
      Fix some issues with handling of version messages: * fields were deserialized into variables of platform-dependent length; * pfrom->nVersion was set even if it does not meet the minimum version   requirements; * make sure that pfrom->nTimeOffset is set before   pfrom->fSuccessfullyConnected.
      Treat a malformed version message as cause for disconnection.
      Reject duplicate verack.

Daira-Emma Hopwood (3):
      zcutil/release-notes.py: fix a bug in the counting of commits for each author.
      zcutil/release-notes.py: Make the doc/authors.md file correct Markdown.
      net: define NodeId as an int64_t

Fabian Jahr (1):
      naming nits

Jack Grigg (23):
      depends: Update Rust to 1.78.0
      depends: Update Clang / libcxx to LLVM 18.1.4
      rust: Fix 1.78.0 Clippy lints
      Fix `-Wmissing-field-initializers` warnings
      depends: Apply libsodium patch to fix aarch64 compilation with Clang 18
      CI: Clean up workflow
      CI: Downgrade Debian Buster to Tier 2
      CI: Minor adjustment to CI workflow to reduce reliance on `jq`
      CI: Don't run full CI on doc-only or unrelated-workflow commits
      CI: Separate flaky RPC tests into individual jobs
      CI: Add required-pass steps check
      CI: Add workflow that sets required-pass status for no-CI PRs
      CI: Give `statuses: write` permission to `ci-skip.yml`
      CI: `ci-skip.yml` doesn't affect `ci.yml`, not `ci.yml`
      CI: Give `statuses: write` permission to `ci.yml`
      qa: Fix `updatecheck.py` after `native_libtinfo5` rename
      depends: native_cmake 3.29.3
      qa: cargo vet prune
      rust: Migrate to `zcash_primitives 0.15`
      depends: cxx 1.0.122
      CI: Move required-pass checks behind a `workflow_run` event
      CI: Alter how RPC test venvs are cached to improve reliability
      CI: Alter `ci-skip.yml` to run in the context of the base of the PR

Kris Nuttycombe (8):
      src/bech32.cpp: Make objects in range declarations immutable by default.
      Add `converttex` wallet RPC method.
      Apply suggestions from code review
      Update release notes to note the addition of `z_converttex`
      Postpone updates for libcxx and native_clang version 18.1.6
      Update release notes for v5.9.1 release.
      make-release.py: Versioning changes for 5.9.1-rc1.
      make-release.py: Updated manpages for 5.9.1-rc1.

Marius Kjærstad (1):
      New checkpoint at block 2400000 for mainnet

Pieter Wuille (4):
      Avoid VLA in hash.h
      Add some general std::vector utility functions
      Implement Bech32m encoding/decoding
      Add references for the generator/constant used in Bech32(m)

Samuel Dobson (1):
      Assert that the HRP is lowercase in Bech32::Encode

Yasser Isa (3):
      Yasser's GPG key for Gitian
      Update CI with Github actions artifacts
      Improvement of the CI/CD process for Docker image releases

Yasser Isa Isa (1):
      CI: Run RPC tests

dependabot[bot] (2):
      build(deps): bump peaceiris/actions-mdbook from 1 to 2
      build(deps): bump peaceiris/actions-gh-pages from 3 to 4

fanquake (1):
      [depends] Don't build libevent sample code

murrayn (1):
      Tighten up bech32::Decode(); add tests.

