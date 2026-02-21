(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Removed the P2P network alert system
------------------------------------

`zcashd` will no longer respond to alert messages from the P2P network.
The `-alerts` config option has been removed and will be ignored. This
ensures that, if any party were to maintain a zcashd fork, none of the
original developers or associated organizations (including ECC and ZODL)
will hold keys that could affect nodes running such a fork.

This leaves intact alerting for end-of-service and detected chain forks,
and the `-alertnotify` functionality.
