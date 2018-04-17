(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

`z_mergetoaddress` promoted out of experimental status
------------------------------------------------------

The `z_mergetoaddress` API merges funds from t-addresses, z-addresses, or both,
and sends them to a single t-address or z-address. It was added in v1.0.15 as an
experimental feature, to simplify the process of combining many small UTXOs and
notes into a few larger ones.

In this release we are promoting `z_mergetoaddress` out of experimental status.
It is now a stable RPC, and any future changes to it will only be additive. See
`zcash-cli help z_mergetoaddress` for API details and usage.

Unlike most other RPC methods, `z_mergetoaddress` operates over a particular
quantity of UTXOs and notes, instead of a particular amount of ZEC. By default,
it will merge 50 UTXOs and 10 notes at a time; these limits can be adjusted with
the parameters `transparent_limit` and `shielded_limit`.

`z_mergetoaddress` also returns the number of UTXOs and notes remaining in the
given addresses, which can be used to automate the merging process (for example,
merging until the number of UTXOs falls below some value).

Option parsing behavior
-----------------------

Command line options are now parsed strictly in the order in which they are
specified. It used to be the case that `-X -noX` ends up, unintuitively, with X
set, as `-X` had precedence over `-noX`. This is no longer the case. Like for
other software, the last specified value for an option will hold.

Low-level RPC changes
---------------------

- Bare multisig outputs to our keys are no longer automatically treated as
  incoming payments. As this feature was only available for multisig outputs for
  which you had all private keys in your wallet, there was generally no use for
  them compared to single-key schemes. Furthermore, no address format for such
  outputs is defined, and wallet software can't easily send to it. These outputs
  will no longer show up in `listtransactions`, `listunspent`, or contribute to
  your balance, unless they are explicitly watched (using `importaddress` or
  `importmulti` with hex script argument). `signrawtransaction*` also still
  works for them.
