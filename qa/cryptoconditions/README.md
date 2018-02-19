# Integration tests for Crypto-Conditions

These tests are for the [Crypto-Conditions](https://github.com/rfcs/crypto-conditions) functionality in Komodod, using [Hoek](https://github.com/libscott/hoek) as a tool to create and sign transactions.

## How to run the tests

1. Install hoek: https://github.com/libscott/hoek
2. Start komodod: `src/komodod -ac_name=CCTEST -ac_supply=21000000 -gen -pubkey=0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47 -ac_cc=1`
3. Set environment variable pointing to Komodo conf: `export KOMODO_CONF_PATH=~/.komodo/CCTEST/CCTEST.conf`
4. Run tests: `python test_integration.py` (you can also use a test runner such as `nose`).
