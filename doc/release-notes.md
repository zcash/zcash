(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Fixed
-----

This release fixes an error "Assertion `uResultHeight == rewindHeight` failed" (#5958)
that could sometimes happen when restarting a node.

Memory Usage Improvement
------------------------

The memory usage of zcashd has been reduced by not keeping Equihash solutions for all
block headers in memory.

RPC changes
-----------

The following RPC methods that query wallet state now support an optional `asOfHeight`
parameter, to execute the query as if it were run when the blockchain was at the height
specified by this argument:

* `getbalance`
* `getreceivedbyaddress`
* `gettransaction` (*)
* `getwalletinfo`
* `listaddressgroupings`
* `listreceivedbyaddress` (*)
* `listsinceblock` (*)
* `listtransactions`
* `listunspent` (*)
* `z_getbalanceforaccount`
* `z_getbalanceforviewingkey`
* `z_getmigrationstatus`
* `z_getnotescount`
* `z_listreceivedbyaddress`
* `z_listunspent`

(*) For these methods, additional parameters have been added to maintain
compatibility of parameter lists with Bitcoin Core. Default values should be
passed for these additional parameters in order to use `asOfHeight`. See the
[RPC documentation](https://zcash.github.io/) for details.
