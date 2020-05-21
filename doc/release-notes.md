(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

The mainnet activation of the Heartwood network upgrade is supported by this
release, with an activation height of 903000, which should occur in the middle
of July — following the targeted EOS halt of our 2.1.2-3 release. Please upgrade
to this release, or any subsequent release, in order to follow the Heartwood
network upgrade.

The following two ZIPs are being deployed as part of this upgrade:

- [ZIP 213: Shielded Coinbase](https://zips.z.cash/zip-0213)
- [ZIP 221: FlyClient - Consensus-Layer Changes](https://zips.z.cash/zip-0221)

In order to help the ecosystem prepare for the mainnet activiation, Heartwood
has already been activated on the Zcash testnet. Any node version 2.1.2 or
higher, including this release, supports the Heartwood activation on testnet.

## Mining to Sapling addresses

After the mainnet activation of Heartwood, miners can mine directly into a
Sapling shielded address. Miners should wait until after Heartwood activation
before they make changes to their configuration to leverage this new feature.
After activation of Heartwood, miners can add `mineraddress=SAPLING_ADDRESS` to
their `zcash.conf` file, where `SAPLING_ADDRESS` represents a Sapling address
that can be generated locally with the `z_getnewaddress` RPC command. Restart
your node, and block templates produced by the `getblocktemplate` RPC command
will now have coinbase transactions that mine directly into this shielded
address.