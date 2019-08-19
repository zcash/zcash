(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Shorter Block Times
-------------------
Shorter block times are coming to Zcash! In the v2.0.7 release we have implemented [ZIP208](https://github.com/zcash/zips/blob/master/zip-0208.rst) which will take effect when Blossom activates. Upon activation, the block times for Zcash will decrease from 150 seconds to 75 seconds, and the block reward will decrease accordingly. This affects at what block height halving events will occur, but should not affect the overall rate at which Zcash is mined. The total founders' reward stays the same, and the total supply of Zcash is decreased only microscopically due to rounding.

Blossom Activation on Testnet
-----------------------------
The v2.0.7 release includes Blossom activation on testnet, bringing shorter block times. The testnet Blossom activation height is 584000.

Insight Explorer
----------------
Changes needed for the Insight explorer have been incorporated into Zcash. *This is an experimental feature* and therefore is subject to change. To enable, add the `experimentalfeatures=1`, `txindex=1`, and `insightexplorer=1` flags to `zcash.conf`. This feature adds several RPCs to `zcashd` which allow the user to run an Insight explorer.
