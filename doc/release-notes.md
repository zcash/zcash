(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Option handling
---------------

- The `-reindex` and `-reindex-chainstate` options now imply `-rescan`
  (provided that the wallet is enabled and pruning is disabled, and unless
  `-rescan=0` is specified explicitly).
- A new string-valued option, `-allowdeprecated` has been introduced to make it possible
  for users to turn off deprecated methods and features in the RPC api. 
  Multiple instances of this argument may be provided. In the case that
  this parameter is not provided, all currently deprecated RPC methods
  that are not slated for removal in the next release remain available.
  A user may disable all deprecated features entirely by providing the
  string "none" as the argument to this parameter, or enable all
  deprecated features, including those slated for removal, by providing
  the string "all" as the argument to this parameter. In the case that
  "all" or "none" is specified, multiple invocations of `-allowdeprecated`
  are not permitted.

  To explicitly enable only a specific set of deprecated features, use
  `-allowdeprecated=<flag1> -allowdeprecated=<flagN> ...` when starting
  zcashd. The following flags are recognized:

  - "all" - enables all deprecated features.
  - "none" - disables all deprecated features.
  - "legacy_privacy" - enables the use of the deprecated "legacy" privacy
    policy for z_sendmany. This causes the default behavior to conform to
    the `FullPrivacy` directive in all cases instead of just for
    transactions involving unified addresses.
  - "getnewaddress" - enables the `getnewaddress` RPC method.
  - "z_getnewaddress" - enables the `z_getnewaddress` RPC method.
  - "zcrawreceive" - enables the `zcrawreceive` RPC method.
  - "zcrawjoinsplit" - enables the `zcrawjoinsplit` RPC method.
  - "zcrawkeygen" - enables the `zcrawkeygen` RPC method.
  - "addrtype" - when this option is set, the deprecated `type` attribute
    is returned in addition to `pool` or `address_type` (which contain the
    same information) in the results of RPC methods that return address metadata.

Build system
------------

- `zcutil/build.sh` now automatically runs `zcutil/clean.sh` to remove
  files created by previous builds. We previously recommended to do this
  manually.

Dependencies
------------

- The `boost` and `native_b2` dependencies have been updated to version 1.79.0
