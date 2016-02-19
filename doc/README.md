Zcash Core 0.11.2
=====================

Setup
---------------------
[Zcash Core](https://z.cash/) is the original Zcash client and it builds the backbone of the network.

Running
---------------------
The following are some helpful notes on how to run Zcash on your native platform.

### Unix

You need the Qt4 run-time libraries to run Zcash-Qt. On Debian or Ubuntu:

	sudo apt-get install libqtgui4

Unpack the files into a directory and run:

- bin/32/zcash-qt (GUI, 32-bit) or bin/32/zcashd (headless, 32-bit)
- bin/64/zcash-qt (GUI, 64-bit) or bin/64/zcashd (headless, 64-bit)



### Windows

Unpack the files into a directory, and then run zcash-qt.exe.

### OS X

Drag Zcash-Qt to your applications folder, and then run Zcash-Qt.

### Need Help?

* See the documentation at the [Zcash Wiki](https://github.com/Electric-Coin-Company/zcash/wiki)
for help and more information.
* Ask for help on [#zcash](https://webchat.oftc.net/?channels=zcash) on OFTC. If you don't have an IRC client use [webchat here](https://webchat.oftc.net/?channels=zcash).
* Ask for help on the [Zcash](https://forum.z.cash/) forums.

Building
---------------------
The following are developer notes on how to build Zcash on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [OS X Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
The Zcash repo's [root README](https://github.com/Electric-Coin-Company/zcash/blob/zc.v0.11.2.latest/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Multiwallet Qt Development](multiwallet-qt.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [Unit Tests](unit-tests.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)

### Resources
* Discuss on the [Zcash](https://forum.z.cash/) forums.
* Discuss on [#zcash](https://webchat.oftc.net/?channels=zcash) on OFTC. If you don't have an IRC client use [webchat here](https://webchat.oftc.net/?channels=zcash).

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)

License
---------------------
Distributed under the [MIT software license](http://www.opensource.org/licenses/mit-license.php).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
