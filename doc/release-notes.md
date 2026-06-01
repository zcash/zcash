(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

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

This release introduces a soft fork that temporarily disables Orchard
actions as a precautionary measure, addressing GHSA-ghc3-g8w4-whf9. After the activation height,
transactions containing Orchard actions are rejected from the mempool
(`bad-tx-has-orchard-actions`) and from blocks by enforcing nodes; at the
activation boundary, any Orchard-containing transactions already in the
mempool are dropped. Orchard is expected to be re-enabled in a future
network upgrade; to avoid connectivity issues in the interim, enforcing
nodes do not increase the DoS score of peers relaying Orchard-containing
blocks after the fork.

The activation height is set to **3363366** on mainnet. No testnet
activation height is set in this release; it will be defined alongside
the next network upgrade.

Platform Support
----------------

- Debian 11 (Bullseye) has been removed from the list of supported platforms.
  It reaches EoL on June 30th 2026, and does not satisfy our Tier 2 policy
  requirements.
