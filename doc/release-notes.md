(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

zcash-cli: arguments privacy
----------------------------

The RPC command line client gained a new argument, `-stdin`
to read extra arguments from standard input, one per line until EOF/Ctrl-D.
For example:

    $ src/zcash-cli -stdin walletpassphrase
    mysecretcode
    120
    ^D (Ctrl-D)

It is recommended to use this for sensitive information such as private keys, as
command-line arguments can usually be read from the process table by any user on
the system.

Asm representations of scriptSig signatures now contain SIGHASH type decodes
----------------------------------------------------------------------------

The `asm` property of each scriptSig now contains the decoded signature hash
type for each signature that provides a valid defined hash type.

The following items contain assembly representations of scriptSig signatures
and are affected by this change:

- RPC `getrawtransaction`
- RPC `decoderawtransaction`
- REST `/rest/tx/` (JSON format)
- REST `/rest/block/` (JSON format when including extended tx details)
- `zcash-tx -json`

For example, the `scriptSig.asm` property of a transaction input that
previously showed an assembly representation of:

    304502207fa7a6d1e0ee81132a269ad84e68d695483745cde8b541e3bf630749894e342a022100c1f7ab20e13e22fb95281a870f3dcf38d782e53023ee313d741ad0cfbc0c509001

now shows as:

    304502207fa7a6d1e0ee81132a269ad84e68d695483745cde8b541e3bf630749894e342a022100c1f7ab20e13e22fb95281a870f3dcf38d782e53023ee313d741ad0cfbc0c5090[ALL]

Note that the output of the RPC `decodescript` did not change because it is
configured specifically to process scriptPubKey and not scriptSig scripts.
