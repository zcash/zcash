(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Disabling old Sprout proofs
---------------------------

As part of our ongoing work to clean up the codebase and minimise the security
surface of `zcashd`, we are removing `libsnark` from the codebase, and dropping
support for creating and verifying old Sprout proofs. Funds stored in Sprout
addresses are not affected, as they are spent using the hybrid Sprout circuit
(built using `bellman`) that was deployed during the Sapling network upgrade.

This change has several implications:

- `zcashd` no longer verifies old Sprout proofs, and will instead assume they
  are valid. This has a minor implication for nodes: during initial block
  download, an adversary could feed the node fake blocks containing invalid old
  Sprout proofs, and the node would accept the fake chain as valid. However,
  `zcashd` internally contains checkpoints after Sapling activation for both
  block heights and cumulative chain work, and does not exit the initial block
  download phase until the active chain contains at least as much work as the
  checkpointed chain work. The node would therefore be non-functional (and would
  not broadcast the fake chain to other peers) until the fake chain contained as
  much work as the main chain, making this a 50% + 1 attack, which the current
  consensus rules already does not protect against.

- Shielded transactions can no longer be created before Sapling has activated.
  This does not affect Zcash itself, but will affect downstream codebases that
  have not yet activated Sapling (or that start a new chain after this point and
  do not activate Sapling from launch). Note that the old Sprout circuit is
  [vulnerable to counterfeiting](https://z.cash/support/security/announcements/security-announcement-2019-02-05-cve-2019-7167/)
  and should not be used in current deployments.

- Starting from this release, the circuit parameters from the original Sprout
  MPC are no longer required to start `zcashd`, and will not be downloaded by
  `fetch-params.sh`. They are not being automatically deleted at this time.

We would like to take a moment to thank the `libsnark` authors and contributors.
It was vital to the success of Zcash, and the development of zero-knowledge
proofs in general, to have this code available and usable.
