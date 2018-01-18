Cameron Boehmer (1):
      point "where do i begin?" readme link to 1.0 guide

Jack Grigg (15):
      Track mined blocks to detect and report orphans and mining revenue
      Refresh mining status to detect setgenerate changes
      Add network stats to metrics screen
      Show mining info once the node has finished loading
      Improve locking in metrics
      Adjust consensus rule to accept genesis block without height in coinbase
      Fix previous commit
      Ensure that no tracked blocks are skipped during orphan detection
      Add build scripts and fetch-params.sh to "make install" and "make dist"
      Use uint64_t for AtomicCounter
      Fix gtest issue introduced into master
      Fix whitespace in Makefile.gtest.include
      Initialise walletdb system in a temp dir for all gtests
      Revert "Initialise walletdb system in a temp dir for all gtests"
      Change execution order of gtests to avoid bug

Kevin Gallagher (1):
      Improves usability of fetch-params.sh

Sean Bowe (6):
      Properly account for joinsplit value when deciding if a transaction should be placed in a mined block.
      Add checkpoint at block 2500.
      Throw more descriptive exceptions when the constraint system is violated.
      Test that a pure joinsplit will mine if other transactions are in the mempool.
      1.0.1 release.
      Update man pages.

Simon (1):
      Closes #1746. Add rpc call z_validateaddress to validate zaddrs.

