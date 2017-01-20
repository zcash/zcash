Eran Tromer (1):
      CreateJoinSplit: add start_profiling() call

Jack Grigg (22):
      Extend createjoinsplit to benchmark parallel JoinSplits
      Add total number of commitments to getblockchaininfo
      Only enable getblocktemplate when wallet is enabled
      Only run wallet tests when wallet is enabled
      Add a tool for profiling the creation of JoinSplits
      Exclude test binaries from make install
      Scan the whole chain whenever a z-key is imported
      Instruct users to run zcash-fetch-params if network params aren't available
      Trigger metrics UI refresh on new messages
      Strip out the SECURE flag in metrics UI so message style is detected
      Handle newlines in UI messages
      Suggest ./zcutil/fetch-params.sh as well
      Update debug categories
      Rename build-aux/m4/bitcoin_find_bdb48.m4 to remove version
      Throw an error if zcash.conf is missing
      Show a friendly message explaining why zcashd needs a zcash.conf
      Fix gtest ordering broken by #1949
      Debian package lint
      Generate Debian control file to fix shlibs lint
      Create empty zcash.conf during performance measurements
      Create empty zcash.conf during coverage checks
      Coverage build system tweaks

Jay Graber (1):
      Update release process to check in with users who opened resolved issues

Paige Peterson (2):
      Create ISSUE_TEMPLATE.md
      move template to subdirectory, fix typo, include prompt under describing issue section, include uploading file directly to github ticket as option for sharing logs

Sean Bowe (4):
      Add test for IncrementalMerkleTree::size().
      Add 'CreateJoinSplit' standalone utility to gitignore.
      Add test for z_importkey rescanning from beginning of chain.
      Bump version to 1.0.5.

Simon Liu (13):
      Fixes #1964 to catch general exception in z_sendmany and catch exceptions as reference-to-const.
      Fixes #1967 by adding age of note to z_sendmany logging.
      Fixes a bug where the unsigned transaction was logged by z_sendmany after a successful sign and send, meaning that the logged hash fragment would be different from the txid logged by "AddToWallet".  This issue occured when sending from transparent addresses, as utxo inputs must be signed.  It did not occur when sending from shielded addresses.
      Bump COPYRIGHT_YEAR from 2016 to 2017.
      Closes #1780. Result of z_getoperationstatus now sorted by creation time of operation
      Remove UTF-8 BOM efbbbf from zcash.conf to avoid problems with command line tools
      Closes #1097 so zcash-cli now displays license info like zcashd.
      Fixes #1497 ZCA-009 by restricting data exporting to user defined folder.
      Closes #1957 by adding tx serialization size to listtransactions output.
      Fixes #1960: z_getoperationstatus/result now includes operation details.
      Update walletbackup.py qa test to use -exportdir option
      Add missing header required by std::accumulate
      Increase timeout for z_sendmany transaction in wallet.py qa test

Wladimir J. van der Laan (1):
      rpc: Implement random-cookie based authentication

