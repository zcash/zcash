(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Wallet Performance Improvements
-------------------------------

- We now parallelize and batch trial decryption of Orchard outputs.
- The `zcashd.wallet.batchscanner.outputs.scanned` metrics counter (enabled when
  `-prometheusport` is set) now includes an `orchard` kind.
