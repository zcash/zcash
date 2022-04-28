(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Option handling
---------------

- The `-reindex` and `-reindex-chainstate` options now imply `-rescan`
  (provided that the wallet is enabled and pruning is disabled, and unless
  `-rescan=0` is specified explicitly).

RPC Interface
-------------

- The default `minconf` value for `z_sendmany` is now 10 confirmations instead
  of 1.
- When spending notes using `z_sendmany`, Orchard anchors are now selected at
  a depth of 10 blocks instead of at the chain tip. If the `minconf` value is
  less than 10, `minconf` is used for the anchor selection depth instead.

Build system
------------

- `zcutil/build.sh` now automatically runs `zcutil/clean.sh` to remove
  files created by previous builds. We previously recommended to do this
  manually.

Dependencies
------------

- The `boost` and `native_b2` dependencies have been updated to version 1.79.0
