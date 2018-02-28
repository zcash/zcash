(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Overwinter network upgrade
--------------------------

The code preparations for the Overwinter network upgrade, as described in [ZIP
200](https://github.com/zcash/zips/blob/master/zip-0200.rst), [ZIP
201](https://github.com/zcash/zips/blob/master/zip-0201.rst), [ZIP
202](https://github.com/zcash/zips/blob/master/zip-0202.rst), [ZIP
203](https://github.com/zcash/zips/blob/master/zip-0203.rst), and [ZIP
143](https://github.com/zcash/zips/blob/master/zip-0143.rst) are
finished and included in this release. Overwinter will activate on testnet at
height 207500, and can also be activated at a specific height in regtest mode
by setting the config option `-nuparams=5ba81b19:HEIGHT`.

However, because the Overwinter activation height is not yet specified for
mainnet, version 1.0.15 will behave similarly as other pre-Overwinter releases
even after a future activation of Overwinter on the network. Upgrading from
1.0.15 will be required in order to follow the Overwinter network upgrade on
mainnet.

Overwinter transaction format
-----------------------------

Once Overwinter has activated, transactions must use the new v3 format
(including coinbase transactions). All RPC methods that create new transactions
(such as `createrawtransaction` and `getblocktemplate`) will create v3
transactions once the Overwinter activation height has been reached.

Overwinter transaction expiry
-----------------------------

Overwinter transactions created by `zcashd` will also have a default expiry
height set (the block height after which the transaction becomes invalid) of 20
blocks after the height of the next block. This can be configured with the
config option `-txexpirydelta`.

UTXO and note merging
---------------------

In order to simplify the process of combining many small UTXOs and notes into a
few larger ones, a new RPC method `z_mergetoaddress` has been added. It merges
funds from t-addresses, z-addresses, or both, and sends them to a single
t-address or z-address.

Unlike most other RPC methods, `z_mergetoaddress` operates over a particular
quantity of UTXOs and notes, instead of a particular amount of ZEC. By default,
it will merge 50 UTXOs and 10 notes at a time; these limits can be adjusted with
the parameters `transparent_limit` and `shielded_limit`.

`z_mergetoaddress` also returns the number of UTXOs and notes remaining in the
given addresses, which can be used to automate the merging process (for example,
merging until the number of UTXOs falls below some value).

UTXO memory accounting
----------------------

The default `-dbcache` has been changed in this release to 450MiB. Users can set
`-dbcache` to a higher value (e.g. to keep the UTXO set more fully cached in
memory). Users on low-memory systems (such as systems with 1GB or less) should
consider specifying a lower value for this parameter.

Additional information relating to running on low-memory systems can be found
here: [reducing-memory-usage.md](https://github.com/zcash/zcash/blob/master/doc/reducing-memory-usage.md).
