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

Removed the P2P network alert system
------------------------------------

`zcashd` will no longer respond to alert messages from the P2P network.
The `-alerts` config option has been removed and will be ignored. This
ensures that, if any other party were to maintain a zcashd fork, ECC will
not hold any keys that could affect nodes running such a fork.

This leaves intact alerting for end-of-service and detected chain forks,
and the `-alertnotify` functionality.
