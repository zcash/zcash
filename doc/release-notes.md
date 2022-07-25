(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Node Performance Improvements
-----------------------------

This release makes several changes to improve the performance of node operations.
These include:

- Backported CuckooCache from upstream to improve the performance of signature
  caching.

- Added caching of proof and signature validation results for Sapling and
  Orchard to eliminate redundant computation.

- Backported SHA-256 assembly optimizations from upstream.

Wallet Performance Improvements
-------------------------------

This release makes several changes to improve the performance of wallet operations.
These include:

- We now parallelize and batch trial decryption of Sapling outputs.

- We now prune witness data in the wallet for notes spent more than 100 blocks
  in the past, so that we can avoid unnecessarily updating those witnesses.
  In order to take advantage of this performance improvement, users will need
  to start their nodes with `-rescan` one time, in order to ensure that witnesses
  for spent notes are in the wallet are properly pruned.

- The process for incrementing the witnesses for notes the wallet is tracking
  has been optimized to avoid redundant passes over the wallet.

- Removed an assertion that was causing a slowdown in wallet scanning post-NU5.

RPC Interface Changes
=====================

- A `version` field was added to the result for the `gettransaction` RPC call to
  avoid the need to make an extra call to `getrawtransaction` just to retrieve
  the version.

Fixes
=====

- Fixed a regression that caused an incorrect process name to appear in the
  process list.
