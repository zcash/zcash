(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Option handling
---------------

- The `-reindex` and `-reindex-chainstate` options now imply `-rescan`
  (provided that the wallet is enabled and pruning is disabled, and unless
  `-rescan=0` is specified explicitly).
- A new string-valued option, `-allowdeprecated` has been introduced to allow
  a user to explicitly manage the availability of deprecated zcashd features.
  This flag makes it possible for users to reenable deprecated methods and
  features api that are currently disabled by default, or alternately to
  explicitly disable all deprecated features if they so choose. Multiple
  instances of this argument may be provided. In the case that no values are
  provided for this parameter, all currently deprecated RPC methods that are
  not slated for removal in the next release remain enabled. A user may
  disable all deprecated features entirely by providing the string "none" as
  the argument to this parameter. In the case that "none" is specified,
  multiple invocations of `-allowdeprecated` are not permitted.

  Specific user-oriented documentation of the deprecation process may be 
  found in `doc/book/src/user/deprecation.md`.

  The following flags are recognized:

  - `none` - disables all deprecated features.
  - `legacy_privacy` - enables the use of the deprecated "legacy" privacy
    policy for z_sendmany. This causes the default behavior to conform to the
    `FullPrivacy` directive in all cases instead of just for transactions
    involving unified addresses.
  - `getnewaddress` - enables the `getnewaddress` RPC method.
  - `z_getnewaddress` - enables the `z_getnewaddress` RPC method.
  - `zcrawreceive` - enables the `zcrawreceive` RPC method.
  - `zcrawjoinsplit` - enables the `zcrawjoinsplit` RPC method.
  - `zcrawkeygen` - enables the `zcrawkeygen` RPC method.
  - `addrtype` - when this option is set, the deprecated `type` attribute
    is returned in addition to `pool` or `address_type` (which contain the same
    information) in the results of RPC methods that return address metadata.

RPC Changes
-----------

- The deprecated `zcrawkeygen`, `zcrawreceive`, and `zcrawjoinsplit` RPC
  methods are now disabled by default. Use `-allowdeprecated=<feature>`
  to select individual features if you wish to continue using these APIs.

Build system
------------

- `zcutil/build.sh` now automatically runs `zcutil/clean.sh` to remove
  files created by previous builds. We previously recommended to do this
  manually.

Dependencies
------------

- The `boost` and `native_b2` dependencies have been updated to version 1.79.0
