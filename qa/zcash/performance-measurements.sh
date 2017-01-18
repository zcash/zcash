#!/bin/bash

set -e

DATADIR=./benchmark-datadir

function zcash_rpc {
    ./src/zcash-cli -datadir="$DATADIR" -rpcwait -rpcuser=user -rpcpassword=password -rpcport=5983 "$@"
}

function zcashd_generate {
    zcash_rpc generate 101 > /dev/null
}

function zcashd_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    touch "$DATADIR/zcash.conf"
    ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 -showmetrics=0 &
    ZCASHD_PID=$!
}

function zcashd_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASH_PID
}

function zcashd_massif_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    touch "$DATADIR/zcash.conf"
    rm -f massif.out
    valgrind --tool=massif --time-unit=ms --massif-out-file=massif.out ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 -showmetrics=0 &
    ZCASHD_PID=$!
}

function zcashd_massif_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
    ms_print massif.out
}

function zcashd_valgrind_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    touch "$DATADIR/zcash.conf"
    rm -f valgrind.out
    valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 -showmetrics=0 &
    ZCASHD_PID=$!
}

function zcashd_valgrind_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
    cat valgrind.out
}

# Precomputation
case "$1" in
    *)
        case "$2" in
            verifyjoinsplit)
                zcashd_start
                RAWJOINSPLIT=$(zcash_rpc zcsamplejoinsplit)
                zcashd_stop
        esac
esac

case "$1" in
    time)
        zcashd_start
        case "$2" in
            sleep)
                zcash_rpc zcbenchmark sleep 10
                ;;
            parameterloading)
                zcash_rpc zcbenchmark parameterloading 10
                ;;
            createjoinsplit)
                zcash_rpc zcbenchmark createjoinsplit 10 "${@:3}"
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1000 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                zcash_rpc zcbenchmark solveequihash 50 "${@:3}"
                ;;
            verifyequihash)
                zcash_rpc zcbenchmark verifyequihash 1000
                ;;
            validatelargetx)
                zcash_rpc zcbenchmark validatelargetx 5
                ;;
            trydecryptnotes)
                zcash_rpc zcbenchmark trydecryptnotes 1000 "${@:3}"
                ;;
            incnotewitnesses)
                zcash_rpc zcbenchmark incnotewitnesses 100 "${@:3}"
                ;;
            *)
                zcashd_stop
                echo "Bad arguments."
                exit 1
        esac
        zcashd_stop
        ;;
    memory)
        zcashd_massif_start
        case "$2" in
            sleep)
                zcash_rpc zcbenchmark sleep 1
                ;;
            parameterloading)
                zcash_rpc zcbenchmark parameterloading 1
                ;;
            createjoinsplit)
                zcash_rpc zcbenchmark createjoinsplit 1 "${@:3}"
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                zcash_rpc zcbenchmark solveequihash 1 "${@:3}"
                ;;
            verifyequihash)
                zcash_rpc zcbenchmark verifyequihash 1
                ;;
            trydecryptnotes)
                zcash_rpc zcbenchmark trydecryptnotes 1 "${@:3}"
                ;;
            incnotewitnesses)
                zcash_rpc zcbenchmark incnotewitnesses 1 "${@:3}"
                ;;
            *)
                zcashd_massif_stop
                echo "Bad arguments."
                exit 1
        esac
        zcashd_massif_stop
        rm -f massif.out
        ;;
    valgrind)
        zcashd_valgrind_start
        case "$2" in
            sleep)
                zcash_rpc zcbenchmark sleep 1
                ;;
            parameterloading)
                zcash_rpc zcbenchmark parameterloading 1
                ;;
            createjoinsplit)
                zcash_rpc zcbenchmark createjoinsplit 1 "${@:3}"
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                zcash_rpc zcbenchmark solveequihash 1 "${@:3}"
                ;;
            verifyequihash)
                zcash_rpc zcbenchmark verifyequihash 1
                ;;
            trydecryptnotes)
                zcash_rpc zcbenchmark trydecryptnotes 1 "${@:3}"
                ;;
            incnotewitnesses)
                zcash_rpc zcbenchmark incnotewitnesses 1 "${@:3}"
                ;;
            *)
                zcashd_valgrind_stop
                echo "Bad arguments."
                exit 1
        esac
        zcashd_valgrind_stop
        rm -f valgrind.out
        ;;
    valgrind-tests)
        case "$2" in
            gtest)
                rm -f valgrind.out
                valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/zcash-gtest
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
