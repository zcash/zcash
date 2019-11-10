Updated RPC unit-tests infrastructure for Antara smart-chain custom modules

Using pytest as testing framework and slickrpc as rpc proxy. No more python2 support.

To start just set test nodes RPC credentials in `test_config.json`.
I thought such config usage might be useful as in some manual testing plays as well as in some CI configuration tests integration.

`is_fresh_chain=False` param allows to run tests on existing chains (it skips some tests which expecting first CC usage on chain)

So yes - you can run these tests on existing chains, just RPC creds (and wallets with some balance) needed.

# Dependencies

`pip3 install setuptools wheel slick-bitcoinrpc pytest wget`

# Usage

In `~/komodo/qa/rpc-tests/cc_pytest` directory:

`python3 -m pytest -s` - starts all tests suite
`python3 -m pytest test_dice.py -s` - starts specific test, dice in this case

`-s` flag is optional, just displaying python prints which might be helpful in debugging

`ci_test.sh` script will start a all CCs full test suite from bootstrapped chain - best way to start the tests

The `start_chains.py` script can spin needed amount of nodes and start the test chain.
You can find an example of this script usage in `ci_setup.sh`. Don't forget to change `test_config.json` accordingly to the chain params.

Also there is bootstrap downloading functionality in `start_chains.py` what should be quite useful for automated testing setups

Happy testing! <3