# `zcashd` Release Support

## Release cadence and support window

`zcashd` releases happen approximately every six weeks, although this may change if a
particular release is delayed, or if a hotfix release occurs.

Each `zcashd` release is generally supported for 16 weeks. Occasionally this changes for
individual releases (for example, near to a Network Upgrade activation).

These two policies mean that there are generally at least two separate `zcashd` versions
currently supported at any given time.

## End-of-Support halt

Every `zcashd` version released by ECC has an End-of-Support height. When the Zcash chain
reaches this height, `zcashd` will automatically shut down, and the binary will refuse to
restart. This is for several reasons:

- The `zcashd` maintainers do not have the resources to maintain old versions of `zcashd`
  indefinitely.
- Each time a user upgrades their `zcashd` node, they are re-confirming that they are
  happy to run the Zcash consensus rules encoded in the version of `zcashd` they are
  running. This is an important part of the overall strategy for changes to the node and
  consensus rules; users who want to follow different rules (or even just have a different
  End-of-Support halt policy) will obtain a `zcashd` binary from some other source, with
  its own support policies.
- Knowing that old versions will shut down is useful for planning backwards-incompatible
  changes in Network Upgrades. A Network Upgrade activation can be targeted for a height
  where we know that all released `zcashd` versions which _did not_ support the Network
  Upgrade will have shut down by the time the Network Upgrade activates.

## End-of-Support heights

The End-of-Support height for a running `zcashd` can be queried over JSON-RPC using the
`getdeprecationinfo` method.

The following table shows End-of-Support information for recent `zcashd` releases. It is
automatically updated during each release. "End of Support" dates are estimated at that
time, and may shift due to changes in network solution power.

<!-- RELEASE_SCRIPT_START_MARKER - If you make changes here, check make-release.py -->
| `zcashd` version | Release date | Halt height | End of Support |
| ---------------- | ------------ | ----------- | -------------- |
| 6.1.0 | 2025-01-02 | 2901800 | 2025-04-24 |
| 6.2.0-rc1 | 2025-04-10 | 3013824 | 2025-07-31 |
| 6.2.0 | 2025-04-16 | 3021000 | 2025-08-06 |
<!-- RELEASE_SCRIPT_END_MARKER -->
