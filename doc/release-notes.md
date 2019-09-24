(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

This release fixes a security issue described at
https://z.cash/support/security/announcements/security-announcement-2019-09-24/ .

The service period of this release is shorter than normal due to the upcoming
v2.1.0 Blossom release. The End-of-Service of v2.0.7-3 will occur at block height
653012, expected to be on 2019-12-10.


Shrinking of debug.log files is temporarily disabled
----------------------------------------------------

In previous versions, `zcashd` would shrink the `debug.log` file to 200 KB on
startup if it was larger than 10 MB. This behaviour, and the `-shrinkdebugfile`
option that controlled it, has been disabled.
