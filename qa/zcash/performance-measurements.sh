#!/usr/bin/env bash

export LC_ALL=C
set -u


DATADIR=./benchmark-datadir
SHA256CMD="$(command -v sha256sum || echo shasum)"
SHA256ARGS="$(command -v sha256sum >/dev/null || echo '-a 256')"

function zcash_rpc {
    ./src/zcash-cli -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 "$@"
}

function zcash_rpc_slow {
    # Timeout of 1 hour
    zcash_rpc -rpcclienttimeout=3600 "$@"
}

function zcash_rpc_veryslow {
    # Timeout of 2.5 hours
    zcash_rpc -rpcclienttimeout=9000 "$@"
}

function zcash_rpc_wait_for_start {
    zcash_rpc -rpcwait getinfo > /dev/null
}

function zcashd_generate {
    zcash_rpc generate 101 > /dev/null
}

function extract_benchmark_datadir {
    if [ -f "$1.tar.xz" ]; then
        # Check the hash of the archive:
        "$SHA256CMD" $SHA256ARGS -c <<EOF
$2  $1.tar.xz
EOF
        ARCHIVE_RESULT=$?
    else
        echo "$1.tar.xz not found."
        ARCHIVE_RESULT=1
    fi
    if [ $ARCHIVE_RESULT -ne 0 ]; then
        zcashd_stop
        echo
        echo "Please download it and place it in the base directory of the repository."
        exit 1
    fi
    xzcat "$1.tar.xz" | tar x
}

function use_200k_benchmark {
    rm -rf benchmark-200k-UTXOs
    extract_benchmark_datadir benchmark-200k-UTXOs dc8ab89eaa13730da57d9ac373c1f4e818a37181c1443f61fd11327e49fbcc5e
    DATADIR="./benchmark-200k-UTXOs/node$1"
}

function zcashd_start {
    case "$1" in
        sendtoaddress|loadwallet|listunspent)
            case "$2" in
                200k-recv)
                    use_200k_benchmark 0
                    ;;
                200k-send)
                    use_200k_benchmark 1
                    ;;
                *)
                    echo "Bad arguments to zcashd_start."
                    exit 1
            esac
            ;;
        *)
            rm -rf "$DATADIR"
            mkdir -p "$DATADIR/regtest"
            touch "$DATADIR/zcash.conf"
    esac
    ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 -showmetrics=0 &
    ZCASHD_PID=$!
    zcash_rpc_wait_for_start
}

function zcashd_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
}

function zcashd_heaptrack_start {
    TEST_NAME="$1"
    case "$1" in
        sendtoaddress|loadwallet|listunspent)
            case "$2" in
                200k-recv)
                    use_200k_benchmark 0
                    TEST_NAME="${TEST_NAME}-200k-recv"
                    ;;
                200k-send)
                    use_200k_benchmark 1
                    TEST_NAME="${TEST_NAME}-200k-send"
                    ;;
                *)
                    echo "Bad arguments to zcashd_heaptrack_start."
                    exit 1
            esac
            ;;
        *)
            rm -rf "$DATADIR"
            mkdir -p "$DATADIR/regtest"
            touch "$DATADIR/zcash.conf"
    esac
    heaptrack -o "${TEST_NAME}" ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 -showmetrics=0 &
    ZCASHD_PID=$!
    zcash_rpc_wait_for_start
}

function zcashd_heaptrack_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
}

function zcashd_valgrind_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR/regtest"
    touch "$DATADIR/zcash.conf"
    rm -f valgrind.out
    valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 -showmetrics=0 &
    ZCASHD_PID=$!
    zcash_rpc_wait_for_start
}

function zcashd_valgrind_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
    cat valgrind.out
}

function extract_benchmark_data {
    if [ -f "$1.tar.xz" ]; then
        # Check the hash of the archive:
        "$SHA256CMD" $SHA256ARGS -c <<EOF
$2  $1.tar.xz
EOF
        ARCHIVE_RESULT=$?
    else
        echo "$1.tar.xz not found."
        ARCHIVE_RESULT=1
    fi
    if [ $ARCHIVE_RESULT -ne 0 ]; then
        zcashd_stop
        echo
        echo "Please generate it using qa/zcash/create_benchmark_archive.py"
        echo "and place it in the base directory of the repository."
        echo "Usage details are inside the Python script."
        exit 1
    fi
    xzcat $1.tar.xz | tar x -C "$DATADIR/regtest"
}

function extract_benchmark_data_107134 {
    extract_benchmark_data block-107134 4bd5ad1149714394e8895fa536725ed5d6c32c99812b962bfa73f03b5ffad4bb
}

function extract_benchmark_data_1708048 {
    extract_benchmark_data block-1708048 bdae4b1baf528fa93d86b902e1032d4d1fc3d3080f76cd284635787e472f2534
}

function extract_benchmark_data_1723244 {
    extract_benchmark_data block-1723244 94acfef482103a61c848ddaa3129877cba987c4a00fa42425f4cf030b7c3fa4c
}


if [ $# -lt 2 ]
then
    echo "$0 : At least two arguments are required!"
    exit 1
fi

# Precomputation
case "$1" in
    *)
        case "$2" in
            verifyjoinsplit)
                zcashd_start "${@:2}"
                RAWJOINSPLIT=$(zcash_rpc zcsamplejoinsplit)
                zcashd_stop
        esac
esac

