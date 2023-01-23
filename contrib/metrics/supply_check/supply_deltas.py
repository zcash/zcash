import pprint
import bisect

class SupplyDeltas:
    def __init__(self, fr_addrs, miner_deltas, flush_interval = 500):
        self.fr_addrs = fr_addrs
        self.miner_deltas = miner_deltas

        self.delta_total = 0
        self.delta_cache = []
        self.flush_interval = flush_interval
        
        deltas_flat = [pair for deltas in miner_deltas.values() for pair in deltas]
        for (deltaHeight, delta) in sorted(deltas_flat):
            self.AddSupplyDelta(deltaHeight, delta)

    # AddSupplyDelta must be called with heights in increasing order.
    # It will raise an assertion error if an out-of-order insertion is
    # attempted.
    def AddSupplyDelta(self, deltaHeight, delta):
        assert len(self.delta_cache) == 0 or deltaHeight > self.delta_cache[-1][0]

        self.delta_total += delta
        bisect.insort(self.delta_cache, (deltaHeight, self.delta_total), key=lambda x: x[0])
        
    def DeviationUpToHeight(self, height):
        i = bisect.bisect(self.delta_cache, height, key=lambda x: x[0])
        return 0 if i == 0 else self.delta_cache[i - 1][1]
    
    def SaveMismatch(self, block, theoretical, empirical):
        height = block['height']
        coinbase_tx = block['tx'][0]
        delta = theoretical - empirical

        print('Mismatch at height {}: {} != {} ({}) miner_deltas: {}'.format(
            height,
            theoretical,
            empirical,
            delta,
            len(self.miner_deltas),
        ))

        # if delta ever goes negative, we will halt
        if delta >= 0:
            miner_addrs = set([addr for out in coinbase_tx['vout'] for addr in out['scriptPubKey'].get('addresses', [])]) - self.fr_addrs
            if len(miner_addrs) > 0:
                self.miner_deltas.setdefault(",".join(sorted(miner_addrs)), []).append((height, delta))
                self.AddSupplyDelta(height, delta)
                if len(self.delta_cache) % 500 == 0:
                    with open("delta_cache.{}.out".format(len(self.delta_cache)), 'w', encoding="utf8") as f:
                        pprint.pprint(self.miner_deltas, stream = f, compact = True)

                return True

        pprint.pprint(coinbase_tx['vout'], indent = 4)
        return False

    def PrintDeltas(self):
        with open("delta_cache.out", 'w', encoding="utf8") as f:
            pprint.pprint(self.miner_deltas, stream = f, compact = True)
