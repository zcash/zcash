(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Wallet Performance Improvements
-------------------------------

`zcashd 5.2.0` improved the performance of wallet scanning with multithreaded
batched trial decryption of Sapling outputs. However, for some nodes this
resulted in growing memory usage that would eventually cause an OOM abort. We
have identified the cause of the growth, and made significant improvements to
reduce the memory usage of the batch scanner. In addition, the batch scanner now
has a memory limit of 100 MiB.

`zcashd` now reports the following new metrics when `-prometheusport` is set:

- (counter) `zcashd.wallet.batchscanner.outputs.scanned`
- (gauge) `zcashd.wallet.batchscanner.size.transactions`
- (gauge) `zcashd.wallet.batchscanner.usage.bytes`
