# Design

Zcash was originally a fork of Bitcoin 0.11.2, and as such the `zcashd` node architecture
is very similar to `bitcoind`. There are however several differences, most notably the
addition of shielded pools to the consensus logic and full node state.

In this section of the book, we describe the overall architecture that we inherit from
`bitcoind`, the changes we have made to the inherited components, and the new components
we have introduced.
