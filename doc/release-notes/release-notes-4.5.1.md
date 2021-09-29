Notable changes
===============

Fixed bugs in the testnet Orchard circuit
-----------------------------------------

In the `zcashd v4.5.0` release notes we indicated that a testnet rollback might
occur to update the consensus rules, if we needed to make backwards-incompatible
changes. Shortly after `zcashd v4.5.0` was released, during another internal
review of the Orchard circuit, we identified two bugs that would affect the
upcoming testnet activation of NU5:

- The diversifier base `g_d_old`, for the note being spent, is required to be a
  non-identity point. A note created from a payment address with `g_d` set to
  the identity (via collaboration between sender and recipient) could be spent
  multiple times with different nullifiers (corresponding to different `ivk`s).
  The code outside the circuit correctly enforced the non-identity requirement,
  but the circuit did not correctly constrain this, and allowed the prover to
  witness the identity.

- SinsemillaCommit can be modeled as a Pedersen commitment to an output of
  SinsemillaHash: `SinsemillaCommit(r, M) = SinsemillaHashToPoint(M) + [r] R`.
  The specification used incomplete addition here, matching its use inside
  SinsemillaHash. However, unlike in SinsemillaHash, an exceptional case can be
  produced here when `r = 0`. The derivations of `rivk` (for computing `ivk`)
  and `rcm` (for computing the note commitment) normally ensure that `r = 0`
  can only occur with negligible probability, but these derivations are not
  checked by the circuit for efficiency; thus SinsemillaCommit needs to use
  complete addition.

These bugs do not affect mainnet, as `zcashd v4.5.0` only set the activation
height for NU5 on testnet for testing purposes. Nevertheless, in the interest of
keeping the testnet environment as close to mainnet as possible, we are fixing
these bugs immediately. This means a change to the NU5 consensus rules, and a
new testnet activation height for NU5.

To this end, the following changes are made in `zcashd v4.5.1`:

- The consensus branch ID for NU5 is changed to `0x37519621`.
- The protocol version indicating NU5-aware testnet nodes is set to `170015`.
- The testnet activation height for NU5 is set to **1,599,200**.

Testnet nodes that upgrade to `zcashd v4.5.1` prior to block height 1,590,000
will follow the new testnet network upgrade. Testnet nodes that are running
`zcashd v4.5.0` at that height will need to upgrade to `v4.5.1` and then run
with `-reindex`.

As always, it is possible that further backwards-incompatible changes might be
made to the NU5 consensus rules in this testing phase, prior to setting the
mainnet activation height, as we continue to conduct additional internal review.
In the event that this happens, testnet will be rolled back in (or prior to)
v5.0.0, and a new testnet activation will occur.

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

Changelog
=========

Jack Grigg (5):
      wallet: Check spent shielded notes in CWalletTx::IsFromMe
      Fix bugs in testnet Orchard circuit
      depends: Postpone native_ccache 4.4.2
      make-release.py: Versioning changes for 4.5.1.
      make-release.py: Updated manpages for 4.5.1.

