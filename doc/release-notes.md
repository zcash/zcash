(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

This release fixes a security issue described at
https://z.cash/support/security/announcements/security-announcement-2019-09-24/ .


Shrinking of debug.log files is temporarily disabled
----------------------------------------------------

In previous versions, `zcashd` would shrink the `debug.log` file to 200 KB on
startup if it was larger than 10 MB. This behaviour, and the `-shrinkdebugfile`
option that controlled it, has been disabled.
