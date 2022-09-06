#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
"""
analysis.py -
"""

import datetime
import itertools
from statistics import mean
import sys

from test_framework.authproxy import AuthServiceProxy

### TODO: Get host/port from config
if len(sys.argv) > 1:
    node = AuthServiceProxy(sys.argv[1])
else:
    raise Exception("%s needs to be provided a connection string, like \"http://user:pass@localhost:port\"." % sys.argv[0])

class Analyzer:
    """
    An analyzer collects a single aggregated data structure from the blockchain.

    If you had a block and a single tx from that block, you could simply
   `my_analyzer.aggregate(my_analyzer.extract(block, tx))` to generate the stats
    for that analysis. However, since we generally want to aggregate across many
    transactions in many blocks and also because we usually want to collect
    multiple statistics at once (because re-fetching blocks and tx is slow),
   `extract` and `aggregate are separated out. See `bucket_transactions` for how
    to take advantage of this structure.
    """

    def __init__(self, name, tx_filter, bucketers, extractor):
        """
    It takes various functions to apply to the transactions therein. The functions are typed as follows:

    tx_filter :: Block -> Tx -> Boolean
    tx_bucketers :: [ ...,
                      (Block -> Tx -> k_n-2, [(k_n-1, a)] -> b),
                      (Block -> Tx -> k_n-1, [(k_n, a)] -> b),
                      (Block -> Tx -> k_n,   [v] -> a)
                    ]
    tx_extractor :: Block -> Tx -> v

    `tx_filter` decides whether the given transaction should be included in the result,
    `tx_extractor` reduces each transaction to the parts we care about in the results,
    `tx_bucketers` is a list of pairs of functions -- the first of each pair produces a key for bucketing the results and the second is how to accumulate the values in that bucket. The list allows us to create buckets of buckets.

    If no bucketers are provided, this returns a list of all the extracted data in a list, one for each transaction. If there are bucketers, it returns a map, with the keys from the first bucketer in the list and the values from the first accumulator in the list.
        """
        self.name = name
        self.__filter = tx_filter
        self.__bucketers = bucketers
        self.__extractor = extractor

    def extract(self, block, tx):
        """
        Extracts all the data from a given transaction (and its block) needed to
        compute the statistics for this analyzer.

        TODO: Allow a bucketer to return multiple keys. This hopefully allows
              things like sub-transaction extraction. E.g., looking at the sizes
              of all vouts by day, without caring which ones are in the same tx
        """
        if self.__filter(block, tx):
            value = self.__extractor(block, tx)
            keys = [x[0](block, tx) for x in self.__bucketers]
            return [(keys, value)]
        else:
            return []

    def aggregate(self, kvs):
        """
        """
        kvs.sort(key=lambda x: x[0])
        return self.__group(kvs, [x[1] for x in self.__bucketers])

    def __group(self, kvs, accumulators):
        if kvs[0][0]:
            buck = []
            accum, *remaining_accum = accumulators
            for k, g in itertools.groupby(kvs, lambda x: x[0].pop(0)):
                buck.append((k, accum(self.__group(list(g), remaining_accum))))
            return buck
        else:
            return [x[1] for x in kvs]

def bucket_transactions(block_range, analyzers):
    """
    This function executes multiple analyzers over a common range of blocks,
    returning results keyed by the name of the analyzer.
    """
    bucketses = [(x.name, []) for x in analyzers]
    for block_height in block_range: # TODO: check the range is limited to be 0-current
        block = node.getblock(str(block_height), 2)
        for tx in block['tx']:
            for analyzer in analyzers:
                dict(bucketses)[analyzer.name].extend(analyzer.extract(block, tx))

    result = []
    for analyzer in analyzers:
        result.append((analyzer.name, analyzer.aggregate(dict(bucketses)[analyzer.name])))

    return result

### Helpers

def identity(x):
    return x

def concat(lists):
    return [j for i in lists for j in i]

def get_shielded_spends(tx):
    try:
        shielded_spends = len(tx['vShieldedSpend'])
    except:
        shielded_spends = 0

    return shielded_spends

def get_shielded_outputs(tx):
    try:
        shielded_outputs = len(tx['vShieldedOutput'])
    except:
        shielded_outputs = 0

    return shielded_outputs

def get_orchard_actions(tx):
    try:
        orchard_actions = len(tx['orchard']['actions'])
    except:
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
    Returns -1 if there's no expiry, also returns ~35,000 if the expiry is
    beyond ~1 month.
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
    except:
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
        tx['orchard']
        return True
    except:
        return False

def is_saplingspend_tx(tx):
    try:
        tx['vSaplingSpend']
        return True
    except:
        return False

def orchard_anchorage(block, tx):
    """
    Returns -1 if there is no anchor
    """
    try:
        return block['height'] - node.getblockheader(tx['orchard']['anchor'])['height']
    except:
        return -1

def sapling_anchorage(block, tx):
    return [block['height'] - node.getblockheader(x['anchor'])['height']
            for x in tx['vShieldedSpend']]

blocks_per_hour = 48 # half this before NU2?

# start about a month before sandblasting
start_range = blocks_per_hour * 24 * 7 * 206

# a range covering 12 weeks
some_range = range(start_range, start_range + (blocks_per_hour * 24 * 7 * 12))

### Requested Statistics

