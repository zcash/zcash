# End of Life

> ## ⚠️ `zcashd` has reached its End of Life
>
> `zcashd` is **deprecated** and does **not** support NU6.3. Its automatic
> **End-of-Support halt** was reached on **July 18th 2026** at block height 3417100, and
> every `zcashd` 6.20.0 node has now automatically shut down and will refuse to restart.
> **If you have not migrated yet, do so now:** move to
> [Zebra](https://github.com/ZcashFoundation/zebra) if you do not need the `zcashd` wallet,
> or to [Zallet](https://zcash.github.io/zallet/) — migrating your `wallet.dat` — if you do.

## Two key dates: End-of-Support halt vs. NU6.3 activation

`zcashd`'s End of Life involves two distinct milestones. Don't conflate them:

- **`zcashd` End-of-Support halt — reached July 18th 2026, at block height 3417100.**
  The point at which every `zcashd` 6.20.0 binary automatically shuts down and refuses to
  restart. This is hard-coded as the deprecation height (`DEPRECATION_HEIGHT` in
  `src/deprecation.h`). The chain reached this height on 2026-07-18, so all `zcashd` 6.20.0
  nodes have now halted. See [Release Support](release-support.md) for more on the
  End-of-Support halt.
- **NU6.3 mainnet activation — estimated July 28th 2026.** The Ironwood/NU6.3 network
  upgrade activates on mainnet a few days *after* the End-of-Support halt. The halt was
  deliberately targeted to precede activation so that every un-upgraded `zcashd` node had
  already shut down and the network is Zebra-only before NU6.3 takes effect. **`zcashd`
  does not support NU6.3.**

## Context

Zcash organizations continue to work on the different components that are needed to
specify, develop and deploy the Ironwood shielded pool in the next Network Upgrade,
NU6.3. **`zcashd` maintainers do NOT** support this upgrade. `zcashd` is deprecated and
has reached end of life ahead of the activation of NU6.3 on mainnet.

The decision to accelerate `zcashd` deprecation is specifically related to the recent
Orchard vulnerability disclosed on June 1st 2026 and the associated risks that the C++
codebase poses for future (or present) AI advancements. Deprecating `zcashd` is a
**mandatory** step to conclude the remediation of the reported Orchard bug.

## Timeline

NU6.3 activation was integrated into testnet code around July 2nd 2026, and testnet
activation followed on July 4th (block height 4134000). The same followed for mainnet on
later dates (see timeline below).

With regards to mainnet, `zcashd` 6.20.0's automatic **End-of-Support halt** (block height
3417100) was reached on July 18th 2026. This precedes NU6.3 **mainnet activation** on July
28th, ensuring a Zebra-only network before the upgrade takes effect.

| Date | Event |
| ----- | ----- |
| July 2nd 2026 | Ironwood/NU6.3 Testnet deployment |
| July 4th 2026 | Ironwood/NU6.3 Testnet Activation (block height 4134000) |
| July 9th 2026 | Ironwood/NU6.3 **Mainnet** deployment |
| July 18th 2026 (reached) | `zcashd` **End-of-Support halt** (block height 3417100) |
| ~July 28th 2026 | Ironwood/NU6.3 **Mainnet** Activation |

## The Path Forward: Zebra + Zallet

The Zcash Foundation has developed and maintains the
[Zebra full node](https://github.com/ZcashFoundation/zebra), a Rust implementation of the
Zcash protocol. All `zcashd` users that don't need the `zcashd` wallet should migrate to
Zebra **immediately**.

For those who do depend on the `zcashd` wallet, ZODL developers have released Zallet
Alpha.4, which allows migration of `zcashd`'s `wallet.dat` files into Zallet. You can find
the documentation for Zallet [here](https://zcash.github.io/zallet/). Because `zcashd` has
now halted, these users should migrate their `wallet.dat` to Zallet as soon as possible and
send feedback to its maintainers.
