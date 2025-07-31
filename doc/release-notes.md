(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Fixed Orchard bug in transparent balance APIs
---------------------------------------------

Several RPC methods inherited from Bitcoin Core internally rely on
`CWalletTx::IsFromMe` for detecting involvement of the wallet in the input
side of a transaction. For example:
- The `getbalance` RPC method uses it as part of identifying "trusted"
  zero-confirmation transactions to include in the balance calculation.
- The `gettransaction` RPC method uses it to decide whether to include a
  `fee` field in its response.

When Orchard was integrated into `zcashd`, this method was not updated to
account for it, meaning that unshielding transactions spending Orchard notes
would not be correctly accounted for in transparent-specific RPC methods. A
similar bug involving Sprout and Sapling spends was fixed in v4.5.1.

Updates to default values
-------------------------

- The default for the `-preferredtxversion` config option has been changed from
  `4` (the v4 transaction format introduced in the 2018 Sapling Network Upgrade)
  to `5` (the v5 transaction format introduced in the 2022 NU5 Network Upgrade).
  Use `-preferredtxversion=4` to retain the previous behaviour.

Mining
------

- The address to which mining outputs are sent, set by `-mineraddress`, can
  now be a P2SH address.
