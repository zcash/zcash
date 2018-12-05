Notable changes
===============

Other issues
============

Revoked key error when upgrading on Debian
------------------------------------------

If you see the following error when updating to a new version of zcashd:

`The following signatures were invalid: REVKEYSIG AEFD26F966E279CD`

Remove the key marked as revoked:

`sudo apt-key del AEFD26F966E279CD`

Then retrieve the updated key:

`wget -qO - https://apt.z.cash/zcash.asc | sudo apt-key add -`

Then update the package lists:

`sudo apt-get update`

[Issue](https://github.com/zcash/zcash/issues/3612)

Changelog
=========

Charlie O'Keefe (2):
      Save and restore current_path in TestingSetup constructor/destructor
      Add a call to SetupNetworking in BasicTestingSetup

Eirik Ogilvie-Wigley (17):
      Rename GenerateNewZKey to include Sprout
      GenerateNewSproutZKey can return a SproutPaymentAddress
      Remove unnecessary call to IsValidPaymentAddress
      Remove unspent note entry structs
      Add functionality from GetUnspentFilteredNotes to GetFilteredNotes
      Remove GetUnspentFilteredNotes
      Wrap long line and update comments
      Fix error message
      Fix potentially misleading test failures
      Fix z_mergetoaddress parameter tests
      Add fail method to rpc test utils
      Extend Sprout mergetoaddress rpc test to also work for Sapling
      Add Sapling support to z_mergetoaddress
      Add locking for Sapling notes
      Better error messages when sending between Sprout and Sapling
      Add additional z_mergetoaddress parameter tests
      Adjust z_mergetoaddress assertions in Sapling rpc test

Gareth Davies (2):
      Add clarifying text for parameter size
      Cleaning up RPC output

Jack Grigg (7):
      Update IncrementalMerkleTree test vectors to use valid commitments
      Migrate to current librustzcash
      Pass parameter paths as native strings to librustzcash
      Build librustzcash package without changing directory
      Set nSaplingValue in-memory when loading block index from disk
      Comment in CDiskBlockIndex that LoadBlockIndexGuts also needs updating
      Test Sapling value pool accounting

Jonathan "Duke" Leto (2):
      Clarify in sendmany/z_sendmany rpc docs that amounts are not floating point
      Fix another instance of incorrectly saying amount is double precision, and s/ZC/ZEC/

Larry Ruane (3):
      sapling z_sendmany default memo 0xf6 + zeros
      update bug in wallet_listreceived.py, now it highlights the fix
      don't ban peers when loading pre-sapling (and pre-blossom) blocks

Simon Liu (14):
      Add test to verify final sapling root in block header is updated.
      Closes #3467. Add benchmarks for Sapling spends and outputs.
      Closes #3616.  Document revoked key error when upgrading on Debian.
      Closes #3329. Send alert to put non-Sapling nodes into safe mode.
      Closes #3597. TransactionBuilder checks tx version before adding Sapling spends and outputs.
      Closes #3671 to make "sapling" the default for z_getnewaddress RPC.
      Update rpc_wallet_tests for new "sapling" default for z_getnewaddress.
      Update qa tests for new "sapling" default for z_getnewaddress.
      Add support for "notfound" message to mininode.
      For ZEC-013. Mitigate potential tx expiry height related DoS vector.
      For ZEC-013. Don't propagate txs which are expiring soon in p2p messages.
      For ZEC-013. RPC createrawtransaction returns error if tx expiring soon.
      For ZEC-013. Update qa tests broken by expiring soon threshold.
      For ZEC-013. RPC sendrawtransaction returns error if tx expiring soon.

Suhas Daftuar (2):
      Do not inv old or missing blocks when pruning
      Enable block relay when pruning

arielgabizon (2):
      add test for sapling spend with transparent recipient
      rename HaveJoinSplitRequirements for Sapling

mdr0id (2):
      make-release.py: Versioning changes for 2.0.2-rc1.
      make-release.py: Updated manpages for 2.0.2-rc1.

tpantin (1):
      Updating copyright year from 2017 to 2018

