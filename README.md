SodaToken 1.0.14
=============

What is SodaToken?
--------------

[SodaToken](https://sodatoken.org/) is an implementation of the "Zerocash" protocol.
Based on Bitcoin's code, it intends to offer a far higher standard of privacy
through a sophisticated zero-knowledge proving scheme that preserves
confidentiality of transaction metadata. Technical details are available
in our [Protocol Specification](https://github.com/sodatoken/zips/raw/master/protocol/protocol.pdf).

This software is the SodaToken client. It downloads and stores the entire history
of SodaToken transactions; depending on the speed of your computer and network
connection, the synchronization process could take a day or more once the
blockchain has reached a significant size.

Security Warnings
-----------------

See important security warnings on the
[Security Information page](https://sodatoken.org/support/security/).

**SodaToken is experimental and a work-in-progress.** Use at your own risk.

Deprecation Policy
------------------

This release is considered deprecated 16 weeks after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this 16 week time period. The automatic feature is based on block
height and can be explicitly disabled.

Where do I begin?
-----------------
We have a guide for joining the main SodaToken network:
https://github.com/sodatoken/sodatoken/BinaryOmen/sodatoken2/wiki/1.0-User-Guide

### Need Help?

* See the documentation at the [SodaToken Wiki](https://github.com/sodatoken/sodatoken/BinaryOmen/sodatoken2/wiki)
  for help and more information.
* Ask for help on the [SodaToken](https://forum.sodatoken.org/) forum.

Participation in the SodaToken project is subject to a
[Code of Conduct](code_of_conduct.md).

Building
--------

Build SodaToken along with most dependencies from source by running
./zcutil/build.sh. Currently only Linux is officially supported.

License
-------

For license information see the file [COPYING](COPYING).
Updated on January 26th 2018 R0otChiXor
-------
