Notable changes
===============

Security Fixes
--------------

This release addresses several security vulnerabilities that could have
caused nodes to crash or diverge from consensus with Zebra, or that could
have disabled the defence-in-depth provided by pool turnstile checks.

These vulnerabilities were reported privately by white-hat security
researcher Alex “Scalar” Sol and handled through coordinated disclosure
by Shielded Labs, Zcash Open Development Lab (ZODL), and Zcash Foundation
engineers, in coordination with mining pools.

The following vulnerabilities are addressed by this release:

* An Orchard transaction with an `rk` encoding the identity point on
  the Pallas curve could cause zcashd to panic during proof verification.
  This has been fixed by rejecting identity `rk` before proof verification
  is reached.
* The Zcash protocol specification requires that the ephemeral public key
  `epk` in each Orchard action encode a non-identity point. Zebra enforced
  this requirement but zcashd did not, creating a potential consensus split.
  This has been fixed by rejecting identity `epk`.
* A bug in zcashd's block processing allowed a duplicate block header to
  silently reset pool balance tracking fields, disabling ZIP 209 turnstile
  enforcement. The fix moves pool value initialization to after the
  duplicate-data check, ensuring pool accounting is never reset by replayed
  or duplicate headers.
* The duplicate-header bug could also cause corrupted per-block pool
  deltas to be persisted to disk under certain conditions. This release
  adds a chain supply checkpoint at NU6.1 activation covering all value
  pools, and recomputes shielded pool deltas from block data on startup
  for blocks after the checkpoint. If persisted deltas are inconsistent
  with the recomputed values, the node aborts with a message to reindex.

Related changes also harden pool balance accumulation against undefined
behaviour due to signed integer overflow, and improve exception safety
around calls to value computation functions.

Changelog
=========

Daira-Emma Hopwood (28):
      hardening: Add range checks to prevent signed integer overflow UB.
      hardening: Catch value range exceptions as consensus rejections.
      test: Extend regression test with different-transactions scenario.
      test: Add persistence-across-restart scenario to regression test.
      consensus: Move asserts earlier.
      consensus: Tighten the shielded pool and lockbox turnstile checks.
      consensus: Abort node on supply sum mismatch.
      miner: Replace conditional pool monitoring with assertions.
      refactor: Use .has_value() and .value() for std::optional access.
      refactor: Refactor `TestBlockValidity` to `TestNewBlockAtTipValidity`.
      Postpone dependency updates for v6.12.1 hotfix.
      refactor: Rename `BLOCK_VALID_TRANSACTIONS` to `BLOCK_PARTIALLY_VALID_TRANSACTIONS`.
      consensus: Add chain supply checkpoint covering all pools.
      refactor: Replace `BLOCK_VALID_SCRIPTS` with `BLOCK_VALID_CONSENSUS`.
      refactor: Add -regtestenablezip209 flag to enable ZIP 209 without pool zeroing.
      consensus: Abort node on missing supply data.
      doc: Restore comment explaining SetChainPoolValues placement in ReceivedBlockTransactions.
      test: Exercise the chain supply checkpoint fallback.
      doc: Prove that the Merkle mutation check is sufficient (CVE-2012-2459).
      doc: Document the ZIP209Enabled() chain parameter.
      refactor: Factor out CheckBlockMerkleRoot from CheckBlock.
      hardening: Revalidate UTXO values from disk and handle exceptions.
      Update nMinimumChainWork for v6.12.1.
      Update bytes to 1.11.1 (RUSTSEC-2026-0007).
      refactor: Extract ComputePoolDeltas helper from SetChainPoolValues.
      hardening: Recompute shielded pool deltas from block data on load.
      test: End-to-end test for delta corruption detection.
      Add release notes for 6.12.1.

Kris Nuttycombe (13):
      consensus: Reject Orchard actions with identity rk or epk
      consensus: Defer setting chain pool values until after duplicate-header check.
      test: Regression test for duplicate-header chain value clobbering
      test: SetChainPoolValues eagerly accumulates chain pool values.
      consensus: Factor chain pool value accumulation into a helper.
      consensus: Add assertion in SetChainPoolValues that `pindex->pprev` is only null for the genesis block.
      consensus: Make ConnectBlock turnstile checks unconditional.
      refactor: Use AccumulateChainPoolValues in LoadBlockIndexDB.
      Fix timeouts in finalsaplingroot tests.
      Change RELEASE_TO_DEPRECATION_WEEKS to 13 for 6.12.1 hotfix release.
      Postpone updates for the v6.12.1 release.
      make-release.py: Versioning changes for 6.12.1.
      make-release.py: Updated manpages for 6.12.1.

