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

Changelog
=========

Daira-Emma Hopwood (2):
      Removed the P2P network alert system.
      Update release notes to take account of the original zcashd developers moving from ECC.

Jack Grigg (19):
      rust: Migrate to `cargo-vet 0.10.2` format
      depends: native_cmake 4.2.2
      depends: native_fmt 12.1.0
      depends: native_ccache 4.12.2
      depends: tl_expected 1.3.1
      depends: cxx 1.0.186
      depends: utfcpp 4.0.9
      depends: native_zstd 1.5.7
      qa: Postpone remaining dependency updates
      Fix book build with latest `mdbook`
      depends: native_cmake 4.3.0
      depends: native_ccache 4.12.3
      qa: Postpone remaining dependency updates
      cargo vet prune
      rust: Require at least `tracing-subscriber 0.3.20`
      rust: Update to latest Zcash crate versions
      rust: Add MSRV pins
      rust: Update `cargo vet`-ed dependencies
      Speed up Sprout-to-Sapling migration

Kris Nuttycombe (5):
      Clean up remaining alert system remnants.
      CI: Use a variable to allow configuration of larger runners.
      Postpone native_cmake and native_rust updates.
      make-release.py: Versioning changes for 6.12.0.
      make-release.py: Updated manpages for 6.12.0.

dependabot[bot] (1):
      build(deps): bump actions/cache from 4 to 5

