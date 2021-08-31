# "Coins" view

TBD

## Notes

- This is the main context in which `CTxOut::IsNull()` is used. The other is a
  single spot in the mempool code. Once we've backported the
  [per-txout CoinsDB](https://github.com/bitcoin/bitcoin/pull/10195) we can
  hopefully eliminate this method.
