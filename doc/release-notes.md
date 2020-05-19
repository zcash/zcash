(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

The node's known peers are persisted to disk in a file called `peers.dat`. The
format of this file has been changed in a backwards-incompatible way in order to
accommodate the storage of Tor v3 and other BIP155 addresses. This means that if
the file is modified by 0.21.0 or newer then older versions will not be able to
read it. Those old versions, in the event of a downgrade, will log an error
message that deserialization has failed and will continue normal operation
as if the file was missing, creating a new empty one. (#19954)

Notable changes
===============


Updated settings
----------------
- Netmasks that contain 1-bits after 0-bits (the 1-bits are not contiguous on
  the left side, e.g. 255.0.255.255) are no longer accepted. They are invalid
  according to RFC 4632.
