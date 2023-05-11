(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

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
