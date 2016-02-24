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
