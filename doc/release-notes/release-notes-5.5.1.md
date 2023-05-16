Notable changes
===============

Fixes
-----

Fixes an issue that could cause a node to crash if the privacy policy does not
include `AllowRevealedRecipients` when attempting to create a transaction that
results in transparent change. See
[#6662](https://github.com/zcash/zcash/pull/6662) for details.

Also corrects an underestimate of the [ZIP 317](https://zips.z.cash/zip-0317)
conventional fee when spending UTXOs, which can result in mining of the
transaction being delayed or blocked. See
[#6660](https://github.com/zcash/zcash/pull/6660) for details.

Changelog
=========

Greg Pfeil (10):
      Add a test for WalletTxBuilder with legacy account
      Handle errors when getting change addr for account
      Test that WalletTxBuilder uses correct ZIP 317 fee
      Correct fee calculation for vin in tx creation
      Calculate consensusBranchId sooner
      Extract fee check from test
      Delay postponed dependencies
      Add release notes for v5.5.1
      make-release.py: Versioning changes for 5.5.1.
      make-release.py: Updated manpages for 5.5.1.

