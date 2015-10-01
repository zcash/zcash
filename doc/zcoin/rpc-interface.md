**Goal:**

Users comfortable with the commandline and JSON who are careful at
managing cryptographic secrets can create ZC addresses, Protect cleartext
currency they control, then Pour to other addresses. Also, they pay fees
following the normal default upstream fee rules.

**Definitions:**

* "fully functional" - users *could in theory* use this interface
  for all of their confidential/fungible currency transfer needs. We
  *could in theory* launch with just this interface, and a small subset
  of commandline users wouldn't be blocked on anything.
* "minimally" - this excludes *everything else* unneccessary for the Goal:
  no backup/restore, no smart caching of incremental merkle trees, no GUI,
  no "account management", etc...  * "stateless" - The node doesn't store
  anything as a side effect of handling these requests. The resulting
  transactions can be submitted via ``sendrawtransaction`` and that will
  store something (to the mempool), but this ticket shouldn't alter that
  codepath at all.
* "off-chain-Bucket-transmission" - The sending user
  must send the recipient the Bucket (Coin) secrets out-of-band. They are
  not decrypted from the blockchain by anything.

**Approach Suggestion:**

From my experience with ``zerocoinmint`` and ``zerocoinpour`` operations,
I think it'll be cleaner to implement these new RPC operations separately
(without altering the older two operations). Removing those methods
(and associated state tracking) will be a follow up ticket.

**Interface Outline:**

Here's a proposed interface with lots of hand-waving. Please update this
description in-place as it evolves:

``zc-stateless-keygen``
- **Synopsis:** Generate a ZC address and return the public and private
  parts.
- **Returns:** ``{ "zcaddress": <public address hex>, "zcsecretkey":
  <hex encoded secret keys string> }``
- **Security UX Note:** The caller *must* confidentially store this
  value. If the ``zcsecretkey`` is lost, no funds sent to the ``zcaddress``
  can be recovered and the value is lost. If the confidentiality is
  compromised, a thief can steal past or future payments to ``zcaddress``.

``zc-stateless-protect ZCADDRESS AMOUNT FEE [ CLEARFROMACCT ]``
- **Returns:** ``{ "commitment": <hex>, "bucketsecret":
  <hex of bucket/coin>, "rawtxn": <hex of raw/unsigned transaction> }``
- **Synopsis:** Protect ``AMOUNT - FEE`` currency units to ``ZCADDRESS``,
  sending ``FEE`` as a cleartext miner fee, optionally using the
  ``CLEARFROMACCT`` if given, which is of the same type that upstream
  commandline interface calls an "account".
- **Security UX Note:** The caller *must* confidentially store
  ``bucketsecret`` for later use in a Pour. If they lose this data, the
  contained currency value is lost. If confidentiality is compromised,
  the malicious observer can learn some things about the transaction.
  (*FIXME:* Which data? The value? The timing/block in which it was
  submitted. The recipient address. Anything else?)

``zc-stateless-pour SECRETKEY1 BUCKET1 SECRETKEY2 BUCKET2 ZCDEST1 AMT1 ZCDEST2 AMT2 CLEARDEST CLEARAMT FEE``
- **Returns:** ``{ "commitment1": <hex>, "commitment2": <hex>,
  "bucketsecret1": <hex>, "bucketsecret2": <hex>, "rawtxn":#
  <hex of unsigned txn containing a Pour> }``
- **Synopsis:** Create a rawtxn for a Pour given the secrets necessary
  to spend the two buckets, sending the results to the two ``ZCDEST<N>``
  ZC addresses in the given amounts, and a cleartext value to ``CLEARDEST``
  (which is a Bitcoin-style address), and pay a (clear) ``FEE``.
- **Security UX Note:** The caller *must* confidentially transmit the
  returned ``bucketsecret<N>`` values to the necessary recipients. If
  either are to self, the caller *must* confidentially store the secret.
  Losing these secrets results in losing the encapsulated value.
- **Performance Note:** The initial implementation will
  *scan the entire blockchain* and *create the commitment merkletree in memory*
  in order to find the (secret) commitment paths for both ``BUCKET<N>``.
  It will also do a Pour proof, so this operation will be slow and
  potentially require a lot of memory.

**Design Flaw:** These return "raw transactions", but the Pour has no
meaningful TxIns to sign, so what does the node do when asked to sign
the raw transaction?

**Design Note:** We *probably* want the Pour signatures to bind to all
of the transaction except signatures just like normal ``SIGHASH_ALL``
signatures. This prevents an interceptor from ripping out the Pour and
sticking it in a different transaction.
