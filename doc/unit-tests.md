Compiling/running automated tests
---------------------------------

Automated tests will be automatically compiled if dependencies were met in configure
and tests weren't explicitly disabled.

There are two scripts for running tests:

* ``qa/zcash/full-test-suite.sh``, to run the main test suite
* ``qa/pull-tester/rpc-tests.sh``, to run the RPC tests.

The main test suite uses two different testing frameworks. Tests using the Boost
framework are under ``src/test/``; tests using the Google Test/Google Mock
framework are under ``src/gtest/`` and ``src/wallet/gtest/``. The latter framework
is preferred for new Zcash unit tests.

RPC tests are implemented in Python under the ``qa/rpc-tests/`` directory.
