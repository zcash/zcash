# P2P data propagation

This page contains notes about how block and transaction data is tracked and propagated by
`zcashd`. Most of this behaviour is inherited from Bitcoin Core, but some differences have
developed.

Some of this content is duplicated from in-code comments, but assembling this summary in
one place is generally useful for understanding the overall dynamic :)

## `recentRejects`

When a transaction is rejected by `AcceptToMemoryPool`, the transaction is added to the
`recentRejects` Bloom filter, so that we don't process it again. The Bloom filter resets
whenever the chain tip changes, as previously invalid transactions might become valid.

To prevent DoS attacks against wallets submitting transactions, `recentRejects` needs to
store a commitment to the entire transaction. This ensures that if a transaction is
malleated by a network peer to invalidate its authorizing data, the node will ignore
future advertisements of that specific transaction, but still request alternative versions
of the same txid (which might have valid authorizing data).

- For pre-v5 transactions, the txid commits to the entire transaction, and the wtxid is
  the txid with a globally-fixed (all-ones) suffix.
- For v5+ transactions, the wtxid commits to the entire transaction.

## `mapOrphanTransactions`

Upstream uses this map to store transactions that are rejected by `AcceptToMemoryPool`
because the node doesn't have their transparent inputs. `zcashd` inherits this behaviour
but limits it to purely-transparent transactions (that is, if a transaction contains any
shielded components, the node rejects it as invalid and adds it to `recentRejects`).

`mapOrphanTransactions` indexes transactions by txid. This means that if an orphan
transaction is received (spending transparent UTXOs the node does not know about), and it
also happens to be invalid for other reasons (subsequent `AcceptToMemoryPool` rules that
haven't yet been checked), then the node will not request any v5+ transactions with the
same txid but different authorizing data. This does not create a DoS problem for wallets,
because an adversary that manipulated an orphan transaction to be invalid under the above
constraints would also need to prevent the orphan's parent from entering the mempool, and
eventually a parent is reached that is not an orphan. Once the orphan's direct parent is
accepted, the orphan is re-evaluated, and if it had been manipulated to be invalid, its
wtxid is added to `recentRejects` while its txid is removed from `mapOrphanTransactions`,
enabling the wallet to rebroadcast the unmodified transaction.
