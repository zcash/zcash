Deprecated Features
===================

In order to support the continuous improvement of `zcashd`, features are
periodically deprecated and removed when they have been superseded or are no
longer useful.  Deprecation follows a 3-stage process:

1. Initially, a feature will be marked as DEPRECATED in the release notes and
   user-facing documentation, but no other changes are made; the feature
   continues to be available and function as normal. While features at this
   stage remain enabled by default, they may be explicitly disabled by
   specifying `-allowdeprecated=none` on the command line when starting the
   node, or by including `allowdeprecated=none` as a line in the `zcash.conf`
   file. 
2. In the next stage of deprecation, the feature will be disabled by default.
   Disabled features may be reenabled via use of the `-allowdeprecated` flag.
3. In the third stage, the feature is fully removed and is no longer available.

Features that enter Stage 1 in a particular release will be disabled by default
after no fewer than 3 releases that update `zcashd`'s minor-version, and
features will only be fully removed after a total of at least 6 minor-version updates.
`zcashd`'s release schedule intends to produce a release that updates the minor
version every 6 weeks, so deprecated features remain accessible by default for
approximately 18 weeks, and then can be expected to be removed no less than 36
weeks after their initial deprecation. Deprecation and removal timelines might
be extended beyond this on a case-by-case basis to satisfy user requirements. 

Currently Deprecated
====================

Stage 1
-------

The following features are deprecated, but remain enabled by default. These features
will be disabled if `-allowdeprecated=none` is added to the CLI arguments when starting
the node, or if an `allowdeprecated=none` line is added to `zcash.conf`.

| `feature`             | Deprecated | Feature details
|-----------------------|------------|----------------
| `z_getbalance`        | 5.0.0      | The `z_getbalance` RPC method.
| `z_gettotalbalance`   | 5.0.0      | The `z_gettotalbalance` RPC method.
| `gbt_oldhashes`       | 5.4.0      | The `finalsaplingroothash`, `lightclientroothash`, and `blockcommitmentshash` fields in the output of `getblocktemplate`, which are replaced by the `defaultroots` field.

Stage 2
-------

Each feature in the table below may be enabled by adding `-allowdeprecated=<feature>`
to the CLI arguments when starting the node, or by adding an `allowdeprecated=<feature>`
line to `zcash.conf`.

| `feature`             | Deprecated | Feature details
|-----------------------|------------|----------------
| `legacy_privacy`      | 5.0.0      | The default "legacy" privacy policy for `z_sendmany` has been replaced by the `FullPrivacy` directive.
| `getnewaddress`       | 5.0.0      | The `getnewaddress` RPC method.
| `getrawchangeaddress` | 5.0.0      | The `getrawchangeaddress` RPC method.
| `z_getnewaddress`     | 5.0.0      | The `z_getnewaddress` RPC method.
| `z_listaddresses`     | 5.0.0      | The `z_listaddresses` RPC method.
| `addrtype`            | 5.0.0      | The `type` attribute is deprecated in the results of RPC methods that return address metadata. It is recommended that applications using this metadata be updated to use the `pool` or `address_type` attributes, which have replaced the `type` attribute, as appropriate.
| `wallettxvjoinsplit`  | 5.1.0      | The `vjoinsplit` attribute returned by the `gettransaction` RPC method.
