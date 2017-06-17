(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Signature validation using libsecp256k1
---------------------------------------

ECDSA signatures inside Zcash transactions now use validation using
[https://github.com/bitcoin/secp256k1](libsecp256k1) instead of OpenSSL.

Depending on the platform, this means a significant speedup for raw signature
validation speed. The advantage is largest on x86_64, where validation is over
five times faster. In practice, this translates to a raw reindexing and new
block validation times that are less than half of what it was before.

Libsecp256k1 has undergone very extensive testing and validation upstream.

A side effect of this change is that libconsensus no longer depends on OpenSSL.
