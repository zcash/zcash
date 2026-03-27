(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Removed the P2P network alert system
------------------------------------

`zcashd` will no longer respond to alert messages from the P2P network.
The `-alerts` config option has been removed and will be ignored. This
ensures that, if any party were to maintain a zcashd fork, none of the
original developers or associated organizations (including ECC and ZODL)
will hold keys that could affect nodes running such a fork.

This leaves intact alerting for end-of-service and detected chain forks,
and the `-alertnotify` functionality.

Faster Sprout-to-Sapling migration
----------------------------------

The Sprout-to-Sapling migration tool added in `zcashd` 2.0.5 was designed to
slowly migrate funds between the two shielded pools over time in a way that
minimised linkability. In the seven years since then, the balance of the Sprout
pool has gradually decreased, and migration transactions are much less common
now than they were, in part because the original migration parameters specified
in ZIP 308 were very conservative.

With the original parameters, up to 5 migration transactions would be created
every 500 blocks, with an average migrated amount of 5 ZEC per transaction
(50 * 10^7 zatoshis). There is currently about 25k ZEC remaining in the Sprout
pool; if controlled by a single `zcashd` node, this would take around 500,000
blocks to migrate, or around 434 days.

This release updates the parameters of the migration tool to significantly
increase the migration speed, and thus requiring the user to run `zcashd` for a
shorter length of time. With these new parameters, the 5 migration transactions
are created every 100 blocks, with an average migrated amount of 500 ZEC per
transaction (50 * 10^9 zatoshis) if the remaining balance is greater than 1 ZEC.
The 25k ZEC in the Sprout pool (if controlled by a single `zcashd` node) would
instead take around 1000 blocks to migrate, or around 21 hours.
