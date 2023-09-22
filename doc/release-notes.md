(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Fix to `getbalance` output
--------------------------

`getbalance` will no longer include transparent value associated with
transparent receivers of unified addresses belonging to the wallet as part
of its returned balance; instead, it will only return the balance associated
with legacy transparent addresses. The inclusion of transparent balance for
unified address receivers in this result was a bug inadvertently introduced
in zcashd 4.7.0. See [issue #5941](https://github.com/zcash/zcash/issues/5941)
for additional detail.

Deprecation of `fetch-params.sh`
--------------------------------

The `fetch-params.sh` script (also `zcash-fetch-params`) is now deprecated. The
zcashd binary now bundles zk-SNARK parameters directly and so parameters do not
need to be downloaded or stored separately. The script will now do nothing but
state that it is deprecated; it will be removed in a future release.

Previously, parameters were stored by default in these locations:

* `~/.zcash-params` (on Linux); or
* `~/Library/Application Support/ZcashParams` (on Mac); or
* `C:\Users\Username\AppData\Roaming\ZcashParams` (on Windows)

Unless you need to generate transactions using the deprecated Sprout shielded
pool, you can delete the parameter files stored in these directories to save
space as they are no longer read or used by zcashd. If you do wish to use the
Sprout pool, you will need the `sprout-groth16.params` file in the
aforementioned directory. This file is currently available for download
[here](https://download.z.cash/downloads/sprout-groth16.params).
