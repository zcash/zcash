(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Network Upgrade 4: Canopy
--------------------------

The code preparations for the Canopy network upgrade are finished and included in this release. The following ZIPs are being deployed:

- [ZIP 207: Funding Streams](https://zips.z.cash/zip-0207)
- [ZIP 211: Disabling Addition of New Value to the Sprout Value Pool](https://zips.z.cash/zip-0211)
- [ZIP 212: Allow Recipient to Derive Sapling Ephemeral Secret from Note Plaintext](https://zips.z.cash/zip-0212)
- [ZIP 214: Consensus rules for a Zcash Development Fund](https://zips.z.cash/zip-0214)
- [ZIP 215: Explicitly Defining and Modifying Ed25519 Validation Rules](https://zips.z.cash/zip-0215)

Canopy will activate on testnet at height TODO, and can also be activated at a specific height in regtest mode by setting the config option `-nuparams=0xe9ff75a6:HEIGHT`.

Canopy will activate on mainnet at height 1046400.

See [ZIP 251](https://zips.z.cash/zip-0251) for additional information about the deployment process for Canopy.

Flush witness data to disk only when it's consistent
-----------------------------------------------------
This fix prevents the wallet database from getting into an inconsistent state. By flushing witness data to disk from the wallet thread instead of the main thread, we ensure that the on-disk block height is always the same as the witness data height. Previously, the database occasionally got into a state where the latest block height was one ahead of the witness data. This then triggered an assertion failure in `CWallet::IncrementNoteWitnesses()` upon restarting after a zcashd shutdown.

Note that this code change will not automatically repair a data directory that has been affected by this problem; that requires starting zcashd with the `-rescan` or `-reindex` options.

New DNS seeders
----------------
DNS seeders hosted at "zfnd.org" and "yolo.money" have been added to the list in `chainparams.cpp`. They're running [CoreDNS](https://coredns.io) with a [Zcash crawler plugin](https://github.com/ZcashFoundation/dnsseeder), the result of a Zcash Foundation in-house development effort to replace `zcash-seeder` with something memory-safe and easier to maintain.

These are validly operated seeders per the [existing policy](https://zcash.readthedocs.io/en/latest/rtd_pages/dnsseed_policy.html). For general questions related to either seeder, contact george@zfnd.org or mention @gtank in the Zcash Foundation's Discord. For bug reports, open an issue on the [dnsseeder](https://github.com/ZcashFoundation/dnsseeder) repo.

Changed command-line options
-----------------------------
- `-debuglogfile=<file>` can be used to specify an alternative debug logging file.

RPC methods
------------
- `joinSplitPubKey` and `joinSplitSig` have been added to verbose transaction outputs. This enables the transaction's binary form to be fully reconstructed from the RPC output.
- The output of `getblockchaininfo` now includes an `estimatedheight` parameter. This can be shown in UIs as an indication of the current chain height while `zcashd` is syncing, but should not be relied upon when creating transactions.

Metrics screen
-----------------------
- A progress bar is now visible when in Initial Block Download mode, showing both the prefetched headers and validated blocks. It is only printed for TTY output. Additionally, the "not mining" message is no longer shown on mainnet, as the built-in CPU miner is not effective at the current network difficulty.
- The number of block headers prefetched during Initial Block Download is now displayed alongside the number of validated blocks. With current compile-time defaults, a Zcash node prefetches up to 160 block headers per request without a limit on how far it can prefetch, but only up to 16 full blocks at a time.
