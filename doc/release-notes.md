(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

This release fixes a security issue described at
https://z.cash/support/security/announcements/security-announcement-2020-02-05/ .

This release also adds a `-maxtimeadjustment` option to set the maximum time, in
seconds, by which the node's clock can be adjusted based on the clocks of its
peer nodes. This option defaults to 0, meaning that no such adjustment is performed.
This is a change from the previous behaviour, which was to adjust the clock by up
to 70 minutes forward or backward. The maximum setting for this option is now
25 minutes (1500 seconds).
