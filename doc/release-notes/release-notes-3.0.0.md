Notable changes
===============

The mainnet activation of the Heartwood network upgrade is supported by this
release, with an activation height of 903000, which should occur in the middle
of July — following the targeted EOS halt of our 2.1.2-3 release. Please upgrade
to this release, or any subsequent release, in order to follow the Heartwood
network upgrade.

The following two ZIPs are being deployed as part of this upgrade:

- [ZIP 213: Shielded Coinbase](https://zips.z.cash/zip-0213)
- [ZIP 221: FlyClient - Consensus-Layer Changes](https://zips.z.cash/zip-0221)

In order to help the ecosystem prepare for the mainnet activiation, Heartwood
has already been activated on the Zcash testnet. Any node version 2.1.2 or
higher, including this release, supports the Heartwood activation on testnet.

## Mining to Sapling addresses

After the mainnet activation of Heartwood, miners can mine directly into a
Sapling shielded address. Miners should wait until after Heartwood activation
before they make changes to their configuration to leverage this new feature.
After activation of Heartwood, miners can add `mineraddress=SAPLING_ADDRESS` to
their `zcash.conf` file, where `SAPLING_ADDRESS` represents a Sapling address
that can be generated locally with the `z_getnewaddress` RPC command. Restart
your node, and block templates produced by the `getblocktemplate` RPC command
will now have coinbase transactions that mine directly into this shielded
address.

Changelog
=========

Aaron Clauson (1):
      Minimal code changes to allow msvc compilation.

Adam Langley (1):
      Switch memory_cleanse implementation to BoringSSL's to ensure memory clearing even with link-time optimization.

Alfredo Garcia (12):
      fix rpc testcase
      add blockheight, blockindex and blocktime to z_listreceivedbyaddress
      change time to blocktime in help
      add status to transactions
      minor fix
      minor cleanup style, var names
      Add a new safe chars rule for node version string
      fix wallet nullifiers test
      Fix typo
      add a test case
      implement z_getnotescount api call
      remove not needed help parameters to dump and import impl

Ben Wilson (4):
      Added Dockerfile to contrib with README
      Fixed README grammar, reuse Dockerfile vars
      Fixed Docker README grammar
      Dockerfiles for zcashd CI builds

Daira Hopwood (2):
      Fix a null pointer dereference that occurs when formatting an error message, if we haven't activated an upgrade as expected.
      Explicitly assert that chainActive[upgrade.nActivationHeight] is non-null at this point.

Dimitris Apostolou (1):
      Fix typos

Jack Grigg (5):
      Use BOOST_SCOPE_EXIT_TPL to clean and free datValue in CDB::Read
      Improve memory_cleanse documentation
      Add NU4 to upgrade list
      Add NU4 test helpers
      Update URLs for prior network upgrades

Jonathan "Duke" Leto (1):
      Add confirmations to z_listreceivedbyaddress

Kris Nuttycombe (25):
      Add a test reproducing the off-by-one error.
      Check network reunification.
      Narrow down the test case.
      Make the test reproduce the actual off-by-one error in rewind length.
      Fix #4119.
      The last valid height condition reads better flipped.
      Restart node in a chain split state to allow the test to complete.
      Trivial comment.
      Remove option to load new blocks from ConnectTip
      Make condition closer to original, Fix incorrect comment.
      Ensure that we don't pass a null block pointer to ConnectTip.
      Update all crates.
      Update to the Cargo V2 lockfile format.
      Clean up imports in sapling_rewind_check.py
      Use `%x` formatter for branch id hex string in test_framework/util.py
      Update qa/rpc-tests/test_framework/mininode.py
      Update qa/rpc-tests/sapling_rewind_check.py
      Add Zcash copyright to sapling_rewind_check.py
      Update test description and clarify internal comments.
      Revert "Update qa/rpc-tests/sapling_rewind_check.py"
      Remove unused imports.
      Add baseline for golden testing across network upgrade boundaries.
      Update golden test for heartwood network upgrade.
      Fully remove the regtest tree from restored nodes.
      Remove unused imports.

Sean Bowe (11):
      Add NU4 activation to golden test.
      Update minimum chain work on testnet to reflect Heartwood activation.
      Pass DO_NOT_UPDATE_CONFIG_SCRIPTS=1 to autogen.sh in libsodium dependency, to avoid updating config scripts over the network.
      Set the Heartwood activation height to 903000.
      Bump the protocol version, as this node supports Heartwood on mainnet.
      make-release.py: Versioning changes for 3.0.0-rc1.
      make-release.py: Updated manpages for 3.0.0-rc1.
      make-release.py: Updated release notes and changelog for 3.0.0-rc1.
      Set deprecation of 3.0.0 to target EOS halt mid-September.
      make-release.py: Versioning changes for 3.0.0.
      make-release.py: Updated manpages for 3.0.0.

Taylor Hornby (2):
      Add univalue to updatecheck.py and update univalue, removing calls to deprecated methods
      Avoid names starting with __.

Thomas Snider (1):
      [wallet] Securely erase potentially sensitive keys/values

Tim Ruffing (2):
      Clean up logic in memory_cleanse() for MSVC
      Improve documentation of memory_cleanse()

therealyingtong (1):
      Fix off-by-one error in CreateNewBlock()

