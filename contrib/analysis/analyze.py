#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
"""Simple Transaction Analysis

This contains a class, `Analyzer`, for defining analyses of the blocks and
transactions on the blockchain. It also exposes a function
`analyze_blocks`, which handles applying multiple analyses simultaneously over
some common range of blocks.
"""

import datetime
import itertools
import os.path
from statistics import mean
import sys

sys.path.insert(
    1,
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 "../../qa/rpc-tests")
)

from test_framework.authproxy import AuthServiceProxy

### TODO: Get host/port from config
if len(sys.argv) > 1:
    node = AuthServiceProxy(sys.argv[1])
else:
    raise Exception(
        "%s needs to be provided a connection string, like \"http://user:pass@localhost:port\"."
        % sys.argv[0])

class Analyzer:
    """
    An analyzer collects a single aggregated data structure from the blockchain.

    If you had a block and a single tx from that block, you could simply
   `my_analyzer.aggregate(my_analyzer.extract(block, tx))` to generate the stats
    for that analysis. However, since we generally want to aggregate across many
    transactions in many blocks and also because we usually want to collect
    multiple statistics at once (because re-fetching blocks and tx is slow),
   `extract` and `aggregate are separated out. See `analyze_blocks` for how to
    take advantage of this structure.
    """

    def __init__(self, name, tx_filter, bucketers, extractor, cache = ((), lambda c, _: c), preCache = 0):
        """It takes various functions to apply to the transactions therein. The functions are typed as follows:

    tx_filter :: cache -> Block -> Tx -> Boolean
    bucketers :: [ ...,
                   (cache -> Block -> Tx -> k_n-2, [(k_n-1, a)] -> b),
                   (cache -> Block -> Tx -> k_n-1, [(k_n, a)] -> b),
                   (cache -> Block -> Tx -> k_n,   [v] -> a)
                 ]
    extractor :: cache -> Block -> Tx -> v
    cache :: (cache, cache -> Block -> cache)
    preCache = Natural

    `tx_filter` decides whether the given transaction should be included in the
                result,
    `extractor` reduces each transaction to the parts we care about in the
                results,
    `bucketers` is a list of pairs of functions -- the first of each pair
                produces a key for bucketing the results and the second is how
                to accumulate the values in that bucket. The list allows us to
                create buckets of buckets.
    `cache`, if provided, is a tuple of an initial cache value and a function to
             update it so that later transactions can look at information from
             previous blocks.
    `preCache` is how many blocks before the start of our range to start
               caching. This is generally a _minimum_, don't be suprised if the
               cache is updated from some much earlier point. Also, it may be
               truncated if there aren't enough blocks between the beginning of
               the chain and and the start of the range.

    If no bucketers are provided, this returns a list of all the extracted data
    in a list, one for each transaction. If there are bucketers, it returns a
    map, with the keys from the first bucketer in the list and the values from
    the first accumulator in the list.

        """
        self.name = name
        self.__filter = tx_filter
        self.__bucketers = bucketers
        self.__extractor = extractor
        (self.__cache, self.__cacheUpdater) = cache
        self.preCache = preCache
        self.__lastCachedBlock = 0

    def updateCache(self, block):
        """
        This is exposed in order to handle the "precache", where we need to
        build up the cache for blocks before the blocks we actually care to have
        in our results.
        """
        if block['height'] > self.__lastCachedBlock:
            self.__cache = self.__cacheUpdater(self.__cache, block)
            self.__lastCachedBlock = block['height']

    def extract(self, block, tx):
        """
        Extracts all the data from a given transaction (and its block) needed to
        compute the statistics for this analyzer.

        TODO: Allow a bucketer to return multiple keys. This hopefully allows
              things like sub-transaction extraction. E.g., looking at the sizes
              of all vouts by day, without caring which ones are in the same tx
        """
        self.updateCache(block)

        if self.__filter(self.__cache, block, tx):
            value = self.__extractor(self.__cache, block, tx)
            keys = [x[0](self.__cache, block, tx) for x in self.__bucketers]
            return [(keys, value)]
        else:
            return []

    def aggregate(self, kvs):
        """
        Given a `[([k_0, k_1, ..., k_n-1], v)]` (where `n` is the length of the
        bucketer list provided at initialization and `k_*` are the results of
        each bucketer), this groups and accumulates the results, returning their
        final form.
        """
        kvs.sort(key=lambda x: x[0])
        return self.__group(kvs, [x[1] for x in self.__bucketers])

    def __group(self, kvs, accumulators):
        if accumulators:
            buck = []
            accum, *remaining_accum = accumulators
            for k, g in itertools.groupby(kvs, lambda x: x[0].pop(0)):
                buck.append((k, accum(self.__group(list(g), remaining_accum))))
            return buck
        else:
            return [x[1] for x in kvs]

