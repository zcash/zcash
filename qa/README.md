The [pull-tester](/qa/pull-tester/) folder contains a script to call
multiple tests from the [rpc-tests](/qa/rpc-tests/) folder.

Every pull request to the zcash repository is built and run through
the regression test suite. You can also run all or only individual
tests locally.

Test dependencies
=================
Before running the tests, the following must be installed.

Unix
----
The python3-zmq library and simplejson are required. On Ubuntu or Debian they
can be installed via:
```
sudo apt-get install python3-simplejson python3-zmq
```

OS X
------
```
pip3 install pyzmq simplejson
```

Running tests
=============

You can run any single test by calling

    RPC_TEST=<testname> make rpc-tests

Or you can run any combination of tests by calling

    RPC_TEST="<testname1> <testname2> <testname3> ..." make rpc-tests

Run the regression test suite with

    make rpc-tests

Run all possible tests with

    RPC_TEST="--extended" make rpc-tests

You can also run the tests directly using `qa/pull-tester/rpc-tests.py` instead
of `make`, but that won't ensure that zcashd is up-to-date with any changes.

By default, tests will be run in parallel. To specify how many jobs to run,
append `--jobs=n` (default n=4).

If you want to create a basic coverage report for the RPC test suite, append `--coverage`.

Possible options, which apply to each individual test run:

```
  -h, --help            show this help message and exit
  --nocleanup           Leave zcashds and test.* datadir on exit or error
  --noshutdown          Don't stop zcashds after the test execution
  --srcdir=SRCDIR       Source directory containing zcashd/zcash-cli
                        (default: ../../src)
  --tmpdir=TMPDIR       Root directory for datadirs
  --tracerpc            Print out all RPC calls as they are made
  --coveragedir=COVERAGEDIR
                        Write tested RPC commands into this directory
```

If you set the environment variable `PYTHON_DEBUG=1` you will get some debug
output (example: `PYTHON_DEBUG=1 qa/pull-tester/rpc-tests.py wallet`).

A 200-block -regtest blockchain and wallets for four nodes
is created the first time a regression test is run and
is stored in the cache/ directory.  Each node has the miner
subsidy from 25 mature blocks (25*10=250 ZEC) in its wallet.

After the first run, the cache/ blockchain and wallets are
copied into a temporary directory and used as the initial
test state.

If you get into a bad state, you should be able
to recover with:

```bash
rm -rf cache
killall zcashd
```

Writing tests
=============
You are encouraged to write tests for new or existing features.
Further information about the test framework and individual RPC
tests is found in [qa/rpc-tests](/qa/rpc-tests).
