(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Security Fixes
--------------

This release addresses several security vulnerabilities that could have
caused nodes to diverge from consensus with Zebra.

These vulnerabilities were reported privately by white-hat security researchers
and handled through coordinated disclosure by Shielded Labs, Zcash Open
Development Lab (ZODL), and Zcash Foundation engineers, in coordination with
mining pools.

The following vulnerabilities are addressed by this release:

* The v4 (Sapling) transaction deserializer silently accepted transactions
  with no Spend descriptions and no Output descriptions but with a non-zero
  `valueBalanceSapling` — the value was discarded during deserialization, so
  the consensus check that should have rejected such transactions ran against
  an already-normalized zero and never fired. Zebra rejects this encoding at
  deserialization time as required by §7.1.2 of the protocol specification,
  so a malformed transaction that propagated unchanged from a malicious or
  custom block producer would have caused a consensus split. The malformed
  encoding is now rejected at deserialization, matching Zebra.

* In NU5 and later, the authorizing data of v5 transactions (transparent
  input scripts, Sapling and Orchard proofs and signatures) is committed by the
  header's `hashBlockCommitments` via `hashAuthDataRoot`, but not by
  `hashMerkleRoot`. An adversary able to mutate authorizing data could submit a
  "poisoned" block body whose `hashMerkleRoot` value matched the honest header
  but whose contents were inconsistent with `hashBlockCommitments`. zcashd's
  previous handling could (a) persist the poisoned body to disk before
  detecting the mismatch, and (b) on the sidechain path, mark the corresponding
  header `BLOCK_FAILED_VALID`, permanently invalidating an otherwise valid
  header.

  The fix pre-checks `hashBlockCommitments` in `AcceptBlock` for active-tip
  extensions (rejecting the body before it is written to disk), treats the
  remaining sidechain mismatch in `ConnectBlock` as body-replaceable, and
  resets only the affected block-index entries' body state via the new
  `CBlockIndex::ResetBodyState` so that the canonical body can replace the
  poisoned one without losing the header. A regression test exercises both
  the active-tip and sidechain paths.

* The v6.12.1 release added a check rejecting Orchard actions whose
  ephemeral public key `epk` encoded the identity point on the Pallas
  curve, to match the consensus rule in §5.4.9.4 of the protocol
  specification. That check rejected only the all-zeros identity encoding;
  it did not reject other 32-byte strings that fail to encode a valid
  Pallas curve point, namely:

  - non-canonical x (x ≥ q_P, the Pallas base field modulus), and
  - canonical x in [0, q_P) for which (x³ + 5) is not a quadratic residue mod
    q_P (so no curve point exists for that x).

  Zebra rejects all three categories, so accepting them in zcashd would
  have left the same potential consensus split that the v6.12.1 fix was
  intended to close. The check has been broadened to require that `epk`
  decodes to a valid non-identity Pallas point, and the regression test
  added in v6.12.1 has been extended to cover the additional categories.
