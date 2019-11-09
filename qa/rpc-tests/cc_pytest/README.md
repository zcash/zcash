Updated RPC unit-tests infrastructure for Antara smart-chain custom modules

Using pytest as testing framework and slickrpc as rpc proxy.
No more python2, sorry oldfags !

To start just set test nodes RPC credentials in `test_config.json`.
I thought such config usage might be useful as in some manual testing plays as well as in some CI configuration tests integration.

`is_fresh_chain=False` param allows to run tests on existing chains (it skips some tests which expecting first CC usage on chain)

So yes - you can run these tests on existing chains, just RPC creds (and wallets with some balance) needed.

# Dependencies

`pip3 install setuptools wheel slick-bitcoinrpc pytest`

# Usage

In `~/komodo/qa/rpc-tests/cc_pytest` directory:

`python3 -m pytest -s` - starts all tests suite
`python3 -m pytest test_dice.py -s` - starts specific test, dice in this case

`-s` flag is optional, just displaying python prints which might be helpful in debugging

Happy testing! <3