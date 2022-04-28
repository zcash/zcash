Deprecation Policy
==================

From time to time, features of zcashd and its associated wallet and RPC API are deprecated
to allow eventual removal of functionality that has been superseded by more recent
improvements. Deprecation follows a process whereby deprecate features can be explicitly
turned on or off using the `-allowdeprecated=<feature>` CLI argument.

Zcashd internally supports two sets of deprecated feature flags in `src/deprecation.h`:
- `DEFAULT_ALLOW_DEPRECATED` contains the set of features that remain available for
  use without having to be specifically enabled using `-allowdeprecated`.
- `DEFAULT_DENY_DEPRECATED` contains the set of features that are not enabled by
  default, and must be explicitly enabled using `-allowdeprecated`.

Deprecation occurs as a 3-step process:
- When a feature is initially deprecated in a release, the fact of its deprecation is
  announced in the release notes, any functionality that supersedes the deprecated feature
  (if any) is documented in the release notes, and the string `DEPRECATED` is added to
  user-facing API documentation and CLI help text. Internally, the deprecated feature
  added to `DEFAULT_ALLOW_DEPRECATED`. The feature remains available to end-users
  without additional action on their part.
- After 3 minor-version releases, or 1 major-version release, the deprecated feature
  is removed from `DEFAULT_ALLOW_DEPRECATED` and added to `DEFAULT_DENY_DEPRECATED`.
  After this point, the user must explicitly enable the feature using an
  `-allowdeprecated` zcash.conf or CLI parameter in order for the feature to remain
  available to them.
- After at least 2 major-version releases, the deprecated feature may be removed entirely. 

