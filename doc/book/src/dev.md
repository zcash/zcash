# Developer Documentation

This section contains documentation aimed at contributors to the `zcashd`
codebase.

Development tips and tricks
---------------------------

**compiling for debugging**

Run `configure` with the `--enable-debug` option, then `make`. Or run
`configure` with `CXXFLAGS="-g -ggdb -O0"` or whatever debug flags you need.

**compiling for gprof profiling**

Run `configure` with the `--enable-gprof` option, then `make`.

**debug.log**

If the code is behaving strangely, take a look in the `debug.log` file in the
data directory; error and debugging messages are written there.

The `-debug=...` command-line option controls debugging; running with just
`-debug` or `-debug=1` will turn on all categories (and give you a very large
`debug.log` file).

**testnet and regtest modes**

Run with the `-testnet` option to run with "play zcash" on the test network, if
you are testing multi-machine code that needs to operate across the internet.
Testnet ZEC can be obtained through local mining (the difficulty on the testnet
is typically low) or from one of the Zcash testnet faucets at
https://faucet.testnet.z.cash/ or https://faucet.zecpages.com/.

If you are testing something that can run on one machine, run with the
`-regtest` option.  In regression test mode, blocks can be created on-demand;
see `qa/rpc-tests/` for tests that run in `-regtest` mode.

Pull Request Terminology
------------------------

The following terms are commonly used in review of pull requests:

Concept ACK - Agree with the idea and overall direction, but have neither reviewed nor tested the code changes.

utACK (untested ACK) - Reviewed and agree with the code changes but haven't actually tested them.

Tested ACK - Reviewed the code changes and have verified the functionality or bug fix.

ACK -  A loose ACK can be confusing. It's best to avoid them unless it's a documentation/comment only change in which case there is nothing to test/verify; therefore the tested/untested distinction is not there.

NACK - Disagree with the code changes/concept. Should be accompanied by an explanation.

See the [Development Guidelines](https://zcash.readthedocs.io/en/latest/rtd_pages/development_guidelines.html) documentation for preferred workflows, information on continuous integration and release versioning.

Scripted diffs
--------------

For reformatting and refactoring commits where the changes can be easily automated using a bash script, we use
scripted-diff commits. The bash script is included in the commit message and our Travis CI job checks that
the result of the script is identical to the commit. This aids reviewers since they can verify that the script
does exactly what it's supposed to do. It is also helpful for rebasing (since the same script can just be re-run
on the new master commit).

To create a scripted-diff:

- start the commit message with `scripted-diff:` (and then a description of the diff on the same line)
- in the commit message include the bash script between lines containing just the following text:

    - `-BEGIN VERIFY SCRIPT-`
    - `-END VERIFY SCRIPT-`

The scripted-diff is verified by the tool `test/lint/commit-script-check.sh`

Commit `ccd074a5` is an example of a scripted-diff.
