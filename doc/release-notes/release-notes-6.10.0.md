Notable changes
===============

Network Upgrade 6.1
-------------------

The mainnet activation of the NU6.1 network upgrade is supported by the 6.10.0
release, with an activation height of 3146400, which should occur on
approximately November 23, 2025. Please upgrade to this release, or any
subsequent release, in order to follow the NU6.1 network upgrade.

The following ZIPs are being deployed as part of this upgrade:

- [ZIP 271: Deferred Dev Fund Lockbox Disbursement](https://zips.z.cash/zip-0271)
- [ZIP 1016: Community and Coinholder Funding Model](https://zips.z.cash/zip-1016) (partially)

In order to help the ecosystem prepare for the mainnet activation, NU6.1 has
already been activated on the Zcash testnet. Any node version 6.3.0 or higher,
including this release, supports the NU6 activation on testnet.

Changelog
=========

Kris Nuttycombe (4):
      Postpone dependency updates for the v6.10.0 release.
      Set `RELEASE_TO_DEPRECATION_WEEKS` back to 16 weeks.
      make-release.py: Versioning changes for 6.10.0.
      make-release.py: Updated manpages for 6.10.0.

y4ssi (2):
      Update Dockerfile - entrypoint.sh
      Update release-process.md

