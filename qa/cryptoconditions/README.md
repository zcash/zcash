# Integration tests for Crypto-Conditions

These tests are for the [Crypto-Conditions](https://github.com/rfcs/crypto-conditions) functionality in Komodod, using [Hoek](https://github.com/libscott/hoek) as a tool to create and sign transactions.

The tests use Python as a scripting language and each one should "just work" given Linux and a 2.7+ Python interpreter. Each file prefixed with "test_" is a test case.

The tests require `hoek` to be on the $PATH. Included is a script to perform a complete installation of `hoek`.
