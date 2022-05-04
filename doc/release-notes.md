(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Feature Deprecation and removal
-------------------------------

`zcashd` now has a [process](https://zcash.github.io/zcash/user/deprecation.html)
for how features of the public API may be deprecated and removed. Feature
deprecation follows a series of steps whereby, over a series of releases,
features first remain enabled by default (but may be explicitly disabled), then
switch to being disabled by default, and eventually are removed entirely.

A new string-valued option, `-allowdeprecated` has been introduced to allow a
user to explicitly manage the availability of deprecated `zcashd` features.  This
flag makes it possible for users to reenable deprecated methods and features
api that are currently disabled by default, or alternately to explicitly
disable all deprecated features if they so choose. Multiple instances of this
argument may be provided. A user may disable deprecated features entirely
by providing the string `none` as the argument to this parameter. In the case
that `none` is specified, multiple invocations of `-allowdeprecated` are not
permitted.

Deprecated
----------

As of this release, the following features are deprecated, but remain 
available by default. These features may be disabled by setting 
`-allowdeprecated=none`. After release 5.3.0, these features will be
disabled by default and the following flags to `-allowdeprecated` will
be required to permit their continued use:

  - `legacy_privacy` - the default "legacy" privacy policy for z_sendmany
    is deprecated. When disabled, the default behavior of z_sendmany will
    conform to the `FullPrivacy` directive (introduced in 4.7.0) in all cases
    instead of just for transactions involving unified addresses.
  - `getnewaddress` - controls availability of the `getnewaddress` RPC method.
  - `z_getnewaddress` - controls availability of the `z_getnewaddress` RPC method.
  - `addrtype` - controls availability of the deprecated `type` attribute
    returned by RPC methods that return address metadata. 

As of this release, the following previously deprecated features are disabled
by default, but may be be reenabled using `-allowdeprecated=<feature>`.

  - The `zcrawreceive` RPC method is disabled. It may be reenabled with
    `allowdeprecated=zcrawreceive`
  - The `zcrawjoinsplit` RPC method is disabled. It may be reenabled with
    `allowdeprecated=zcrawjoinsplit`
  - The `zcrawkeygen` RPC method is disabled. It may be reenabled with
    `allowdeprecated=zcrawkeygen`

Option handling
---------------

- The `-reindex` and `-reindex-chainstate` options now imply `-rescan`
  (provided that the wallet is enabled and pruning is disabled, and unless
  `-rescan=0` is specified explicitly).

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

Tests
-----

- The environment variable that allows users of the rpc (Python) tests to
  override the default path to the `zcashd` executable has been changed
  from `BITCOIND` to `ZCASHD`.
