(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

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
