This page is copyright The Zcash Developers, 2026. It is posted in order to
conform to this standard: https://github.com/RD-Crypto-Spec/Responsible-Disclosure/tree/d47a5a3dafa5942c8849a93441745fdd186731e6

# Security Disclosures

## Receiving Disclosures

The Zcash maintainers are committed to working with researchers who submit
security vulnerability notifications to us to resolve those issues on an
appropriate timeline and perform a coordinated release, giving credit to the
reporter if they would like.

Please submit issues via Signal. We have no email address to report security
issues; email is not suitable for this purpose for both reliability and
security reasons (even if encryption is used).

Please create a Signal group with the following users to report a security
issue. Do not reuse a previous group for a new issue.

```
dairaemma.31
nuttycom.01
pilizcash.01
```

## Sending Disclosures

In the case where we become aware of security issues affecting other projects
that has never affected Zcash, our intention is to inform those projects of
security issues on a best effort basis.

In the case where we fix a security issue in Zcash that also affects the
following neighboring projects, our intention is to engage in responsible
disclosures with them as described in
https://github.com/RD-Crypto-Spec/Responsible-Disclosure, subject to the
deviations described in the section at the bottom of this document.

## Bilateral Responsible Disclosure Agreements

We have set up agreements with the following neighboring projects to share
vulnerability information, subject to the deviations described in the next
section.

Specifically, we have agreed to engage in responsible disclosures for security
issues affecting this repository with the following contacts:

- Zcash Foundation https://github.com/ZcashFoundation/zebra/security/policy
- Horizen security@horizen.io https://github.com/HorizenOfficial/zen/blob/master/SECURITY.md
- Komodo ca333@komodoplatform.com via PGP
- BitcoinABC https://github.com/Bitcoin-ABC/bitcoin-abc/blob/master/DISCLOSURE_POLICY.md

## Deviations from the Standard

Zcash is a technology that provides strong privacy. Notes are encrypted to
their destination, and then the monetary base is kept via zero-knowledge proofs
intended to only be creatable by the real holder of Zcash. If this fails, and a
counterfeiting bug results, that counterfeiting bug might be exploited without
any way for blockchain analyzers to identify the perpetrator or which data in
the blockchain has been used to exploit the bug. Rollbacks before that point,
such as have been executed in some other projects in such cases, are therefore
impossible.

The standard describes reporters of vulnerabilities including full details of
an issue, in order to reproduce it. This is necessary for instance in the case
of an external researcher both demonstrating and proving that there really is a
security issue, and that security issue really has the impact that they say it
has - allowing the development team to accurately prioritize and resolve the
issue.

In the case of a counterfeiting bug, however, just like in CVE-2019-7167, we
might decide not to include those details with our reports to partners ahead of
coordinated release, so long as we are sure that they are vulnerable.
