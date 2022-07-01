(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Option handling
---------------

- A new `-preferredtxversion` argument allows the node to preferentially create
  transactions of a specified version, if a transaction does not contain
  components that necessitate creation with a specific version. For example,
  setting `-preferredtxversion=4` will cause the node to create V4 transactions
  whenever the transaction does not contain Orchard components. This can be
  helpful if recipients of transactions are likely to be using legacy wallets
  that have not yet been upgraded to support parsing V5 transactions.

Deprecated
----------

As of this release, the following previously deprecated features are disabled
by default, but may be be reenabled using `-allowdeprecated=<feature>`.

  - The `dumpwallet` RPC method is disabled. It may be reenabled with
    `allowdeprecated=dumpwallet`. `dumpwallet` should not be used; it is
    unsafe for backup purposes as it does not return any key information
    for keys used to derive shielded addresses. Use `z_exportwallet` instead.

As of this release, the following features are deprecated, but remain available
by default. These features may be disabled by setting `-allowdeprecated=none`.
After at least 3 minor-version releases, these features will be disabled by
default and the following flags to `-allowdeprecated` will be required to
permit their continued use:

  - `wallettxvjoinsplit` - controls availability of the deprecated `vjoinsplit`
    attribute returned by the `gettransaction` RPC method.
