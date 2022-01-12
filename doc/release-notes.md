(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Wallet
------

`z_sendmany`
------------

- The `z_sendmany` RPC call no longer permits Sprout recipients in the 
  list of recipient addresses. Transactions spending Sprout funds will
  still result in change being sent back into the Sprout pool, but no
  other `Sprout->Sprout` transactions will be constructed by the Zcashd
  wallet. 

- The restriction that prohibited `Sprout->Sapling` transactions has been 
  lifted; however, since such transactions reveal the amount crossing 
  pool boundaries, they must be explicitly enabled via a parameter to
  the `z_sendmany` call.

- A new boolean parameter, `allowRevealedAmounts`, has been added to the
  list of arguments accepted by `z_sendmany`. This parameter defaults to
  `false` and is only required when the transaction being constructed 
  would reveal transaction amounts as a consequence of ZEC value crossing
  shielded pool boundaries via the turnstile.

- Since Sprout outputs are no longer created (with the exception of change)
  `z_sendmany` no longer generates payment disclosures (which were only 
  available for Sprout outputs) when the `-paymentdisclosure` experimental
  feature flag is set.

