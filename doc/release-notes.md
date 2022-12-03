(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

RPC Changes
-----------

- `z_sendmany` will no longer select transparent coinbase when "ANY\_TADDR" is
  used as the `fromaddress`. It was already documented to do this, but the
  previous behavior didn’t match. When coinbase notes were selected in this
  case, they would (properly) require that the transaction didn’t have any
  change, but this could be confusing, as the documentation stated that these
  two conditions (using "ANY\_TADDR" and disallowing change) wouldn’t coincide.
