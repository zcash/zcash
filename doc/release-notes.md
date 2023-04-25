(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

RPC Changes
-----------

- `getdeprecationinfo` has several changes:
  - It now returns additional information about currently deprecated and
    disabled features.
  - A new `end_of_service` object that contains both the block height for
    end-of-service and the estimated time that the end-of-service halt is
    expected to occur. Note that this height is just an approximation and
    will change over time as the end-of-service block height approaches,
    due to the variability in block times. The
    `end_of_service` object is intended to replace the `deprecationheight`
    field; see the [Deprecations](#deprecations) section for additional detail.
  - This RPC method was previously only available for mainnet nodes; it is now
    also available for testnet and regtest nodes, in which case it does not
    return end-of-service halt information (as testnet and regtest nodes do not
    have an end-of-service halt feature.)
- Several `z_sendmany`, `z_shieldcoinbase` and `z_mergetoaddress` failures have
  moved from synchronous to asynchronous, so while you should already be
  checking the async operation status, there are now more cases that may trigger
  failure at that stage.
- The `AllowRevealedRecipients` privacy policy is now required in order to choose a
  transparent change address for a transaction. This will only occur when the wallet
  is unable to construct the transaction without selecting funds from the transparent
  pool, so the impact of this change is that for such transactions, the user must specify
  `AllowFullyTransparent`.
- The `z_shieldcoinbase` RPC method now supports an optional memo.
- The `z_shieldcoinbase` and `z_mergetoaddress` RPC methods now support an
  optional privacy policy.
- The `z_mergetoaddress` RPC method can now merge _to_ UAs and can also send
  between different shielded pools (when `AllowRevealedAmounts` is specified).
- The `estimatepriority` RPC call has been removed.
- The `priority_delta` argument to the `prioritisetransaction` RPC call now has
  no effect and must be set to a dummy value (0 or null).

Changes to Transaction Fee Selection
------------------------------------

- The zcashd wallet now uses the conventional transaction fee calculated according
  to [ZIP 317](https://zips.z.cash/zip-0317) by default. This conventional fee
  will be used unless a fee is explicitly specified in an RPC call, or for the
  wallet's legacy transaction creation APIs (`sendtoaddress`, `sendmany`, and
  `fundrawtransaction`) when the `-paytxfee` option is set.
- The `-mintxfee` and `-sendfreetransactions` options have been removed. These
  options previously instructed the legacy transaction creation APIs to increase
  fees to this limit and to use a zero fee for "small" transactions that spend
  "old" inputs, respectively. They will now cause a warning on node startup if
  used.


Changes to Block Template Construction
--------------------------------------

We now use a new block template construction algorithm documented in
[ZIP 317](https://zips.z.cash/zip-0317#recommended-algorithm-for-block-template-construction).

- This algorithm no longer favours transactions that were previously considered
  "high priority" because they spent older inputs. The `-blockprioritysize` config
  option, which configured the portion of the block reserved for these transactions,
  has been removed and will now cause a warning if used.
- The `-blockminsize` option, which configured the size of a portion of the block
  to be filled regardless of transaction fees or priority, has also been removed
  and will cause a warning if used.
- A `-blockunpaidactionlimit` option has been added to control the limit on
  "unpaid actions" that will be accepted in a block for transactions paying less
  than the ZIP 317 fee. This defaults to 50.

Change to Transaction Relay Policy
----------------------------------

The allowance for "free transactions" in mempool acceptance and relay has been
removed. All transactions must pay at least the minimum relay threshold, currently
100 zatoshis per 1000 bytes up to a maximum of 1000 zatoshis, in order to be
accepted and relayed. (Individual nodes can change this using `-minrelaytxfee`
but in practice the network default needs to be adhered to.) This policy is under
review and [might be made stricter](https://zips.z.cash/zip-0317#transaction-relaying);
if that happens then the ZIP 317 conventional fee will still be sufficient for
mempool acceptance and relay.

Removal of Priority Estimation
------------------------------

Estimation of "priority" needed for a transaction to be included within a target
number of blocks, and the associated `estimatepriority` RPC call, have been
removed. The format for `fee_estimates.dat` has also changed to no longer save
these priority estimates. It will automatically be converted to the new format
which is not readable by prior versions of the software. The `-txconfirmtarget`
config option is now obsolete and has also been removed. It will cause a
warning if used.

[Deprecations](https://zcash.github.io/zcash/user/deprecation.html)
--------------

The following features have been deprecated, but remain available by default.
These features may be disabled by setting `-allowdeprecated=none`. 18 weeks
after this release, these features will be disabled by default and the following
flags to `-allowdeprecated` will be required to permit their continued use:

- `deprecationinfo_deprecationheight`: The `deprecationheight` field of
  `getdeprecationinfo` has been deprecated and replaced by the `end_of_service`
  object.
