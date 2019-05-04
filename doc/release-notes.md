This release of Zcashd v2.0.5 adds the Sprout to Sapling migration tool, introduces a new consensus rule on mainnet to reject blocks that violate turnstiles, and adds 64-bit ARMv8 support.  Zcashd v2.0.5 will be supported until block 592512, expected to arrive around 26th August 2019, at which point the software will end-of-support halt and shut down.

# Notable Changes in this Release

## Sprout to Sapling Migration Tool

This release includes the addition of a tool that will enable users to migrate shielded funds from the Sprout pool to the Sapling pool while minimizing information leakage. 

The migration can be enabled using the RPC `z_setmigration` or by including `-migration` in the `zcash.conf` file. Unless otherwise specified funds will be migrated to the wallet's default Sapling address; it is also possible to set the receiving Sapling address using the `-migrationdestaddress` option in `zcash.conf`.

See [ZIP308](https://github.com/zcash/zips/blob/master/zip-0308.rst) for full details. 

## New consensus rule: Reject blocks that violate turnstile

In the 2.0.4 release the consensus rules were changed on testnet to enforce a consensus rule which marks blocks as invalid if they would lead to a turnstile violation in the Sprout or Shielded value pools.
**This release enforces the consensus rule change on mainnet**

The motivations and deployment details can be found in the accompanying [ZIP draft](https://github.com/zcash/zips/pull/210) and [PR 3968](https://github.com/zcash/zcash/pull/3968).

Developers can use a new experimental feature `-developersetpoolsizezero` to test Sprout and Sapling turnstile violations. See [PR 3964](https://github.com/zcash/zcash/pull/3964) for more details.

## 64-bit ARMv8 support

Added ARMv8 (AArch64) support. This enables users to build zcash on even more devices.

For information on how to build see the [User Guide](https://zcash.readthedocs.io/en/latest/rtd_pages/user_guide.html#build)

Users on the Zcash forum have reported successes with both the Pine64 Rock64Pro and Odroid C2 which contain 4GB and 2GB of RAM respectively.

Just released, the Odroid N2 looks like a great solution with 4GB of RAM. The newly released Jetson Nano Developer Kit from Nvidia (also 4GB of RAM) is also worth a look. The NanoPC-T3 Plus is another option but for the simplest/best experience choose a board with 4GB of RAM. Just make sure before purchase thatthe CPU supports the 64-bit ARMv8 architecture.

# Summary of the Changes Included in this Release

1. Sprout to Sapling migration (#3848, #3888, #3967, #3973, #3977) 
1. Activate turnstile on mainnet (#3968)
1. Update boost to v1.70.0 to eliminate build warning (#3951)
1. Fix new HD seed generation for previously-encrypted wallets (#3940)
1. Fix enable-debug build DB_COINS undefined (#3907)
1. Update "Zcash Company" to "Electric Coin Company" (#3901)
1. Support additional cross-compilation targets in Rust (#3505)

For a more complete list of changes, please see the [2.0.5](https://github.com/zcash/zcash/pulls?q=is%3Apr+milestone%3Av2.0.5+is%3Aclosed) milestone.