def analyze_blocks(block_range, analyzers):
    """
    This function executes multiple analyzers over a common range of blocks,
    returning results keyed by the name of the analyzer.
    """
    current_height = node.getblockchaininfo()['estimatedheight']
    bounded_range = range(
        max(0, min(current_height, block_range[0])),
        max(0, min(current_height, block_range[1]))
    )
    longest_precache = max([x.preCache for x in analyzers])
    data_start = bounded_range[0]
    for i in range(max(0, data_start - longest_precache), data_start):
        [x.updateCache(node.getblock(str(i), 2)) for x in analyzers]

    bucketses = [(x, []) for x in analyzers]
    for block_height in block_range:
        block = node.getblock(str(block_height), 2)
        for tx in block['tx']:
            for analyzer in analyzers:
                dict(bucketses)[analyzer].extend(analyzer.extract(block, tx))

    result = []
    for analyzer in analyzers:
        result.append((analyzer.name, analyzer.aggregate(dict(bucketses)[analyzer])))

    return result

### Helpers

def identity(x):
    return x

def concat(lists):
    return [j for i in lists for j in i]

def get_shielded_spends(tx):
    try:
        shielded_spends = len(tx['vShieldedSpend'])
    except KeyError:
        shielded_spends = 0

    return shielded_spends

def get_shielded_outputs(tx):
    try:
        shielded_outputs = len(tx['vShieldedOutput'])
    except KeyError:
        shielded_outputs = 0

    return shielded_outputs

def get_orchard_actions(tx):
    try:
        orchard_actions = len(tx['orchard']['actions'])
    except KeyError:
        orchard_actions = 0

    return orchard_actions

def count_inputs(tx):
    return len(tx['vin']) + len(tx['vjoinsplit']) + get_shielded_spends(tx) + get_orchard_actions(tx)

def count_outputs(tx):
    return len(tx['vout']) + len(tx['vjoinsplit']) + get_shielded_outputs(tx) + get_orchard_actions(tx)

def count_ins_and_outs(tx):
    return (len(tx['vin'])
            + len(tx['vout'])
            + get_shielded_spends(tx)
            + get_shielded_outputs(tx)
            + 2 * len(tx['vjoinsplit'])
            + 2 * get_orchard_actions(tx))

def count_actions(tx):
    return (max(len(tx['vin']) + get_shielded_spends(tx), len(tx['vout']) + get_shielded_outputs(tx))
            + len(tx['vjoinsplit'])
            + get_orchard_actions(tx))

def expiry_height_delta(block, tx):
    """
    Returns -1 if there's no expiry, also returns approximately 35,000 (the
    number of blocks in a month) if the expiry is beyond 1 month.
    """
    month = blocks_per_hour * 24 * 30
    try:
        expiry_height = tx['expiryheight']
        if expiry_height == 0:
            return -1
        elif tx['expiryheight'] - block['height'] > month:
            return month
        else:
            return tx['expiryheight'] - block['height']
    except KeyError:
        # `tx['expiryheight']` is ostensibly an optional field, but it seems
        # like `0` is what tends to be used for "don't expire", so this case
        # generally isn't hit.
        return -1

def tx_type(tx):
    """
    Categorizes all tx into one of nine categories: (t)ransparent, (z)shielded,
    or (m)ixed for both inputs and outputs. So some possible results are "t-t",
    "t-z", "m-z", etc.
    """
    if tx['vjoinsplit'] or get_shielded_spends(tx) != 0 or get_orchard_actions(tx) != 0:
        if tx['vin']:
            ins = "m"
        else:
            ins = "z"
    else:
        ins = "t"

    if tx['vjoinsplit'] or get_shielded_outputs(tx) != 0 or get_orchard_actions(tx) != 0:
        if tx['vout']:
            outs = "m"
        else:
            outs = "z"
    else:
        outs = "t"

    return ins + "-" + outs

