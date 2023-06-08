(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Change to Transaction Relay Policy
----------------------------------

Transactions paying less than the [ZIP 317](https://zips.z.cash/zip-0317)
conventional fee to the extent that they have more than `-txunpaidactionlimit`
unpaid actions (default: 50) will not be accepted to the mempool or relayed.
For the default values of `-txunpaidactionlimit` and `-blockunpaidactionlimit`,
these transactions would never be mined by the ZIP 317 block construction
algorithm. (If the transaction has been prioritised by `prioritisetransaction`,
the modified fee is used to calculate the number of unpaid actions.)

Removal of Fee Estimation
-------------------------

The `estimatefee` RPC call, which estimated the fee needed for a transaction to
be included within a target number of blocks, has been removed. The fee that
should be paid under normal circumstances is the ZIP 317 conventional fee; it
is not possible to compute that without knowing the number of logical actions
in a transaction, which was not an input to `estimatefee`. The `fee_estimates.dat`
file is no longer used.

RPC Changes
-----------

A new RPC method, `z_getsubtreesbyindex`, has been added to the RPC interface.
This method is only enabled when running with the `-experimentalfeatures=1` and
`-lightwalletd=1` node configuration options. This method makes available to the
caller precomputed node values within the Sapling and Orchard note commitment 
trees. Wallets can make use of these precomputed values to make their existing
notes spendable without needing to fully scan the sub-trees whose roots
correspond to the returned node values.

In conjunction with this change, the `getblock` RPC method now returns an
additional `trees` field as part of its result. The value for this field is an
object that contains the final sizes of the Sapling and Orchard note commitment
trees after the block's note commitments have been appended to their respective
trees.

Error reporting has also been improved for a number of RPC methods.

Privacy Policy Changes
----------------------

The `AllowRevealedSenders` privacy policy no longer allows sending from
multiple taddrs in the same transaction. This now requires
`AllowLinkingAccountAddresses`. Care should be taken in using
`AllowLinkingAccountAddresses` too broadly, as it can also result in linking
UAs when transparent funds are sent from them. The practical effect is that an
explicit privacy policy is always required for `z_mergetoaddress`,
`z_sendmany`, and `z_shieldcoinbase` when sending from multiple taddrs, even
when using wildcards like `*` and `ANY_TADDR`.

Wallet Updates
--------------

A number of libraries that zcashd relies upon have been updated as part of this
release, including some changes that result in updates to wallet serialization
formats for Orchard note commitment tree data. As always, it is recommended
that users back up their wallets prior to upgrading to a new release to help
guarantee the continued availability of their funds.

Platform Support
----------------

- Ubuntu 18.04 LTS has been removed from the list of supported platforms. It
  reached End of Support on May 31st 2023, and no longer satisfies our Tier 2
  policy requirements.
