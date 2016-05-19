#!/bin/bash

set -e

DATADIR=./benchmark-datadir

function zcash_rpc {
    ./src/zcash-cli -rpcwait -rpcuser=user -rpcpassword=password -rpcport=5983 "$@"
}

function zcashd_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    ZCASHD_PID=$!
}

function zcashd_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASH_PID
}

function zcashd_massif_start {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    rm -f massif.out
    valgrind --tool=massif --time-unit=ms --massif-out-file=massif.out ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
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
    rm -f valgrind.out
    valgrind --leak-check=yes --log-file="valgrind.out" ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    ZCASHD_PID=$!
}

function zcashd_valgrind_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
    cat valgrind.out
}

RAWTXWITHPOUR=0200000000000000000001000000000000000000000000000000000323f2850bf3444f4b4c5c09a6057ec7169190f45acb9e46984ab3dfcec4f06a8d79e396e84944d99b460d3754333077a928a3c5f14ec09caa0fdd673c7fc15081dc0f4fe0d4857f4358196c95851726e04e4a95ec3611296cf420a8c71d2049981e9dc335c795cfadf4d95c4b9202a921258359fd7fd4f7850d0a806cd7e209f70177da93e9cdc2d07d20828ff131d848cc73e6f3886dfd40f0838d9374f2745f782a6cfc7aa298cc202c4db4c59dd39cda9fcedd4b2aae54686870c7c8b003b51b5451a453f8c6b9f55b3b981e978d7715a7080895f1ae2f7bbb9e8496faee51830df988fe808e3ef5f38d225cb71cb0f60a88cd921d3430a059b081fd5601148f7ca17825ad331ed5c7fdf4c17e15240ae85f762c97bba31d8dcf51753b067d1052890a55f9d83ae6e603af1d192dd0ac5d25b65c933627b615d76c34656452e4e9e19c7a414ca2cc9b66a4900d039021506a078a532d2487e752a854434246d8f5ac93d148b90dcf3b024d6d4df16ddaafb651079150fcd50b3dd2f680c0d80f7b2617426de29aa7f792c41d6f0d5552fde7eddc18d6795a9767d69e2ffcc5d6d1d0ae71a6386f12bb7df9c9d5169855805f31f42bc04d313e414ae1c4794d8079b174a2fc05f70677e8e008cd267ba71bf44c0829ec033b757cb426097f260ebbb29ed331168e1a8572473e3cfa421671f5d2b69187f17475d485f7ae0bdefdfce7af74fe2b8ffb4bbe3e1c7ab17e0f666d0c738ada456e1abc8c1dae24ed0e0791b3d7fd037c17b00f5d25abbe96c6ab0a7aecee5bc7520ae74dcc8acfde6c734b9cfae0085998ef16431b90fad0614fe67e48d859a86efdea236566350ffbf519932462df325cba7f9d7c5ad5279169de710ff6eceed2248eab72838200d249fc7ef9c429d7f774ff5e37a65f5572cf4c7956953e763e7e5b439d5cd0db5ddbaefe2943bebac008b555fd96b9425ac3d320378d7423f488ed14754894399776f7c13578e2a00d3289a82813a2718c30f3b267cb441a5dd217302c447747cb1c592b5e0ccfdc7d36e0f4150650f7b10d0336050c3e444729ab0d5e8cd860ad442d25517474c1ec5a7d1a47fab6c7190a3056a83d83525ac2d20bb462b0d57447b61fecc9c905204dda0a99d1580d757121e63c53fd6ac8364ca4caf0b1b646c51f317637725df2687c2e4dfb6587f16d1d303dc568bf6419df13b5a342bdce8ad531befaef8deee524fe81b0503e0b338812e2e1db117f13adfd9bd362021832f7e5d983687e15f13dd654251d7eb768ac17dd282065004071e472c756a8ab577eb1db77afc56d6f2466aca110d7815d8016707e7fc7e96c049112428b41627f1f456bee4fe833f48e4d9729ffa23025e31230711efd6f5fd254c076a73c2d0822267076d5fe6dc3548e423de7d1259b03d309775378033b5eaabc0c683f9d4088beb4848706a175caf9ce81cf794542259c2b3097370ae0ea118c1be181ee48703bdb7a72672d1a4e95dd562cad6207ed9201276f63557110e965ea018f3f73965aec10e4719952c2020081fc91a7b594f9c20830c7aed6b45c5dcd8b7f6f7feb66c47e5fd66d77d300b8892407ca26cef4c3061d17b291577892711bb3fba39c638113275087eb033b33ceb1a6eff0ef3023770530b93375d746f4b16f33e8d5ac8874f06cd02310dfa2949564a11e76c0b07f030ea112adacf5a0660208e182934547b1369145c103f34ba0056e76bcaf5be9091030937e784c03b28557749d3937b175fca117be92e139faf2cbab8225251ac4eb2c898f7177c21bd2abf7e6149b79fb95647975e2b6d97ecd5f854d5de5a9d52208

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
                zcash_rpc zcbenchmark createjoinsplit 10
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1000 "$RAWTXWITHPOUR"
                ;;
            solveequihash)
                zcash_rpc zcbenchmark solveequihash 10
                ;;
            verifyequihash)
                zcash_rpc zcbenchmark verifyequihash 1000
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
                zcash_rpc zcbenchmark createjoinsplit 1
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1 "$RAWTXWITHPOUR"
                ;;
            solveequihash)
                zcash_rpc zcbenchmark solveequihash 1
                ;;
            verifyequihash)
                zcash_rpc zcbenchmark verifyequihash 1
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
                zcash_rpc zcbenchmark createjoinsplit 1
                ;;
            verifyjoinsplit)
                zcash_rpc zcbenchmark verifyjoinsplit 1 "$RAWTXWITHPOUR"
                ;;
            solveequihash)
                zcash_rpc zcbenchmark solveequihash 1
                ;;
            verifyequihash)
                zcash_rpc zcbenchmark verifyequihash 1
                ;;
            *)
                zcashd_valgrind_stop
                echo "Bad arguments."
                exit 1
        esac
        zcashd_valgrind_stop
        rm -f valgrind.out
        ;;
    *)
        echo "Bad arguments."
        exit 1
esac

# Cleanup
rm -rf "$DATADIR"
