#!/bin/bash

set -e

DATADIR=./benchmark-datadir

function dwcash_rpc {
    ./src/dwcash-cli -rpcwait -rpcuser=user -rpcpassword=password -rpcport=5983 "$@"
}

function dwcashd_generate {
    dwcash_rpc generate 101 > /dev/null
}

function dwcashd_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    ./src/dwcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    DWCASHD_PID=$!
}

function dwcashd_stop {
    dwcash_rpc stop > /dev/null
    wait $DWCASH_PID
}

function dwcashd_massif_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    rm -f massif.out
    valgrind --tool=massif --time-unit=ms --massif-out-file=massif.out ./src/dwcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    DWCASHD_PID=$!
}

function dwcashd_massif_stop {
    dwcash_rpc stop > /dev/null
    wait $DWCASHD_PID
    ms_print massif.out
}

function dwcashd_valgrind_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    rm -f valgrind.out
    valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/dwcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    DWCASHD_PID=$!
}

function dwcashd_valgrind_stop {
    dwcash_rpc stop > /dev/null
    wait $DWCASHD_PID
    cat valgrind.out
}

# Precomputation
case "$1" in
    *)
        case "$2" in
            verifyjoinsplit)
                dwcashd_start
                RAWJOINSPLIT=$(dwcash_rpc zcsamplejoinsplit)
                dwcashd_stop
        esac
esac

case "$1" in
    time)
        dwcashd_start
        case "$2" in
            sleep)
                dwcash_rpc zcbenchmark sleep 10
                ;;
            parameterloading)
                dwcash_rpc zcbenchmark parameterloading 10
                ;;
            createjoinsplit)
                dwcash_rpc zcbenchmark createjoinsplit 10
                ;;
            verifyjoinsplit)
                dwcash_rpc zcbenchmark verifyjoinsplit 1000 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                dwcash_rpc zcbenchmark solveequihash 50 "${@:3}"
                ;;
            verifyequihash)
                dwcash_rpc zcbenchmark verifyequihash 1000
                ;;
            validatelargetx)
                dwcash_rpc zcbenchmark validatelargetx 5
                ;;
            *)
                dwcashd_stop
                echo "Bad arguments."
                exit 1
        esac
        dwcashd_stop
        ;;
    memory)
        dwcashd_massif_start
        case "$2" in
            sleep)
                dwcash_rpc zcbenchmark sleep 1
                ;;
            parameterloading)
                dwcash_rpc zcbenchmark parameterloading 1
                ;;
            createjoinsplit)
                dwcash_rpc zcbenchmark createjoinsplit 1
                ;;
            verifyjoinsplit)
                dwcash_rpc zcbenchmark verifyjoinsplit 1 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                dwcash_rpc zcbenchmark solveequihash 1 "${@:3}"
                ;;
            verifyequihash)
                dwcash_rpc zcbenchmark verifyequihash 1
                ;;
            *)
                dwcashd_massif_stop
                echo "Bad arguments."
                exit 1
        esac
        dwcashd_massif_stop
        rm -f massif.out
        ;;
    valgrind)
        dwcashd_valgrind_start
        case "$2" in
            sleep)
                dwcash_rpc zcbenchmark sleep 1
                ;;
            parameterloading)
                dwcash_rpc zcbenchmark parameterloading 1
                ;;
            createjoinsplit)
                dwcash_rpc zcbenchmark createjoinsplit 1
                ;;
            verifyjoinsplit)
                dwcash_rpc zcbenchmark verifyjoinsplit 1 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                dwcash_rpc zcbenchmark solveequihash 1 "${@:3}"
                ;;
            verifyequihash)
                dwcash_rpc zcbenchmark verifyequihash 1
                ;;
            *)
                dwcashd_valgrind_stop
                echo "Bad arguments."
                exit 1
        esac
        dwcashd_valgrind_stop
        rm -f valgrind.out
        ;;
    valgrind-tests)
        case "$2" in
            gtest)
                rm -f valgrind.out
                valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/dwcash-gtest
                cat valgrind.out
                rm -f valgrind.out
                ;;
            test_bitcoin)
                rm -f valgrind.out
                valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/test/test_bitcoin
                cat valgrind.out
                rm -f valgrind.out
                ;;
            *)
                echo "Bad arguments."
                exit 1
        esac
        ;;
    *)
        echo "Bad arguments."
        exit 1
esac

# Cleanup
rm -rf "$DATADIR"
