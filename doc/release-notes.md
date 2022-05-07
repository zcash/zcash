(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Option handling
---------------

- The `-reindex` and `-reindex-chainstate` options now imply `-rescan`
  (provided that the wallet is enabled and pruning is disabled, and unless
  `-rescan=0` is specified explicitly).
- A new `-anchorconfirmations` argument has been added to allow the user
  to specify the number of blocks back from the chain tip that anchors will be
  selected from when spending notes. By default, anchors will now be selected
  to have 3 confirmations. Values greater than 100 are not supported.

RPC Interface
-------------

- The default `minconf` value for `z_sendmany` is now 10 confirmations instead
  of 1. If `minconf` and specifies a value less than that provided for
  `-anchorconfirmations`, it will also override that value as it is not
  possible to spend notes that are more recent than the anchor. Selecting
  `minconf` values less than 3 is not recommended, as it allows the transaction
  to be distinguished from transactions using the default for
  `-anchorconfirmations`.

Build system
------------

- `zcutil/build.sh` now automatically runs `zcutil/clean.sh` to remove
  files created by previous builds. We previously recommended to do this
  manually.

Dependencies
------------

- The `boost` and `native_b2` dependencies have been updated to version 1.79.0
