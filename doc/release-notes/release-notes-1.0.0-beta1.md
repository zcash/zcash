Daira Hopwood (1):
      Update steps after D

Jack Grigg (43):
      Undo debugging change from 5be6abbf84c46e8fc4c8ef9be987a44de22d0d05
      Output Equihash solution in RPC results as a hex string
      Add optional bool to disable computation of proof in JSDescription constructor
      Add wallet method for finding spendable notes in a CTransaction
      Store mapping between notes and PaymentAddresses in CWalletTx
      Keep track of spent notes, and detect and report conflicts
      Create mapping from nullifiers to received notes
      Add caching of incremental witnesses for spendable notes
      Update cached incremental witnesses when the active block chain tip changes
      Test solution output of blockToJSON()
      Pass ZCIncrementalMerkleTree to wallet to prevent race conditions
      Remove GetNoteDecryptors(), lock inside FindMyNotes() instead
      Replace vAnchorCache with a cache size counter
      mapNullifiers -> mapNullifiersToNotes for clarity
      Set witness cache size equal to coinbase maturity duration
      Add transactions to wallet if we spend notes in them
      Add test for GetNoteDecryptor()
      Keep any existing cached witnesses when updating transactions
      Changes after review
      Add test showing that the witness cache isn't being serialised
      Fix the failing test!
      Increase coverage of GetNoteDecryptor()
      Add coverage of the assertion inside GetNoteWitnesses()
      Separate concepts of block difficulty and network difficulty in RPC
      Add test comparing GetDifficulty() with GetNetworkDifficulty()
      Remove mainnet DNS seeds, set checkpoint to genesis
      Fix failing test
      Adjust from average difficulty instead of previous difficulty
      Remove testnet-only difficulty rules
      Add comments explaining changed semantics of pow_tests
      Expand bounds on difficulty adjustment
      Remove accidental double-semicolon (harmless but odd)
      Add test of difficulty averaging
      Simplify difficulty averaging code
      Restrict powLimit due to difficulty averaging
      Regenerate genesis blocks for new powLimits
      Update tests for new genesis blocks
      Adjust test to avoid spurious failures
      Remove unnecessary method
      Adjust test to account for integer division precision loss
      Refactor wallet note code for testing
      Add tests for refactored wallet code
      Remove .z# suffix from version

Lars-Magnus Skog (1):
      changed module name from "bitcoin" to "Zcash" in FormatException()

Sean Bowe (7):
      Deallocate the public parameters during Shutdown.
      Update libsnark again.
      Fix CheckTransaction bugs.
      Remove TODO 808.
      Fix transaction test in test_bitcoin.
      Change version to 1.0.0. This is just a beta.
      Update pchMessageStart and add testnet DNS boostrapper.

