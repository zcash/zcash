(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

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
