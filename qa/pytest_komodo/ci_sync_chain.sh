#!/bin/bash
# Usage: ./ci_sync_chain.sh "CHAIN" "BOOL_BOOTSTRAP_NEEDED" "BOOTSTRAP_LINK"
# example: ./ci_sync_chain.sh "KMD" "False" ""
# chains start script params
export CLIENTS=1
export CHAIN=$1
export TEST_ADDY0="RPWhA4f4ZTZxNi5K36bcwsWdVjSVDSjUnd"
export TEST_WIF0="UpcQympViQpLmv1WzMwszKPrmKUa28zsv8pdLCMgNMXDFBBBKxCN"
export TEST_PUBKEY0="02f0ec2d3da51b09e4fc8d9ba334c275b02b3ab6f22ce7be0ea5059cbccbd1b8c7"
export CHAIN_MODE="REGULAR"
export IS_BOOTSTRAP_NEEDED=$2
export BOOTSTRAP_URL=$3
export NOTARIZATIONS="True"
export BLOCKTIME_AVR=60

# starting the chains
python3 chainstart.py

# starting the tests
python3 -m pytest sync/test_sync.py -s -vv