Simon (91):
      Implemented RPC calls z_importkey, z_exportkey, z_getnewaddress. Modified RPC calls dumpwallet and importwallet to include spending keys.
      Add z_importwallet and z_exportwallet to handle keys for both taddr and zaddr.  Restore behaviour of dumpwallet and importwallet to only handle taddr.
      Implemented z_listaddresses to return all the zaddr in the wallet.
      Add gtest to cover new methods in: CWallet - GenerateNewZKey() - AddZKey() - LoadZKey() - LoadZKeyMetadata() CWalletDB - WriteZKey()
      Don't mark wallet as dirty if key already exists. Fix incorrect method name used in error message.
      Added wallet rpc tests to cover: z_importwallet, z_exportwallet z_importkey, z_exportkey z_listaddresses
      Add test coverage for RPC call z_getnewaddress.
      Fix comment.
      Remove one line of dead code.
      Add "zkey" to list of key types (used by the wallet to decide whether or not it can be recovered if it detects bad records).
      Fix comments.
      Rename methods to avoid using prefix of _ underscore which is reserved. Added logging of explicit exception rather than a catch all. Removed redundant spending key check. Updated user facing help message.
      Fixes #1122 where json_spirit could stack overflow because there was no maximum limit set on the number of nested compound elements.
      Throw a domain error as json_spirit is a third-party library.
      Closes #1315.  RPC getblocksubsidy height parameter is now optional and a test has been added to verify parameter input and results.
      Remove #1144 from transaction.h.
      Remove #1144 from transaction.cpp by reverting back to commit 942bc46.
      Remove #1144 from bloom_tests by reverting to commit 5012190.
      Remove #1144 from input data of script_tests.
      Update txid gtest to verify #1144 has been removed: GetTxid() and GetHash() return the same result.
      Refactor: replace calls to GetTxid() with GetHash()
      Remove GetTxid() from CTransaction and update test_txid
      Replace GetTxid() with GetHash() after rebase on latest.
      Add async RPC queue and operation classes. Add z_getoperationstatus RPC command. Add z_sendmany RPC command (dummy implementation, does not send actual coins).
      Add prefix to async operation id so it is easier to manage on cli.
      Add config option 'rpcasyncthreads' to specify number of async rpc workers. Default is 1.
      Add public field 'memo' to JSOutput to enable creation of notes with custom memos.
      Implement z_sendmany RPC call.
      Update find_unspent_notes() as mapNoteAddrs_t has been replaced by mapNoteData_t.
      z_sendmany from a taddr now routes change to a new address instead of back to the sender's taddr,
      Successful result of z_sendmany returns txid so it doesn't need to return raw hex.
      Add public method to get state as a human readable string from an AsyncRPCOperation.
      Add public method to AsycnRPCQueue to retrieve all the known operation ids.
      Implement RPC call z_listoperationids and update z_getoperationstatus to take a list parameter.
      Refactoring and small improvements to async rpc operations.
      Closes #1293 by adding z_getoperationresult and making z_getoperationstatus idempotent.
      Add chaining of JoinSplits within a transaction.
      Disable option to allow multiple async rpc workers.
      Coinbase utxos can only be spent when sending to a single zaddr. Change from the transaction will be sent to the same zaddr.
      Fix bug where call to sign and send a transaction was in wrong scope.
      Added option to close a queue and wait for queued up operations to finish, rather than just closing a queue and immediately cancelling all operations.
      Fix bug where wallet was not persisting witnesses to disk. Author: str4d
      Refactor to use wallet note tracking from commit a72379
      Clear the operation queue when closing it.
      Add test for AsyncRPCQueue and AsyncRPCOperation.
      Add shared queue to AsynRPCQueue.
      Update RPCServer to use AsyncRPCQueue's shared queue.
      Remove redundant check when getting spending key for a payment address.
      Add tests for async queue and rpc commands: z_getoperationstatus, z_getoperationresult, z_listoperationids, z_sendmany
      Remove redundant call.
      Add logging under the category "asyncrpc".
      Add extra checking of memo data in hexadecimal string format.
      Add friend class for testing private members of AsyncRPCOperation_sendmany.
      Add z_getbalance and z_gettotalbalance RPC calls to close #1201.
      Fix typo in error message
      Disable proof generation when testmode is enabled in async SendMany operation.
      Reduce use of global pzcashParams with private member variable
      Revert "Reduce use of global pzcashParams with private member variable"
      Replace zcashParams_ with global.
      Add tests to try and improve coverage of perform_joinsplit.
      Add GetUnspentNotes to wallet.
      Add test for GetUnspentNotes() in wallet.
      Refactor async sendmany and getbalance calls to use GetUnspentNotes().
      Add more logging.
      Disable z_sendmany in safe mode
      Rename GetUnspentNotes to GetFilteredNotes
      Add z_listreceivedbyaddress RPC call
      Add 'DEPRECATED' to help message of zcraw* commands
      Update formatting and documentation.
      Move lock guard to start of addOperation to protect isClosed() and isFinishing()
      Fix formatting
      Add lock guard to getNumberOfWorkers()
      Replace unique_lock with lock_guard, where appropriate, for consistency
      Add extra RPC parameter checks for minconf<0 and zaddr not belonging to wallet.
      Add test for calling RPC z_getbalance, z_gettotalbalance, z_listreceivedbyaddress with invalid parameters.
      Fix formatting
      Update log statement to include fee.
      Fix incorrect default value for argument of GetFilteredNotes.
      Formatting and updated test per review.
      Add lock for member variables. Clean up and clarify that id_ and creation_time_ are never to be mutated anywhere. Fix incomplete copy/assignment constructors.
      Remove unused varible.
      Add ticket number to issues raised in comment.
      Add assert for two mutually exclusive member variables.
      Improve error reporting when attempting to spend coinbase utxos.
      Use zcash constants
      Fix formatting
      Add assert
      Update comment with ticket issue number
      Remove line of commented out code we don't need
      Improve check that user supplied memo field is too long.
      Replace GetTxid() with GetHash()
      Update payment-api.md
      Update security-warnings.md about REST interface
      Update payment API documentation for beta 1

Taylor Hornby (2):
      Add -Wformat -Wformat-security
      Use -Wformat in the test for -Wformat-security
