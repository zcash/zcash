(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Changes
-------

This release includes a change to the behaviour of the ``getblocktemplate``
RPC, motivated by reducing orphan rates for miners. When preparing a block
template, the previous behaviour was to immediately consider a transaction
accepted into the mempool as eligible to be included in the template.
This release, by default, waits 10 seconds before considering each
transaction eligible. This allows time for other nodes to have received
and validated each transaction, which tends to reduce the latency for a
block containing it to be accepted and propagated. As a result, blocks
mined by the node are less likely to be orphaned.

The old behaviour (of immediately considering mempool transactions to be
eligible for inclusion in a block template) can be restored with the
config option `enabletxminingdelay=0`.

If a transaction is positively prioritised by the `prioritisetransaction`
RPC (i.e. with either priority or fee delta > 0), it is not subject to
this delay, regardless of the value of the `enabletxminingdelay` option.
