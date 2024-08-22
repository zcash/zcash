(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

RPC Changes
-----------

- A bug in parameter handling has been fixed for the `getaddresstxids` and
  `getaddressdeltas` RPC methods. Previously, leaving the end height unset
  would cause the start height to be set to zero and therefore ignored.
  The end height is now also automatically bounded to the chain tip; passing
  an end height greater than the chain tip height no longer results in an
  error; values up to and including the chain tip are returned instead.

Platform Support
----------------

- Debian 10 (Buster) has been removed from the list of supported platforms.
  It reached EoL on June 30th 2024, and does not satisfy our Tier 2 policy
  requirements.

Other
-----

- The `zcash-inspect` tool (which was never distributed, and was present in this
  repository for debugging purposes) has been moved to the `devtools` subfolder
  of the https://github.com/zcash/librustzcash repository.

Fixes
-----

- Security fixes for vulnerabilities disclosed in
  https://bitcoincore.org/en/2024/07/03/disclose-orphan-dos/
  and https://bitcoincore.org/en/2024/07/03/disclose-inv-buffer-blowup/ have
  been backported to `zcashd`.
