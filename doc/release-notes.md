(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

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
