Notable changes
===============

Prometheus metrics
------------------

`zcashd` can now be configured to optionally expose an HTTP server that acts as
a Prometheus scrape endpoint. The server will respond to `GET` requests on any
request path.

To enable the endpoint, add `-prometheusport=<port>` to your `zcashd`
configuration (either in `zcash.conf` or on the command line). After
restarting `zcashd` you can then test the endpoint by querying it with e.g.
`curl http://127.0.0.1:<port>`.

By default, access is restricted to localhost. This can be expanded with
`-metricsallowip=<ip>`, which can specify IPs or subnets. Note that HTTPS is not
supported, and therefore connections to the endpoint are not encrypted or
authenticated. Access to the endpoint should be assumed to compromise the
privacy of node operations, by the provided metrics and/or by timing side
channels. Non-localhost access is **strongly discouraged** if the node has a
wallet holding live funds.

The specific metrics names may change in subsequent releases, in particular to
improve interoperability with `zebrad`.

Changelog
=========

Cory Fields (3):
      net: rearrange so that socket accesses can be grouped together
      net: add a lock around hSocket
      net: fix a few races. Credit @TheBlueMatt

Daira Hopwood (6):
      Fix regression introduced in e286250ce49309bfa931b622fabfc37100246266 by adding #include <atomic>. fixes #5014
      native_rust: don't install Rust docs. This speeds up builds, especially native builds on macOS. fixes #5042
      Delete spare mainnet Founders' Reward addresses that will never be used.
      configure.ac: Add check for ntdll on Windows.
      configure.ac: tidy up spacing.
      Cleanup: get the definition of ENABLE_VIRTUAL_TERMINAL_PROCESSING from wincon.h.

Gregory Maxwell (5):
      Replace setInventoryKnown with a rolling bloom filter.
      Remove mruset as it is no longer used.
      Actually only use filterInventoryKnown with MSG_TX inventory messages.
      Add whitelistforcerelay to control forced relaying.
      Blacklist -whitelistalwaysrelay; replaced by -whitelistrelay.

Jack Grigg (39):
      Squashed 'src/secp256k1/' changes from c6b6b8f1bb..3967d96bf1
      Squashed 'src/secp256k1/' changes from 3967d96bf1..a4abaab793
      CI: Correctly build zcashd book
      cargo update
      rust: zcash_{primitives, proofs} 0.5.0
      depends: Update Rust to 1.51.0
      qa: Update BerkeleyDB downloads page URL
      rust: Implement FFI interface to metrics crate
      rust: Add a Prometheus metrics exporter
      Add some metrics that match existing zebrad metrics
      metrics: Add documentation and example configs
      tracing: Merge TracingSpanFields macro into TracingSpan
      rust: Move helper macros into rust/helpers.h
      metrics: Add support for labels
      metrics: Expose binary metadata
      Add more detailed metrics
      rust: Use consistent include guards in header files
      rust: Check for invalid UTF-8 in -prometheusmetrics argument
      Add -prometheusmetrics to release notes
      Mention in release notes that metrics names may still change
      metrics: Implement IP access control on Prometheus scrape endpoint
      rust: Pin hyper 0.14.2
      metrics: Move documentation into zcashd book
      metrics: Enable gauges with fully-static labels
      metrics: Use labels for pool statistics
      metrics: Rename metrics with consistent naming scheme
      metrics: Remove zcash.sync.* metrics
      metrics: Rework pool metrics in anticipation of transparent pool
      net: Clear CNode::strSendCommand if a message is aborted
      rust: Add license header to metrics_ffi::prometheus
      book: Fix typo in metrics documentation
      metrics: Don't assert that the Sprout tree is accessible
      Remove usage of local from fetch-params.sh
      lint: Fix false positive
      qa: Point univalue update checker at correct upstream
      qa: Postpone updates that require CMake in the build system
      qa: Postpone Boost 1.75.0
      make-release.py: Versioning changes for 4.4.0-rc1.
      make-release.py: Updated manpages for 4.4.0-rc1.

Jeremy Rubin (2):
      Fix CCheckQueue IsIdle (potential) race condition and remove dangerous constructors.
      Add CheckQueue Tests

Jonas Schnelli (1):
      Move -blocksonly parameter interaction to the new ParameterInteraction() function

Kaz Wesley (2):
      lock cs_main for State/Misbehaving
      lock cs_main for chainActive

Kris Nuttycombe (14):
      Ignore temporary build artifacts.
      Add feature flagging infrastructure to consensus parameters.
      Relocate contextual Sapling version checks
      Add TxVersionInfo for feature/future-base transaction construction.
      Move sapling version group checks back inside of saplingActive check.
      Add future version group & version checks
      Use SPROUT_MAX_CURRENT_VERSION
      Apply suggestions from code review
      Add feature flagging tests.
      Document FeatureSet type.
      Document UPGRADE_ZFUTURE
      Ensure that Sapling version range checks are always guarded by SAPLING_VERSION_GROUP_ID
      Address review comments.
      CurrentTxVersionInfo should return SPROUT_MIN_CURRENT_VERSION pre-overwinter.

Marco Falke (2):
      Move blocksonly parameter interaction to InitParameterInteraction()
      doc: Mention blocksonly in reduce-traffic.md, unhide option

Matt Corallo (13):
      Fix race when accessing std::locale::classic()
      Lock mapArgs/mapMultiArgs access in util
      Ensure cs_vNodes is held when using the return value from FindNode
      Access WorkQueue::running only within the cs lock.
      Make nTimeConnected const in CNode
      Avoid copying CNodeStats to make helgrind OK with buggy std::string
      Access fRelayTxes with cs_filter lock in copyStats
      Make nStartingHeight atomic
      Make nServices atomic
      Move [clean|str]SubVer writes/copyStats into a lock
      Make nTimeBestReceived atomic
      Move CNode::addrName accesses behind locked accessors
      Move CNode::addrLocal access behind locked accessors

Patick Strateman (13):
      Add blocksonly mode
      Do not process tx inv's in blocksonly mode
      Add whitelistalwaysrelay option
      Add help text for blocksonly and whitelistalwaysrelay
      Use DEFAULT_BLOCKSONLY and DEFAULT_WHITELISTALWAYSRELAY constants
      Display DEFAULT_WHITELISTALWAYSRELAY in help text
      Fix fRelayTxs comment
      Fix comment for blocksonly parameter interactions
      Fix relay mechanism for whitelisted peers under blocks only mode.
      Bail early in processing transactions in blocks only mode.
      Improve log messages for blocks only violations.
      Rename setInventoryKnown filterInventoryKnown
      Only use filterInventoryKnown with MSG_TX inventory messages.

Patrick Strateman (1):
      Make nWalletDBUpdated atomic to avoid a potential race.

Pavel Janík (2):
      Notify other serviceQueue thread we are finished to prevent deadlocks.
      Do not shadow LOCK's criticalblock variable for LOCK inside LOCK

Peter Todd (1):
      Add relaytxes status to getpeerinfo

Pieter Wuille (2):
      Clean up lockorder data of destroyed mutexes
      Fix some locks

Russell Yanofsky (1):
      Add missing cs_wallet lock that triggers new lock held assertion

Sjors Provoost (1):
      [doc] mention whitelist is inbound, and applies to blocksonly

Steven Smith (1):
      Adding base NU5 declarations and logic

Yuri Zhykin (1):
      Fix for incorrect locking in GetPubKey() (keystore.cpp)

glowang (1):
      Update -blocksonly documentation

plutoforever (1):
      removed bashisms from build scripts

Jack Grigg (2):
      Add security warnings for -prometheusmetrics option
      Clean up comment

