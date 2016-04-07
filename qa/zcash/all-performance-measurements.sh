#!/bin/bash

set -e

function zcash_rpc {
    ./src/zcash-cli -rpcwait -rpcuser=user -rpcpassword=password -rpcport=5001 "$@"
}


function zcashd_start {
    ./src/zcashd -regtest -rpcuser=user -rpcpassword=password -rpcport=5001 &
    ZCASHD_PID=$!
}

function zcashd_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASH_PID
}

function zcashd_massif_start {
    rm -f massif.out
    valgrind --tool=massif --time-unit=ms --massif-out-file=massif.out ./src/zcashd -regtest -rpcuser=user -rpcpassword=password -rpcport=5001 &
    ZCASHD_PID=$!
}

function zcashd_massif_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
    ms_print massif.out
}

echo ""
echo "Time"
echo "=============================="

echo ""
echo "Sleep (1s test)"
echo "------------------"
zcashd_start
zcash_rpc zcbenchmark sleep 10
zcashd_stop

echo ""
echo "Parameter Loading"
echo "------------------"
zcashd_start
zcash_rpc zcbenchmark parameterloading 10
zcashd_stop

echo ""
echo "Create JoinSplit"
echo "------------------"
zcashd_start
zcash_rpc zcbenchmark createjoinsplit 10
zcashd_stop

echo ""
echo ""
echo "Memory"
echo "=============================="

echo ""
echo "Sleep (1s test)"
echo "------------------"
zcashd_massif_start
zcash_rpc zcbenchmark sleep 1
zcashd_massif_stop

echo ""
echo "Parameter Loading"
echo "------------------"
zcashd_massif_start
zcash_rpc zcbenchmark parameterloading 1
zcashd_massif_stop

echo ""
echo "Create JoinSplit"
echo "------------------"
zcashd_massif_start
zcash_rpc zcbenchmark createjoinsplit 1
zcashd_massif_stop