case "$1" in
    time)
        zcashd_start "${@:2}"
        case "$2" in
            sleep)
                zcash_rpc zcbenchmark sleep 10
                ;;
            parameterloading)
                zcash_rpc zcbenchmark parameterloading 10
                ;;
            createsaplingspend)
                zcash_rpc zcbenchmark createsaplingspend 10
                ;;
            verifysaplingspend)
                zcash_rpc zcbenchmark verifysaplingspend 1000
                ;;
            createsaplingoutput)
                zcash_rpc zcbenchmark createsaplingoutput 50
                ;;
            verifysaplingoutput)
                zcash_rpc zcbenchmark verifysaplingoutput 1000
                ;;
            createjoinsplit)
                zcash_rpc zcbenchmark createjoinsplit 10 "${@:3}"
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1000 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                zcash_rpc_slow zcbenchmark solveequihash 50 "${@:3}"
                ;;
            verifyequihash)
                zcash_rpc zcbenchmark verifyequihash 1000
                ;;
            validatelargetx)
                zcash_rpc zcbenchmark validatelargetx 10 "${@:3}"
                ;;
            trydecryptnotes)
                zcash_rpc zcbenchmark trydecryptnotes 1000 "${@:3}"
                ;;
            incnotewitnesses)
                zcash_rpc zcbenchmark incnotewitnesses 100 "${@:3}"
                ;;
            connectblockslow)
                extract_benchmark_data_107134
                zcash_rpc zcbenchmark connectblockslow 10
                ;;
            connectblocksapling)
                extract_benchmark_data_1723244
                zcash_rpc zcbenchmark connectblocksapling 10
                ;;
            connectblockorchard)
                extract_benchmark_data_1708048
                zcash_rpc zcbenchmark connectblockorchard 10
                ;;
            sendtoaddress)
                zcash_rpc zcbenchmark sendtoaddress 10 "${@:4}"
                ;;
            loadwallet)
                zcash_rpc zcbenchmark loadwallet 10 
                ;;
            listunspent)
                zcash_rpc zcbenchmark listunspent 10
                ;;
            *)
                zcashd_stop
                echo "Bad arguments to time."
                exit 1
        esac
        zcashd_stop
        ;;
    memory)
        zcashd_heaptrack_start "${@:2}"
        case "$2" in
            sleep)
                zcash_rpc zcbenchmark sleep 1
                ;;
            parameterloading)
                zcash_rpc zcbenchmark parameterloading 1
                ;;
            createsaplingspend)
                zcash_rpc zcbenchmark createsaplingspend 1
                ;;
            verifysaplingspend)
                zcash_rpc zcbenchmark verifysaplingspend 1
                ;;
            createsaplingoutput)
                zcash_rpc zcbenchmark createsaplingoutput 1
                ;;
            verifysaplingoutput)
                zcash_rpc zcbenchmark verifysaplingoutput 1
                ;;
            createjoinsplit)
                zcash_rpc_slow zcbenchmark createjoinsplit 1 "${@:3}"
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                zcash_rpc_slow zcbenchmark solveequihash 1 "${@:3}"
                ;;
            verifyequihash)
                zcash_rpc zcbenchmark verifyequihash 1
                ;;
            validatelargetx)
                zcash_rpc zcbenchmark validatelargetx 1
                ;;
            trydecryptnotes)
                zcash_rpc zcbenchmark trydecryptnotes 1 "${@:3}"
                ;;
            incnotewitnesses)
                zcash_rpc zcbenchmark incnotewitnesses 1 "${@:3}"
                ;;
            connectblockslow)
                extract_benchmark_data_107134
                zcash_rpc zcbenchmark connectblockslow 1
                ;;
            connectblocksapling)
                extract_benchmark_data_1723244
                zcash_rpc zcbenchmark connectblocksapling 1
                ;;
            connectblockorchard)
                extract_benchmark_data_1708048
                zcash_rpc zcbenchmark connectblockorchard 1
                ;;
            sendtoaddress)
                zcash_rpc zcbenchmark sendtoaddress 1 "${@:4}"
                ;;
            loadwallet)
                # The initial load is sufficient for measurement
                ;;
            listunspent)
                zcash_rpc zcbenchmark listunspent 1
                ;;
            *)
                zcashd_heaptrack_stop
                echo "Bad arguments to memory."
                exit 1
        esac
        zcashd_heaptrack_stop
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
            createsaplingspend)
                zcash_rpc zcbenchmark createsaplingspend 1
                ;;
            verifysaplingspend)
                zcash_rpc zcbenchmark verifysaplingspend 1
                ;;
            createsaplingoutput)
                zcash_rpc zcbenchmark createsaplingoutput 1
                ;;
            verifysaplingoutput)
                zcash_rpc zcbenchmark verifysaplingoutput 1
                ;;
            createjoinsplit)
                zcash_rpc_veryslow zcbenchmark createjoinsplit 1 "${@:3}"
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1 "\"$RAWJOINSPLIT\""
                ;;
            solveequihash)
                zcash_rpc_veryslow zcbenchmark solveequihash 1 "${@:3}"
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
            connectblockslow)
                extract_benchmark_data_107134
                zcash_rpc zcbenchmark connectblockslow 1
                ;;
            connectblocksapling)
                extract_benchmark_data_1723244
                zcash_rpc zcbenchmark connectblocksapling 1
                ;;
            connectblockorchard)
                extract_benchmark_data_1708048
                zcash_rpc zcbenchmark connectblockorchard 1
                ;;
            *)
                zcashd_valgrind_stop
                echo "Bad arguments to valgrind."
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
                echo "Bad arguments to valgrind-tests."
                exit 1
        esac
        ;;
    *)
        echo "Invalid benchmark type."
        exit 1
esac

# Cleanup
rm -rf "$DATADIR"