# "how old of anchors are people picking"
# --- https://zcash.slack.com/archives/CP6SKNCJK/p1660103126252979
anchor_age_orchard = Analyzer(
    "how old of anchors are people picking (for orchard)",
    lambda block, tx: is_orchard_tx(tx),
    [(orchard_anchorage, sum)],
    lambda *_: 1
)

anchor_age_sapling = Analyzer(
    "how old of anchors are people picking (for sapling)",
    lambda block, tx: is_saplingspend_tx(tx),
    [(sapling_anchorage, sum)],
    lambda *_: 1
)

# "what's the distribution of expiry height deltas"
# --- https://zcash.slack.com/archives/CP6SKNCJK/p1660103126252979
expiry_height_deltas = Analyzer(
    "distribution of expiry height deltas",
    lambda *_: True,
    [(expiry_height_delta, sum)],
    lambda *_: 1
)

# "does anyone use locktime"
# --- https://zcash.slack.com/archives/CP6SKNCJK/p1660103126252979
locktime_usage = Analyzer(
    "proportion of tx using locktime",
    lambda *_: True,
    [(lambda *_: 1,
      lambda d: dict(d)[True] / (dict(d)[False] + dict(d)[True])),
     (lambda _, tx: tx['locktime'] != 0, sum)],
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
    [ (lambda block, _: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: dict(d)[False] / (dict(d)[False] + dict(d)[True])),
      (lambda _, tx: count_ins_and_outs(tx) - 4 > 0, sum)
    ],
    lambda *_: 1
)

tx_below_pofm_threshold_5 = Analyzer(
    "rate of transactions below POFM threshold with a grace window of 5",
    lambda *_: True,
    [ (lambda block, _: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: dict(d)[False] / (dict(d)[False] + dict(d)[True])),
      (lambda _, tx: count_ins_and_outs(tx) - 5 > 0, sum)
    ],
    lambda *_: 1
)


tx_below_pofm_threshold_max = Analyzer(
    "rate of transactions below POFM threshold with max",
    lambda *_: True,
    [ (lambda block, _: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: dict(d)[False] / (dict(d)[False] + dict(d)[True])),
      (lambda _, tx: count_actions(tx) - 4 > 0, sum)
    ],
    lambda *_: 1
)

tx_below_pofm_threshold_ins = Analyzer(
    "rate of transactions below POFM threshold only on inputs",
    lambda *_: True,
    [ (lambda block, _: int(block['height'] / (blocks_per_hour * 24)),
       lambda d: dict(d)[False] / (dict(d)[False] + dict(d)[True])),
      (lambda _, tx: count_inputs(tx) - 4 > 0, sum)
    ],
    lambda *_: 1
)

start = datetime.datetime.now()
print(bucket_transactions(some_range,
                          [ # anchor_age,
                            expiry_height_deltas,
                            locktime_usage,
                            tx_below_pofm_threshold,
                            tx_below_pofm_threshold_5,
                            tx_below_pofm_threshold_max,
                            tx_below_pofm_threshold_ins,
                            # anchor_age_orchard,
                            # anchor_age_sapling,
                           ]))
print(datetime.datetime.now() - start)


### Other Examples

tx_per_day = Analyzer(
    "count transactions per day (treating block 0 as midnight ZST)",
    lambda *_: True,
    [(lambda block, _: int(block['height'] / (blocks_per_hour * 24)), sum)],
    lambda *_: 1
)

mean_tx_per_day = Analyzer(
    "mean transactions per day, by block",
    lambda _, t: True,
    [(lambda block, _: int(block['height'] % (blocks_per_hour * 24)), lambda d: mean([x[1] for x in d])),
     (lambda block, _: int(block['height']/(blocks_per_hour * 24)), sum)
    ],
    lambda *_: 1
)

mean_inout_per_tx_per_day = Analyzer(
    "mean inputs, outputs per transaction per day, by block",
    lambda *_: True,
    [(lambda block, _: int(block['height'] % (blocks_per_hour * 24)), lambda d: mean(concat(d.values()))),
     (lambda block, _: int(block['height']/(blocks_per_hour * 24)), identity)
    ],
    lambda _, t: (count_inputs(t), count_outputs(t))
)

mean_inout_per_tx = Analyzer(
    "mean inputs, outputs per transaction, by week",
    lambda *_: True,
    [ ( lambda block, _: int(block['height']/(blocks_per_hour * 24 * 7)),
        lambda d: (mean([x[0] for x in d]), mean([x[1] for x in d]))
       )
     ],
    lambda _, t: (count_inputs(t), count_outputs(t))
)

minimum_pofm_fees_nuttycom = Analyzer(
    "distribution of fees in ZAT, by day, using nuttycom's pricing",
    lambda *_: True,
    [ (lambda block, _: int(block['height'] / (blocks_per_hour * 24)), identity),
      (lambda _, tx: 1000 + 250 * max(0, count_ins_and_outs(tx) - 4), sum)
    ],
    lambda *_: 1
)

input_size_dist = Analyzer(
    "distribution of input sizes",
    lambda *_: True,
    [(lambda _, t: [len(x['scriptSig']['hex']) for x in t['vin']], identity)],
    lambda *_: 1,
)

very_high_inout_tx = Analyzer(
    "tx with very high in/out counts",
    lambda _, t: count_ins_and_outs(t) > 100,
    [(lambda _, t: (count_inputs(t), count_outputs(t)), identity)],
    lambda _, t: t['txid']
)