def is_orchard_tx(tx):
    try:
        return tx['orchard']['actions']
    except KeyError:
        return False

def is_saplingspend_tx(tx):
    try:
        return tx['vShieldedSpend']
    except KeyError:
        return False

def orchard_anchorage(cache, block, tx):
    """
    Returns -1 if there is no anchor
    """
    try:
        return block['height'] - cache[tx['orchard']['anchor']]
    except KeyError:
        return -1

def sapling_anchorage(cache, block, tx):
    """
    Returns -1 if there is no anchor
    """
    try:
        return block['height'] - cache[tx['vShieldedSpend'][0]['anchor']]
    except KeyError:
        return -1

blocks_per_hour = 48 # half this before NU2?

# start about a month before sandblasting
start_range = blocks_per_hour * 24 * 7 * 206

# a range covering 12 weeks
some_range = range(start_range, start_range + (blocks_per_hour * 24 * 7 * 12))

### Requested Statistics

def storeAnchor(pool, cache, block):
    """
    Caches the block height as the value for its anchor hash.
    """
    try:
        final_root = block[pool]
        try:
            cache[final_root]
        except KeyError:
            cache[final_root] = block['height']
    except KeyError:
        None

    return cache

# "how old of anchors are people picking"
# --- https://zcash.slack.com/archives/CP6SKNCJK/p1660103126252979
anchor_age_orchard = Analyzer(
    "how old of anchors are people picking (for orchard)",
    lambda _c, _b, tx: is_orchard_tx(tx),
    [(orchard_anchorage, sum)],
    lambda *_: 1,
    ({}, lambda c, b: storeAnchor('finalorchardroot', c, b)),
    blocks_per_hour * 24
)

anchor_age_sapling = Analyzer(
    "how old of anchors are people picking (for sapling)",
    lambda _c, _b, tx: is_saplingspend_tx(tx),
    [(sapling_anchorage, sum)],
    lambda *_: 1,
    ({}, lambda c, b: storeAnchor('finalsaplingroot', c, b)),
    blocks_per_hour * 24
)

# "what's the distribution of expiry height deltas"
# --- https://zcash.slack.com/archives/CP6SKNCJK/p1660103126252979
expiry_height_deltas = Analyzer(
    "distribution of expiry height deltas",
    lambda *_: True,
    [(lambda _, b, t: expiry_height_delta(b, t), sum)],
    lambda *_: 1
)

# "does anyone use locktime"
# --- https://zcash.slack.com/archives/CP6SKNCJK/p1660103126252979
locktime_usage = Analyzer(
    "proportion of tx using locktime",
    lambda *_: True,
    [(lambda *_: 1,
      lambda d: dict(d)[True] / (dict(d)[False] + dict(d)[True])),
     (lambda _c, _b, tx: tx['locktime'] != 0, sum)],
    lambda *_: 1
)

# "I'm seeing a slightly different pattern to the sandblasting transactions,
#  unless I've just missed this before. The transactions I've looked at recently
#  have had > 400 sapling outputs. Has this been the case before and I just
#  missed it? I thought primarily these transactions had slightly over 100
#  outputs in most cases."
# --- https://zcash.slack.com/archives/CP6SKNCJK/p1660195664187769


# "Calculate the POFM threshold for historical transactions on-chain and
#  calculate what proportion of those transactions would fall below the POFM
#  threshold"
# --- https://docs.google.com/document/d/18wtGFCB2N4FO7SoqDPnEgVudAMlCArHMz0EwhE1HNPY/edit
tx_below_pofm_threshold = Analyzer(
    "rate of transactions below POFM threshold",
    lambda *_: True,
    [ (lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: dict(d)[False] / (dict(d)[False] + dict(d)[True])),
      (lambda _c, _b, tx: count_ins_and_outs(tx) - 4 > 0, sum)
    ],
    lambda *_: 1
)

tx_below_pofm_threshold_abs = Analyzer(
    "transactions below POFM threshold",
    lambda *_: True,
    [ (lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: (dict(d)[False], dict(d)[True])),
      (lambda _c, _b, tx: count_ins_and_outs(tx) - 4 > 0, sum)
    ],
    lambda *_: 1
)

