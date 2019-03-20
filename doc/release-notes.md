(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Sprout note validation bug fixed in wallet
------------------------------------------
We include a fix for a bug in the Zcashd wallet which could result in Sprout
z-addresses displaying an incorrect balance. Sapling z-addresses are not
impacted by this issue. This would occur if someone sending funds to a Sprout
z-address intentionally sent a different amount in the note commitment of a
Sprout output than the value provided in the ciphertext (the encrypted message
from the sender).

Users should install this update and then rescan the blockchain by invoking
“zcashd -rescan”. Sprout address balances shown by the zcashd wallet should
then be correct.

Thank you to Alexis Enston for bringing this to our attention.

[Security Announcement 2019-03-19](https://z.cash/support/security/announcements/security-announcement-2019-03-19/)

[Pull request](https://github.com/zcash/zcash/pull/3897)

Miner address selection behaviour fixed
---------------------------------------
Zcash inherited a bug from upstream Bitcoin Core where both the internal miner
and RPC call `getblocktemplate` would use a fixed transparent address, until RPC
`getnewaddress` was called, instead of using a new transparent address for each
mined block.  This was fixed in Bitcoin 0.12 and we have now merged the change.

Miners who wish to use the same address for every mined block, should use the
`-mineraddress` option. 

[Mining Guide](https://zcash.readthedocs.io/en/latest/rtd_pages/zcash_mining_guide.html)


New consensus rule: Reject blocks that violate turnstile (Testnet only)
-----------------------------------------------------------------------
Testnet nodes will now enforce a consensus rule which marks blocks as invalid
if they would lead to a turnstile violation in the Sprout or Shielded value
pools. The motivations and deployment details can be found in the accompanying
[ZIP draft](https://github.com/zcash/zips/pull/210).

The consensus rule will be enforced on mainnet in a future release.

[Pull request](https://github.com/zcash/zcash/pull/3885)

