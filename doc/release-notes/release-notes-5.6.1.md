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

Changelog
=========

Kris Nuttycombe (13):
      Add `getblocksubsidy` checks to `nuparams` test.
      Restore `height` parameter to being optional in `getblocksubsidy` RPC.
      Re-create serialized v5.5.0 Orchard wallet state.
      Remove -regtestwalletsetbestchaineveryblock after golden state generation.
      Enable mining to Orchard on regtest
      Add persistent example of corrupted Orchard wallet state post v5.6.0
      Update wallet_golden_5_6_0 test to fail due to Orchard wallet parsing errors.
      Add a test demonstrating inability to spend from the tarnished state.
      Fix Orchard bridgetree parsing.
      Trigger a rescan on wallet load if spend information is missing from the wallet.
      Update changelog & postpone updates for v5.6.1 hotfix.
      make-release.py: Versioning changes for 5.6.1.
      make-release.py: Updated manpages for 5.6.1.

Sean Bowe (1):
      Add sapling.h and rpc/common.h to makefile sources.

