(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Fixed
-----

This release fixes an error "Assertion `uResultHeight == rewindHeight` failed" (#5958)
that could sometimes happen when restarting a node.

Memory Usage Improvement
------------------------

The memory usage of zcashd has been reduced by not keeping Equihash solutions for all
block headers in memory.
