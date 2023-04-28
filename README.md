Zcash 5.5.0
<img align="right" width="120" height="80" src="doc/imgs/logo.png">
===========

What is Zcash?
--------------

[Zcash](https://z.cash/) is an implementation of the "Zerocash" protocol.
Initially based on Bitcoin's design, Zcash intends to offer a far
higher standard of privacy through a sophisticated zero-knowledge
proving scheme that preserves confidentiality of transaction
metadata. More technical details are available in our [Protocol
Specification](https://zips.z.cash/protocol/protocol.pdf).

## The `zcashd` Full Node

This repository hosts the `zcashd` software, a Zcash consensus node
implementation. It downloads and stores the entire history of Zcash
transactions. Depending on the speed of your computer and network
connection, the synchronization process could take several days.

<p align="center">
  <img src="doc/imgs/zcashd_screen.gif" height="500">
</p>

The `zcashd` code is derived from a source fork of
[Bitcoin Core](https://github.com/bitcoin/bitcoin). The code was forked
initially from Bitcoin Core v0.11.2, and the two codebases have diverged
substantially.

#### :lock: Security Warnings

See important security warnings on the
[Security Information page](https://z.cash/support/security/).

**Zcash is experimental and a work in progress.** Use it at your own risk.

####  :ledger: Deprecation Policy

This release is considered deprecated 16 weeks after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this 16-week period. The automatic feature is based on block
height.

## Other Zcash Implementations

The [Zebra](https://github.com/ZcashFoundation/zebra) project offers a
different Zcash consensus node implementation, written largely from the
ground up.

## Getting Started

Please see our [user
guide](https://zcash.readthedocs.io/en/latest/rtd_pages/rtd_docs/user_guide.html)
for instructions on joining the main Zcash network.

### Need Help?

* :blue_book: See the documentation at the [ReadTheDocs](https://zcash.readthedocs.io)
  for help and more information.
* :incoming_envelope: Ask for help on the [Zcash](https://forum.z.cash/) forum.
* :speech_balloon: Join our community on [Discord](https://discordapp.com/invite/PhJY6Pm)

Participation in the Zcash project is subject to a
[Code of Conduct](code_of_conduct.md).

### Building

Build Zcash along with most dependencies from source by running the following command:

```
./zcutil/build.sh -j$(nproc)
```

Currently, Zcash is only officially supported on Debian and Ubuntu. See the
[Debian / Ubuntu build](https://zcash.readthedocs.io/en/latest/rtd_pages/Debian-Ubuntu-build.html)
for detailed instructions.

License
-------

For license information see the file [COPYING](COPYING).
