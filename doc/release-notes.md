(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

The mainnet activation of the Canopy network upgrade is supported by the 4.0.0
release, with an activation height of 1046400, which should occur roughly in the
middle of November — following the targeted EOS halt of our 3.1.0 release.
Please upgrade to this release, or any subsequent release, in order to follow
the Canopy network upgrade.

The following ZIPs are being deployed as part of this upgrade:

* [ZIP 207: Funding Streams](https://zips.z.cash/zip-0207) in conjunction with [ZIP 214: Consensus rules for a Zcash Development Fund](https://zips.z.cash/zip-0214)
* [ZIP 211: Disabling Addition of New Value to the Sprout Value Pool](https://zips.z.cash/zip-0211)
* [ZIP 212: Allow Recipient to Derive Sapling Ephemeral Secret from Note Plaintext](https://zips.z.cash/zip-0212)
* [ZIP 215: Explicitly Defining and Modifying Ed25519 Validation Rules](https://zips.z.cash/zip-0215)

In order to help the ecosystem prepare for the mainnet activiation, Canopy has
already been activated on the Zcash testnet. Any node version 3.1.0 or higher,
including this release, supports the Canopy activation on testnet.

Disabling new value in the Sprout value pool
--------------------------------------------

After the mainnet activation of Canopy, it will not be possible to send funds to
Sprout z-addresses from any _other_ kind of address, as described in [ZIP 211](https://zips.z.cash/zip-0211).
It will still be possible to send funds _from_ a Sprout z-address and to send
funds between Sprout addresses. Users of Sprout z-addresses are encouraged to
use Sapling z-addresses instead, and to migrate their remaining Sprout funds
into a Sapling z-address using the migration utility in zcashd: set `migrate=1`
in your `zcash.conf` file, or use the `z_setmigration` RPC.

New logging system
------------------

The `zcashd` logging system is now powered by the Rust `tracing` crate. This
has two main benefits:

- `tracing` supports the concept of "spans", which represent periods of time
  with a beginning and end. These enable logging additional information about
  temporality and causality of events. (Regular log lines, which represent
  moments in time, are called `events` in `tracing`.)
- Spans and events are structured, and can record typed data in addition to text
  messages. This structure can then be filtered dynamically.

The existing `-debug=target` config flags are mapped to `tracing` log filters,
and will continue to correctly enable additional logging when starting `zcashd`.
A new `setlogfilter` RPC method has been introduced that enables reconfiguring
the log filter at runtime. See `zcash-cli help setlogfilter` for its syntax.

As a minor note, `zcashd` no longer reopens the `debug.log` file on `SIGHUP`.
This behaviour was originally introduced in upstream Bitcoin Core to support log
rotation using external tools. `tracing` supports log rotation internally (which
is currently disabled), as well as a variety of interesting backends (such as
`journald` and OpenTelemetry integration); we are investigating how these might
be exposed in future releases.

Compatibility
-------------
macOS versions earlier than 10.12 (Sierra) are no longer supported.
