(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

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

The default -dbcache has been changed in this release to 450MiB. Users can set -dbcache to a higher value (e.g. to keep the UTXO set more fully cached in memory). Users on low-memory systems (such as systems with 1GB or less) should consider specifying a lower value for this parameter.

Additional information relating to running on low-memory systems can be found here: [reducing-memory-usage.md](https://github.com/zcash/zcash/blob/master/doc/reducing-memory-usage.md).
