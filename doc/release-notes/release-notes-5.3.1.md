Notable changes
===============

Fixed
-----

This release fixes an error "Assertion `uResultHeight == rewindHeight` failed" (#5958)
that could sometimes happen when restarting a node.

Memory Usage Improvement
------------------------

The memory usage of zcashd has been reduced by not keeping Equihash solutions for all
block headers in memory.

RPC changes
-----------

The following RPC methods that query wallet state now support an optional `asOfHeight`
parameter, to execute the query as if it were run when the blockchain was at the height
specified by this argument:

* `getbalance`
* `getreceivedbyaddress`
* `gettransaction` (*)
* `getwalletinfo`
* `listaddressgroupings`
* `listreceivedbyaddress` (*)
* `listsinceblock` (*)
* `listtransactions`
* `listunspent` (*)
* `z_getbalanceforaccount`
* `z_getbalanceforviewingkey`
* `z_getmigrationstatus`
* `z_getnotescount`
* `z_listreceivedbyaddress`
* `z_listunspent`

(*) For these methods, additional parameters have been added to maintain
compatibility of parameter lists with Bitcoin Core. Default values should be
passed for these additional parameters in order to use `asOfHeight`. See the
[RPC documentation](https://zcash.github.io/) for details.

Changelog
=========

Daira Hopwood (21):
      Always use a tuple as right argument of % in new Python code.
      Report the prevout for each transparent input as it is being checked
      Update authors of librustzcash to include Greg Pfeil.
      Ensure that the optimization of not scanning blocks prior to the wallet's birthday does not cause us to try to "rewind" the Orchard wallet to a height after its current checkpoint.
      Improve a comment about the wallet birthday scanning optimization.
      Add release notes for the fix to #5958.
      Fix a Markdown syntax error
      Error reporting improvements.
      Fix a dependency of the `show_help` RPC test on the number of cores, and an incompatibility with Python 3.9 in the test framework that affected the `receivedby` extended RPC test.
      Avoid storing the Equihash solution in a CBlockIndex object once it has been written to the leveldb database.
      Improve handling of database read errors.
      Add Prometheus metrics so we have more visibility into what is going on with the Equihash solution trimming:
      Declare `CBlockTreeDB::Read*` methods as `const` when they are trivially so.
      Update constants
      Add release notes for #6122 (`asOfHeight` RPC parameters).
      Update release notes: z_getnotescount already had the minconf parameter
      Add release notes for #6122 (`asOfHeight` RPC parameters).
      Update release notes: z_getnotescount already had the minconf parameter
      Bump timestamps and add libcxx/native_clang 15.0.6 in `postponed-updates.txt`.
      make-release.py: Versioning changes for 5.3.1.
      make-release.py: Updated manpages for 5.3.1.

Greg Pfeil (40):
      Fix display of binary name in error messages.
      Address review feedback and fixed test failures
      Check dependency updates on the correct branch
      updatecheck: fix GitHub auth
      updatecheck: simplify token handling
      updatecheck: support XDG-based token location
      `zcash --help` test improvements
      Remove the PR template
      Apply suggestions from code review
      Small formatting change
      Improve z_sendmany documentation
      Avoid inconsistent Python lookup
      Propagate asOfHeight to all relevant RPC calls
      Implement `asOfHeight`
      Add additional asOfHeight tests
      Don’t ignore asOfHeight in IsSpent calls
      Extract asOfHeight info from RPC calls
      Ignore mempool when asOfHeight is set
      Fix calls that should have specified asOfHeight
      GetUnconfirmedBalance should not take asOfHeight
      Require minconf > 0 when asOfHeight is provided
      Add error cases and default to `asOfHeight`
      Work around #6262 in wallet_listunspent
      Don’t trust mempool tx when using `asOfHeight`
      Apply suggestions from code review
      Add matured_at_height test helper
      Add FIXMEs to repair comments after #6262 is fixed
      Update src/rpc/server.cpp
      Apply suggestions from code review
      Fix small error in code review suggestions
      Revert change to getbalance minconf
      Revert getinfo support of asOfHeight
      Change asOfHeight to use -1 as default
      Change asOfHeight to preserve Bitcoin compat
      Apply suggestions from code review
      Simplify filtering AvailableCoins by destination
      Postpone dependency updates for v5.3.1
      make-release.py: Versioning changes for 5.3.1-rc1.
      make-release.py: Updated manpages for 5.3.1-rc1.
      make-release.py: Updated release notes and changelog for 5.3.1-rc1.

Jack Grigg (1):
      Place zcashd.debug.* metrics behind a -debugmetrics config option

Kris Nuttycombe (3):
      Add extra detail related to transparent inputs and outputs.
      Add `unspent_as_of` argument to `listunspent`
      Add RPC test for wallet_listunspent changes

Miodrag Popović (2):
      FindNextBlocksToDownload(): Fetch active consensus params to read nMinimumChainWork
      Headers sync timeout: Use EstimateNetHeight() for closer approximation of remaining headers to download

Suhas Daftuar (2):
      Delay parallel block download until chain has sufficient work
      Add timeout for headers sync

idm (1):
      fix aarch64 dependency native clang download URL

sasha (3):
      Update gitian-linux-parallel.yml
      Fix gitian version string issue by reverting the GIT_DIR backport commit
      Remove `git_check_in_repo` from genbuild.sh to fix gitian version string

