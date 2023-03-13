Notable changes
===============

This hotfix remediates memory exhaustion vulnerabilities that zcashd inherited
as a fork of bitcoind. These bugs could allow an attacker to use peer-to-peer
messages to fill the memory of a node, resulting in a crash.


Changelog
=========

Daira Hopwood (3):
      Enable a CRollingBloomFilter to be reset to a state where it takes little memory.
      Ensure that CNode::{addrKnown, filterInventoryKnown} immediately take little memory when we disconnect the node.
      Improve the encapsulation of `CNode::filterInventoryKnown`.

Greg Pfeil (1):
      Remove `ResetRequestCount`

Jon Atack (1):
      p2p, rpc, test: address rate-limiting follow-ups

Kris Nuttycombe (4):
      Update release notes for v5.3.3 hotfix
      Postpone dependency updates for v5.4.2 hotfix.
      make-release.py: Versioning changes for 5.4.2.
      make-release.py: Updated manpages for 5.4.2.

Matt Corallo (1):
      Remove useless mapRequest tracking that just effects Qt display.

Pieter Wuille (3):
      Rate limit the processing of incoming addr messages
      Randomize the order of addr processing
      Add logging and addr rate limiting statistics

