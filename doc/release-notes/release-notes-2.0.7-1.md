Notable changes
===============

Testnet Blossom Rewind
----------------------
Testnet users needed to upgrade to 2.0.7 before Blossom activated. The amount
of notice given to these users was brief, so many were not able to upgrade in
time. These users may now be on the wrong branch. v2.0.7-1 adds an "intended
rewind" to prevent having to manually reindex when reconnecting to the correct
chain.


Insight Explorer Logging Fix
----------------------------
Fixed an issue where `ERROR: spent index not enabled` would be logged unnecessarily.

Changelog
=========

Alex Tsankov (2):
      Update INSTALL
      Typo Fix

Daira Hopwood (1):
      Add intended rewind for Blossom on testnet.

Eirik Ogilvie-Wigley (6):
      Notable changes for v2.0.7-1
      fix typo
      Add note about logging fix
      Better wording in release notes
      make-release.py: Versioning changes for 2.0.7-1.
      make-release.py: Updated manpages for 2.0.7-1.

Larry Ruane (2):
      #4114 don't call error() from GetSpentIndex()
      better fix: make GetSpentIndex() consistent with others...

