#!/usr/bin/env python3

import os
import progressbar
from slickrpc.rpc import Proxy

from .deltas_mainnet import MainnetSupplyDeltas
from .theoretical import Network, MAINNET

COIN=100000000

# Returns the theoretical supply, the total block rewards claimed,
# and the block at the given height.
#
# - `zcashd` a slickrpc.rpc.Proxy that can be used to access zcashd
# - `deltas` a SupplyDeltas object tracking the deltas observed
# - `height` the block height to consider
# - `flag` Either `1` or `2`. This flag will be provided to the
#   getblock RPC call to indicate how much data should be returned.
#   When `1` is provided, data for transactions in the block  will
#   be limited to transaction identifiers; when `2` is provided
#   full transaction data will be returned for each transaction in
#   the block.
def TheoreticalAndEmpirical(zcashd, deltas, height, flag):
    theoreticalSupply = Network(MAINNET).SupplyAfterHeight(height)
    block = zcashd.getblock(str(height), flag)
    measuredSupply = block['chainSupply']['chainValueZat']
    empiricalMaximum = measuredSupply + deltas.DeviationAtHeight(height)
    return (theoreticalSupply, empiricalMaximum, block)

# Returns `True` if the theoretical supply matches the empirically
# determined supply after accounting for known deltas over the specified
# range, or `False` if it was not possible to determine a miner address
# for a block containing unclaimed fee and/or block reward amounts.
#
# - `startHeight` The block height at which to begin checking
# - `endHeight` The end of the block height range to check (inclusive)
def Bisect(bar, zcashd, deltas, startHeight, endHeight):
    assert startHeight <= endHeight
    bar.update(startHeight)

    flag = 2 if startHeight == endHeight else 1
    (theoretical, empirical, block) = TheoreticalAndEmpirical(zcashd, deltas, endHeight, flag)
    if theoretical == empirical:
        return True
    elif startHeight == endHeight:
        return deltas.SaveMismatch(block, theoretical, empirical)
    else:
        midpoint = (startHeight + endHeight) // 2
        return (Bisect(bar, zcashd, deltas, startHeight, midpoint) and
                Bisect(bar, zcashd, deltas, midpoint + 1, endHeight))


def main():
    zcashd = Proxy('http://{rpc_user}:{rpc_pass}@{rpc_host}:{rpc_port}'.format(
        rpc_user=os.environ['ZCASHD_RPC_USER'],
        rpc_pass=os.environ['ZCASHD_RPC_PASS'],
        rpc_host=os.environ['ZCASHD_RPC_HOST'],
        rpc_port=os.environ['ZCASHD_RPC_PORT'],
    ))

    latestHeight = zcashd.getblockchaininfo()['blocks']
    deltas = MainnetSupplyDeltas(zcashd)
    (theoretical, empirical, block) = TheoreticalAndEmpirical(zcashd, deltas, latestHeight, 1)
    if theoretical != empirical:
        with progressbar.ProgressBar(max_value = latestHeight, redirect_stdout = True) as bar:
            try:
                Bisect(bar, zcashd, deltas, 0, latestHeight)
            except KeyboardInterrupt:
                pass
            deltas.PrintDeltas()
    print("Block height: {}".format(latestHeight))
    print("Chain total value: {} ZEC".format(block['chainSupply']['chainValueZat'] / COIN))
    print("Theoretical maximum supply: {} ZEC".format(theoretical / COIN))
    print("Blocks with unclaimed balance: {}".format(deltas.delta_count))
    print("Unclaimed total: {} ZEC".format(deltas.delta_total / COIN))

if __name__ == "__main__":
    main()
