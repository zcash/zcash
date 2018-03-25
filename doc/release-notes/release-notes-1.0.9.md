Amgad Abdelhafez (2):
      Update timedata.cpp
      Update timedata.cpp

Daira Hopwood (4):
      Fix an error reporting bug due to BrokenPipeError and ConnectionResetError not existing in Python 2. refs #2263
      Alert 1002 (versions 1.0.0-1.0.2 inclusive).
      Alert 1003 (versions 1.0.3-1.0.8 inclusive).
      Disable building Proton by default.

Jack Grigg (14):
      Fix prioritisetransaction RPC test
      torcontrol: Handle escapes in Tor QuotedStrings
      torcontrol: Add missing copyright header
      Convert Zcash versions to Debian format
      [manpage] Handle build numbers in versions
      Address Daira's comments
      Address Daira's further comments
      Correctly handle three-digit octals with leading digit 4-7
      Check that >3-digit octals are truncated.
      Implement automatic shutdown of deprecated Zcash versions
      Wrap messages nicely on metrics screen
      Regenerate miner tests
      Add a benchmark for calling ConnectBlock on a block with many inputs
      Remove additional sources of determinism from benchmark archive

Jay Graber (2):
      Change help text examples to use Zcash addresses
      Poll on getblocktemplate result rather than use bare sleep to avoid race condition.

Nathan Wilcox (39):
      [Direct master commit] Fix a release snafu in debian version string.
      Show toolchain versions in build.sh.
      Start on a make-release.py script; currently just arg parsing and unittests [unittests fail].
      Update version spec by altering test; also update regex to pass single 0 digits in major/minor/patch.
      Add another case from debian-style versions.
      Add all of the zcash release tags in my current repo as positive test vector.
      Add support for beta/rc release versions.
      Add version sorting, assert that RELEASE_PREV is the most recent release.
      Make SystemExit errors less redundant in output; verify clean git status on master.
      Always run unittests prior to actual runs.
      Make --help output clean by not running self-test.
      Add an option to run against a different repo directory.
      Make sure to pull the latest master.
      Exit instead of raising an unexpected exception, since it's already logged.
      Implement `PathPatcher` abstraction, `clientversion.h` rewrite, and build numbering w/ unittests.
      Implement the IS_RELEASE rule for betas.
      Generalize buildnum patching for both `clientversion.h` and `configure.ac`.
      Modify the `APPROX_RELEASE_HEIGHT`.
      Remove portions of `./doc/release-process.md` now implemented in `make-release.py`.
      Switch from `sh_out_logged` to `sh_log`.
      Shorten the arg log line.
      Commit the version changes and build.
      Generate manpages; commit that; improve error output in sh_log.
      Polish logging a bit more.
      Tidy up / systematize logging output a bit more.
      First full-release-branch version of script; rewrite large swatch of release-process.md. [Manually tested.]
      Enable set -u mode.
      Fix a variable name typo.
      Reuse zcash_rpc.
      Do not use `-rpcwait` on all `zcash_rpc` invocations, only block when starting zcashd.
      Fix `release-process.md` doc usage for `make-release.py` to have correct arguments and order.
      Include release version in commit comments.
      Examine all future versions which are assumed to follow the same Version parser schema.
      Consider both beta and rc versions to be `IS_RELEASE == false`.
      Add a few more version strings to positive parser test.
      Define the deprecation policy for 1.0.9.
      Clarify that the feature is automated *shutdown*.
      make-release.py: Versioning changes for 1.0.9.
      make-release.py: Updated manpages for 1.0.9.

Paige Peterson (4):
      wallet backup instructions
      typo and rewording edits
      str4d and Ariel's suggestions
      specify exportdir being within homedirectory

Sean Bowe (1):
      Check that pairings work properly when the G1 point is at infinity.

Simon Liu (5):
      Add AMQP 1.0 support via Apache Qpid Proton C++ API 0.17.0
      Add --disable-proton flag to build.sh.  Proton has build/linker issues with gcc 4.9.2 and requires gcc 5.x.
      Fix proton build issue with debian jessie, as used on CI servers.
      Change regtest port to 18344.  Closes #2269.
      Patch to build Proton with minimal dependencies.

emilrus (1):
      Replace bitcoind with zcashd

