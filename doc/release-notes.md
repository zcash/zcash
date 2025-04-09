(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

zcashd is being deprecated in 2025. Full nodes are being migrated to zebrad,
and the Zallet wallet is being built as a replacement for the zcashd wallet.

For some of zcashd's JSON-RPC methods, zebrad or Zallet should be a drop-in
replacement. Other JSON-RPC methods may require modified usage, and some
JSON-RPC methods will not be supported.

You can find all information about the zcashd deprecation process on this
webpage, which you can monitor for future updates:
<https://z.cash/support/zcashd-deprecation/>

We are collecting information about how zcashd users are currently using the
existing JSON-RPC methods. The above webpage has a link to a spreadsheet
containing the information we have collected so far, and the planned status
for each JSON-RPC method based on that information. If you have not provided
feedback to us about how you are using the zcashd JSON-RPC interface, please
do so as soon as possible.

To confirm that you are aware that zcashd is being deprecated and that you
will need to migrate to zebrad and/or Zallet in 2025, add the following
option to your config file:

i-am-aware-zcashd-will-be-replaced-by-zebrad-and-zallet-in-2025=1

RPC Changes
-----------

* The RPC methods `getnetworkhashps`, `keypoolrefill`, `settxfee`,
  `createrawtransaction`, `fundrawtransaction`, and `signrawtransaction`
  have been deprecated, but are still enabled by default.
* The previously deprecated RPC methods `z_getbalance` and `getnetworkhashps`,
  and the features `gbt_oldhashes` and `deprecationinfo_deprecationheight`
  have been disabled by default. The `addrtype` feature is now disabled by
  default even when zcashd is compiled without the `ENABLE_WALLET` flag.
