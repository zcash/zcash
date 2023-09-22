(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Deprecation of `fetch-params.sh`
--------------------------------

The `fetch-params.sh` script (also `zcash-fetch-params`) is now deprecated. The
`zcashd` binary now bundles zk-SNARK parameters directly and so parameters do not
need to be downloaded or stored separately. The script will now do nothing but
state that it is deprecated; it will be removed in a future release.

Previously, parameters were stored by default in these locations:

* `~/.zcash-params` (on Linux); or
* `~/Library/Application Support/ZcashParams` (on Mac); or
* `C:\Users\Username\AppData\Roaming\ZcashParams` (on Windows)

Unless you need to generate transactions using the deprecated Sprout shielded
pool, you can delete the parameter files stored in these directories to save
space as they are no longer read or used by `zcashd`. If you do wish to use the
Sprout pool, you will need the `sprout-groth16.params` file in the
aforementioned directory. This file is currently available for download
[here](https://download.z.cash/downloads/sprout-groth16.params).

Mempool metrics
---------------

`zcashd` now reports the following new metrics when `-prometheusport` is set:

- (gauge) `zcash.mempool.actions.unpaid { "bk" = ["< 0.2", "< 0.4", "< 0.6", "< 0.8", "< 1"] }`
- (gauge) `zcash.mempool.actions.paid`
- (gauge) `zcash.mempool.size.weighted { "bk" = ["< 1", "1", "> 1", "> 2", "> 3"] }`

The `zcash.mempool.actions` metrics count the number of [logical actions] across
the transactions in the mempool, while `zcash.mempool.size.weighted` is a
weight-bucketed version of the `zcash.mempool.size.bytes` metric.

The [ZIP 317 weight ratio][weight_ratio] of a transaction is used to bucket its
logical actions and byte size. A weight ratio of at least 1 means that the
transaction's fee is at least the ZIP 317 conventional fee, and all of its
logical actions are considered "paid". A weight ratio lower than 1 corresponds
to the fraction of the transaction's logical actions that are "unpaid", and
subject to the unpaid action limit that miners configure for their blocks with
`-blockunpaidactionlimit`.

[logical actions]: https://zips.z.cash/zip-0317#fee-calculation
[weight_ratio]: https://zips.z.cash/zip-0317#recommended-algorithm-for-block-template-construction
