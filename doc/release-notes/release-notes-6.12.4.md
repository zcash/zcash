Notable changes
===============

Security Fixes
--------------

This release addresses several consensus and denial-of-service
vulnerabilities in `zcashd`, all reachable by a remote peer or miner.

**This is a time-critical upgrade.** It is released in coordination with a
soft fork that activates at mainnet block height **3363366**
(approximately 02:00 UTC on 2026-06-02). Mining nodes that have not
upgraded before activation are likely to produce blocks the upgraded
majority rejects, resulting in high orphan rates; exchanges and other
operators should upgrade within the same window to remain in consensus
with the network. **A further release will be required shortly after
activation** — operators should keep deployment staff reachable and be
prepared to deploy it promptly.

The following advisories are addressed:

| GHSA | Summary | Impact |
| --- | --- | --- |
| [GHSA-rpcw-q5mr-gq35](https://github.com/zcash/zcash/security/advisories/GHSA-rpcw-q5mr-gq35) | NU5+ block-body poisoning: v5 authorizing data is bound by `hashAuthDataRoot` (not `hashMerkleRoot`), so a body-derived rejection firing before the auth-commitment check could mark an honest header `BLOCK_FAILED_VALID`. | Permanent rejection of a valid header (DoS / consensus divergence). |
| [GHSA-qvwc-hc2r-82qv](https://github.com/zcash/zcash/security/advisories/GHSA-qvwc-hc2r-82qv) | Incomplete fix for the above via the `bad-blk-sigops` rejection: mutated v5 `scriptSig` changes the per-block sigop count before the auth commitment is verified. | Same header-poisoning DoS as GHSA-rpcw-q5mr-gq35. |
| [GHSA-wmwc-773c-qcvv](https://github.com/zcash/zcash/security/advisories/GHSA-wmwc-773c-qcvv) | Same class via the `bad-cb-length` (coinbase length) trigger path. | Same header-poisoning DoS. |
| [GHSA-382w-958v-m5jr](https://github.com/zcash/zcash/security/advisories/GHSA-382w-958v-m5jr) | Same class via the `bad-blk-length` (near-`MAX_BLOCK_SIZE` body) trigger path. | Same header-poisoning DoS. |
| [GHSA-g4x5-crjh-29ff](https://github.com/zcash/zcash/security/advisories/GHSA-g4x5-crjh-29ff) | A coinbase transaction with a positive Sapling or Orchard value balance desynchronizes chain-supply accounting from pool balances in `ConnectBlock`, triggering `AbortNode` before binding-signature validation. The block stays `MODE_ERROR`, so it is retried on restart. | Node shutdown / crash loop from a single valid-PoW block. |
| [GHSA-78pp-mc9g-g4mw](https://github.com/zcash/zcash/security/advisories/GHSA-78pp-mc9g-g4mw) | A block whose aggregate per-pool value delta is out of monetary range failed in `ReceivedBlockTransactions` with `MODE_VALID`, so the sending peer was never banned and replaying the P2P message re-wrote the body to disk indefinitely. | Unbanned peer + unbounded disk-write DoS. |
| [GHSA-ghc3-g8w4-whf9](https://github.com/zcash/zcash/security/advisories/GHSA-ghc3-g8w4-whf9) | Addressed by a soft fork that temporarily disables Orchard actions as a precautionary mitigation (see below). | Mitigated by consensus rule until Orchard is re-enabled in a future network upgrade. |

### NU5+ block-body poisoning (GHSA-rpcw-q5mr-gq35 and follow-ups)

The initial mitigation for this class shipped in v6.12.2. This release
*structurally* closes the class so that no per-rejection audit is
required:

- `CValidationState` now tracks, as monotonic flags, whether each
  header-to-body commitment (`hashMerkleRoot`, and for NU5+
  `hashBlockCommitments` → `hashAuthDataRoot`) has been verified against
  the body. `state.DoS(...)` takes a `BodyCorruption` argument and
  automatically classifies body-derived rejections as
  `corruptionPossible` whenever the body is not yet pinned to the header
  by both commitments. New rejections in `CheckBlock` /
  `CheckTransaction` / `ContextualCheckBlock` are safe by construction.
- For active-tip extensions, `AcceptBlock` now runs the
  `CheckBlockBodyAuthCommitment()` pre-check *before* `CheckBlock()` /
  `ContextualCheckBlock()`, so a body whose `hashAuthDataRoot` does not
  match the header is rejected before any body-mutable check can fire.
- For sidechain blocks (where reconstructing the chain-history MMR at an
  arbitrary `pprev` is infeasible), the pre-check waits until
  `ConnectBlock`; the `bad-blk-sigops` rejection retains
  `corruptionIn=true` as the minimal fix for the known
  GHSA-qvwc-hc2r-82qv vector.

Regression coverage exercises the `bad-blk-sigops`, `bad-cb-length`, and
`bad-blk-length` trigger paths on both the active-tip and sidechain paths.

### Coinbase shielded value balance abort (GHSA-g4x5-crjh-29ff)

`CheckTransaction` now rejects a positive coinbase Sapling or Orchard
value balance (with specific reject reasons) before it can reach the
supply-consistency check that calls `AbortNode`. As defence in depth,
the Sapling and Orchard bundle authorization checks now run ahead of the
supply-consistency check.

### Out-of-range pool-value deltas (GHSA-78pp-mc9g-g4mw)

A `SetChainPoolValues` failure in `ReceivedBlockTransactions` is now
routed through `state.DoS(100, ...)`, banning the sending peer and
bounding the otherwise-unbounded disk-write replay. The penalty is
applied only to the block the current peer actually sent; descendants
pulled from `mapBlocksUnlinked` are left for `ConnectBlock` to reject to
avoid mis-attributing the ban.

Temporary disabling of Orchard actions (GHSA-ghc3-g8w4-whf9)
------------------------------------------------------------

This release introduces a soft fork that temporarily disables Orchard actions
as a precautionary measure, addressing GHSA-ghc3-g8w4-whf9. After the
activation height, transactions containing Orchard actions are rejected from
the mempool (`bad-tx-has-orchard-actions`) and from blocks by enforcing nodes;
at the activation boundary, any Orchard-containing transactions already in the
mempool are dropped. Orchard is expected to be re-enabled shortly by a second
release with sources to be made available immedately following the soft fork;
to avoid connectivity issues in the interim, enforcing nodes do not increase
the DoS score of peers relaying Orchard-containing blocks after the fork.

Node operators should observe the soft fork and then be ready to build the
follow-on release.

The activation height is set to **3363366** on mainnet. No testnet
activation height is set in this release; it will be defined alongside
the next network upgrade.

Platform Support
----------------

- Debian 11 (Bullseye) has been removed from the list of supported platforms.
  It reaches EoL on June 30th 2026, and does not satisfy our Tier 2 policy
  requirements.

Changelog
=========

Ben Woosley (2):
      [moveonly] Split glibc sanity_test_fdelt out
      Include cstring for sanity_test_fdelt if required

Daira-Emma Hopwood (14):
      test: Open NU5 body-poisoning commitments scenarios with self.sync_all()
      test: Add bad-blk-sigops scenarios to NU5 block-body-poisoning test
      test: Add bad-cb-length scenarios to NU5 block-body-poisoning test
      test: Add bad-blk-length scenarios to NU5 block-body-poisoning test
      Revert "consensus: Drop `corruptionIn=true` from `bad-blk-sigops` rejection."
      consensus: Run active-tip auth-commitment pre-check before CheckBlock
      consensus: Track header-to-body commitments on CValidationState
      refactor: Define a `MAX_MONEY` constant in `test_framework.util.mininode`.
      test: Cover Orchard spends and the activation boundary for the disabling soft fork
      refactor: Add txs_from_template / solve_block_from_template to blocktools
      qa: Add shielded balance accounting tests for GHSA-g4x5-crjh-29ff
      qa: Add regression test for GHSA-78pp-mc9g-g4mw
      consensus: Ban peers sending blocks with out-of-range pool-value deltas
      consensus: Reject coinbase shielded value balance that can abort the node

Jack Grigg (27):
      depends: Update Clang / libcxx to LLVM 22.1.2
      depends: Update Rust to 1.96.0
      qa: Reorder postponed updates
      rust: Fix "shared reference to mutable static" warnings
      rust: Fix clippy lints after update to Rust 1.96
      contrib/devtools/symbol-check.py: Update allowed library versions and documentation
      Remove Debian 11 from the list of supported platforms
      depends: Boost 1.88.0
      CI: Use separate large runners for native and cross-compile
      depends: Migrate the Windows cross toolchain to llvm-mingw
      CI: Drop the system mingw-w64 packages for the Windows cross-compile
      Temporarily disable Orchard actions
      cargo vet prune
      rust: Remove now-unnecessary MSRV pins
      cargo vet prune
      depends: native_cmake 4.3.3
      depends: cxx 1.0.194
      Drop txs containing Orchard actions from the mempool at soft fork
      qa: Add integration test for the Orchard-disabling soft fork
      qa: Test both the soft fork boundary and the block afterwards
      Add default value for new primitive field
      Fix pyflakes lints
      depends: utfcpp 4.1.1
      qa: Postpone remaining updates
      Don't increase DoS level on Orchard blocks after the soft fork
      qa: Add new RPC test to list of RPC tests so CI runs it
      Set mainnet height for the soft fork

Kris Nuttycombe (26):
      Remove out-of-date Electric Coin Company strings & update SECURITY.md
      Update zcashd SECURITY.md to follow the model of https://github.com/zcash/.github/SECURITY.md
      Fix broken exception throwing in coinbase output construction
      Fix `throw new` for invalid-expiry-height errors
      Fix `throw new` in zcbenchmarks.cpp
      Differentiate sustained vs. bounded full-node DoS in vulnerability rubric
      Fix shell injection in `ci-status.yml`
      Use env-var indirection for `rpc_test_matrix` interpolation in `ci.yml`
      Remove out-of-date Electric Coin Company strings & update SECURITY.md
      Update zcashd SECURITY.md to follow the model of https://github.com/zcash/.github/SECURITY.md
      Fix broken exception throwing in coinbase output construction
      Fix `throw new` for invalid-expiry-height errors
      Fix `throw new` in zcbenchmarks.cpp
      Differentiate sustained vs. bounded full-node DoS in vulnerability rubric
      Clarify that maintainers do not classify vulns in service of bug bounty programs.
      Remove CI-config files from the `ci-skip.yml` bypass set
      Fail closed when a required CI job category produces no match
      Grant `actions: read` so the CI-status gate can list workflow-run jobs
      qa: Exempt zcash-cli from the FORTIFY_SOURCE check
      Update to zcash_primitives-0.27
      qa: Fix orchard_action_identity_point for zcash_primitives 0.27
      Set EOS halt to 3417100 for v6.12.4
      Update release notes for release v6.12.4
      Fix updatecheck.py for v6.12.4 release.
      make-release.py: Versioning changes for 6.12.4.
      make-release.py: Updated manpages for 6.12.4.

Pasta (1):
      refactor: remove references to deprecated values under std::allocator

Taylor Hornby (1):
      depends: Vendor libxml2.so.2 for the prebuilt LLVM toolchain

dependabot[bot] (9):
      build(deps): bump actions/upload-artifact from 4 to 7
      build(deps): bump actions/download-artifact from 4 to 8
      build(deps): bump actions/checkout from 4 to 6
      build(deps): bump actions/github-script from 7 to 8
      build(deps): bump docker/login-action from 3.7.0 to 4.1.0
      build(deps): bump docker/build-push-action from 6.19.2 to 7.1.0
      build(deps): bump actions/github-script from 8 to 9
      build(deps): bump actions/checkout from 4.3.1 to 6.0.2
      build(deps): bump EmbarkStudios/cargo-deny-action from 1 to 2

fanquake (10):
      build: remove linking librt for backwards compatibility
      build: remove fdelt_chk backwards compatibility code
      test: remove glibc fdelt sanity check
      tests: run test-security-check.py in CI
      compat: remove memcpy -> memmove backwards compatibility alias
      build: remove glibc backcompat requirement for Linux symbol checks
      script: remove gitian reference from symbol-check.py
      build: remove glibc-back-compat from build system
      compat: remove glibc_compat.cpp
      compat: remove glibcxx sanity checks

practicalswift (1):
      Use the noexcept specifier (C++11) instead of deprecated throw()

y4ssi (3):
      fix(docker): modernize Dockerfile + fix CI workflow
      ci: restore zcashd as canonical Docker Hub image name, mirror zcash
      ci: rename release-docker-hub.yaml -> .yml for consistency

