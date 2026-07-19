# End of Life

> ## ⚠️ `zcashd` has reached its End of Life
>
> `zcashd` has reached its **End of Life** and will **not** be upgraded to
> support the upcoming NU6.3 or subsequent network upgrades. Its automatic
> **End-of-Support halt** was reached on **July 18th 2026** at block height
> 3417100, and every `zcashd` 6.20.0 node has now automatically shut down and
> will refuse to restart. **If you have not migrated yet, do so now:** move to
> [Zebra](https://github.com/ZcashFoundation/zebra), the new replacement
> consensus node implementation, and the
> [Zallet](https://zcash.github.io/zallet/) full node wallet, which provides
> support for the zcashd wallet's `wallet.dat` file.

## Two key dates: End-of-Support halt and NU6.3 activation

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
NU6.3. Support for NU6.3 is provided by the [Zebra](https://github.com/ZcashFoundation/zebra)
consensus node, developed by the [Zcash Foundation](https://zfnd.org).

`zcashd` deprecation hs been planned for a long time, but the timeline has been
accelerated by the advancement of AI capabilities for bug discovery. The zcashd
codebase is large and complex, and the lack of memory safety in C++ provides
fertile ground for dangerous bugs. Since being forked from Bitcoin, the zcashd
codebase has diverged substantially, and many years worth of patches applied
to bitcoind were never adopted by zcashd; this has included patches for
previously-undisclosed Bitcoin bugs with remote code execution potential.
`zebrad` by comparison was written in the memory-safe Rust language and
its architecture benefits from many lessons learned from `zcashd` maintenance.

`zebrad` intentionally does not provide an embedded full-node wallet. One of
the lessons learned from `zcashd` is that the direct coupling of wallet state
to consensus node operation introduces a significant amount of complexity,
and also poses risks to user funds as any exploit of the consensus node could
potentially result in compromise of the wallet as well. As a consequence,
the [Zallet](https://zcash.github.io/zallet/) full node wallet has been
developed to support a migration path for `zcashd` wallet users, including the
ability to migrate keys and data from the `zcashd` `wallet.dat` wallet data
file.

## Timeline

NU6.3 integrated into `zebrad` testnet code around July 2nd 2026, and testnet
activation followed on July 4th (block height 4134000). Mainnet activation of
NU6.3 will occur on July 28.

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
