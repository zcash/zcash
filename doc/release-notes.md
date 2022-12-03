(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Fixed
-----

This is a hotfix release that fixes a regression in memory usage during
Initial Block Download. The regression was indirectly caused by a change
to prioritize downloading headers (PR #6231), introduced in release 5.3.1.
It caused memory usage for new nodes to spike to roughly 11 GiB about an
hour after starting Initial Block Download.

The issue fixed by this release does not affect nodes that start from
a fully synced chain, or that had sufficient memory available to get
past the memory usage spike.
