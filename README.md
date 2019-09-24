Ycash v2.0.7
<img align="right" width="120" height="80" src="https://www.ycash.xyz/y_sign.png">

What is Ycash?
--------------

[Ycash](https://www.ycash.xyz) is a chain fork of [Zcash](https://z.cash/).

This software is the Ycash client. It downloads and stores the entire history
of Ycash transactions; depending on the speed of your computer and network
connection, the synchronization process could take a day or more once the
blockchain has reached a significant size.

#### :lock: Security Warnings

(Coming soon)

**Ycash is experimental and a work-in-progress.** Use at your own risk.

####  :ledger: Deprecation Policy

This release is considered deprecated 16 weeks after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this 16-week period. The automatic feature is based on block
height.

## Getting Started

For most topics, you can rely on Zcash-related documentation. Supplemental Ycash-specific documentation will be hosted here:

http://www.ycash.xyz/docs

Refer also to the [Zcash user guide](https://zcash.readthedocs.io/en/latest/rtd_pages/rtd_docs/user_guide.html).

### Need Help?

* :blue_book: See the Zcash documentation at the [ReadtheDocs](https://zcash.readthedocs.io)
  for help and more information.
* :incoming_envelope: Ask for help on the [Ycash Discord](https://discord.gg/Yz8rW7P).

Participation in the Ycash project is subject to a
[Code of Conduct](code_of_conduct.md).

### Building

Build Ycash along with most dependencies from source by running:

```
./zcutil/build.sh -j$(nproc)
```

Currently, Zcash is only officially supported on Debian and Ubuntu.

License
-------

For license information see the file [COPYING](COPYING).
