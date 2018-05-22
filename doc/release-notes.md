(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Sapling network upgrade
-----------------------

The consensus code preparations for the Sapling network upgrade, as described
in [ZIP 243](https://github.com/zcash/zips/blob/master/zip-0243.rst) and the
[Sapling spec](https://github.com/zcash/zips/blob/master/protocol/sapling.pdf)
are finished and included in this release. Sapling support in the wallet and
RPC is ongoing, and is expected to land in master over the next few weeks.

The [Sapling MPC](https://blog.z.cash/announcing-the-sapling-mpc/) is currently
working on producing the final Sapling parameters. In the meantime, Sapling will
activate on testnet with dummy Sapling parameters at height XXXXXX. This
activation will be temporary, and the testnet will be rolled back by version
2.0.0 so that both mainnet and testnet will be using the same parameters.
Users who want to continue testing Overwinter should continue to run version
1.1.0 on testnet, and then upgrade to 2.0.0 (which will be released after
Overwinter activates).

Sapling can also be activated at a specific height in regtest mode by
setting the config options `-nuparams=5ba81b19:HEIGHT -nuparams=76b809bb:HEIGHT`.
These config options will change when the testnet is rolled back for 2.0.0
(because the branch ID for Sapling will change, due to us following the safe
upgrade conventions we introduced in Overwinter).

Users running testnet or regtest nodes will need to run
`./zcutil/fetch-params.sh --testnet` (for users building from source) or
`zcash-fetch-params --testnet` (for binary / Debian users).

As a reminder, because the Sapling activation height is not yet specified for
mainnet, version 1.1.1 will behave similarly as other pre-Sapling releases even
after a future activation of Sapling on the network. Upgrading from 1.1.1 will
be required in order to follow the Sapling network upgrade on mainnet.

Sapling transaction format
--------------------------

Once Sapling has activated, transactions must use the new v4 format (including
coinbase transactions). All RPC methods that create new transactions (such as
`createrawtransaction` and `getblocktemplate`) will create v4 transactions once
the Sapling activation height has been reached.

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
