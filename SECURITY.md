This page is copyright The Zcash Developers, 2026. It is posted in order to
conform to this standard: https://github.com/RD-Crypto-Spec/Responsible-Disclosure/tree/d47a5a3dafa5942c8849a93441745fdd186731e6

# Security Disclosures

## Vulnerability Categorization

The Zcash maintainers have defined the following categories for
vulnerabilities. This vulnerability scoring is used by the maintainers to
prioritize how urgently each potential vulnerability needs to be fixed; it's
not a metric to be gamed for payouts from any bounty program.

### Critical

* Violations of the monetary base
* Violations of consensus rules that result in consensus-invalid transactions
  being accepted by network full nodes.
* Remotely-exploitable vulnerabilities that may result in individual user loss
  of funds or compromise of their devices without user interaction. Think on
  the order of vulnerabilities that might result in remote code execution or
  remote key extraction.
* Vulnerabilities that could result in network-wide deanonyimization or
  exposure of the shielded transaction graph.

### High

* All other forms of remotely-triggered consensus divergence or network split.
* DoS vulnerabilities against full node or light client server software that
  enable sustained service denial, unbounded resource consumption, or that
  bypass existing peer-management protections (e.g. `Misbehaving`-based
  banning).
* Other vulnerabilities that might result in remotely-triggered individual user
  loss of funds. This does not include vulnerabilities resulting from careless
  or intentional API misuse by wallet software.

### Moderate

* Other vulnerabilities that might result in individual user loss of funds.

### Low

* Individual user wallet DoS.
* Bounded resource-consumption amplifications against full nodes where total
  per-attacker impact is capped by existing peer-management protections.

### Not Vulnerability

The following categories of problems are explicitly not considered vulnerabilities.

* Bugs that may cause temporary unavailability of funds which can be resolved,
  in the limit, by a wallet re-scan or importing keys into another wallet.
* Behavior that is explicitly or implicitly acknowledged in the
  [light wallet threat model](https://zcash.readthedocs.io/en/latest/rtd_pages/wallet_threat_model.html).
  Light wallet servers are considered explicitly trusted for the correctness of
  the data that they return to users, and misbehavior arising from processing
  maliciously-crafted compact block or transaction data returned from light wallet
  servers is not considered a vulnerability unless its processing results
  in a compromise that risks loss of funds or deanonyimization beyond what
  is acknowledged by the light wallet threat model.

## Receiving Disclosures

We have no email address to report security issues; email is not suitable for
this purpose for both reliability and security reasons (even if encryption is
used).

For critical vulnerabilities, please create a Signal group with the following
users to report a security issue. Do not reuse a previous group for a new
issue.

```
dairaemma.31
pilizcash.01
nuttycom.01
```

For all other vulnerabilities, please use the GitHub "Report a Vulnerability"
feature, available at https://github.com/zcash/zcash/security/advisories.

WARNING: The Zcash maintainers are currently experiencing a large number of
spurious or low-severity issues being incorrectly reported as critical
vulnerabilities. Overstating the severity of any reported vulnerability,
according to the rubric presented above, MAY MAKE THE REPORTER INELIGIBLE FOR
COMPENSATION UNDER ANY BUG BOUNTY PROGRAM.

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
