Notable changes
===============

Reduce download traffic
-----------------------

We have made several changes to reduce the amount of data downloaded by `zcashd`
during initial block download (IBD):

- Significant time and bandwidth is spent in issuing `getheaders` P2P requests.
  This results in noticeable bandwidth usage due to the large size of Zcash
  block headers.

  We now eliminate redundant requests in cases where we already know the last
  header in the message. This optimization is enabled by default, but can be
  disabled by setting the config option `-nooptimize-getheaders`.

- Transactions in the mempool are no longer downloaded during IBD (`zcashd` will
  only request block data).

Reduce upload traffic
---------------------

A major part of the outbound traffic is caused by serving historic blocks to
other nodes in initial block download state.

It is now possible to reduce the total upload traffic via the `-maxuploadtarget`
parameter. This is *not* a hard limit but a threshold to minimize the outbound
traffic. When the limit is about to be reached, the uploaded data is cut by not
serving historic blocks (blocks older than one week).
Moreover, any SPV peer is disconnected when they request a filtered block.

This option can be specified in MiB per day and is turned off by default
(`-maxuploadtarget=0`).
The recommended minimum is 1152 * MAX_BLOCK_SIZE (currently 2304MB) per day.

Whitelisted peers will never be disconnected, although their traffic counts for
calculating the target.

A more detailed documentation about keeping traffic low can be found in
[/doc/reduce-traffic.md](/doc/reduce-traffic.md).

`libzcashconsensus` replaced by `libzcash_script`
-------------------------------------------------

The `libzcashconsensus` library inherited from upstream has been unusable since
the Overwinter network upgrade in 2018. We made changes to signature digests
similar to those made in Bitcoin's SegWit, which required additional per-input
data that could not be added to the existing APIs without breaking backwards
compatibility.

