(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Platform Support
----------------

- Debian 10 (Buster) has been removed from the list of supported platforms.
  It reached EoL on June 30th 2024, and does not satisfy our Tier 2 policy
  requirements.

Mining
------

- The default setting of `-blockunpaidactionlimit` is now zero, which has
  the effect of no longer allowing "unpaid actions" in [block production].
  This adapts to current network conditions. If you have overridden this
  setting as a miner, we recommend removing the override. This configuration
  option may be removed entirely in a future release.

[block production]: https://zips.z.cash/zip-0317#block-production
