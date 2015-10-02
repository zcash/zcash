Zerocash Glossary
=================

Zerocash has evolved out of Bitcoin and multiple stages of academic
research.  Additionally, there is a potential for confusion between the
protocols, the cryptographic techniques, the libraries, source code,
network, and currency units.

This document attempts to standardize our terminology to avoid confusion.
If a term is not present in this glossary, we defer to the conventional
or explicit Bitcoin definitions.

Standard Terminology
--------------------

These terms are accepted as standard by the Zerocash developers.

Proposed Terminology
--------------------

These terms are proposed, but not yet accepted as standard.

Pour transaction
: Nathan proposes we standardize this for users.  It's already standard in
  the academic publications and codebase and the metaphor seems
  unconflated with any other aspect of Bitcoin-likes that I'm aware of.
  (By contrast "mint" seems confusing and conflated, see below.)

Zerocash protocol
: Nathan proposes we standardize on the term "zerocash" referring to
  the protocol and cryptographic constructions (and maybe the network
  and standard software).  However, he wants a distinct term for the
  currency units to prevent the kind of conflation that plagues Bitcoin.

Deprecated Terminology
----------------------

These terms appeared in discussions, publications, design docs, source
code, issue tickets, etc...  However, we wish to discontinue their use
so that all parts of the system (concepts, protocols, cryptographic
techniques, implementations) are easier for new-comers to understand.

Note these terms are *accepted* as deprecated.  The next section contains
proposed deprecations.

Proposed Deprecated Terminology
-------------------------------

This section proposes deprecated terms, but the Zerocash developers have
not agreed to standardize these deprecations.

Mint transaction
: Zooko and Nathan wish to deprecate this name for two reasons: "mint" is
  conflated with changes to the monetary base and some users might
  believe that "minting" and "mining" are the same or inter-related.
  Additionally, "minting zerocash from basecoin" reinforces the notion
  that "zerocash" and "basecoin" are distinct currencies or assets,
  but we wish to reinforce the notion that there is only a single asset
  within the protocol/network.

basecoin
: Zooko and Nathan wish to deprecate the notion that there are two
  distinct "kinds of units" / currencies / assets within the network. What
  are currently referred to as "basecoins", "basecoin transactions",
  etc... can instead be called "public balances", "public transactions".
  Meanwhile instead of calling balances within zerocash commitments
  "Zerocash (as distinct from basecoin)" we can simply call these
  "private balances" and "privacy protected transactions".

zerocoin
: Nathan wishes to deprecate this term which is conflated amongst
  an older set of cryptographic constructions and protocol, an older
  codebase, a proposal to extend Bitcoin, units of account within the
  Zerocash codebase, etc...  Nathan still commonly hears most people refer
  to units of account within the Zerocash design as "zerocoins" because
  saying "zerocashes" or "units of zerocash" is awkward or cumbersome.
  Nathan wants to find a new unused term for the units of account.
  Maybe we should select the minimal unit as some variation on Bitcoin's
  "satoshi" plus SI unit prefixes (ie: "megazatoshi" or somesuch).
  Also having a different name for the units, versus the protocol/crypto,
  versus the software would prevent a lot of confusion that surrounds
  Bitcoin.  (How many times have you read "bitcoin is *two* things:
  a network *and* a currency"?)
