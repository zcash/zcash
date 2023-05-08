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

Account recovery from emergency recovery phrase
-----------------------------------------------

Accounts can now be recovered using the emergency recovery phrase. This is done
by starting zcashd with the "-recoverwallet" flag, and providing a path to the
wallet recovery file. Below is an example recovery file to illustrate the
expected format:

    recovery_phrase="abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art"
    language="English"
    fingerprint="examplefingerprint"
    num_accounts=3

Only the `recovery_phrase=` line is mandatory. If there is no provided language,
zcashd will default to English. If there is no provided num_accounts, zcashd
will default to `num_accounts=1` (i.e. it will recover only account 0).
In particular, if we are recovering from a wallet dump taken before v5.3.1,
only account 0 will be recovered, unless `num_accounts` is manually edited.

In a future release, wallet recovery will be updated to also restore legacy
addresses.
