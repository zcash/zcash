# End of Life

> ## ⚠️ `zcashd` is reaching its End of Life
>
> `zcashd` is **deprecated** and will **not** support NU6.3.
> **If you do not need the `zcashd` wallet, migrate to [Zebra](https://github.com/ZcashFoundation/zebra) now.**
> If you depend on the `zcashd` wallet, start testing [Zallet](https://zcash.github.io/zallet/)
> migrating your `wallet.dat` as soon as possible. The binary's **End-of-Support halt** is
> estimated for **July 18th 2026** at block height 3417100, after which every `zcashd` node
> automatically shuts down.

## Two key dates: End-of-Support halt vs. NU6.3 activation

`zcashd`'s End of Life involves two distinct milestones. Don't conflate them:

- **`zcashd` End-of-Support halt — estimated July 18th 2026, at block height 3417100.**
  The point at which every `zcashd` 6.20.0 binary automatically shuts down and refuses to
  restart. This is hard-coded as the deprecation height (`DEPRECATION_HEIGHT` in
  `src/deprecation.h`). The date is an estimate that may shift with network solution power;
  query the exact height your node will halt at with the `getdeprecationinfo` JSON-RPC
  method. See [Release Support](release-support.md) for more on the End-of-Support halt.
- **NU6.3 mainnet activation — estimated July 21st 2026.** The Ironwood/NU6.3 network
  upgrade activates on mainnet a few days *after* the End-of-Support halt. The halt is
  deliberately targeted to precede activation so that every un-upgraded `zcashd` node has
  already shut down and the network is Zebra-only before NU6.3 takes effect. **`zcashd`
  will not support NU6.3.**

## Context

Zcash organizations continue to work on the different components that are needed to
specify, develop and deploy the Ironwood shielded pool in the next Network Upgrade,
NU6.3. **`zcashd` maintainers will NOT** support this upgrade. `zcashd` is deprecated and
will reach end of life ahead of the activation of NU6.3 on mainnet.

The decision to accelerate `zcashd` deprecation is specifically related to the recent
Orchard vulnerability disclosed on June 1st 2026 and the associated risks that the C++
codebase poses for future (or present) AI advancements. Deprecating `zcashd` is a
**mandatory** step to conclude the remediation of the reported Orchard bug.

## Timeline

The current timeline estimates that NU6.3 activation will be integrated into testnet
code around July 2nd 2026, and testnet activation will follow on July 3rd (block height 4134000).
The same will follow for mainnet on later dates (see timeline below).

With regards to mainnet, `zcashd` 6.20.0's automatic **End-of-Support halt** (block height
3417100) is estimated for July 18th. This precedes NU6.3 **mainnet activation** on July
21st, ensuring a Zebra-only network before the upgrade takes effect.

| Date | Event |
| ----- | ----- |
| July 2nd 2026 | Ironwood/NU6.3 Testnet deployment |
| July 3rd 2026 | Ironwood/NU6.3 Testnet Activation (block height 4134000) |
| July 9th 2026 | Ironwood/NU6.3 **Mainnet** deployment |
| ~July 18th 2026 | `zcashd` **End-of-Support halt** (block height 3417100) |
| ~July 21st 2026 | Ironwood/NU6.3 **Mainnet** Activation |

## The Path Forward: Zebra + Zallet

The Zcash Foundation has developed and maintains the
[Zebra full node](https://github.com/ZcashFoundation/zebra), a Rust implementation of the
Zcash protocol. All `zcashd` users that don't need the `zcashd` wallet should migrate to
Zebra **immediately**.

For those who do depend on the `zcashd` wallet, ZODL developers have released Zallet
Alpha.4, which allows migration of `zcashd`'s `wallet.dat` files into Zallet. You can find
the documentation for Zallet [here](https://zcash.github.io/zallet/). We encourage these
users to start testing Zallet and to send feedback to its maintainers as soon as possible.
