(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

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
