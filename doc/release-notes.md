(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

RPC Changes
-----------

* The RPC methods `getnetworkhashps`, `keypoolrefill`, `settxfee`,
  `createrawtransaction`, `fundrawtransaction`, and `signrawtransaction`
  have been deprecated, but are still enabled by default.
* The previously deprecated RPC methods `z_getbalance` and `getnetworkhashps`,
  and the features `gbt_oldhashes` and `deprecationinfo_deprecationheight`
  have been disabled by default. The `addrtype` feature is now disabled by
  default even when zcashd is compiled without the `ENABLE_WALLET` flag.
