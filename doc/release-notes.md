(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Wallet Performance Improvements
-------------------------------

`zcashd` now reports the following new metrics when `-prometheusport` is set:

- (counter) `zcashd.wallet.batchscanner.outputs.scanned`
- (gauge) `zcashd.wallet.batchscanner.size.transactions`
- (gauge) `zcashd.wallet.batchscanner.usage.bytes`
