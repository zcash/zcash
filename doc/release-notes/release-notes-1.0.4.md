Bitcoin Error Log (1):
      Edit for grammar: "block chain"

Christian von Roques (1):
      bash-completion: Adapt for 0.12 and 0.13

Jack Grigg (32):
      Add getlocalsolps and getnetworksolps RPC calls, show them in getmininginfo
      Add benchmark for attempting decryption of notes
      Add benchmark for incrementing note witnesses
      Add -metricsui flag to toggle between persistent screen and rolling metrics
      Add -metricsrefreshtime option
      Only show metrics by default if stdout is a TTY
      Document metrics screen options
      Clarify that metrics options are only useful without -daemon and -printtoconsole
      Increase length of metrics divider
      Write witness caches when writing the best block
      Apply miniupnpc patches to enable compilation on Solaris 11
      Add an upstream miniupnpc patch revision
      Address review comments, tweak strings
      Change function names to not clash with Bitcoin, apply to correct binaries
      Add bash completion files to Debian package
      Always bash-complete the default account
      Add Zcash RPC commands to CLI argument completion
      Document behaviour of CWallet::SetBestChain
      Fix indentation
      Generate JS for trydecryptnotes, make number of addresses a variable
      Add JS to second block to ensure witnesses are incremented
      Skip JoinSplit verification before the last checkpoint
      Add a reindex test that fails because of a bug in decrementing witness caches
      Make the test pass by fixing the bug!
      Only check cache validity for witnesses being incremented or decremented
      Fix bug in wallet tests
      Extract block-generation wallet test code into a function
      Rewrite reindex test to check beyond the max witness cache size
      Fix bug in IncrementNoteWitness()
      Update payment API docs to recommend -rescan for fixing witness errors
      Update version to 1.0.4
      Update man pages

Jay Graber (2):
      Replace bitcoin with zcash in rpcprotocol.cpp
      Gather release notes from previous release to HEAD

Jeffrey Walton (1):
      Add porter dev overrides for CC, CXX, MAKE, BUILD, HOST

Scott (1):
      Metrics - Don't exclaim unless > 1

Sean Bowe (8):
      Isolate verification to a `ProofVerifier` context object that allows verification behavior to be tuned by the caller.
      Regression test.
      Ensure cache contains valid entry when anchor is popped.
      Ensure ProofVerifier cannot be accidentally copied.
      Rename Dummy to Disabled.
      Add more tests for ProofVerifier.
      ASSERT_TRUE -> ASSERT_FALSE
      Check that E' points are actually in G2 by ensuring they are of order r.

Simon Liu (8):
      Fix stale comment referencing upstream block interval
      Add checkpoint at block height 15000
      Closes #1857. Fixes bug where tx spending only notes had priority of 0.
      Closes #1901. Increase default settings for the max block size when     mining and the amount of space available for priority transactions.
      Closes #1903. Add fee parameter to z_sendmany.
      Fixes #1823. Witness anchors for input notes no longer cross block boundaries.
      Increase timeout as laptops on battery power have cpu throttling.
      WitnessAnchorData only needs to store one witness per JSOutPoint.

lpescher (3):
      Make command line option to show all debugging consistent with similar options
      Update documentation to match the #4219 change
      Update help message to match the #4219 change

