(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Incoming viewing keys
---------------------

Support for incoming viewing keys, as described in
[the Zcash protocol spec](https://github.com/zcash/zips/blob/master/protocol/protocol.pdf),
has been added to the wallet.

Use the `z_exportviewingkey` RPC method to obtain the incoming viewing key for a
z-address in a node's wallet. For Sprout z-addresses, these always begin with
"ZiVK" (or "ZiVt" for testnet z-addresses). Use `z_importviewingkey` to import
these into another node.

A node that possesses an incoming viewing key for a z-address can view all past
transactions received by that address, as well as all future transactions sent
to it, by using `z_listreceivedbyaddress`. They cannot spend any funds from the
address. This is similar to the behaviour of "watch-only" t-addresses.

`z_gettotalbalance` now has an additional boolean parameter for including the
balance of "watch-only" addresses (both transparent and shielded), which is set
to `false` by default. `z_getbalance` has also been updated to work with
watch-only addresses.

- **Caution:** for z-addresses, these balances will **not** be accurate if any
  funds have been sent from the address. This is because incoming viewing keys
  cannot detect spends, and so the "balance" is just the sum of all received
  notes, including ones that have been spent. Some future use-cases for incoming
  viewing keys will include synchronization data to keep their balances accurate
  (e.g. [#2542](https://github.com/zcash/zcash/issues/2542)).
