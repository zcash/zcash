Notable changes
===============


Pre-Blossom EOS Halt
--------------------
v2.0.7-2 contains a shortened EOS halt so that is in alignment with v2.0.7. 


Testnet Blossom Rewind
----------------------
Testnet users needed to upgrade to 2.0.7 before Blossom activated. The amount
of notice given to these users was brief, so many were not able to upgrade in
time. These users may now be on the wrong branch. v2.0.7-2 adds an "intended
rewind" to prevent having to manually reindex when reconnecting to the correct
chain.


Insight Explorer Logging Fix
----------------------------
Fixed an issue where `ERROR: spent index not enabled` would be logged unnecessarily.

Changelog
=========

Eirik Ogilvie-Wigley (3):
      Notable changes for v2.0.7-2
      make-release.py: Versioning changes for 2.0.7-2.
      make-release.py: Updated manpages for 2.0.7-2.

