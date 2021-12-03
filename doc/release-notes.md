(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Direct headers announcement (BIP 130)
-------------------------------------

Between compatible peers, BIP 130 direct headers announcement is used. This
means that blocks are advertized by announcing their headers directly, instead
of just announcing the hash. In a reorganization, all new headers are sent,
instead of just the new tip. This can often prevent an extra roundtrip before
the actual block is downloaded.
