(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Debian Stretch is now a Supported Platform
------------------------------------------

We now provide reproducible builds for Stretch as well as for Jessie.


Fixed a bug in ``z_mergetoaddress``
-----------------------------------

We fixed a bug which prevented users sending from ``ANY_SPROUT`` or ``ANY_SAPLING``
with ``z_mergetoaddress`` when a wallet contained both Sprout and Sapling notes.


Insight Explorer
----------------

We have been incorporating changes to support the Insight explorer directly from
``zcashd``. v2.0.6 includes the first change to an RPC method. If ``zcashd`` is
run with the flag ``--insightexplorer``` (this requires an index rebuild), the
RPC method ``getrawtransaction`` will now return additional information about
spend indices.
