(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

RPC Changes
-----------

- `getbalance` will no longer include transparent value associated with
  transparent receivers of unified addresses belonging to the wallet as part
  of its returned balance; instead, it will only return the balance associated
  with legacy transparent addresses. The inclusion of transparent balance for
  unified address receivers in this result was a bug inadvertently introduced
  in zcashd 4.7.0. See [issue #5941](https://github.com/zcash/zcash/issues/5941)
  for additional detail.
