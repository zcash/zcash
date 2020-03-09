(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Removal of time adjustment and the -maxtimeadjustment= option
-------------------------------------------------------------

Prior to v2.1.1-1, `zcashd` would adjust the local time that it used by up
to 70 minutes, according to a median of the times sent by the first 200 peers
to connect to it. This mechanism was inherently insecure, since an adversary
making multiple connections to the node could effectively control its time
within that +/- 70 minute window (this is called a "timejacking attack").

In the v2.1.1-1 security release, in addition to other mitigations for
timejacking attacks, the maximum time adjustment was set to zero by default.
This effectively disabled time adjustment; however, a `-maxtimeadjustment=`
option was provided to override this default.

As a simplification the time adjustment code has now been completely removed,
together with `-maxtimeadjustment=`. Node operators should instead simply
ensure that local time is set reasonably accurately.

If it appears that the node has a significantly different time than its peers,
a warning will still be logged and indicated on the metrics screen if enabled.

View shielded information in wallet transactions
------------------------------------------------

In previous `zcashd` versions, to obtain information about shielded transactions
you would use either the `z_listreceivedbyaddress` RPC method (which returns all
notes received by an address) or `z_listunspent` (which returns unspent notes,
optionally filtered by addresses). There were no RPC methods that directly
returned details about spends, or anything equivalent to the `gettransaction`
method (which returns transparent information about in-wallet transactions).

This release introduces a new RPC method `z_viewtransaction` to fill that gap.
Given the ID of a transaction in the wallet, it decrypts the transaction and
returns detailed shielded information for all decryptable new and spent notes,
including:

- The address that each note belongs to.
- Values in both decimal ZEC and zatoshis.
- The ID of the transaction that each spent note was received in.
- An `outgoing` flag on each new note, which will be `true` if the output is not
  for an address in the wallet.
- A `memoStr` field for each new note, containing its text memo (if its memo
  field contains a valid UTF-8 string).

Information will be shown for any address that appears in `z_listaddresses`;
this includes watch-only addresses linked to viewing keys imported with
`z_importviewingkey`, as well as addresses with spending keys (both generated
with `z_getnewaddress` and imported with `z_importkey`).

Build system
------------

- The `--enable-lcov`, `--disable-tests`, and `--disable-mining` flags for
  `zcutil/build.sh` have been removed. You can pass these flags instead by using
  the `CONFIGURE_FLAGS` environment variable. For example, to enable coverage
  instrumentation (thus enabling "make cov" to work), call:

  ```
  CONFIGURE_FLAGS="--enable-lcov --disable-hardening" ./zcutil/build.sh
  ```

- The build system no longer defaults to verbose output. You can re-enable
  verbose output with `./zcutil/build.sh V=1`
