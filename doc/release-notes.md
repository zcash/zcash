(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

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
