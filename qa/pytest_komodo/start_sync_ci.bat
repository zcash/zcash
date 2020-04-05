set CLIENTS=1
set CHAIN=%1
set TEST_ADDY0=RPWhA4f4ZTZxNi5K36bcwsWdVjSVDSjUnd
set TEST_WIF0=UpcQympViQpLmv1WzMwszKPrmKUa28zsv8pdLCMgNMXDFBBBKxCN
set TEST_PUBKEY0=02f0ec2d3da51b09e4fc8d9ba334c275b02b3ab6f22ce7be0ea5059cbccbd1b8c7
set CHAIN_MODE=REGULAR
set IS_BOOTSTRAP_NEEDED=%2
set BOOTSTRAP_URL=%3
set NOTARIZATIONS=True
set BLOCKTIME_AVR=60

python.exe chainstart.py

python.exe -m pytest sync\test_sync.py -s -vv
