(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Fixes
-----

Fixes an issue introduced in v5.6.0 that could cause loss of data from the
wallet's Orchard note commitment tree. Users upgrading directly from v5.5.0
should upgrade directly to v5.6.1. Wallets that were previously upgraded to
v5.6.0 whose wallets contained unspent Orchard notes at the time of the upgrade
will be automatically re-scanned on startup to repair the corrupted note
commitment tree.

Also, the `height` parameter to the `getblocksubsidy` RPC call had accidentally
been made required instead of optional as part of the v5.5.0 upgrade. This
hotfix restores `height` to being treated as an optional parameter.
