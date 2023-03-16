(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

RPC Changes
-----------

- `getdeprecationinfo` has several changes:
  - It now returns additional information about currently deprecated and
    disabled features.
  - A new `end_of_service` object that contains both the block height for
    end-of-service and the estimated time that the end-of-service halt is
    expected to occur. Note that this height is just an approximation and
    will change over time as the end-of-service block height approaches,
    due to the variability in block times. The
    `end_of_service` object is intended to replace the `deprecationheight`
    field; see the [Deprecations](#deprecations) section for additional detail.
  - This RPC method was previously only available for mainnet nodes; it is now
    also available for testnet and regtest nodes, in which case it does not
    return end-of-service halt information (as testnet and regtest nodes do not
    have an end-of-service halt feature.)
- Several `z_sendmany` failures have moved from synchronous to asynchronous, so
  while you should already be checking the async operation status, there are now
  more cases that may trigger failure at that stage.

[Deprecations](https://zcash.github.io/zcash/user/deprecation.html)
--------------

The following features have been deprecated, but remain available by default.
These features may be disabled by setting `-allowdeprecated=none`. 18 weeks
after this release, these features will be disabled by default and the following
flags to `-allowdeprecated` will be required to permit their continued use:

- `deprecationinfo_deprecationheight`: The `deprecationheight` field of
  `getdeprecationinfo` has been deprecated and replaced by the `end_of_service`
  object.
