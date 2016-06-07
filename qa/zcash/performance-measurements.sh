#!/bin/bash

set -e

DATADIR=./benchmark-datadir

function zcash_rpc {
    ./src/zcash-cli -rpcwait -rpcuser=user -rpcpassword=password -rpcport=5983 "$@"
}

function zcashd_generate {
    zcash_rpc generate 101 > /dev/null
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

function zcashd_massif_start_chain {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    rm -f massif.out
    ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    ZCASHD_PID=$!
    zcashd_generate
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
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
    valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    ZCASHD_PID=$!
}

function zcashd_valgrind_start_chain {
    rm -rf "$DATADIR"
    mkdir -p "$DATADIR"
    rm -f valgrind.out
    ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    ZCASHD_PID=$!
    zcashd_generate
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
    valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    ZCASHD_PID=$!
}

function zcashd_valgrind_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
    cat valgrind.out
}

RAWTXWITHPOUR=0200000000000000000001000000000000000000000000000000000323f2850bf3444f4b4c5c09a6057ec7169190f45acb9e46984ab3dfcec4f06a4ca90fde5d902715a99c7941401c0e5648747cf504a5cbe3bdab88c32039c5aa5da9f762a8f80312931db58afe544fcb5d83da6cf2d08e1f7a383efb7a653d417e071560f9a07e379a9db1d4dc4a47ca2ac6e3af47600118461a7ba66350141662ede694951d57a6b826f5b0200137b9ab50ed7b5f6a1e837b291567c5cc5e5e6cae9f20d7730fb03dfa688b23b4957a1b0e7a89aad7decc778dff0bc0add47e8d93f457c548d9c0689f69491d931f06c000b55a0a87675353fbf187610d7255f662cc66524d5eb3b51f024ef7bda310915b835fba21eeeaa1cd39adf9f3045273d648b955ef270496385d6128386cd23f8bec8647942fe5750ca69c65c2fdad63060ddac4290ae85d5fbeacd392c59781a5fb640ac7c00aee10a4b1f64e3e1840fcb249ac2b4470afab4c0dbb8a87c033de2b583684e2eb2c5a4343b3983a4ded84b883902ec5bd1a9a6c536344b6d4a1e36ed738dbbe65b7515e95712837d1c5e4bf1f12ffcaf96a97af127283290e96930b3e629c4813439619ab3754af68eb85571e1e8a351a9b4d33736a75b5291911de3a68934c03495a706c3f9ce75f2721b03196cc4c336d3c5bc94789282cda62d45a57ef73716ac9e4ea5c6f22a054eb2ac81e9cf80474f7b5e4d23dabad1744ca850fd0ed63dd88b2a46b30d7c63aae899201f8e5b2a1c5321ff4bcfd019ab210c1e7b778ca40b0b86f5cfff527964ab9767b24b6c647fea3655e959b459dcde72a89526746c91cd32f049ba84ad483c4584fc17890b894091a5ca10a3833154c216d1934c4a36fa103a3ef0e889437f1f17056441fa34df7bfc98bda8475da37779901cf1d340cf3cadbf72ca62663bb9ffbd3a2804c689f3aa352b52469189fcbe0defc3a31fe53c5497b568853b7d64337712f23cd479664b5f5ae566e9744ad2463f729fbe9cf4906aac73c0f8266f7334a012fff3079ae13302200d7ac30ae6766768e7b418636674e32ad7870159478248a8d7eda0b7b92b49e0b357417984a158bffba6ff85efab3855a81300cad3ec3c5380dce6b0f9c72ffd4765b3030d571d527c47c6c641855abfc5bfa3b14c05a4232b899d84f45c829e6c1e9481c5e6fed7dbf2029d0d1c8e77e0abbd31ddf61aeae51d3f14a97cd63709dce681b30a575e5397d16901faa00e7bed4edd8bf60d8e1649f5338b11fed8e037ad68f119495f9d800f6fb26b21798855733fd4147314f9e3f834eecb433631a69e9fe119d35e3b64bb654ca78d3befa24af81fd34b9b585395c8e3b6fe3952cd720682f0c294e4b7a47dc68a284d61abd4276a8a0900afed40d32d4e1bb94a566766c1f3038dfa6e80a78c8dd4a0f1533bcacf591cccdc2a66905563caa4d041f179ca52dae880ab838d6d6ba2b917ebbb180704a30d279aa93ce44bf89d9aad387787610303ddf938f09974ced9e8a5c5b1162fd3c2fa7809f5add210f8bfeee0b6f6f670bb3b7b2c841cf929e262098927e91e05747fba0a66529197b21ee10faba97a0113014f1602df3d6467dc26ec462f282966c46aa8298efbb9c24103aa5bf48ef2808a241754e9e4e349df0626f9feac937221eb37606c4d40e4922827c3150ebe30330205d36d40f824c03ab45973c5a13e99fab321d755a4225a9110576edaa5b720ce968a7ecb9675031c622d3834cf5d59d6ef33b183df0a430713143df39fdf81f30a30e1d96abc49316d22567f9bd2e60d20130322eded241d97cb01cccf3bcde25314e0e0eb18258202233df9219ebfdcb1ca7005d028c48ee3e10b363fd66f2201712e06ac49c39114fcd156b7748c95b928eda60fe4e7043b1f53249281065e27dc0e75c820fb44200b18af63cab7ed6d2d93c03702a90552171f6f38cda7108a1ee5a0550771ef500ce371a8dd358316c28c032550674e4cb70edfc097f260b

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
            validatefullblock)
                zcashd_generate
                zcash_rpc zcbenchmark validatefullblock 10
                ;;
            validatefullblockacs)
                zcashd_generate
                zcash_rpc zcbenchmark validatefullblockacs 10
                ;;
            *)
                zcashd_stop
                echo "Bad arguments."
                exit 1
        esac
        zcashd_stop
        ;;
    memory)
        case "$2" in
            validatefullblock|validatefullblockacs)
                zcashd_massif_start_chain
                ;;
            *)
                zcashd_massif_start
        esac
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
            validatefullblock)
                zcash_rpc zcbenchmark validatefullblock 1
                ;;
            validatefullblockacs)
                zcash_rpc zcbenchmark validatefullblockacs 1
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
        case "$2" in
            validatefullblock|validatefullblockacs)
                zcashd_valgrind_start_chain
                ;;
            *)
                zcashd_valgrind_start
        esac
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
            validatefullblock)
                zcash_rpc zcbenchmark validatefullblock 1
                ;;
            validatefullblockacs)
                zcash_rpc zcbenchmark validatefullblockacs 1
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
