Notable changes
===============

Security Fixes
--------------

This release addresses CVE-2024-52911, a critical use-after-free
vulnerability in `zcashd`'s block validation. Any inbound peer can
trigger the bug by sending a crafted invalid block, causing the
receiving node to access freed memory during script verification.
The most likely impact is a single-node crash; the bug is reachable
on every multi-core node running with the default `-par` setting,
and bouncing invalid blocks between peers can amplify the impact to
a network-wide outage. Remote code execution cannot be conclusively
ruled out, but is considered unlikely given the constrained shape
of the freed data the workers read.

The root cause is a stack-object lifetime mismatch in `ConnectBlock`
(`src/main.cpp`): a `CCheckQueueControl<CScriptCheck>` was declared
before the `std::vector<PrecomputedTransactionData>` it depends on,
so on early-return paths between the queue's `Add` and the explicit
`Wait()` the vector destructed first while script-verify worker
threads were still dereferencing pointers into it. The fix moves
the vector declaration above the queue control, so LIFO destruction
of automatic objects now guarantees that `~CCheckQueueControl`
joins all worker threads before the vector is destroyed.

Nodes running with `-par=1` (single-thread script verification) are
not affected. All other configurations — including the default on
multi-core hosts — are vulnerable on v6.12.2 and earlier.

This is the same root-cause shape as Bitcoin Core's CVE-2024-52911,
covertly mitigated upstream in
[bitcoin/bitcoin#31112](https://github.com/bitcoin/bitcoin/pull/31112)
(merged 2024-12-03) and cleanly fixed in
[bitcoin/bitcoin#35209](https://github.com/bitcoin/bitcoin/pull/35209)
(merged 2026-05-06; publicly disclosed 2026-05-05). `zcashd` forked
from Bitcoin Core circa 2018 and never received either fix until
this release. The patch in this release is equivalent in shape to
the upstream cleanup in #35209.

The vulnerability was reported under the
[ZCG Security Vulnerability Disclosure Initiative](https://forum.zcashcommunity.com/t/zcg-security-vulnerability-disclosure-initiative/55545)
by `fivelittleducks`. Operators are urged to upgrade promptly.

Changelog
=========

Kris Nuttycombe (3):
      ConnectBlock: fix CVE-2024-52911 use-after-free in script-verify path
      make-release.py: Versioning changes for 6.12.3.
      make-release.py: Updated manpages for 6.12.3.

