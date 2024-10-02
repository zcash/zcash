(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

The mainnet activation of the NU6 network upgrade is supported by the 6.0.0
release, with an activation height of 2726400, which should occur on
approximately November 23, 2024. Please upgrade to this release, or any
subsequent release, in order to follow the NU6 network upgrade.

The following ZIPs are being deployed, or have been updated, as part of this upgrade:

* [ZIP 207: Funding Streams (updated)](https://zips.z.cash/zip-0207)
* [ZIP 214: Consensus rules for a Zcash Development Fund (updated)](https://zips.z.cash/zip-0214)
* [ZIP 236: Blocks should balance exactly](https://zips.z.cash/zip-0236)
* [ZIP 253: Deployment of the NU6 Network Upgrade](https://zips.z.cash/zip-0253)
* [ZIP 1015: Block Reward Allocation for Non-Direct Development Funding](https://zips.z.cash/zip-1015)
* [ZIP 2001: Lockbox Funding Streams](https://zips.z.cash/zip-2001)

In order to help the ecosystem prepare for the mainnet activation, NU6 has
already been activated on the Zcash testnet. Any node version 5.10.0 or higher,
including this release, supports the NU6 activation on testnet.

Mining
------

- The default setting of `-blockunpaidactionlimit` is now zero, which has
  the effect of no longer allowing "unpaid actions" in [block production].
  This adapts to current network conditions. If you have overridden this
  setting as a miner, we recommend removing the override. This configuration
  option may be removed entirely in a future release.

[block production]: https://zips.z.cash/zip-0317#block-production

Platform Support
----------------

- Windows builds have been fixed.
