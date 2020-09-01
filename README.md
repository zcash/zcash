Zcash 4.0.0
<img align="right" width="120" height="80" src="doc/imgs/logo.png">
===========

What is Zcash?
--------------

[Zcash](https://z.cash/) is an implementation of the "Zerocash" protocol.
Based on Bitcoin's code, Zcash intends to offer a far higher standard of privacy
through a sophisticated zero-knowledge proving scheme that preserves
confidentiality of transaction metadata. More technical details are available
in our [Protocol Specification](https://github.com/zcash/zips/raw/master/protocol/protocol.pdf).

This software is the Zcash client. It downloads and stores the entire history
of Zcash transactions; depending on the speed of your computer and network
connection, the synchronization process could take a day or more once the
blockchain has reached a significant size.

<p align="center">
  <img src="doc/imgs/zcashd_screen.gif" height="500">
</p>

#### :lock: Security Warnings

See important security warnings on the
[Security Information page](https://z.cash/support/security/).

**Zcash is experimental and a work in progress.** Use it at your own risk.

####  :ledger: Deprecation Policy

This release is considered deprecated 16 weeks after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this 16-week period. The automatic feature is based on block
height.

## Getting Started

Please see our [user guide](https://zcash.readthedocs.io/en/latest/rtd_pages/rtd_docs/user_guide.html) for joining the main Zcash network.

### Need Help?

* :blue_book: See the documentation at the [ReadTheDocs](https://zcash.readthedocs.io)
  for help and more information.
* :incoming_envelope: Ask for help on the [Zcash](https://forum.z.cash/) forum.
* :speech_balloon: Chat with our support community on [Discord](https://discordapp.com/channels/669694001464737815/671029188353851393/)

Participation in the Zcash project is subject to a
[Code of Conduct](code_of_conduct.md).

### Building

Build Zcash along with most dependencies from source by running the following command:

```
./zcutil/build.sh -j$(nproc)
```

Currently, Zcash is only officially supported on Debian and Ubuntu.

License
-------

For license information see the file [COPYING](COPYING).
