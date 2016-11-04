DeepWebCash 1.0.1

What is DeepWebCash?
--------------

[DeepWebCash](https://dw.cash/) is an implementation of the "Zerocash" protocol.
Based on Bitcoin's code, it intends to offer a far higher standard of privacy
through a sophisticated zero-knowledge proving scheme that preserves
confidentiality of transaction metadata. Technical details are available
in our [Protocol Specification](https://github.com/deepwebcash/zips/raw/master/protocol/protocol.pdf).


This software is the DeepWebCash client. It downloads and stores the entire history
of DeepWebCash transactions; depending on the speed of your computer and network
connection, the synchronization process could take a day or more once the
block chain has reached a significant size.

Security Warnings
-----------------

See important security warnings in
[doc/security-warnings.md](doc/security-warnings.md).

**DeepWebCash is unfinished and highly experimental.** Use at your own risk.

Where do I begin?
-----------------
We have a guide for joining the main DeepWebCash network:
https://github.com/deepwebcash/deepwebcash/wiki/1.0-User-Guide

### Need Help?

* See the documentation at the [DeepWebCash Wiki](https://github.com/deepwebcash/deepwebcash/wiki)
  for help and more information.
* Ask for help on the [DeepWebCash](https://forum.dw.cash/) forum.

Participation in the DeepWebCash project is subject to a
[Code of Conduct](code_of_conduct.md).

Building
--------

Build DeepWebCash along with most dependencies from source by running
./zcutil/build.sh. Currently only Linux is officially supported.

License
-------

For license information see the file [COPYING](COPYING).
