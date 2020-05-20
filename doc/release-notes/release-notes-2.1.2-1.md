Notable changes
===============

This release solves an issue where nodes that did not follow the Heartwood activation on testnet (by running a version prior to v2.1.2) but then upgraded to v2.1.2 or later would be incapable of rolling back and following the Heartwood activation without performing a reindex operation.

Changelog
=========

Jack Grigg (1):
      txdb/chain: Restrict Heartwood chain consistency check to block index objects that were created by Heartwood-unaware clients.

Sean Bowe (4):
      Add the intended testnet activation block of Heartwood to our intended rewind logic.
      Don't throw exception in PopHistoryNode when popping from empty tree.
      make-release.py: Versioning changes for 2.1.2-1.
      make-release.py: Updated manpages for 2.1.2-1.

