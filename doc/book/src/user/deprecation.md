Deprecated Features
===================

In order to support the continuous improvement of zcashd, features are
periodically deprecated and removed when they have been superseded or are no
longer useful.  Deprecation follows a 3-stage process:

1. Initially, a feature will be marked as DEPRECATED in the release notes and
   user-facing documentation, but no other changes are made; the feature
   continues to be available and function as normal. While features at this
   stage remain enabled by default, they may be explicitly disabled by
   specifying `-allowdeprecated=none` on the command line when starting the
   node, or by including `allowdeprecated=none` as a line in the `zcash.conf`
   file. 
2. In the stage of deprecation, the feature will be disabled by default.
   Disabled features may be reenabled via use of the `-allowdeprecated` flag.
3. In the third stage, the feature is fully removed and is no longer available.

Features that enter Stage 1 in a particular release will be disabled by default
after no less than than a single major-version release, or 3 minor-version
releases, and features are only fully removed after two major-version releases.
For example, a feature entering Stage 1 in release 5.0.0:

  - Will be disabled by default in either release 5.3.0 or release 6.0.0,
    whichever comes first.
  - Will be fully removed in release 7.0.0 

Currently Deprecated
====================

Stage 1
-------

The following features are deprecated, but remain enabled by default. These features
will be disabled if `-allowdeprecated=none` is added to the CLI arguments when starting
the node, or if an `allowdeprecated=none` line is added to `zcash.conf`.

### Deprecated in 5.0.0

The following features are deprecated as of release 5.0.0 and will be disabled by
default as of either release 5.3.0 or release 6.0.0, whichever comes first.

  - `legacy_privacy` - The default "legacy" privacy policy for z_sendmany is
    deprecated. Use `-allowdeprecated=none` to require the default behavior to
    conform to the `FullPrivacy` directive in all cases instead of just for
    transactions involving unified addresses. 
  - `getnewaddress` - The `getnewaddress` RPC method is deprecated.
  - `z_getnewaddress` - The `z_getnewaddress` RPC method is deprecated.
  - `addrtype` - The `type` attribute is deprecated in the results of RPC
    methods that return address metadata. It is recommended that applications
    using this metadata be updated to use the `pool` or `address_type`
    attributes, which have replaced the `type` attribute, as appropriate.

Stage 2
-------

Each feature in the lists below may be enabled by adding `-allowdeprecated=<feature>`
to the CLI arguments when starting the node, or by adding an `allowdeprecated=<feature>`
line to `zcash.conf`.

### Disabled in 5.0.0

The following features are disabled by default, and will be removed in release 6.0.0.

  - `zcrawreceive` - The `zcrawreceive` RPC method is disabled.
  - `zcrawjoinsplit` - The `zcrawjoinsplit` RPC method is disabled.
  - `zcrawkeygen` - The `zcrawkeygen` RPC method is disabled.
