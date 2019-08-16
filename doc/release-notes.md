(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Shorter Block Times
-------------------
Shorter block times are coming to Zcash! In the v2.0.7 release we have implemented [ZIP208](https://github.com/zcash/zips/blob/master/zip-0208.rst) which take effect when Blossom activates. Upon activation, the block times for Zcash will decrease from 150s to 75s, and the block reward will will decrease accordingly. This effects at what block height halving events will occur, but should not effect the overall rate at which Zcash is mined. The total founder's reward stays the same, and the total supply of Zcash is decreased microscopically due to rounding.

Blossom Activation on Testnet
-----------------------------
The v2.0.7 release includes Blossom activation on testnet bringing shorter block times. Testnet Blossom activation height is 584000.

Insight Explorer
----------------
The insight explorer has been incorporated in to Zcash. *This is an experimental feature* and therefore is subject change. To enable add the `experimentalfeatures=1`, `txindex=1`, and `insightexplorer=1` flags to `zcash.conf`. This feature adds several RPCs to `zcashd` which allow the user to run an insight explorer.
