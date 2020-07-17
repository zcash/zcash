Changelog
=========

Alex Morcos (3):
      ModifyNewCoins saves database lookups
      Make CCoinsViewTest behave like CCoinsViewDB
      Add unit test for UpdateCoins

Charlie O'Keefe (1):
      initialize pCurrentParams in TransactionBuilder tests

Dimitris Apostolou (1):
      Update zmq to 4.3.1

Eirik Ogilvie-Wigley (11):
      Remove --disable-libs flag from help output
      Update z_mergetoaddress documentation
      flake8 cleanup
      Move common code to helper
      Make variables local
      Check entire contents of mempool
      fail test if pong is not received
      Extract helper methods
      Strategically sync to prevent intermittent failures
      Return more information when building a transaction fails
      throw an exception rather than returning false when building invalid transactions

George Tankersley (2):
      zmq: add flag to publish all checked blocks
      zmq: remove extraneous space from zmq_sub.py

Jack Grigg (5):
      Remove the testing of duplicate coinbase transactions
      wallet: Skip transactions with no shielded data in CWallet::SetBestChain()
      Use nullptr instead of NULL in wallet tests
      Add Sapling test cases
      Set Sprout note data in WalletTest.WriteWitnessCache

Larry Ruane (1):
      wait for miner threads to exit (join them)

Pieter Wuille (3):
      Make sigcache faster and more efficient
      Evict sigcache entries that are seen in a block
      Don't wipe the sigcache in TestBlockValidity

Sean Bowe (1):
      Allow user to ask server to save the Sprout R1CS out during startup.

ca333 (1):
      update libsodium dl-path

mdr0id (7):
      Make pythonisms consistent
      make-release.py: Versioning changes for 2.0.3-rc1.
      make-release.py: Updated manpages for 2.0.3-rc1.
      make-release.py: Updated release notes and changelog for 2.0.3-rc1.
      Update nMinimumChainWork with information from the getblockchaininfo RPC
      make-release.py: Versioning changes for 2.0.3.
      make-release.py: Updated manpages for 2.0.3.

