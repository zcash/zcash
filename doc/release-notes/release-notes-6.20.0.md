Notable changes
===============

Network Upgrade 6.2
-------------------

The mainnet activation of the immediate NU6.2 network upgrade is supported
by the 6.20.0 release, with an activation height of 3364600, which should
occur at approximately 03:00 UTC, June 3, 2026. Please upgrade to this release,
or any subsequent release, in order to follow the NU6.2 network upgrade.

Security fix: remediation of the Orchard circuit
------------------------------------------------

The Zcash Open Development Lab team received a report of a critical flaw
in the implmentation of the Orchard zero-knowledge proof circuit in the
`halo2_gadgets` crate. This vulnerability was privately disclosed to us 
on May 29, 2026 at 11:53 PM by Taylor Hornby, former security engineer for
the Electric Coin Company and an independent security researcher contracted
by Shielded Labs to perform vulnerability research on the Orchard protocol.

The specific problem was that the incomplete double-and-add loop in
`ecc::chip::mul` kept the per-iteration base `(x_p, y_p)` constant across
loop rows via `q_mul_2`, but never tied it to the real base: the coordinates
were written with `assign_advice`, and the constancy chain reached neither
the doubling-row nor the complete-addition base anchors. A prover could
therefore run the incomplete loop against a free constant `B' != base`,
making the gadget output `[a] base + [b] B'` rather than `[scalar] base`. 

As a consequence, it is necessary to deploy an immediate NU6.2 network upgrade
to update the pinned verifying key for the Orchard circuit. User privacy and the
total supply cap of Zcash are not impacted; however, exploitation of this bug
could have permitted minting of funds within the Orchard pool.

Security fix: disallow non-canonical Orchard proofs
---------------------------------------------------

Prior to this release of zcashd, Orchard proofs were not validated for lengths.
Orchard transactions could contain arbitrary data concatenated to the end of
valid proofs, and this was not accounted for by ZIP 317 fee rules (which do
take into account the variable length of the valid Orchard proof itself).

The NU6.2 network upgrade alters the consensus rules to enforce a strict length
limit on Orchard proofs of precisely the valid proof data.

Changelog
=========

Daira-Emma Hopwood (6):
      Temporary git-revision patches for the security-fix crates, and orchard 0.14 API adaptations
      Add NU6.2 to the upgrade list
      Test that the canonical Orchard proof size is enforced from NU6.2
      Verify Orchard proofs against the correct circuit across NU6.2
      qa: Add an RPC test for Orchard across NU6.2
      Prove Orchard transactions against the correct circuit version

Jack Grigg (9):
      gtest: Fix Orchard unit tests for epk validation
      Restore AcceptBlockHeader ban level outside of soft fork
      qa: Update soft-fork test to ensure NU 6.2 re-enables Orchard
      Bump protocol version
      qa: Fix orchard_action_identity_point RPC test
      qa: Postpone remaining dependency updates
      Add release notes
      make-release.py: Versioning changes for 6.20.0.
      make-release.py: Updated manpages for 6.20.0.

Kris Nuttycombe (1):
      Clarify that maintainers do not classify vulns in service of bug bounty programs.

