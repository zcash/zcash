(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Mining
------

- The default setting of `-blockunpaidactionlimit` is now zero, which has
  the effect of no longer allowing "unpaid actions" in [block production].
  This adapts to current network conditions. If you have overridden this
  setting as a miner, we recommend removing the override. This configuration
  option may be removed entirely in a future release.

[block production]: https://zips.z.cash/zip-0317#block-production

Platform Support
----------------

- Windows builds have been fixed.
