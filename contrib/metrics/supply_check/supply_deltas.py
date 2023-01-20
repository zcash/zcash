import pprint

class SupplyDeltas:
    def __init__(self, zcashd, fr_addrs, miner_deltas, flush_interval = 500):
        self.zcashd = zcashd
        self.fr_addrs = fr_addrs
        self.miner_deltas = miner_deltas

        self.delta_count = 0
        self.delta_total = 0
        self.delta_cache = []
        self.flush_interval = flush_interval
        
        deltas_flat = [pair for deltas in miner_deltas.values() for pair in deltas]
        for (deltaHeight, delta) in sorted(deltas_flat):
            self.AddSupplyDelta(deltaHeight, delta)

    def AddSupplyDelta(self, deltaHeight, delta):
        assert len(self.delta_cache) == 0 or deltaHeight > self.delta_cache[-1][0]

        self.delta_count += 1
        self.delta_total += delta
        self.delta_cache.append((deltaHeight, self.delta_total))
        
    def DeviationAtHeight(self, height):
        # search for the cached entry at the maximum deltaHeight <= height
        def binary_search(start, end, max_found):
            if start >= end:
                if max_found:
                    return max_found[1]
                else:
                    return 0
            else:
                mid = (start + end) // 2
                item = self.delta_cache[mid]
                if item[0] <= height:
                    return binary_search(mid+1, end, item)
                else:
                    return binary_search(start, mid, max_found)

        return binary_search(0, len(self.delta_cache), None)
    
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
                if self.delta_count % 500 == 0:
                    with open("delta_cache.{}.out".format(self.delta_count), 'w', encoding="utf8") as f:
                        pprint.pprint(self.miner_deltas, stream = f, indent = 4)

                return True

        pprint.pprint(coinbase_tx['vout'], indent = 4)
        return False

    def PrintDeltas(self):
        with open("delta_cache.out", 'w', encoding="utf8") as f:
            pprint.pprint(self.miner_deltas, stream = f, indent = 4)
