(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Testnet Blossom Rewind
----------------------
The warning that users of testnet got to upgrade for Blossom was somewhat
short. Users who were running a pre-2.0.7 version of zcashd at testnet Blossom
activation may now be on the wrong branch. We have added an "intended rewind"
to prevent having to manually reindex when reconnecting to the correct chain.


Insight Explorer Logging Fix
----------------------------
Fixed an issue where `ERROR: spent index not enabled` would be logged unnecessarily.
