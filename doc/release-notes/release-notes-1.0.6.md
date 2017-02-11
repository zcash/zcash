Cory Fields (4):
      Depends: Add ZeroMQ package
      travis: install a recent libzmq and pyzmq for tests
      build: Make use of ZMQ_CFLAGS
      build: match upstream build change

Daira Hopwood (2):
      Better error reporting for the !ENABLE_WALLET && ENABLE_MINING case.
      Address @str4d's comment about the case where -gen is not set. Also avoid shadowing mineToLocalWallet variable.

Daniel Cousens (3):
      init: amend ZMQ flag names
      init: add zmq to debug categories
      zmq: prepend zmq to debug messages

Jack Grigg (33):
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

Jonas Schnelli (24):
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

João Barbosa (2):
      Add UpdatedBlockTip signal to CMainSignals and CValidationInterface
      Fix ZMQ Notification initialization and shutdown

Paragon Initiative Enterprises, LLC (1):
      Use libsodium's CSPRNG instead of OpenSSL's

Scott (1):
      Update random.h

Sean Bowe (3):
      Bump protocol version in release process if necessary.
      Fix use after free in transaction_tests.
      Update libsnark.

Simon Liu (16):
      Closes #2057 by adding extra zrpcunsafe logging
      Update z_sendmany logging
      Add test to verify z_sendmany logging
      Update test to verify order of zrpcunsafe log messages
      Closes #2045 by allowing z_sendmany with 0 fee
      Closes #2024 by documenting and testing method field in z_getoperationstatus
      Add parameter interaction, where zrpcunsafe implies zrpc
      Update zrpc vs zrpcunsafe logging in z_sendmany operation
      Add test for z_sendmany with fee of 0
      Update test to check for more joinsplit related fields in getrawtransaction
      Add comment about fix for #2026.
      Update test to check for updated error messages in AmountFromValue().
      Bump version to 1.0.6 as part of release process
      Debian man pages updated as part of release process
      Update release notes as part of release process
      Update debian changelog as part of release process

Wladimir J. van der Laan (10):
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

fanquake (3):
      [depends] zeromq 4.0.7
      [depends] ZeroMQ 4.1.4
      [depends] ZeroMQ 4.1.5

isle2983 (1):
      [copyright] add MIT License copyright header to zmq_sub.py

mrbandrews (1):
      Fixes ZMQ startup with bad arguments.

paveljanik (1):
      [Trivial] start the help texts with lowercase