Additionally, it has become increasingly inaccurately named; it only covers
(Zcash's subset of) the Bitcoin scripting system, and not the myriad of other
consensus changes: in particular, Zcash's shielded pools.

We have now renamed the library to `libzcash_script`, and reworked it to instead
focus on transparent script verification:

- The script verification APIs are altered to take `consensusBranchId` and
  `amount` fields.
- New precomputing APIs have been added that enable multiple transparent inputs
  on a single transaction to be verified more efficiently.
- Equihash has been removed from the compiled library. The canonical Equihash
  validator is the [`equihash` Rust crate](https://crates.io/crates/equihash)
  since v3.1.0.

The C++ library can be built by compiling `zcashd` with the environment variable
`CONFIGURE_FLAGS=--with-libs`. It is also wrapped as the
[`zcash_script` Rust crate](https://crates.io/crates/zcash_script)
(maintained by the Zcash Foundation for use in `zebrad`).

Other P2P Changes
-----------------

The list of banned peers is now stored on disk rather than in memory. Restarting
`zcashd` will no longer clear out the list of banned peers; instead the
`clearbanned` RPC method can be used to manually clear the list. The `setban`
RPC method can also be used to manually ban or unban a peer.

Build system updates
--------------------

- We now build with Clang 11 and Rust 1.49.
- We have downgraded Boost to 1.74 to mitigate `statx`-related breakage in some
  container environments.

Changelog
=========

Alex Wied (3):
      Add support for FreeBSD 12
      Set rust_target for all FreeBSD versions
      Use parentheses for defined in windows-unused-variables.diff

Alfredo Garcia (3):
      split wallet.py tests
      hide password in -stdin `walletpassphrase` command
      Apply suggestions from code review

Bernhard M. Wiedemann (1):
      Make tests pass after 2020

BtcDrak (1):
      Remove bad chain alert partition check

Carl Dong (13):
      build: Add variable printing target to Makefiles
      depends: Propagate well-known vars into depends
      depends: boost: Specify toolset to bootstrap.sh
      depends: boost: Split target-os from toolset
      depends: boost: Use clang toolset if clang in CXX
      depends: Propagate only specific CLI variables to sub-makes
      depends: boost: Refer to version in URL
      depends: boost: Split into non-/native packages
      depends: boost: Disable all compression
      depends: boost: Cleanup architecture/address-model
      depends: boost: Cleanup toolset selection
      depends: boost: Remove unnecessary _archiver_
      depends: boost: Specify cflags+compileflags

Casey Rodarmor (3):
      Make limited map actually respect max size
      Disallow unlimited limited maps
      Add limitedmap test

Cory Fields (6):
      depends: fix boost mac cross build with clang 9+
      build: add missing leveldb defines
      net: make Ban/Unban/ClearBan functionality consistent
      net: No need to export DumpBanlist
      net: move CBanDB and CAddrDB out of net.h/cpp
      net: Drop CNodeRef for AttemptToEvictConnection

Daira Hopwood (2):
      qa/zcash/updatecheck.py: remove dead code; print instructions to run `cargo outdated` and `cargo update`.
      Ensure that `CONFIGURE_FLAGS=--enable-debug` correctly uses -O0 for dependencies and main build.

Daniel Kraft (1):
      doc: add comment explaining initial header request

Dimitris Apostolou (1):
      Discord invite instead of direct link

Ethan Heilman (1):
      Fix de-serialization bug where AddrMan is corrupted after exception * CAddrDB modified so that when de-serialization code throws an exception Addrman is reset to a clean state * CAddrDB modified to make unit tests possible * Regression test created to ensure bug is fixed * StartNode modifed to clear adrman if CAddrDB::Read returns an error code.

Gregory Maxwell (2):
      Return early in IsBanned.
      Disconnect on mempool requests from peers when over the upload limit.

Igor Cota (1):
      Define TARGET_OS when host is android

Jack Grigg (46):
      Add JSDescriptionInfo for constructing JSDescriptions
      Remove JSDescription::h_sig
      prevector: Terminate without logging on failed allocation
      Remove init_and_check_sodium from crypto/common.h
      Store inputs and outputs by reference in JSDescriptionInfo
      depends: Update Rust to 1.49.0
      rust: Use renamed broken_intra_doc_links lint
      depends: cargo update
      depends: Move to Clang 11
      depends: Fix Boost warnings under Clang 11
      depends: Allow per-host package download paths
      depends: Ensure the native_clang download path is for the builder
      Revert "Update boost to 1.75, postpone other updates."
      qa: Postpone Boost and native_b2 updates
      QA: Remove unused update postponements
      QA: Postpone BDB update again
      depends: ZeroMQ 4.3.4
      depends: Postpone updates that require adding CMake
      cargo update
      Squashed 'src/univalue/' changes from 9ef5b78c1..98fadc090
      Remove crypto/equihash from libzcashconsensus
      Add amount and consensus branch ID to zcashconsensus_verify_script
      Rename src/script/zcashconsensus.* -> src/script/zcash_script.*
      Rename zcashconsensus_* -> zcash_script_* in APIs
      Rename libzcashconsensus.la -> libzcash_script.la
      Set up an mdbook in which we can document zcashd's architecture design
      Actions: Add a workflow to deploy the zcashd book
      Show README as root of zcashd book
      Link to zips.z.cash for protocol spec
      test: Convert Bech32 test vectors into known-answer test vectors
      zcash_script: Add API to verify scripts with precomputed tx data
      test: Migrate maxuploadtarget.py to Python 3
      Pass current PoWTargetSpacing() into CNode::OutboundTargetReached()
      init: Pass post-Blossom spacing into CNode::SetMaxOutboundTarget()
      Fix some typos
      zcash_script: Clarify return values in docs
      Rename responsible_disclosure.md to SECURITY.md
      Add IBD download traffic reduction to release notes
      Fill out the rest of the 4.3.0 release notes
      make-release.py: Versioning changes for 4.3.0-rc1.
      make-release.py: Updated manpages for 4.3.0-rc1.
      make-release.py: Updated release notes and changelog for 4.3.0-rc1.
      cargo update
      rust: Pin funty =1.1.0
      make-release.py: Versioning changes for 4.3.0.
      make-release.py: Updated manpages for 4.3.0.

Jeremy Rubin (1):
      Minimal fix to slow prevector tests as stopgap measure

Jonas Schnelli (14):
      banlist.dat: store banlist on disk
      CAddrDB/CBanDB: change filesize variables from int to uint64_t
      use CBanEntry as object container for banned nodes
      Adding CSubNet constructor over a single CNetAddr
      [Qt] add ui signal for banlist changes
      [Qt] banlist, UI optimizing and better signal handling
      net: use CIDR notation in CSubNet::ToString()
      [QA] fix netbase tests because of new CSubNet::ToString() output
      Introduce -maxuploadtarget
      [doc] add documentation how to reduce traffic
      don't enforce maxuploadtargets disconnect for whitelisted peers
      [docs] rename reducetraffic.md to reduce-traffic.md
      add documentation for exluding whitelistes peer from maxuploadtarget
      add (max)uploadtarget infos to getnettotals RPC help

Josh Lehan (1):
      Re-organize -maxconnections option handling

Karel Bilek (1):
      scripted-diff: Use UniValue.pushKV instead of push_back(Pair())

Kaz Wesley (4):
      fix race that could fail to persist a ban
      prevector: destroy elements only via erase()
      test prevector::swap
      prevector::swap: fix (unreached) data corruption

Kris Nuttycombe (4):
      Skip "tx" messages during initial block download.
      Fix pyflakes complaints
      Whitespace-only fix in chainparams.cpp
      Update the maxuploadtarget.py tests to accommodate zcash.

Larry Ruane (1):
      #4624 improve IBD sync by eliminating getheaders requests

Marco Falke (3):
      [net] Cleanup maxuploadtarget
      [doc] Add -maxuploadtarget release notes
      [walletdb] Add missing LOCK() in Recover() for dummyWallet

Marius Kjærstad (3):
      Update _COPYRIGHT_YEAR in configure.ac to 2021
      Update COPYRIGHT_YEAR in clientversion.h to 2021
      Update of copyright year to 2021

Marshall Gaucher (1):
      add libxml2

Matt Corallo (6):
      Default to defining endian-conversion DECLs in compat w/o config
      Make fDisconnect an std::atomic
      Make fImporting an std::atomic
      Fix AddrMan locking
      Remove double brackets in addrman
      Fix unlocked access to vNodes.size()

Patrick Strateman (1):
      Avoid recalculating vchKeyedNetGroup in eviction logic.

Peter Bushnell (1):
      depends: Consistent use of package variable

Peter Todd (1):
      Remove LOCK(cs_main) from decodescript

Philip Kaufmann (4):
      banlist: update set dirty to be more fine grained
      banlist: better handling of banlist in StartNode()
      banlist: add more banlist infos to log / add GUI signal
      banlist (bugfix): allow CNode::SweepBanned() to run on interval

Pieter Wuille (11):
      Add SipHash-2-4 primitives to hash
      Use SipHash-2-4 for CCoinsCache index
      Switch CTxMempool::mapTx to use a hash index for txids
      Use SipHash-2-4 for address relay selection
      Add extra message to avoid a long 'Loading banlist'
      Support SipHash with arbitrary byte writes
      Use 64-bit SipHash of netgroups in eviction
      Use C++11 thread-safe static initializers
      Add ChaCha20
      Switch FastRandomContext to ChaCha20
      Add a FastRandomContext::randrange and use it

Suhas Daftuar (1):
      Add RPC test for -maxuploadtarget

Taylor Hornby (2):
      Move the github API token out of updatecheck.py into an untracked file.
      Document the required .updatecheck-token file in the release docs

Wladimir J. van der Laan (6):
      build: Updates for OpenBSD
      net: Fix CIDR notation in ToString()
      test: Add more test vectors for siphash
      timedata: Prevent warning overkill
      Always allow getheaders from whitelisted peers
      rpc: remove cs_main lock from `createrawtransaction`

fanquake (2):
      [build] Add NETBSD leveldb target to configure.ac
      [doc] Improve lanaguge in reducetraffic.md

kirkalx (1):
      peers.dat, banlist.dat recreated when missing

Marshall Gaucher (1):
      add zstd package

randy-waterhouse (1):
      Re-instate TARGET_OS=linux in configure.ac. Removed by 351abf9e035.

Jack Grigg (1):
      Update URL for Zcash Foundation security policy

Ying Tong Lai (2):
      Cargo update
      Postpone dependencies

tulip (1):
      Move time data log print to 'net' category to reduce log noise

Benjamin Winston (1):
      Added foundation to responsible_disclosure.md

