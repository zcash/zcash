(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Merkle branches removed from wallet
-----------------------------------

Previously, every wallet transaction stored a Merkle branch to prove its
presence in blocks. This wasn't being used for more than an expensive
sanity check. Since 5.1.0, these are no longer stored. When loading a
5.1.0 wallet into an older version, it will automatically rescan to avoid
failed checks.
