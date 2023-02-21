(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

This hotfix remediates memory exhaustion vulnerabilities that zcashd inherited
as a fork of bitcoind. These bugs could allow an attacker to use peer-to-peer
messages to fill the memory of a node, resulting in a crash.

