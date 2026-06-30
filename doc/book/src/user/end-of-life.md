# End of Life

> ## ⚠️ `zcashd` is reaching its End of Life
>
> `zcashd` is **deprecated as of July 15th 2026** and will **not** support NU6.3.
> **If you do not need the `zcashd` wallet, migrate to [Zebra](https://github.com/ZcashFoundation/zebra) now.**
> If you depend on the `zcashd` wallet, start testing [Zallet](https://zcash.github.io/zallet/)
> migrating your `wallet.dat` as soon as possible. The binary's **End-of-Support halt**
> follows at block height 3417100 (estimated July 21st 2026).

## Two key dates: Deprecation vs. End-of-Support halt

`zcashd`'s End of Life involves two distinct milestones. Don't conflate them:

- **Deprecation — July 15th 2026.** The date on which `zcashd` maintainers end support
  for `zcashd` 6.20.0. This is a maintainer/support decision and is **not** enforced by
  the binary; nodes keep running past this date. Operators should have migrated to Zebra
  (and Zallet, if they need the wallet) by July 15th so that the network is Zebra-only
  before NU6.3 activates.
- **End-of-Support halt — estimated July 21st 2026, at block height 3417100.** The point
  at which every `zcashd` 6.20.0 binary automatically shuts down and refuses to restart.
  This is hard-coded as the deprecation height (`DEPRECATION_HEIGHT` in
  `src/deprecation.h`) and coincides with NU6.3 mainnet activation. The date is an estimate
  that may shift with network solution power; query the exact height your node will halt at
  with the `getdeprecationinfo` JSON-RPC method. See
  [Release Support](release-support.md) for more on the End-of-Support halt.

## Context

Zcash organizations continue to work on the different components that are needed to
specify, develop and deploy the Ironwood shielded pool in the next Network Upgrade,
NU6.3. **`zcashd` maintainers will NOT** support this upgrade. `zcashd` will be
deprecated and reach end of life upon activation of NU6.3 on mainnet.

The decision to accelerate `zcashd` deprecation is specifically related to the recent
Orchard vulnerability disclosed on June 1st 2026 and the associated risks that the C++
codebase poses for future (or present) AI advancements. Deprecating `zcashd` is a
**mandatory** step to conclude the remediation of the reported Orchard bug.

## Timeline

The current timeline estimates that NU6.3 activation will be integrated into testnet
code around July 1st 2026, and testnet activation will follow on July 3rd. The same will
follow for mainnet on later dates (see timeline below).

With regards to mainnet activation, `zcashd` 6.20.0 **deprecation** is scheduled for
July 15th. This is needed to achieve a Zebra-only network prior to NU6.3 activation on
July 21st, which is also when the automatic **End-of-Support halt** (block height 3417100)
takes effect.

| Date | Event |
| ----- | ----- |
| July 1st 2026 | Ironwood/NU6.3 Testnet deployment |
| July 3rd 2026 | Ironwood/NU6.3 Testnet Activation |
| July 9th 2026 | Ironwood/NU6.3 **Mainnet** deployment |
| July 15th 2026 | `zcashd` **Deprecation** (maintainer support ends) |
| ~July 21st 2026 | Ironwood/NU6.3 **Mainnet** Activation & `zcashd` **End-of-Support halt** (block height 3417100) |

## The Path Forward: Zebra + Zallet

The Zcash Foundation has developed and maintains the
[Zebra full node](https://github.com/ZcashFoundation/zebra), a Rust implementation of the
Zcash protocol. All `zcashd` users that don't need the `zcashd` wallet should migrate to
Zebra **immediately**.

For those who do depend on the `zcashd` wallet, ZODL developers have released Zallet
Alpha.4, which allows migration of `zcashd`'s `wallet.dat` files into Zallet. You can find
the documentation for Zallet [here](https://zcash.github.io/zallet/). We encourage these
users to start testing Zallet and to send feedback to its maintainers as soon as possible.
