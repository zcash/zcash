Adam Weiss (1):
      Buffer log messages and explicitly open logs

Alex van der Peet (1):
      New RPC command disconnectnode

Allan Niemerg (1):
      Pause mining during joinsplit creation

Casey Rodarmor (1):
      Don't share objects between TestInstances

Cory Fields (6):
      Depends: Add ZeroMQ package
      travis: install a recent libzmq and pyzmq for tests
      build: Make use of ZMQ_CFLAGS
      build: match upstream build change
      locking: teach Clang's -Wthread-safety to cope with our scoped lock macros
      locking: add a quick example of GUARDED_BY

Daira Hopwood (3):
      Better error reporting for the !ENABLE_WALLET && ENABLE_MINING case.
      Address @str4d's comment about the case where -gen is not set. Also avoid shadowing mineToLocalWallet variable.
      Don't assume sizes of unsigned short and unsigned int in GetSizeOfCompactSize and WriteCompactSize. Fixes #2137

Daniel Cousens (3):
      init: amend ZMQ flag names
      init: add zmq to debug categories
      zmq: prepend zmq to debug messages

Daniel Kraft (1):
      Fix univalue handling of \u0000 characters.

Eran Tromer (1):
      CreateJoinSplit: add start_profiling() call

Florian Schmaus (1):
      Add BITCOIND_SIGTERM_TIMEOUT to OpenRC init scripts

Forrest Voight (1):
      When processing RPC commands during warmup phase, parse the request object before returning an error so that id value can be used in the response.

Gavin Andresen (2):
      configure --enable-debug changes
      Testing infrastructure: mocktime fixes

Jack Grigg (66):
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
      Update comment
      Remove OpenSSL PRNG reseeding
      Address review comments
      Fix linking error in CreateJoinSplit
      Add compile flag to disable compilation of mining code
      Upgrade OpenSSL to 1.1.0d
      Show all JoinSplit components in getrawtransaction and decoderawtransaction
      Use a more specific exception class for note decryption failure
      Switch miner to P2PKH, add -mineraddress option
      Update help text for mining options
      Correct #ifdef nesting of miner headers and helper functions
      Add ZMQ libs to zcash-gtest
      Fix python syntax in ZMQ RPC test
      [qa] py2: Unfiddle strings into bytes explicitly in ZMQ RPC test
      Bitcoin -> Zcash in ZMQ docs
      Add ZeroMQ license to contrib/debian/copyright
      [depends] ZeroMQ 4.2.1
      Clarify that user only needs libzmq if not using depends system
      Bump suggested ZMQ Debian package to 4.1 series
      Add -minetolocalwallet flag, enforced on -mineraddress
      Add test to check for presence of vpub_old & vpub_new in getrawtransaction
      Add a flag for enabling experimental features
      Require -experimentalmode for wallet encryption
      Migrate Zcash-specific code to UniValue
      Manually iterate over UniValue arrays in tests
      Remove JSON Spirit from contrib/debian/copyright
      unsigned int -> size_t for comparing with UniValue.size()
      [cleanup] Remove unused import
      [cleanup] Simplify test code
      Squashed 'src/univalue/' content from commit 9ef5b78
      Update UniValue includes in Zcash-specific code
      UniValue::getValues const reference
      Get rid of fPlus argument to FormatMoney in Zcash-specific code
      Remove reference to -reindex-chainstate
      Treat metrics screen as non-interactive for now
      Adjust gen-manpages.sh for Zcash, use in Debian builds
      Regenerate and collate Zcash manpages, delete Bitcoin ones
      Update release process with gen-manpages.sh
      Adjust blockheaderToJSON() for Zcash block header
      Adjust fundrawtransaction RPC test for Zcash
      Re-encode t-addrs in disablewallet.py with Zcash prefixes
      BTC -> ZEC in paytxfee RPC docs
      Update default RPC port in help strings
      Fix typo in listbanned RPC keys

Jay Graber (4):
      Update release process to check in with users who opened resolved issues
      Add rpc test for prioritisetransaction
      Inc num of txs in test mempool
      Update release to 1.0.7, generate manpages

Jeff Garzik (4):
      Add ZeroMQ support. Notify blocks and transactions via ZeroMQ
      UniValue: prefer .size() to .count(), to harmonize w/ existing tree
      UniValue: export NullUniValue global constant
      Convert tree to using univalue. Eliminate all json_spirit uses.

