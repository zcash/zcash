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
    valgrind --leak-check=yes -v --error-limit=no --log-file="valgrind.out" ./src/zcashd -regtest -datadir="$DATADIR" -rpcuser=user -rpcpassword=password -rpcport=5983 &
    ZCASHD_PID=$!
}

function zcashd_valgrind_stop {
    zcash_rpc stop > /dev/null
    wait $ZCASHD_PID
    cat valgrind.out
}

RAWTXWITHPOUR=020000000000000000000100000000000000000000000000000000d7c612c817793191a1e68652121876d6b3bde40f4fa52bc314145ce6e5cdd259df64fc01b984cd264470d2493b27cb38032874c549fa188ea9ed246d881d01b6033614a202b4f7e64805076652aa036ff94997285df9ddd5a3d2b1220c08dfc9fafd306aa43429b34a0329755698ee934a358ebac3e8aaf06d51eaafb32ac82a0e0ba078e1ae3dc14231d885ffcafa66af334e8c6b9d444a52075c70466900fe6829c1bb02f56451246bedc77d271c6fcf51c1130331e5bf7691682d977af162dd79fbde8a64909ab7f2fc76d8ca9afaa9a065a88dc91d3e6606da73f58ade8e16dbded49b93002bf4aecb466a2d57bda375c4ed4e1b1f6d77f81bd4a2f0e28c2262b98b972370ffbf8742748fd3050332530b3c0a3176806c703e2e7b5530688a53be9e9cfc6228ab1e5e909848614b7279f316c5b4338289e055fbf9eb1bbecdb259d4777628e7c90e97c4c6f185f7c7ed57024e39651d94b6db8d2906d798f4fa82275abb64060264cbbd2e9657ca9934957bbf2fef7670013419ba56a2faa761bb7b4a9639f2f1a266a083d71c1a0f8e4fd3c83234c925ca255effe51a8a527a37132447366a00a57a6032cdcb0b4e85781fe6ab912e5e4f51a20764e5543a7fecec8a2ff3fb16496608f605f167aded4f71b9dc8fcb37d7006da32fc85a496e85755a13e3ed6708a80355212c028966ea3f3b4afdf28e3578d491add09a4bf04554d8486f232b7d66e0e57d4129166b8d22ec1bb7e85edf6c89ff4aa8b84b92e83625965e4fd531f6caf9c48656fdf6c6ee6b57a947c203213146103ea966fb907a9fd2a7ec3d90179a47dac944792055ff22a04f0c8841623159909f0f9d1ef3542aa9776723bce6b7ad1c8c4906494cccd226935f04ab6335a7c6807f9eaed8fc14183cbec8090ce10c1eb389a720034d96c6a4fad081ca70ad0b5a5ee0250b37d0c5745eb8fd8bfbb284ceb8df4d9ed8136f4c46fd0fb50ae8328bc8e55eae2ac986392e87658fae606647b7884b30fb95621c378ad36b3ace9bcfeaaf47b38fc815a604526e52995bf17448c91e2a3b072fdaf717d7dd4a2743c3a0460e65a2d3520313e1aafb0894ce7be6829b2f30b4f2a541dbbe37b455383a0d9ec2d06d9e2e2872935b7c77bc00dc9185b40e23c53041d4024a5e24747b84a7acf8abb6906eb2f05e5e82b7a995ce727e9d3a04304f46df202583aa09c440f712d08eb93c16de89d761baed8794d14cfc6a41000b10e835ee6ba661bb0da8f3811c85920d1a069055488eb8a10301c22a4d20bc297a2687e805e2988fdefb48d94bac775f39505f92c45bc7f02d1cda3357306909cd6c5fbaefc987b556a406cf7a1bef6867a3d809063332db8aa5c4e08380610a30f2629b1d1d8f0f2b8d2cd37d611f574c074bbc1236436667208d5077b08d0303c24de96169fba61de2d6c1e8def6e5ac409b3dc36eaafa25425f073d63a2d21d3095ff55cfd9c8e1dd1f9071d3d35465b403470ff2c631a9cbc3654a8a6279680c6cd975a8a6a7bf8971171b9eb1b5f6c9de58889022ca32e2d9df7280e8a1fd12306ed060c4748e4e1205f0eeaa30a6e06cd9f25fa2f1407d935846fd0101b80400f09065198362eeef213d974edfcb60a99230c05d7d572a16f78a64124ea1480c30531cfc54ef4adb571eef63a5feb614d79f840f17a4e84a5dfceae86679ac93015ac290d8e47b08215381d83f7116b82d4244231bc3ea50394f7e86cc4913810e3081e54cbf537fba6bb3191252b75393f33fe5c6bcaa73bc088c3fcf3e23ab4c13329f89f42094b05e57e8a10d8e61962cd200cd11470e1bc20ccfc05006f38517b8b000e40fc91358db1919054aa8c2b470a23da91f6469ca271fc44852b3cb4527f5c4cfc2ba376da776412b4b900cc3ea3da80205824631f6acab1f56c5d9cbeb96d72577e4428fccee81e2111802c7ce1ecf8b99ad3f22fd4aca719a48ce0b

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
