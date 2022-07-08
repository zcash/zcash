(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Faster block validation for Sapling and Orchard transactions
------------------------------------------------------------

Block validation in `zcashd` is a mostly single-threaded process, due to how the
chain update logic inherited from Bitcoin Core is written. However, certain more
computationally intensive checks are performed more efficiently than checking
everything individually:

- ECDSA signatures on transparent inputs are checked via multithreading.
- RedPallas signatures on Orchard actions are checked via batch validation.

As of this release, `zcashd` applies these techniques to more Sapling and
Orchard components:

- RedJubjub signatures on Sapling Spends are checked via batch validation.
- Groth16 proofs for Sapling Spends and Outputs are checked via batch validation
  and multithreading.
- Halo 2 proofs for Orchard Actions are checked via batch validation and
  multithreading.

This reduces worst-case block validation times for observed historic blocks by
around 80% on a Ryzen 9 5950X CPU.

The number of threads used for checking Groth16 and Halo 2 proofs (as well as
for creating them when spending funds) can be set via the `RAYON_NUM_THREADS`
environment variable.

Option handling
---------------

- A new `-preferredtxversion` argument allows the node to preferentially create
  transactions of a specified version, if a transaction does not contain
  components that necessitate creation with a specific version. For example,
  setting `-preferredtxversion=4` will cause the node to create V4 transactions
  whenever the transaction does not contain Orchard components. This can be
  helpful if recipients of transactions are likely to be using legacy wallets
  that have not yet been upgraded to support parsing V5 transactions.

RPC interface
-------------

- The `getblocktemplate` RPC method now skips proof and signature checks on
  templates it creates, as these templates only include transactions that have
  previously been checked when being added to the mempool.

- The `getrawtransaction` RPC method now includes details about Orchard actions
  within transactions.

Wallet
------

- Rescan performance of post-NU5 blocks has been slightly improved (overall
  rescan time for a single-account wallet decreases by around 6% on a Ryzen 9
  5950X). Further improvements will be implemented in future releases to help
  mitigate the effect of blocks full of shielded outputs.

- The `CWallet::UpdatedTransaction` signal is no longer called while holding the
  `cs_main` lock. This fixes an issue where RPCs could block for long periods of
  time on `zcashd` nodes with large wallets. Downstream code forks that have
  reconnected the `NotifyTransactionChanged` wallet signal should take note of
  this change, and not rely there on access to globals protected by `cs_main`.

- Some `zcashd 5.0.0` nodes would shut down some time after start with the error
  `ThreadNotifyWallets: Failed to read block X while notifying wallets of block disconnects`.
  `zcashd` now attempts to rectify the situation, and otherwise will inform the
  user before shutting down that a reindex is required.

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
