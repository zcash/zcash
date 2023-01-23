(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Fixes
-----

This release fixes an issue that could potentially cause a node to crash with the
log message "The wallet's best block hash `<hash>` was not detected in restored
chain state. Giving up; please restart with `-rescan`."

Transparent pool and chain supply tracking
------------------------------------------

Since v2.0.0, `zcashd` has tracked the change in value within the Sprout and
Sapling shielded pools for each block; v5.0.0 added the Orchard pool. This
release completes the set, by tracking the change in value within the
"transparent" pool (more precisely, the value stored in Bitcoin-style UTXOs).

`zcashd` also now tracks the change in "chain supply" for each block, defined as
the sum of coinbase output values, minus unclaimed fees. This is precisely equal
to the sum of the value in the transparent and shielded pools, and equivalent to
the sum of all unspent coins/notes on the chain. It is bounded above by the
theoretical maximum supply, but in practice is lower due to, for example, miners
not claiming transaction fees.

> Bitcoin-style consensus rules implement fees as an imbalance between spent
> coins/notes and newly-created coins/notes. The consensus rules require that a
> coinbase transaction's outputs have a total value no greater than the sum of
> that block's subsidy and the fees made available by the transactions in the
> block. However, the consensus rules do not require that all of the available
> funds are claimed, and a miner can create coinbase transactions with lower
> value in the outputs (though in the case of Zcash, the consensus rules do
> require the transaction to include [ZIP 1014](https://zips.z.cash/zip-1014)
> Funding Stream outputs).

After upgrading to v5.4.0, `zcashd` will start tracking changes in transparent
pool value and chain supply from the height at which it is restarted. Block
heights prior to this will not have any information recorded. To track changes
from genesis, and thus monitor the total transparent pool size and chain supply,
you will need to restart your node with the `-reindex` option.

RPC Changes
-----------

- `z_sendmany` will no longer select transparent coinbase when "ANY\_TADDR" is
  used as the `fromaddress`. It was already documented to do this, but the
  previous behavior didn’t match. When coinbase notes were selected in this
  case, they would (properly) require that the transaction didn’t have any
  change, but this could be confusing, as the documentation stated that these
  two conditions (using "ANY\_TADDR" and disallowing change) wouldn’t coincide.
- A new value pool object with `"id": "transparent"` has been added to the
  `valuePools` list in `getblockchaininfo` and `getblock`.
- A new `chainSupply` key has been added to `getblockchaininfo` and `getblock`
  to report the total chain supply as of that block height (if tracked), and the
  change in chain supply caused by the block (for `getblock`, if measured).

Mining
-------

- Changes to `getblocktemplate` have been backported from upstream Bitcoin Core,
  to significantly improve its performance by doing more work ahead of time in
  the mempool (and reusing the work across multiple `getblocktemplate` calls).

[Deprecations](https://zcash.github.io/zcash/user/deprecation.html)
--------------

The following features have been deprecated, but remain available by default.
These features may be disabled by setting `-allowdeprecated=none`. 18 weeks
after this release, these features will be disabled by default and the following
flags to `-allowdeprecated` will be required to permit their continued use:

- `gbt_oldhashes`: the `finalsaplingroothash`, `lightclientroothash`, and
  `blockcommitmentshash` fields in the output of `getblocktemplate` have been
  replaced by the `defaultroots` field.

The following previously-deprecated features have been disabled by default, and
will be removed in 18 weeks:

- `legacy_privacy`
- `getnewaddress`
- `getrawchangeaddress`
- `z_getbalance`
- `z_gettotalbalance`
- `z_getnewaddress`
- `z_listaddresses`
- `addrtype`
- `wallettxvjoinsplit`

The following previously-deprecated features have been removed:

- `dumpwallet`
- `zcrawreceive`
- `zcrawjoinsplit`
- `zcrawkeygen`

Platform Support
----------------

- CentOS 8 has been removed from the list of supported platforms. It reached EoL
  on December 31st 2021, and does not satisfy our Tier 2 policy requirements.
