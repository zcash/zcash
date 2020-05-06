Notable changes
===============

This release fixes two issues that were identified by the Heartwood activation on testnet.

* v2.1.2 nodes that followed the Heartwood activation on testnet would crash on restart if, prior to shutdown, they had received a block from a miner that had not activated Heartwood, which is very likely. This release fixes that crash.
* Nodes that had not followed the Heartwood activation on testnet (by running a version prior to v2.1.2) but then tried to upgrade to v2.1.2 would have difficulty rolling back. In this release we have ensured that nodes will now roll back if necessary to follow the Heartwood activation on testnet.

Changelog
=========

Jack Grigg (1):
      txdb/chain: Restrict Heartwood chain consistency check to block index objects that were created by Heartwood-unaware clients.

Sean Bowe (4):
      Add the intended testnet activation block of Heartwood to our intended rewind logic.
      Don't throw exception in PopHistoryNode when popping from empty tree.
      make-release.py: Versioning changes for 2.1.2-1.
      make-release.py: Updated manpages for 2.1.2-1.

