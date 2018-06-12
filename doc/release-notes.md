(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

`-disabledeprecation` removal
-----------------------------

In release 1.0.9 we implemented automatic deprecation of `zcashd` software
versions made by the Zcash Company. The configuration option
`-disabledeprecation` was added as a way for users to specifically choose to
stay on a particular software version. However, it incorrectly implied that
deprecated releases would still be supported.

This release removes the `-disabledeprecation` option, so that `zcashd` software
versions made by the Zcash Company will always shut down in accordance with the
defined deprecation policy (currently 16 weeks after release). Users who wish to
use a different policy must now specifically choose to either:

- edit and compile the source code themselves, or
- obtain a software version from someone else who has done so (and obtain
  support from them).

Either way, it is much clearer that the software they are running is not
supported by the Zcash Company.