Johnathan Corgan (5):
      zmq: require version 4.x or newer of libzmq
      zmq: update and cleanup build-unix, release-notes, and zmq docs
      autotools: move checking for zmq library to common area in configure.ac
      zmq: update docs to reflect feature is compiled in automatically if possible
      zmq: point API link to 4.0 as that is what we are conforming to [Trivial]

Jonas Schnelli (47):
      QA: Add ZeroMQ RPC test
      depends: fix platform specific packages variable
      [travis] add zmq python module
      use CBlockIndex* insted of uint256 for UpdatedBlockTip signal
      [ZMQ] refactor message string
      [ZMQ] append a message sequence number to every ZMQ notification
      fix rpc-tests.sh
      extend conversion to UniValue
      expicit set UniValue type to avoid empty values
      special threatment for null,true,false because they are non valid json
      univalue: add support for real, fix percision and make it json_spirit compatible
      univalue: correct bool support
      fix rpc unit test, plain numbers are not JSON compatible object
      remove JSON Spirit UniValue wrapper
      Remove JSON Spirit wrapper, remove JSON Spirit leftovers
      fix rpc batching univalue issue
      fix missing univalue types during constructing
      fix univalue json parse tests
      univalue: add type check unit tests
      fix util_tests.cpp clang warnings
      fix rpcmining/getblocktemplate univalue transition logic error
      remove univalue, prepare for subtree
      [Univalue] add univalue over subtree
      remove $(@F) and subdirs from univalue make
      [net] extend core functionallity for ban/unban/listban
      [RPC] add setban/listbanned/clearbanned RPC commands
      [QA] add setban/listbanned/clearbanned tests
      [net] remove unused return type bool from CNode::Ban()
      [RPC] extend setban to allow subnets
      rename json field "bannedtill" to "banned_until"
      setban: rewrite to UniValue, allow absolute bantime
      fix CSubNet comparison operator
      setban: add RPCErrorCode
      add RPC tests for setban & disconnectnode
      fix missing lock in CNode::ClearBanned()
      setban: add IPv6 tests
      fix lock issue for QT node diconnect and RPC disconnectnode
      fundrawtransaction tests
      UniValue: don't escape solidus, keep espacing of reverse solidus
      [REST] add JSON support for /rest/headers/
      [QA] fix possible reorg issue in rawtransaction.py/fundrawtransaction.py RPC test
      [QA] remove rawtransactions.py from the extended test list
      [QA] add testcases for parsing strings as values
      [bitcoin-cli] improve error output
      fix and extend CBitcoinExtKeyBase template
      extend bip32 tests to cover Base58c/CExtKey decode
      don't try to decode invalid encoded ext keys

Jorge Timón (1):
      Consensus: Refactor: Separate Consensus::CheckTxInputs and GetSpendHeight in CheckInputs

João Barbosa (2):
      Add UpdatedBlockTip signal to CMainSignals and CValidationInterface
      Fix ZMQ Notification initialization and shutdown

Leo Arias (1):
      Fix the path to the example configuration

Luke Dashjr (1):
      Fix various warnings

Matt Corallo (4):
      Small tweaks to CCoinControl for fundrawtransaction
      Add FundTransaction method to wallet
      Add fundrawtransaction RPC method
      Assert on probable deadlocks if the second lock isnt try_lock

Murilo Santana (1):
      Fix sha256sum on busybox by using -c instead of --check

Paige Peterson (2):
      Create ISSUE_TEMPLATE.md
      move template to subdirectory, fix typo, include prompt under describing issue section, include uploading file directly to github ticket as option for sharing logs

Paragon Initiative Enterprises, LLC (1):
      Use libsodium's CSPRNG instead of OpenSSL's

Paul Georgiou (1):
      Update Linearize tool to support Windows paths

Pavel Vasin (1):
      remove unused inv from ConnectTip()

Peter Todd (2):
      Add getblockheader RPC call
      Improve comment explaining purpose of MAX_MONEY constant

Philip Kaufmann (3):
      use const references where appropriate
      [init] add -blockversion help and extend -upnp help
      make CAddrMan::size() return the correct type of size_t

Pieter Wuille (3):
      Do not ask a UI question from bitcoind
      Add DummySignatureCreator which just creates zeroed sigs
      Reduce checkpoints' effect on consensus.

Scott (1):
      Update random.h

Sean Bowe (8):
      Add test for IncrementalMerkleTree::size().
      Add 'CreateJoinSplit' standalone utility to gitignore.
      Add test for z_importkey rescanning from beginning of chain.
      Bump version to 1.0.5.
      Update release notes and Debian package.
      Bump protocol version in release process if necessary.
      Fix use after free in transaction_tests.
      Update libsnark.

