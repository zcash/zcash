This page is copyright The Electric Coin Company, 2019. It is posted in order to conform to this standard: https://github.com/RD-Crypto-Spec/Responsible-Disclosure/tree/d47a5a3dafa5942c8849a93441745fdd186731e6

# Security Disclosures
## Receiving Disclosures

The Electric Coin Company is committed to working with researchers who submit security vulnerability notifications to us to resolve those issues on an appropriate timeline and perform a coordinated release, giving credit to the reporter if they would like.

Please submit issues to security@z.cash, using the following PGP key:

```
-----BEGIN PGP PUBLIC KEY BLOCK-----

mQENBFaegcoBCAC+G82ZBYYm1GoVn4dKa0WiLYD/Q+BuU89PS1X7A4eOOy8g9yS4
wJKMzB0AxFsH/t85P7pPZwHw3i2gmiJKeIqEGhEBL08D3id2u6ZyCnwDuWs0i6My
MXWTwK5shvE61ZI/KPbjemoOG6MPF5QdrouNqei2Vk+4RjbRCyyS0A59GQi2dNZX
BMwTnHnUZ5qi6T0RFelqJ3dE5Nc/UwJPdAcg71c3b3dMOHjaDBMPB6+fTLBeidV6
5B72nGO3eIYkMUNj+qCQmM/esRkmGmlDH/9WGMBOKCq7Yw3LyEoPOi5cba1m8SN2
xFlNzkUGrlVrwZMF+1UdjvN7BGDypA3Dr/STABEBAAG0JVpjYXNoIFNlY3VyaXR5
IFRlYW0gPHNlY3VyaXR5QHouY2FzaD6JATcEEwEIACEFAlaegcoCGwMFCwkIBwIG
FQgJCgsCBBYCAwECHgECF4AACgkQiPuLhti1poxlRAf/bZ6fhUby5bAbViAO4TzQ
yfbD0ksGeF8MHicPz7HqOYuXAE9GrKnVAOFptwRo94O+iRC5aXhW8OAP+38IWorv
gsAuag7Y8k0nlfNdrJRqqJpjyxtiuv+cd2o5dre8E9PVNE5IPv9qEJA4Zag3snmC
a+O4HAqKeXYddunFq2drLkTRlOwuFkGXJzwi3VSNVYCuuGyezFDuaD45ltmsXgid
jZSdnnc6L1BrEd9LVzahvFV0+fT4bNKQHQDk+f0RTnHed+m9NqAoC9K8ftPTQ/i4
5+W/dXJATztWDrJ7ZevHXGR+RAhMNsT1psvnQsJzMkUz1GMdQOtk4PfuZLGSIiTM
ErkBDQRWnoHKAQgAp+w+xsPJWFdadE6Ok1aZC0Lk1J9xU/cqX1aBlkwi5SynwOkV
Eg1xNHLJMelp13bgDjLRsvaMbsseaCVk3goNln4atNbZpqz6FoM/f8pJx52LFD0j
CFFOVUlGEF0h+KdFr+3JHI+mg+3ifXTD4Dajj4lpu8kR/FQjftcxyttByz01wLRO
sK5BDC856WzHXAJuX6TpX4sGJujzKoLXR5V0SkUopqn9g4aJGnWuNh4kyOQI6fd7
YZyPZhWDrXdgInCKAKAgq8r6hgSDMYFvmflp6+reCfeOe1VFF8q3Foio02YPIrQW
WjjH0w6nOvOKCEtxistz1sP6ZoYq4gR41LOwIQARAQABiQEfBBgBCAAJBQJWnoHK
AhsMAAoJEIj7i4bYtaaMiA4IALIy4xP/Btu86yT3b53t8cfYZddFSO8Nlg+y3EMu
1LchdSOpWgpXCvQ7d4ndWGsuBSmQ+jaRwU2UIkq2iIxf5cg63dJz9grAcF6MXCrO
t5BSowFC4m3RFaEaG6G6SjDVIA0ZEdEMFd9Gzc1ikqbVLyNuJXKmzz0evvbAJgVO
D0nht5ifwLjQxM4olvYHUwtT0wPhniH69ghFo8LQiMgncjaukDzbgANiuj07QYy/
DlzhQUsp1qZvqZnVKqUJ3lFb86b6zoqoRNiUnvP9JB2v3kLG0T39UlcXUFnZJ/4H
CDHCrwSovQRMHtoOWZijBNobO2y1d0FkUpzNlNw44Ssw0Vo=
=6GYS
-----END PGP PUBLIC KEY BLOCK-----
```

## Sending Disclosures

In the case where we become aware of security issues affecting other projects that has never affected Zcash, our intention is to inform those projects of security issues on a best effort basis.

In the case where we fix a security issue in Zcash that also affects the following neighboring projects, our intention is to engage in responsible disclosures with them as described in https://github.com/RD-Crypto-Spec/Responsible-Disclosure, subject to the deviations described in the section at the bottom of this document.

## Bilateral Responsible Disclosure Agreements

We have set up agreements with the following neighboring projects to share vulnerability information, subject to the deviations described in the next section.

Specifically, we have agreed to engage in responsible disclosures for security issues affecting Zcash technology with the following contacts:

- Horizen security@horizen.com via PGP
- Komodo ca333@komodoplatform.com via PGP
- BitcoinABC https://github.com/Bitcoin-ABC/bitcoin-abc/blob/master/DISCLOSURE_POLICY.md

## Deviations from the Standard

Zcash is a technology that provides strong privacy. Notes are encrypted to their destination, and then the monetary base is kept via zero-knowledge proofs intended to only be creatable by the real holder of Zcash. If this fails, and a counterfeiting bug results, that counterfeiting bug might be exploited without any way for blockchain analyzers to identify the perpetrator or which data in the blockchain has been used to exploit the bug. Rollbacks before that point, such as have been executed in some other projects in such cases, are therefore impossible.

The standard describes reporters of vulnerabilities including full details of an issue, in order to reproduce it. This is necessary for instance in the case of an external researcher both demonstrating and proving that there really is a security issue, and that security issue really has the impact that they say it has - allowing the development team to accurately prioritize and resolve the issue.

In the case of a counterfeiting bug, however, just like in CVE-2019-7167, we might decide not to include those details with our reports to partners ahead of coordinated release, so long as we are sure that they are vulnerable.


