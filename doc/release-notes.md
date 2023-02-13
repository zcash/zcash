(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

`allowdeprecated` in `zcash.conf`
---------------------------------

In v5.0.0 a [feature deprecation framework](https://zcash.github.io/zcash/user/deprecation.html)
was released, to enable `zcashd` features to be formally deprecated and removed:

- In stage 1, a feature is marked as deprecated, but otherwise left as-is. It
  remains in this stage for at least 18 weeks.
- In stage 2, the feature is default-disabled, but can be re-enabled with the
  `-allowdeprecated` config option. It remains in this stage for at least 18
  weeks.
- Finally, the feature is removed - either entirely, or (e.g. in the case of RPC
  methods that were inherited from Bitcoin Core) with a "tombstone" left to
  inform users of the removal (and what to use instead if applicable).

Config options can be specified either as a `zcashd` argument (`-option=value`)
or in `zcash.conf` (as a `option=value` line). However, due to a bug in the
implementation, `allowdeprecated=feature` lines in `zcash.conf` were ignored.
The bug went unnoticed until v5.4.0, in which the first group of features moved
from stage 1 to stage 2. This hotfix releases fixes the bug.

Fixed RPC blocking and wallet view lag on reindex
-------------------------------------------------

The known issue reported in the v5.4.0 release notes has been fixed.