Simon Liu (48):
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
      Closes #2057 by adding extra zrpcunsafe logging
      Update z_sendmany logging
      Add test to verify z_sendmany logging
      Update test to verify order of zrpcunsafe log messages
      Closes #2045 by allowing z_sendmany with 0 fee
      Closes #2024 by documenting and testing method field in z_getoperationstatus
      Add parameter interaction, where zrpcunsafe implies zrpc
      Update zrpc vs zrpcunsafe logging in z_sendmany operation
      Alert 1000
      Alert 1001
      Add test for z_sendmany with fee of 0
      Update test to check for more joinsplit related fields in getrawtransaction
      Add comment about fix for #2026.
      Update test to check for updated error messages in AmountFromValue().
      Bump version to 1.0.6 as part of release process
      Debian man pages updated as part of release process
      Update release notes as part of release process
      Update debian changelog as part of release process
      Fix bug in release process tool: bad regex in zcutil/release-notes.py.
      Update authors.md using fixed release-notes.py, as part of release process.
      Fix manpage as part of release process.
      Fix debian manpage manually as part of release process
      Add release notes generated by script as part of release process
      Add assert to check alert message length is valid
      Fix bug where test was generating but not saving keys to wallet on disk.
      Update founders reward addresses for testnet
      Keep first three original testnet fr addresses so existing coinbase transactions on testnet remain valid during upgrade.  New addresses will be used starting from block 53127.
      Closes #2083 and #2088. Update release process documentation
      Closes #2084. Fix incorrect year in timestamp.
      Closes #2112 where z_getoperationresult could return stale status.
      Add mainnet checkpoint at block 67500
      Add testnet checkpoint at block 38000
      Closes #1969. Default fee now sufficient for large shielded tx.
      Part of #1969. Changing min fee calculation also changes the dust threshold.
      Part of #1969. Update tests to avoid error 'absurdly high fee' from change in min fee calc.

Stephen (1):
      Add paytxfee to getwalletinfo, warnings to getnetworkinfo

Wladimir J. van der Laan (21):
      rpc: Implement random-cookie based authentication
      Simplify RPCclient, adapt json_parse_error test
      util: Add ParseInt64 and ParseDouble functions
      univalue: add strict type checking
      Don't go through double in AmountFromValue and ValueFromAmount
      Get rid of fPlus argument to FormatMoney
      Changes necessary now that zero values accepted in AmountFromValue
      rpc: Accept scientific notation for monetary amounts in JSON
      rpc: Make ValueFromAmount always return 8 decimals
      univalue: Avoid unnecessary roundtrip through double for numbers
      util: use locale-independent parsing in ParseDouble
      rpc: make `gettxoutsettinfo` run lock-free
      test: Move reindex test to standard tests
      rpc: Remove chain-specific RequireRPCPassword
      univalue: Avoid unnecessary roundtrip through double for numbers
      rpc: Accept strings in AmountFromValue
      Fix crash in validateaddress with -disablewallet
      Improve proxy initialization
      tests: Extend RPC proxy tests
      build: Remove -DBOOST_SPIRIT_THREADSAFE
      tests: Fix bitcoin-tx signing testcase

dexX7 (1):
      Return all available information via validateaddress

fanquake (3):
      [depends] zeromq 4.0.7
      [depends] ZeroMQ 4.1.4
      [depends] ZeroMQ 4.1.5

isle2983 (1):
      [copyright] add MIT License copyright header to zmq_sub.py

mrbandrews (1):
      Fixes ZMQ startup with bad arguments.

mruddy (1):
      add tests for the decodescript rpc. add mention of the rpc regression tests to the testing seciton of the main readme.

nomnombtc (9):
      add script to generate manpages with help2man
      add gen-manpages.sh description to README.md
      add autogenerated manpages by help2man
      add doc/man/Makefile.am to include manpages
      add doc/man to subdir if configure flag --enable-man is set
      add conditional for --enable-man, default is yes
      change help string --enable-man to --disable-man
      regenerated all manpages with commit tag stripped, also add bitcoin-tx
      improved gen-manpages.sh, includes bitcoin-tx and strips commit tag, now also runs binaries from build dir by default, added variables for more control

paveljanik (1):
      [Trivial] start the help texts with lowercase

zathras-crypto (1):
      Exempt unspendable transaction outputs from dust checks

