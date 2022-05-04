Deprecation Procedure
=====================

From time to time, features of `zcashd` and its associated wallet and RPC API are
deprecated to allow eventual removal of functionality that has been superseded
by more recent improvements. Deprecation follows a process whereby deprecate
features can be explicitly turned on or off using the
`-allowdeprecated=<feature>` CLI argument.

`zcashd` internally supports two sets of deprecated feature flags in
`src/deprecation.h`:
- `DEFAULT_ALLOW_DEPRECATED` contains the set of features that remain available
  for use without having to be specifically enabled using `-allowdeprecated`.
- `DEFAULT_DENY_DEPRECATED` contains the set of features that are not enabled
  by default, and must be explicitly enabled using `-allowdeprecated`.

Deprecation of a feature occurs as a 3-step process:

1. A deprecation flag is selected for the feature, and added to
   `DEFAULT_ALLOW_DEPRECATED`. The fact of its deprecation is announced, and
   any functionality that supersedes the deprecated feature (if any) is
   documented, in the release notes. The string `DEPRECATED` is added to
   user-facing API documentation and CLI help text.
2. The deprecation flag is removed from `DEFAULT_ALLOW_DEPRECATED` and added to
   `DEFAULT_DENY_DEPRECATED`.
3. The deprecated feature is removed entirely, and its deprecation flag is
   removed.

Features that enter Stage 1 in a particular release should be disabled by
default after no fewer than 3 releases that update `zcashd`'s
minor-version, and features should only be fully removed after a total of 6 minor-version
updates. `zcashd`'s release schedule intends to produce a release that updates
the minor version every 6 weeks, so deprecated features remain accessible by
default for approximately 18 weeks, and then can be expected to be removed no
less than 36 weeks from their initial deprecation. The deprecation timeline for
each newly deprecated feature should be documented in
[../user/deprecation.md](../user/deprecation.md).

