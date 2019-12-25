#!/bin/bash

# chains start script params
export CLIENTS=2
export CHAIN="TONYCI"
export TEST_ADDY0="RPWhA4f4ZTZxNi5K36bcwsWdVjSVDSjUnd"
export TEST_WIF0="UpcQympViQpLmv1WzMwszKPrmKUa28zsv8pdLCMgNMXDFBBBKxCN"
export TEST_PUBKEY0="02f0ec2d3da51b09e4fc8d9ba334c275b02b3ab6f22ce7be0ea5059cbccbd1b8c7"
export TEST_ADDY1="RHoTHYiHD8TU4k9rbY4Aoj3ztxUARMJikH"
export TEST_WIF1="UwmmwgfXwZ673brawUarPzbtiqjsCPWnG311ZRAL4iUCZLBLYeDu"
export TEST_PUBKEY1="0285f68aec0e2f8b5e817d71a2a20a1fda74ea9943c752a13136a3a30fa49c0149"
export CHAIN_MODE="REGULAR"
export IS_BOOTSTRAP_NEEDED="True"
export BOOTSTRAP_URL="https://sirseven.me/share/bootstrap.tar.gz"

# starting the chains
python3 chainstart.py

# starting the tests
python3 -m pytest $@ -s -vv
