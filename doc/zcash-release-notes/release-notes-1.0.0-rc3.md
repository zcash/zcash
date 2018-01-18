Chirag Davé (1):
      fReopenDebugLog and fRequestShutdown should be type sig_atomic_t

Daira Hopwood (9):
      Refactor README docs to avoid duplication.
      Fix licensing to comply with OpenSSL and Berkeley DB licenses.
      Changes to upgrade bdb to 6.2.23
      util: Update tinyformat
      Tweak descriptions of mining parameters for example zcash.conf.
      Update dnsseeds for mainnet. closes #1369
      Minor update to release process.
      Remove the override of nMaxTipAge that effectively disables it on testnet.
      Update version numbers for rc3.

Jack Grigg (2):
      Disable metrics screen in performance-measurements.sh
      Link to #826 in doc/security-warnings.md, link to new Security website page

Joe Turgeon (2):
      Fixing floating point exception caused by metrics. Using default column width unless in a TTY.
      Adding handling for ioctl failure. Updates from code review in PR #1615.

Kevin Gallagher (2):
      Prefer sha256sum but fall back to shasum if not available
      Adds libgomp1 to Debian package depends

Louis Nyffenegger (1):
      Fix typo in README.md

Paige Peterson (3):
      add zcash.config
      fix per Jack's mod suggestions
      fix per Daira's suggestions

Pieter Wuille (3):
      Include signal.h for sig_atomic_t in WIN32
      Revert "Include signal.h for sig_atomic_t in WIN32"
      Use std::atomic for fRequestShutdown and fReopenDebugLog

Sean Bowe (1):
      Add manpages for zcashd and zcash-cli binaries for debian.

Simon (4):
      Fix incorrect error message in z_sendmany
      Add z_sendmany rule that when sending coinbase utxos to a zaddr they must be consumed entirely, without any change, since there is currently no way to specify a change address in z_sendmany.
      Add assert to AsyncRPCOperation_sendmany
      Bump version number in sendalert.cpp

bitcartel (1):
      Update payment-api.md

