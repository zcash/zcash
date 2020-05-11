Notable changes
===============

This release fixes an issue that was identified by the Heartwood activation on testnet. v2.1.2 nodes that followed the Heartwood activation on testnet would crash on restart if, prior to shutdown, they had received a block from a miner that had not activated Heartwood, which is very likely. This release fixes that crash.

Changelog
=========

Daira Hopwood (1):
      txdb: log additional debug information.

Jack Grigg (1):
      txdb: More complete fix for the Heartwood chain consistency check issue.

Sean Bowe (2):
      make-release.py: Versioning changes for 2.1.2-2.
      make-release.py: Updated manpages for 2.1.2-2.

