Adam Weiss (1):
      Buffer log messages and explicitly open logs

Alex van der Peet (1):
      New RPC command disconnectnode

Allan Niemerg (1):
      Pause mining during joinsplit creation

Casey Rodarmor (1):
      Don't share objects between TestInstances

Cory Fields (2):
      locking: teach Clang's -Wthread-safety to cope with our scoped lock macros
      locking: add a quick example of GUARDED_BY

Daira Hopwood (1):
      Don't assume sizes of unsigned short and unsigned int in GetSizeOfCompactSize and WriteCompactSize. Fixes #2137

Daniel Kraft (1):
      Fix univalue handling of \u0000 characters.

Florian Schmaus (1):
      Add BITCOIND_SIGTERM_TIMEOUT to OpenRC init scripts

Forrest Voight (1):
      When processing RPC commands during warmup phase, parse the request object before returning an error so that id value can be used in the response.

Gavin Andresen (2):
      configure --enable-debug changes
      Testing infrastructure: mocktime fixes

Jack Grigg (11):
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
      Add rpc test for prioritisetransaction
      Inc num of txs in test mempool
      Update release to 1.0.7, generate manpages
      Add 1.0.7 release notes and update authors.md

Jonas Schnelli (23):
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

Simon Liu (14):
      Alert 1000
      Alert 1001
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

Wladimir J. van der Laan (10):
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

zathras-crypto (1):
      Exempt unspendable transaction outputs from dust checks

