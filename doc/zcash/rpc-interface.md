# Goal

Users comfortable with the commandline and JSON who are careful at
managing cryptographic secrets can create ZC addresses, Protect cleartext
currency they control, then Pour to other addresses. Also, they pay fees
following the normal default upstream fee rules.

## Definitions

* "fully functional" - users *could in theory* use this interface
  for all of their confidential/fungible currency transfer needs. We
  *could in theory* launch with just this interface, and a small subset
  of commandline users wouldn't be blocked on anything.
* "minimally" - this excludes *everything else* unneccessary for the Goal:
  no backup/restore, no smart caching of incremental merkle trees, no GUI,
  no "account management", etc...
* "stateless" - The node doesn't store anything as a side effect of
  handling these requests. The resulting transactions can be submitted
  via ``sendrawtransaction`` and that will store something (to the
  mempool), but this ticket shouldn't alter that codepath at all.
* "off-chain-Bucket-transmission" - The sending user
  must send the recipient the Bucket (Coin) secrets out-of-band. They are
  not decrypted from the blockchain by anything.

## Deprecation

These RPC methods will deprecate the older ``zerocoinmint`` and
``zerocoinpour`` operations.

# Interface Outline

All operations begin with the ``zc-raw-`` prefix.

## ``zc-raw-keygen``

**Usage:** ``zc-raw-keygen``

**Return Value:**

```
{
  "zcaddress": <public address hex>,
  "zcsecretkey": <hex encoded secret keys string>
}
```

**Synopsis:** Generate a ZC address and return the public and private
  parts.

**Security UX Note:** The caller *must* confidentially store this
  value. If the ``zcsecretkey`` is lost, no funds sent to the ``zcaddress``
  can be recovered and the value is lost. If the confidentiality is
  compromised, a thief can steal past or future payments to ``zcaddress``.

## ``zc-raw-protect``

**Usage:** ``zc-raw-protect RAWTX ZCADDRESS VALUE_TO_PROTECT``

**Return Value:**

```
{
  "bucketsecret": <hex of bucket/coin>,
  "rawtxn": <hex of raw/unsigned transaction>
}
```

**Synopsis:** Given a raw transaction, add a protect input which protects
``VALUE_TO_PROTECT`` to ``ZCADDRESS``. Returns the bucket, and the unsigned
raw transaction.

**Security UX Note:** The caller *must* confidentially store
  ``bucketsecret`` for later use in a Pour. If they lose this data, the
  contained currency value is lost. If confidentiality is compromised,
  the malicious observer can learn some things about the transaction.
  (*FIXME:* Which data? The value? The timing/block in which it was
  submitted. The recipient address. Anything else?)

## ``zc-raw-pour``

**Usage:** ``zc-raw-pour SECRETKEY1 BUCKET1 SECRETKEY2 BUCKET2 ZCDEST1 AMT1 ZCDEST2 AMT2 CLEARDEST CLEARAMT``

**Return Value:**

```
{
  "encryptedbucket1": <hex>,
  "encryptedbucket2": <hex>,
  "rawtxn": <hex of signed txn containing a Pour>
}
```

**Synopsis:** Create a rawtxn for a Pour given the secrets necessary
  to spend the two buckets, sending the results to the two ``ZCDEST<N>``
  ZC addresses in the given amounts, and a cleartext value to ``CLEARDEST``
  (which is a Bitcoin-style secret key) of value ``CLEARAMT``.
  The remaining value will be sent as a fee. The value is balanced such
  that the input pour value equals the amount of the output pours plus
  `vpub`. `vpub` is CLEARAMT plus the remaining implicit fee.

**Security UX Note:** The caller *must* confidentially transmit the
  returned ``encryptedbucket<N>`` values to the necessary recipients. If
  either are to self, the caller *must* confidentially store the secret.
  Losing these secrets results in losing the encapsulated value.

**Performance Note:** The initial implementation will *scan the entire
  blockchain* and *create the commitment merkletree in memory*
  in order to find the (secret) commitment paths for both ``BUCKET<N>``.
  It will also do a Pour proof, so this operation will be slow and
  potentially require a lot of memory.

## ``zc-raw-receive``

**Usage:** ``zc-raw-receive SECRETKEY ENCRYPTEDBUCKET``

**Return Value:**

```
{
  "bucket": <hex>,
  "amount": <amount>
}
```

**Synopsis:** The ``zc-raw-pour`` operations provides two output
  ``encryptedbucket<N>`` fields. When a sender delivers one of these
  values to a recipient, the recipient uses this method to decrypt and
  verify the bucket, and then returns the bucket secrets blob and the
  represented value. The ``bucket`` output can then be passed to a
  ``zc-raw-pour`` as one of the ``BUCKET<N>`` inputs.

**UX Note:** This may fail in several ways: decryption may fail, the
  resulting proof may be invalid, the commitment not present in the
  block chain, etc...

## Outstanding Issues

**Design Flaw:** These return "raw transactions", but the Pour has no
meaningful TxIns to sign, so what does the node do when asked to sign
the raw transaction?

**Design Note:** We *probably* want the Pour signatures to bind to all
of the transaction except signatures just like normal ``SIGHASH_ALL``
signatures. This prevents an interceptor from ripping out the Pour and
sticking it in a different transaction.
