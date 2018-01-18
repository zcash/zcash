Adam Brown (1):
      [doc] Update port in tor.md

Bob McElrath (1):
      Add explicit shared_ptr constructor due to C++11 error

Cory Fields (2):
      libevent: add depends
      libevent: Windows reuseaddr workaround in depends

Daira Hopwood (15):
      Remove src/qt.
      License updates for removal of src/qt.
      Correct license text for LGPL.
      Remove QT gunk from Makefiles.
      Remove some more QT-related stragglers.
      Update documentation for QT removal.
      Update which libraries are allowed to be linked to zcashd by symbol-check.py.
      Remove NO_QT make option.
      .gitignore cache/ and venv-mnf/
      Remove unused packages and patches.
      Delete -rootcertificates from bash completion script.
      Line-wrap privacy notice. Use <> around URL and end sentence with '.'. Include privacy notice in help text for zcashd -help.
      Update version numbers.
      Improvement to release process doc.
      Generate man pages.

Daniel Cousens (1):
      torcontrol: only output disconnect if -debug=tor

Gregory Maxwell (3):
      Avoid a compile error on hosts with libevent too old for EVENT_LOG_WARN.
      Do not absolutely protect local peers from eviction.
      Decide eviction group ties based on time.

Ian Kelling (1):
      Docs: add details to -rpcclienttimeout doc

Jack Gavigan (2):
      Removed markdown from COPYING
      Updated the Bitcoin Core copyright statement

Jack Grigg (25):
      Add anchor to output of getblock
      Migrate IncrementalMerkleTree memory usage calls
      Add tests for getmempoolinfo
      Usability improvements for z_importkey
      Implement an AtomicTimer
      Use AtomicTimer for more accurate local solution rate
      Metrics: Move local solution rate into stats
      Metrics: Improve mining status
      Expand on reasons for mining being paused
      Simplify z_importkey by making rescan a string
      Revert "Closes #1680, temporary fix for rpc deadlock inherited from upstream."
      Add libevent to zcash-gtest
      [depends] libevent 2.1.8
      Test boolean fallback in z_importkey
      Require that z_importkey height parameter be in valid range
      Update LocalSolPS test
      Add AtomicTimer tests
      Revert "Revert "rpc-tests: re-enable rpc-tests for Windows""
      Wrap error string
      Fix typo
      torcontrol: Improve comments
      torcontrol: Add unit tests for Tor reply parsers
      torcontrol: Fix ParseTorReplyMapping
      torcontrol: Check for reading errors in ReadBinaryFile
      torcontrol: Log invalid parameters in Tor reply strings where meaningful

Jay Graber (5):
      Document returned results of submitblock
      Edit release-process.md for clarity
      Add security warning to zcash-cli --help and --version message output
      Add security warning to zcashd metrics display
      Add security message to license text, rm url from translation string

Jonas Schnelli (1):
      Fix torcontrol.cpp unused private field warning

Karl-Johan Alm (4):
      Added std::unique_ptr<> wrappers with deleters for libevent modules.
      Switched bitcoin-cli.cpp to use RAII unique pointers with deleters.
      Added some simple tests for the RAII-style events.
      Added EVENT_CFLAGS to test makefile to explicitly include libevent headers.

Luke Dashjr (1):
      Skip RAII event tests if libevent is built without event_set_mem_functions

MarcoFalke (2):
      [doc] [tor] Clarify when to use bind
      torcontrol debug: Change to a blanket message that covers both cases

Matt Quinn (1):
      Consolidate individual references to the current maximum peer connection value of 125 into a single constant declaration.

Nathaniel Mahieu (1):
      Clarify documentation for running a tor node

Patrick Strateman (1):
      Remove vfReachable and modify IsReachable to only use vfLimited.

Pavel Janík (3):
      Implement REST mempool API, add test and documentation.
      Prevent -Wshadow warnings with gcc versions 4.8.5, 5.3.1 and 6.2.1.
      Make some global variables less-global (static)

Peter Todd (2):
      Better error message if Tor version too old
      Connect to Tor hidden services by default

Pieter Wuille (3):
      Implement accurate memory accounting for mempool
      Separate core memory usage computation in core_memusage.h
      Fix interrupted HTTP RPC connection workaround for Python 3.5+

Sean Bowe (2):
      Introduce librustzcash and Rust to depends system.
      Allow Rust-language related assets to be disabled with `--disable-rust`.

Simon Liu (4):
      Remove stale Qt comments and dead code
      Remove QT translation support files
      Remove redundant gui options from build scripts
      Closes #2186. RPC getblock now accepts height or hash.

Wladimir J. van der Laan (28):
      doc: remove documentation for rpcssl
      qa: Remove -rpckeepalive tests from httpbasics
      Remove rpc_boostasiotocnetaddr test
      build: build-system changes for libevent
      tests: GET requests cannot have request body, use POST in rest.py
      evhttpd implementation
      Implement RPCTimerHandler for Qt RPC console
      Document options for new HTTP/RPC server in --help
      Fix race condition between starting HTTP server thread and setting EventBase()
      Move windows socket init to utility function
      Revert "rpc-tests: re-enable rpc-tests for Windows"
      init: Ignore SIGPIPE
      http: Disable libevent debug logging, if not explicitly enabled
      rpc: Split option -rpctimeout into -rpcservertimeout and -rpcclienttimeout
      Make RPC tests cope with server-side timeout between requests
      chain: define enum used as bit field as uint32_t
      auto_ptr → unique_ptr
      bitcoin-cli: More detailed error reporting
      depends: Add libevent compatibility patch for windows
      bitcoin-cli: Make error message less confusing
      test: Avoid ConnectionResetErrors during RPC tests
      net: Automatically create hidden service, listen on Tor
      torcontrol improvements and fixes
      doc: update docs for Tor listening
      tests: Disable Tor interaction
      Fix memleak in TorController [rework]
      tor: Change auth order to only use HASHEDPASSWORD if -torpassword
      torcontrol: Explicitly request RSA1024 private key

calebogden (1):
      Fixing typos on security-check.py and torcontrol.cpp

fanquake (1):
      [depends] libevent 2.1.7rc

instagibbs (1):
      Add common failure cases for rpc server connection failure

paveljanik (1):
      [TRIVIAL] Fix typo: exactmath -> exactmatch

unsystemizer (1):
      Clarify `listenonion`

