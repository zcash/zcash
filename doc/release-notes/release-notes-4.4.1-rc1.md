Notable changes
===============

Build system changes
--------------------

- Cross-compilation support for Windows XP, Windows Vista, and 32-bit Windows
  binaries, has been removed. Cross-compiled Windows binaries are now 64-bit
  only, and target a minimum of Windows 7.

Changelog
=========

251 (1):
      Removes unsed `CBloomFilter` constructor.

Akio Nakamura (1):
      Prevent mutex lock fail even if --enable-debug

Anthony Towns (1):
      doc: clarify CRollingBloomFilter size estimate

Ben Woosley (1):
      Drop defunct Windows compat fixes

Chun Kuan Lee (3):
      windows: Set _WIN32_WINNT to 0x0601 (Windows 7)
      windows: Call SetProcessDEPPolicy directly
      build: Remove WINVER pre define in Makefile.leveldb.inlcude

Cory Fields (2):
      rpc: work-around an upstream libevent bug
      rpc: further constrain the libevent workaround

Jack Grigg (7):
      build: Link to libbcrypt on Windows
      doc: Add Windows version support change to release notes
      Postpone dependency updates until after 4.4.1
      Fix Rust dependency name in postponed-updates.txt
      Postpone native_ccache 4.3
      make-release.py: Versioning changes for 4.4.1-rc1.
      make-release.py: Updated manpages for 4.4.1-rc1.

João Barbosa (5):
      bench: Add benchmark for CRollingBloomFilter::reset
      refactor: Improve CRollingBloomFilter::reset by using std::fill
      wallet: Remove unnecessary mempool lock in ReacceptWalletTransactions
      [rpc] Reduce scope of cs_main and cs_wallet locks in listtransactions
      [wallet] Make CWallet::ListCoins atomic

Karl-Johan Alm (1):
      Refactor: Remove using namespace <xxx> from src/*.cpp.

Larry Ruane (1):
      when rewinding, set pindexBestHeader to the highest-work block index

Luke Dashjr (2):
      depends: Patch libevent build to fix IPv6 -rpcbind on Windows
      Move Win32 defines to configure.ac to ensure they are globally defined

Marko Bencun (1):
      keystore GetKeys(): return result instead of writing to reference

Martin Ankerl (1):
      replace modulus with FastMod

Matt Corallo (4):
      Add ability to assert a lock is not held in DEBUG_LOCKORDER
      Remove redundant pwallet nullptr check
      Hold mempool.cs for the duration of ATMP.
      Add braces to meet code style on line-after-the-one-changed.

MeshCollider (1):
      Make fUseCrypto atomic

Pavel Janík (1):
      Do not shadow variables

Pieter Wuille (3):
      Switch to a more efficient rolling Bloom filter
      Fix formatting of NOPs for generated script tests
      More efficient bitsliced rolling Bloom filter

Robert McLaughlin (1):
      trivial: fix bloom filter init to isEmpty = true

Russell Yanofsky (1):
      Acquire cs_main lock before cs_wallet during wallet initialization

S. Matthew English (1):
      unification of Bloom filter representation

Sebastian Falbesoner (2):
      refactor: Remove unused methods CBloomFilter::reset()/clear()
      net: remove is{Empty,Full} flags from CBloomFilter, clarify CVE fix

Wladimir J. van der Laan (3):
      http: Join worker threads before deleting work queue
      http: Remove WaitExit from WorkQueue
      http: Remove numThreads and ThreadCounter

fanquake (6):
      build: remove WINDOWS_BITS from build system
      build: remove configure checks for win libraries we don't link against
      build: remove --large-address-aware linker flag
      build: don't pass -w when building for Windows
      build: enforce minimum required Windows version (7)
      build: pass _WIN32_WINNT=0x0601 when building libevent for Windows

kobake (2):
      Fix msvc compiler error C4146 (minus operator applied to unsigned type)
      Fix msvc compiler error C4146 (unary minus operator applied to unsigned type)

Marshall Gaucher (3):
      Add Debian 11 ci-builder
      clean up ubuntu 18.04 and 20.04 commands
      add tekton build/worker docker, organize legacy buildbot docker

practicalswift (2):
      addrman: Add missing lock in Clear() (CAddrMan)
      Add missing cs_main locks when calling blockToJSON/blockheaderToJSON

Jack Grigg (1):
      doc: Add removal of 32-bit Windows binaries to release notes

ロハン ダル (1):
      param variables made const

