4ZEC (1): Correct line swap

Cory Fields (7): release: add \_IO\_stdin\_used to ignored exports
release: add check-symbols and check-security make targets release:
always link librt for glibc back-compat builds release: add
security/symbol checks to gitian depends: allow for CONFIG\_SITE to be
used rather than stealing prefix gitian: use CONFIG\_SITE rather than
hijacking the prefix gitian: create debug packages for linux/windows

Daira Hopwood (6): Fix RPC tests to not rely on accounts. Cosmetics in
RPC tests. Fix blank lines in DEBIAN/copyright license texts. Move the
increment of nWitnessCacheSize to make the later assertions correct. Add
another assertion to narrow down where the bug occurs. Add another
assertion about the witness cache.

Jack Grigg (25): Update release process to sign release tags
WriteWitnessCache: Catch errors and abort transaction Throw an RPC error
for all accounts except the default Update tests for account deprecation
Deprecated -> Unsupported in RPC error Correct docstring Add unit tests
for WriteWitnessCache Document CWalletTx::FindMyNotes Refactor test to
clarify expectations Add unit test that fails when calling FindMyNotes
on a locked wallet Add RPC test showing correct handling of JS txns from
blockchain Break the RPC test by encrypting the mirroring wallet Delay
caching of nullifiers when wallet is locked Update comments Only ignore
runtime errors caused by failed note decryption Remaining changes from
bitcoin/bitcoin#6854 [gitian] Don't call "make check-symbols" Fix
Makefiles so "make dist" will run Render full version correctly in
configure.ac Update libsnark to include determinism fix Address review
comments Add more asserts to track down the bug Increment witnesses for
new transactions on rescan Add clear error message for upgrading users
Set CBlockIndex.hashAnchor correctly in ConnectBlock

Jay Graber (17): Document wallet reindexing for z\_importkey description
in payment-api.md Rm beta 1 release note about encrypted wallets Note
that Coinbase maturity interval does not protect JoinSplits Refer to
Zcash wiki in INSTALL Rm bitcoin logo Rm build-unix.md, to keep single
copy of build instructions for Zcash on github wiki Rm Bitcoin-specific
documentation Add note that document is not updated for Zcash to
translation policy Rm doc for disabled REST interface Change alpha to
beta testnet, add zcash hidden service Improve documentation on
connecting to zcash hidden server Improve documentation on connecting to
zcash hidden server Update tor.md Distinguish between connecting to 1 vs
multiple tor nodes Revert "Rm Bitcoin-specific documentation" Mv btc
release notes to doc/bitcoin-release-notes Reword joinsplit anchor
paragraph

Kevin Gallagher (24): Set wget retry options for fetching parameters
Increases timeout to 30s, wait before retry to 3s Initial packaging for
Debian Moves zcash-fetch-params to /usr/bin Adds newline between source
and package definition Adds copyright file back to Debian package
Updates Linux gitian descriptor file for Zcash Updates trusty -> jessie
in Gitian Linux descriptor Adds distro: debian to gitian-linux.yml
Updates Gitian descriptor for Zcash Removes Windows and OSX packaging
from EXTRA\_DIST Moves V=1 and NO\_QT=1 to MAKEOPTS Include
contrib/devtools/split-debug.sh from upstream Adds faketime to Gitian
build dependencies Inlude crypto/equihash.tcc in list of sources for
dist Adds zcash/Zcash.h to LIBZCASH sources Adds zcash/Proof.hpp to
LIBZCASH\_H Add alertkeys.h to libbitcoin\_server\_a\_SOURCES Adds files
in src/zcash/circuit to libzcash\_a\_SOURCES Adds zcbenchmarks.h to
libbitcoin\_wallet\_a\_SOURCES Adds json\_test\_vectors.h to
zcash\_gtest\_SOURCES Adds additional licenses to Debian copyright file
Updates Zcash Core developers -> Zcash developers Adds . to blank lines
in Google license

MarcoFalke (3): [gitian] Set reference date to something more recent
[gitian] Default reference\_datetime to commit author date [gitian]
hardcode datetime for depends

Sean Bowe (1): Make 100KB transaction size limit a consensus rule,
rather than a standard rule.

Simon (11): Add vjoinsplit to JSON output of RPC call gettransaction
Fixes #1478 by ensuring wallet tests have the -datadir environment set
appropriately. Fixes #1491 by updating help message for rpc call
z\_importkey Fix incorrect check of number of parameters for
z\_getnewaddress. Add tests to verify that all z\_\* rpc calls return an
error if there are too many input parameters. Rename client identifier
from Satoshi to MagicBean (closes #1481) Use -debug=zrpc for z\_\* rpc
calls (#1504) Document CWallet::GetFilteredNotes and fix return type
which should be void. Fix test so that the encrypted wallet is output to
the test\_bitcoin -datadir folder. Reorder gtests in zcash-gtest. Return
improved error message when trying to spend Coinbase coins (#1373).

Wladimir J. van der Laan (6): devtools: add libraries for bitcoin-qt to
symbol check gitian: use trusty for building gitian: make windows build
deterministic gitian: Need ``ca-certificates`` and ``python`` for LXC
builds build: Remove unnecessary executables from gitian release gitian:
Add --disable-bench to config flags for windows
