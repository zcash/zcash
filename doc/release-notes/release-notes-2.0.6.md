Notable changes
===============

Debian Stretch is now a Supported Platform
------------------------------------------

We now provide reproducible builds for Stretch as well as for Jessie.


Fixed a bug in ``z_mergetoaddress``
-----------------------------------

We fixed a bug which prevented users sending from ``ANY_SPROUT`` or ``ANY_SAPLING``
with ``z_mergetoaddress`` when a wallet contained both Sprout and Sapling notes.


Insight Explorer
----------------

We have been incorporating changes to support the Insight explorer directly from
``zcashd``. v2.0.6 includes the first change to an RPC method. If ``zcashd`` is
run with the flag ``--insightexplorer``` (this requires an index rebuild), the
RPC method ``getrawtransaction`` will now return additional information about
spend indices.

Changelog
=========

Charlie O'Keefe (1):
      Add stretch to list of suites in gitian linux descriptors

Daira Hopwood (9):
      Closes #3992. Remove obsolete warning message.
      make-release.py: Versioning changes for 2.0.6-rc1.
      make-release.py: Updated manpages for 2.0.6-rc1.
      make-release.py: Updated release notes and changelog for 2.0.6-rc1.
      ld --version doesn't work on macOS.
      Tweak author aliases.
      Add coding declaration to zcutil/release-notes.py
      make-release.py: Versioning changes for 2.0.6.
      make-release.py: Updated manpages for 2.0.6.

Daniel Kraft (1):
      Add some const declarations where they are appropriate.

Eirik Ogilvie-Wigley (1):
      Notable changes for 2.0.6

Eirik Ogilvie-Wigley (7):
      Fix tree depth in comment
      Update author aliases
      Remove old mergetoaddress RPC test
      Replace CSproutNotePlaintextEntry with SproutNoteEntry to match Sapling
      z_getmigrationstatus help message wording change
      Fix z_mergetoaddress sending from ANY_SPROUT/ANY_SAPLING when the wallet contains both note types
      Clarify what combinations of from addresses can be used in z_mergetoaddress

Jack Grigg (10):
      Move Equihash parameters into consensus params
      Globals: Remove Zcash-specific Params() calls from main.cpp
      Globals: Explicitly pass const CChainParams& to IsStandardTx()
      Globals: Explicit const CChainParams& arg for main:
      Globals: Explicitly pass const CChainParams& to ContextualCheckTransaction()
      Globals: Explicit const CChainParams& arg for main:
      Globals: Explicitly pass const CChainParams& to DisconnectBlock()
      Consistently use chainparams and consensusParams
      Globals: Explicitly pass const CChainParams& to IsInitialBlockDownload()
      Globals: Explicitly pass const CChainParams& to ReceivedBlockTransactions()

Jorge Timón (6):
      Globals: Explicit Consensus::Params arg for main:
      Globals: Make AcceptBlockHeader static (Fix #6163)
      Chainparams: Explicit CChainParams arg for main (pre miner):
      Chainparams: Explicit CChainParams arg for miner:
      Globals: Remove a bunch of Params() calls from main.cpp:
      Globals: Explicitly pass const CChainParams& to UpdateTip()

Larry Ruane (1):
      add spentindex to getrawtransaction RPC results

Marco Falke (1):
      [doc] Fix doxygen comments for members

Mary Moore-Simmons (1):
      Fixes issue #3504: Changes to --version and adds a couple other useful commands.

Peter Todd (1):
      Improve block validity/ConnectBlock() comments

Simon Liu (1):
      Fix typo and clean up help message for RPC z_getmigrationstatus.

Wladimir J. van der Laan (1):
      Break circular dependency main ↔ txdb

face (2):
      Pass CChainParams to DisconnectTip()
      Explicitly pass CChainParams to ConnectBlock

Benjamin Winston (1):
      Fixes #4013, added BitcoinABC as a disclosure partner

