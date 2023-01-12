(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

RPC Changes
-----------

- `z_sendmany` will no longer select transparent coinbase when "ANY\_TADDR" is
  used as the `fromaddress`. It was already documented to do this, but the
  previous behavior didn’t match. When coinbase notes were selected in this
  case, they would (properly) require that the transaction didn’t have any
  change, but this could be confusing, as the documentation stated that these
  two conditions (using "ANY\_TADDR" and disallowing change) wouldn’t coincide.

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