outs_below_pofm_threshold_abs = Analyzer(
    "outputs below POFM threshold",
    lambda *_: True,
    [ (lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: (dict(d)[False], dict(d)[True])),
      (lambda _c, _b, tx: count_ins_and_outs(tx) - 4 > 0, sum)
    ],
    lambda _c, _b, tx: count_outputs(tx)
)

tx_below_pofm_threshold_5 = Analyzer(
    "rate of transactions below POFM threshold with a grace window of 5",
    lambda *_: True,
    [ (lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: dict(d)[False] / (dict(d)[False] + dict(d)[True])),
      (lambda _c, _b, tx: count_ins_and_outs(tx) - 5 > 0, sum)
    ],
    lambda *_: 1
)


tx_below_pofm_threshold_max = Analyzer(
    "rate of transactions below POFM threshold with max",
    lambda *_: True,
    [ (lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: dict(d)[False] / (dict(d)[False] + dict(d)[True])),
      (lambda _c, _b, tx: count_actions(tx) - 4 > 0, sum)
    ],
    lambda *_: 1
)

tx_below_pofm_threshold_ins = Analyzer(
    "rate of transactions below POFM threshold only on inputs",
    lambda *_: True,
    [ (lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: dict(d)[False] / (dict(d)[False] + dict(d)[True])),
      (lambda _c, _b, tx: count_inputs(tx) - 4 > 0, sum)
    ],
    lambda *_: 1
)

start = datetime.datetime.now()
print(analyze_blocks(some_range,
                     [ # anchor_age_orchard,
                       anchor_age_sapling,
                       expiry_height_deltas,
                       locktime_usage,
                       tx_below_pofm_threshold_abs,
                       outs_below_pofm_threshold_abs,
                       tx_below_pofm_threshold_5,
                       tx_below_pofm_threshold_max,
                       tx_below_pofm_threshold_ins,
                     ]))
print(datetime.datetime.now() - start)


### Other Examples

tx_per_day = Analyzer(
    "count transactions per day (treating block 0 as midnight ZST)",
    lambda *_: True,
    [(lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)), sum)],
    lambda *_: 1
)

mean_tx_per_day = Analyzer(
    "mean transactions per day, by block",
    lambda *_: True,
    [(lambda _c, block, _t: int(block['height'] % (blocks_per_hour * 24)), lambda d: mean([x[1] for x in d])),
     (lambda _c, block, _t: int(block['height']/(blocks_per_hour * 24)), sum)
    ],
    lambda *_: 1
)

mean_inout_per_tx_per_day = Analyzer(
    "mean inputs, outputs per transaction per day, by block",
    lambda *_: True,
    [(lambda _c, block, _t: int(block['height'] % (blocks_per_hour * 24)), lambda d: mean(concat(d.values()))),
     (lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)), identity)
    ],
    lambda _c, _b, tx: (count_inputs(tx), count_outputs(tx))
)

mean_inout_per_tx = Analyzer(
    "mean inputs, outputs per transaction, by week",
    lambda *_: True,
    [ ( lambda _c, block, _t: int(block['height']/(blocks_per_hour * 24 * 7)),
        lambda d: (mean([x[0] for x in d]), mean([x[1] for x in d]))
       )
     ],
    lambda _c, _b, tx: (count_inputs(tx), count_outputs(tx))
)

minimum_pofm_fees_nuttycom = Analyzer(
    "distribution of fees in ZAT, by day, using nuttycom's pricing",
    lambda *_: True,
    [ (lambda _c, block, _t: int(block['height'] / (blocks_per_hour * 24)), identity),
      (lambda _c, _b, tx: 1000 + 250 * max(0, count_ins_and_outs(tx) - 4), sum)
    ],
    lambda *_: 1
)

input_size_dist = Analyzer(
    "distribution of input sizes",
    lambda *_: True,
    [(lambda _c, _b, tx: [len(x['scriptSig']['hex']) for x in tx['vin']], identity)],
    lambda *_: 1,
)

very_high_inout_tx = Analyzer(
    "tx with very high in/out counts",
    lambda _c, _b, tx: count_ins_and_outs(tx) > 100,
    [(lambda _c, _b, tx: (count_inputs(tx), count_outputs(tx)), identity)],
    lambda _c, _b, tx: tx['txid']
)
