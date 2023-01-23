#!/usr/bin/env python3

import os
import progressbar
from slickrpc.rpc import Proxy

from .deltas_mainnet import MainnetSupplyDeltas
from .theoretical import Network, MAINNET

COIN=100000000

TXIDS_ONLY=1
FULL_TX_DATA=2

# Returns the theoretical supply, the total block rewards claimed,
# and the block at the given height.
#
# - `zcashd` a slickrpc.rpc.Proxy that can be used to access zcashd
# - `deltas` a SupplyDeltas object tracking the deltas observed
# - `height` the block height to consider
# - `flag` Either `TXIDS_ONLY` or `FULL_TX_DATA`. This flag will be provided to
#   the getblock RPC call to indicate how much data should be returned.  When
#   `TXIDS_ONLY` is provided, data for transactions in the block  will be limited
#   to transaction identifiers; when `FULL_TX_DATA` is provided full transaction
#   data will be returned for each transaction in the block.
def TheoreticalAndEmpirical(zcashd, deltas, height, flag):
    theoreticalSupply = Network(MAINNET).SupplyAfterHeight(height)
    block = zcashd.getblock(str(height), flag)
    measuredSupply = block['chainSupply']['chainValueZat']
    empiricalMaximum = measuredSupply + deltas.DeviationUpToHeight(height)
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

    flag = FULL_TX_DATA if startHeight == endHeight else TXIDS_ONLY
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
    missing_env = []
    if os.environ.get('ZCASHD_RPC_USER') is None:
        missing_env.append('  ZCASHD_RPC_USER: username for accessing the zcashd RPC API')
    if os.environ.get('ZCASHD_RPC_PASS') is None:
        missing_env.append('  ZCASHD_RPC_PASS: RPC API password for <ZCASHD_RPC_USER>')
    if os.environ.get('ZCASHD_RPC_HOST') is None:
        missing_env.append('  ZCASHD_RPC_HOST: hostname where zcashd is running')
    if os.environ.get('ZCASHD_RPC_PORT') is None:
        missing_env.append('  ZCASHD_RPC_PORT: zcashd RPC API port (usually 8232 for mainnet)')

    if len(missing_env) > 0:
        print("Please ensure that the following environment variables have been set:")
        for v in missing_env:
            print(v)
        return

    zcashd = Proxy('http://{rpc_user}:{rpc_pass}@{rpc_host}:{rpc_port}'.format(
        rpc_user=os.environ['ZCASHD_RPC_USER'],
        rpc_pass=os.environ['ZCASHD_RPC_PASS'],
        rpc_host=os.environ['ZCASHD_RPC_HOST'],
        rpc_port=os.environ['ZCASHD_RPC_PORT'],
    ))

    latestHeight = zcashd.getblockchaininfo()['blocks']
    deltas = MainnetSupplyDeltas()
    (theoretical, empirical, block) = TheoreticalAndEmpirical(zcashd, deltas, latestHeight, TXIDS_ONLY)
    interrupted = False
    if theoretical != empirical:
        with progressbar.ProgressBar(max_value = latestHeight, redirect_stdout = True) as bar:
            try:
                Bisect(bar, zcashd, deltas, 0, latestHeight)
            except KeyboardInterrupt:
                interrupted = True
                pass
            deltas.PrintDeltas()
    print("Block height: {}".format(latestHeight))
    print("Chain total value: {} ZEC".format(block['chainSupply']['chainValueZat'] / COIN))
    print("Theoretical maximum supply: {} ZEC".format(theoretical / COIN))
    if interrupted:
        print("Supply check was interrupted; supply delta evaluation incomplete.")
    else:
        print("Blocks with unclaimed balance: {}".format(len(deltas.delta_cache)))
        print("Unclaimed total: {} ZEC".format(deltas.delta_total / COIN))

if __name__ == "__main__":
    main()
