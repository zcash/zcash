(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Option handling
---------------

- The `-reindex` and `-reindex-chainstate` options now imply `-rescan`
  (provided that the wallet is enabled and pruning is disabled, and unless
  `-rescan=0` is specified explicitly).

Build system
------------

- `zcutil/build.sh` now automatically runs `zcutil/clean.sh` to remove
  files created by previous builds. We previously recommended to do this
  manually.