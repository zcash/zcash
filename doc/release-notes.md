(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Fixed regression in `getbalance` RPC method
-------------------------------------------

`zcashd v4.5.0` removed the account API from the wallet, following its
deprecation and removal in upstream Bitcoin Core. As part of the upstream
changes, the `getbalance` RPC method was altered from using two custom balance
computation methods, to instead relying on `CWallet::GetBalance`. This method
internally relies on `CWalletTx::IsFromMe` as part of identifying "trusted"
zero-confirmation transactions to include in the balance calculation.

There is an equivalent and closely-named `CWallet::IsFromMe` method, which is
used throughout the wallet, and had been updated before Zcash launched to be
aware of spent shielded notes. The change to `getbalance` exposed a bug:
`CWalletTx::IsFromMe` had not been similarly updated, which caused `getbalance`
to ignore wallet-internal (sent between two addresses in the node's wallet)
unshielding transactions with zero confirmations. This release fixes the bug.
