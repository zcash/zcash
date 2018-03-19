(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

`-mempooltxinputlimit` deprecation
----------------------------------

The configuration option `-mempooltxinputlimit` was added in release 1.0.10 as a
short-term fix for the quadratic hashing problem inherited from Bitcoin. At the
time, transactions with many inputs were causing performance issues for miners.
Since then, several performance improvements have been merged from the Bitcoin
Core codebase that significantly reduce these issues.

The Overwinter network upgrade includes changes that solve the quadratic hashing
problem, and so `-mempooltxinputlimit` will no longer be needed - a transaction
with 1000 inputs will take just as long to validate as 10 transactions with 100
inputs each. Starting from this release, `-mempooltxinputlimit` will be enforced
before the Overwinter activation height is reached, but will be ignored once
Overwinter activates. The option will be removed entirely in a future release
after Overwinter has activated.
