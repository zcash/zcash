(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Hierarchical Deterministic Key Generation for Sapling
-----------------------------------------------------
All Sapling addresses will use hierarchical deterministic key generation
according to ZIP 32 (keypath m/32'/133'/k' on mainnet). Transparent and
Sprout addresses will still use traditional key generation.

Backups of HD wallets, regardless of when they have been created, can
therefore be used to re-generate all possible Sapling private keys, even the
ones which haven't already been generated during the time of the backup.
Regular backups are still necessary, however, in order to ensure that
transparent and Sprout addresses are not lost.

[Pull request](https://github.com/zcash/zcash/pull/3492), [ZIP 32](https://github.com/zcash/zips/blob/master/zip-0032.mediawiki)
