(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

- Added a `z_converttex` RPC method to support conversion of transparent
  p2pkh addresses to the ZIP 320 (TEX) format.

- zcashd will now disconnect from a peer on receipt of a malformed `version`
  message, and also will reject duplicate `verack` messages from outbound
  connections. See https://github.com/zcash/zcash/pull/6781 for additional
  details.

