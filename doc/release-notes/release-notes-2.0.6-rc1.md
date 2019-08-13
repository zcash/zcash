Changelog
=========

Daira Hopwood (3):
      Closes #3992. Remove obsolete warning message.
      make-release.py: Versioning changes for 2.0.6-rc1.
      make-release.py: Updated manpages for 2.0.6-rc1.

Daniel Kraft (1):
      Add some const declarations where they are appropriate.

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

MarcoFalke (1):
      [doc] Fix doxygen comments for members

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

