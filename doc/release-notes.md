(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============


Updated settings
----------------
- Netmasks that contain 1-bits after 0-bits (the 1-bits are not contiguous on
  the left side, e.g. 255.0.255.255) are no longer accepted. They are invalid
  according to RFC 4632.
