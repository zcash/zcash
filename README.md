Zcash Network Observatory v0.1
<img align="right" width="120" height="80" src="doc/imgs/logo.png">
===========

What is the Zcash Network Observatory?
--------------

[Zcash](https://z.cash/) is an implementation of the "Zerocash" protocol.
Based on Bitcoin's code, Zcash intends to offer a far higher standard of privacy
through a sophisticated zero-knowledge proving scheme that preserves
confidentiality of transaction metadata. More technical details are available
in our [Protocol Specification](https://github.com/zcash/zips/raw/master/protocol/protocol.pdf).

The Zcash Network Observatory is on open-source suite of Zcash network security tools that
enables any node operator to analyze propagation characteristics and detect phenomena such
as forks, potential selfish mining, double spending, etc.

Features include:
*  Advanced timestamp logging
*  Network performance analysis: prototype complete, results shared
*  Selfish/Stubborn mining: prototype complete
*  Possible double spend detection: prototyped for transparent, nullifier comparison in progress for shielded
*  Global performance analyses with multiple nodes: Work in progress, soliciting research sponsor(s)

This software is the Zcash client. It downloads and stores the entire history
of Zcash transactions; depending on the speed of your computer and network
connection, the synchronization process could take a day or more once the
blockchain has reached a significant size.

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
* :mag: Chat with our support community on [Rocket.Chat](https://chat.zcashcommunity.com/channel/user-support)

Participation in the Zcash project is subject to a
[Code of Conduct](code_of_conduct.md).

### Additional observatory feature flags:
| Flag | Feature |
| ---- | ------- |
| '--collecttimestamps' | enables avdanced timestamp logging into the datadir |
| '--outboundconnections=n' | force more connections made by this node, enables larger volume of data |
| '--silent' | stops outbound block and tx messages, only allows block servicing nodes to connect |

### Additional observatory RPC commands:
> NOTE: These RPC commands require the node to have '--collecttimestamps' enabled
| Command | Feature |
| ------- | ------- |
| 'listforks' | list all forks seen on the network |
| 'detectdoublespends' | find double spend attempts in forks |
| 'detectselishmining' | find selish mining attempts near forks |

### Building

Build Zcash along with most dependencies from source by running the following command:

```
./zcutil/build.sh -j$(nproc)
```

Currently, Zcash is only officially supported on Debian and Ubuntu.

License
-------

For license information see the file [COPYING](COPYING).
