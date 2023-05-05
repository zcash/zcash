(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Removal of Fee Estimation
-------------------------

The `estimatefee` RPC call, which estimated the fee needed for a transaction to
be included within a target number of blocks, has been removed. The fee that
should be paid under normal circumstances is the ZIP 317 conventional fee; it
is not possible to compute that without knowing the number of logical actions
in a transaction, which was not an input to `estimatefee`. The `fee_estimates.dat`
file is no longer used.

Platform Support
----------------

- Ubuntu 18.04 LTS has been removed from the list of supported platforms. It
  reached End of Support on May 31st 2023, and no longer satisfies our Tier 2
  policy requirements.
