(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Updated RPCs
------------

- Fixed an issue where `ERROR: spent index not enabled` would be logged
  unnecessarily on nodes that have either insightexplorer or lightwalletd
  configuration options enabled.

- The `getmininginfo` RPC now omits `currentblockize` and `currentblocktx`
  when a block was never assembled via RPC on this node during its current
  process instantiation. (#5404)
