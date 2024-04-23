# Platform Support

Support for different platforms (build "targets" and operating systems) are organised into
three tiers, each with a different set of guarantees. For more information on the policies
for targets at each tier, see the [Platform Tier Policy](../dev/platform-tier-policy.md).

## Tier 1

Tier 1 platforms can be thought of as "guaranteed to work". ECC builds official binary
releases for each tier 1 platform, and automated testing ensures that each tier 1 platform
builds and passes tests after each change.

"End of Support" dates are the latest currently-known date after which the platform will
be removed from tier 1. These dates are subject to change.

| target                  | OS           | End of Support |
| ----------------------- | ------------ | -------------- |
| `x86_64-pc-linux-gnu`   | Debian 10    | June 2024      |
|                         | Debian 11    | June 2026      |
|                         | Debian 12    | June 2028      |
|                         | Ubuntu 20.04 | April 2025     |

## Tier 2

Tier 2 platforms can be thought of as "guaranteed to build". ECC builds official binary
releases for each tier 2 platform, and automated builds ensure that each tier 2 platform
builds after each change. Automated tests are not always run so it's not guaranteed to
produce a working build, but tier 2 platforms often work to quite a good degree, and
patches are always welcome!

"End of Support" dates are the latest currently-known date after which the platform will
be removed from tier 2. These dates are subject to change.

| target                  | OS           | End of Support |
| ----------------------- | ------------ | -------------- |
| N/A |

## Tier 3

Tier 3 platforms are those for which the `zcashd` codebase has support, but ECC does not
require builds or tests to pass, so these may or may not work. Official builds are not
available.

| target                  | OS           | notes |
| ----------------------- | ------------ | ----- |
| `x86_64-pc-linux-gnu`   | Arch         |
|                         | Ubuntu 22.04 |
| `x86_64-unknown-freebsd`| FreeBSD      |
| `x86_64-w64-mingw32`    | Windows      | 64-bit MinGW |
| `x86_64-apple-darwin16` | macOS 10.14+ |
| `aarch64-linux-gnu`     | ARM64 Linux  |
