addr=$(./src/bitcoind -datadir=/home/ian/1/ getnewaddress)

for foo in 1 2 3 4 5
do
inputx=$(./src/bitcoind -datadir=/home/ian/1/ sendtoaddress $addr 25)
rawmint=$(./src/bitcoind -datadir=/home/ian/1/  zerocoinmint 42 2 $inputx)
signedtx=$(./src/bitcoind -datadir=/home/ian/1 signrawtransaction $rawmint)
tx=$(echo $signedtx | python -c 'import sys,json;data=json.loads(sys.stdin.read()); print data["hex"]')
./src/bitcoind -datadir=/home/ian/1 sendrawtransaction $tx

done