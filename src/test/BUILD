cc_test(
    name = "accounting_tests",
    size = "small",
    srcs = ["accounting_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:main",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "addrman_tests",
    size = "small",
    srcs = ["addrman_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:addrman",
        "//src:hash",
        "//src:util",
        "@boost//:test",
    ],
)

cc_test(
    name = "alert_tests",
    size = "small",
    srcs = ["alert_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:alertkeys",
        "//src:clientversion",
        "//src:key",
        "//src:main",
        "//src:rpcprotocol",
        "//src:serialize",
        "//src:streams",
        "//src:util",
        "//src:utilstrencodings",
        "//src/test/data:alertTests",
        "@boost//:filesystem",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "allocator_tests",
    size = "small",
    srcs = ["allocator_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:util",
        "//src/support/allocators:secure",
        "@boost//:test",
    ],
)

cc_test(
    name = "arith_uint256_tests",
    size = "small",
    srcs = ["arith_uint256_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:arith_uint256",
        "//src:uint256",
        "//src:version",
        "@boost//:test",
    ],
)

cc_test(
    name = "base32_tests",
    size = "small",
    srcs = ["base32_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:utilstrencodings",
        "@boost//:test",
    ],
)

cc_test(
    name = "base58_tests",
    size = "small",
    srcs = ["base58_tests.cpp"],
    deps = [
        ":test_bitcoin",
        ":test_util",
        "//src:key",
        "//src:main",
        "//src:uint256",
        "//src:util",
        "//src:utilstrencodings",
        "//src/script",
        "//src/test/data:base58_encode_decode",
        "//src/test/data:base58_keys_invalid",
        "//src/test/data:base58_keys_valid",
        "//src/univalue",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "base64_tests",
    size = "small",
    srcs = ["base64_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:utilstrencodings",
        "@boost//:test",
    ],
)

cc_library(
    name = "bignum",
    hdrs = ["bignum.h"],
    deps = [
        "@openssl//:crypto",
    ],
)

cc_test(
    name = "bip32_tests",
    size = "small",
    srcs = ["bip32_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:key",
        "//src:main",
        "//src:uint256",
        "//src:util",
        "//src:utilstrencodings",
        "@boost//:test",
    ],
)

py_test(
    name = "bitcoin_util_test",
    srcs = [
        "bitcoin-util-test.py",
        "bctest.py",
    ],
    data = [
        "//src/test/data:bitcoin_util_test_files",
        "//src:zcash-tx",
    ],
    deps = [
        ":buildenv",
    ],
    args = [
        "--zcash-tx=$(location //src:zcash-tx)",
    ],
    main = "bitcoin-util-test.py",
)

cc_test(
    name = "bloom_tests",
    size = "small",
    srcs = ["bloom_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:bloom",
        "//src:clientversion",
        "//src:key",
        "//src:main",
        "//src:merkleblock",
        "//src:serialize",
        "//src:streams",
        "//src:uint256",
        "//src:util",
        "//src:utilstrencodings",
        "@boost//:test",
        "@boost//:tuple",
    ],
)

py_library(
    name = "buildenv",
    srcs = ["buildenv.py"],
    imports = ["."],
)

genrule(
    name = "buildenv_file",
    outs = ["buildenv.py"],
    cmd = r"""echo "exeext=\"\"" > $@""",
)

cc_test(
    name = "checkblock_tests",
    size = "small",
    srcs = ["checkblock_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:clientversion",
        "//src:main",
        "//src:utiltime",
        "//src/consensus:validation",
        "//src/zcash:Proof",
        "@boost//:filesystem",
        "@boost//:test",
    ],
)

cc_test(
    name = "coins_tests",
    size = "small",
    srcs = ["coins_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:hash",
        "//src:main",
        "//src:uint256",
        "//src:undo",
        "//src:util",
        "//src:utilstrencodings",
        "//src/consensus:validation",
        "//src/script:standard",
        "//src/zcash:IncrementalMerkleTree",
        "@boost//:test",
    ],
)

cc_test(
    name = "compress_tests",
    size = "small",
    srcs = ["compress_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:compressor",
        "//src:util",
        "@boost//:test",
    ],
)

cc_test(
    name = "crypto_tests",
    size = "small",
    srcs = ["crypto_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:util",
        "//src:utilstrencodings",
        "//src/crypto:hmac_sha256",
        "//src/crypto:hmac_sha512",
        "//src/crypto:ripemd160",
        "//src/crypto:sha1",
        "//src/crypto:sha256",
        "//src/crypto:sha512",
        "@boost//:assign",
        "@boost//:test",
    ],
)

cc_test(
    name = "DoS_tests",
    size = "small",
    srcs = ["DoS_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:keystore",
        "//src:main",
        "//src:serialize",
        "//src:util",
        "//src/script:sign",
        "@boost//:assign",
        "@boost//:date_time",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "equihash_tests",
    size = "small",
    srcs = ["equihash_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:arith_uint256",
        "//src:uint256",
        "//src/config:bitcoin-config",
        "//src/crypto:equihash",
        "//src/crypto:sha256",
        "@boost//:test",
        "@libsodium//:sodium",
    ],
)

cc_test(
    name = "getarg_tests",
    size = "small",
    srcs = ["getarg_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:util",
        "@boost//:algorithm",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "hash_tests",
    size = "small",
    srcs = ["hash_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:hash",
        "//src:utilstrencodings",
        "@boost//:test",
    ],
)

cc_test(
    name = "key_tests",
    size = "small",
    srcs = ["key_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:key",
        "//src:main",
        "//src:uint256",
        "//src:util",
        "//src:utilstrencodings",
        "//src/script",
        "//src/zcash:Address",
        "@boost//:test",
    ],
)

cc_test(
    name = "main_tests",
    size = "small",
    srcs = ["main_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:main",
        "@boost//:signals2",
        "@boost//:test",
    ],
)

cc_test(
    name = "mempool_tests",
    size = "small",
    srcs = ["mempool_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:main",
        "//src:util",
        "@boost//:test",
    ],
)

cc_test(
    name = "miner_tests",
    size = "small",
    srcs = ["miner_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:arith_uint256",
        "//src:hash",
        "//src:main",
        "//src:uint256",
        "//src:util",
        "//src/consensus:validation",
        "//src/crypto:equihash",
        "@boost//:test",
    ],
)

cc_test(
    name = "mruset_tests",
    size = "small",
    srcs = ["mruset_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:mruset",
        "//src:util",
        "@boost//:test",
    ],
)

cc_test(
    name = "multisig_tests",
    size = "small",
    srcs = ["multisig_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:key",
        "//src:keystore",
        "//src:main",
        "//src:uint256",
        "//src:wallet_wallet_ismine",
        "//src/script",
        "//src/script:interpreter",
        "//src/script:script_error",
        "//src/script:sign",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "netbase_tests",
    size = "small",
    srcs = ["netbase_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:netbase",
        "@boost//:assign",
        "@boost//:test",
    ],
)

cc_test(
    name = "pmt_tests",
    size = "small",
    srcs = ["pmt_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:arith_uint256",
        "//src:merkleblock",
        "//src:serialize",
        "//src:streams",
        "//src:uint256",
        "//src:util",
        "//src:version",
        "@boost//:assign",
        "@boost//:test",
    ],
)

cc_test(
    name = "policyestimator_tests",
    size = "small",
    srcs = ["policyestimator_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:main",
        "//src:uint256",
        "//src:util",
        "@boost//:test",
    ],
)

cc_test(
    name = "pow_tests",
    size = "small",
    srcs = ["pow_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:main",
        "//src:util",
        "@boost//:test",
    ],
)

cc_test(
    name = "raii_event_tests",
    size = "small",
    srcs = ["raii_event_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src/support:events",
        "@boost//:test",
        "@libevent//:event",
    ],
)

cc_test(
    name = "reverselock_tests",
    size = "small",
    srcs = ["reverselock_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:reverselock",
        "@boost//:test",
    ],
)

cc_test(
    name = "rpc_tests",
    size = "small",
    srcs = ["rpc_tests.cpp"],
    deps = [
        ":test_bitcoin",
        ":test_util",
        "//src:main",
        "//src:netbase",
        "//src:rpcclient",
        "//src/univalue",
        "@boost//:test",
    ],
)

cc_test(
    name = "rpc_wallet_tests",
    size = "enormous",
    timeout = "short",
    srcs = ["rpc_wallet_tests.cpp"],
    deps = [
        ":test_bitcoin",
        ":test_util",
        "//src:asyncrpcoperation",
        "//src:asyncrpcqueue",
        "//src:main",
        "//src:rpcclient",
        "//src:rpcprotocol",
        "//src/univalue",
        "//src/zcash:Address",
        "@boost//:algorithm",
        "@boost//:filesystem",
        "@boost//:format",
        "@boost//:optional",
        "@boost//:test",
    ],
)

cc_test(
    name = "sanity_tests",
    size = "small",
    srcs = ["sanity_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:key",
        "//src/compat:sanity",
        "@boost//:test",
    ],
)

cc_test(
    name = "scheduler_tests",
    size = "small",
    srcs = ["scheduler_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:scheduler",
        "//src:util",
        "@boost//:bind",
        "@boost//:random",
        "@boost//:test",
        "@boost//:thread",
    ],
)

cc_test(
    name = "script_P2SH_tests",
    size = "small",
    srcs = ["script_P2SH_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:key",
        "//src:keystore",
        "//src:main",
        "//src:wallet_wallet_ismine",
        "//src/script",
        "//src/script:script_error",
        "//src/script:sign",
        "@boost//:test",
    ],
)

cc_test(
    name = "script_tests",
    size = "small",
    srcs = ["script_tests.cpp"],
    deps = [
        ":test_bitcoin",
        ":test_util",
        "//src:key",
        "//src:keystore",
        "//src:main",
        "//src:util",
        "//src/script",
        "//src/script:script_error",
        "//src/script:sign",
        "//src/script:zcashconsensus",
        "//src/test/data:script_invalid",
        "//src/test/data:script_valid",
        "//src/univalue",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "scriptnum_tests",
    size = "small",
    srcs = ["scriptnum_tests.cpp"],
    deps = [
        ":bignum",
        ":test_bitcoin",
        "//src/script",
        "@boost//:test",
    ],
)

cc_test(
    name = "serialize_tests",
    size = "small",
    srcs = ["serialize_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:hash",
        "//src:serialize",
        "//src:streams",
        "//src:utilstrencodings",
        "@boost//:optional",
        "@boost//:test",
    ],
)

cc_test(
    name = "sha256compress_tests",
    size = "small",
    srcs = ["sha256compress_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:uint256",
        "//src/crypto:sha256",
        "@boost//:test",
    ],
)

cc_test(
    name = "sighash_tests",
    size = "enormous",
    timeout = "moderate",
    srcs = ["sighash_tests.cpp"],
    deps = [
        ":test_bitcoin",
        ":test_util",
        "//src:main",
        "//src:serialize",
        "//src:util",
        "//src:version",
        "//src/consensus:validation",
        "//src/script",
        "//src/script:interpreter",
        "//src/test/data:sighash",
        "//src/univalue",
        "@boost//:test",
        "@libsodium//:sodium",
    ],
)

cc_test(
    name = "sigopcount_tests",
    size = "small",
    srcs = ["sigopcount_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:hash",
        "//src:key",
        "//src:uint256",
        "//src/script",
        "//src/script:standard",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "skiplist_tests",
    size = "small",
    srcs = ["skiplist_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:main",
        "//src:util",
        "@boost//:test",
    ],
)

cc_library(
    name = "test_bitcoin",
    visibility = ["//src/wallet/test:__pkg__"],
    hdrs = ["test_bitcoin.h"],
    srcs = ["test_bitcoin.cpp"],
    deps = [
        "//src:hash",
        "//src:key",
        "//src:main",
        "//src:ui_interface",
        "//src:util",
        "//src:wallet_db",
        "//src/crypto:common",
        "@boost//:filesystem",
        "@boost//:test",
        "@boost//:thread",
    ],
    data = [
        "@sprout-params//:sprout_proving_key",
        "@sprout-params//:sprout_verifying_key",
    ],
    copts = [
        "-DBAZEL_BUILD",
        r"-DSPROUT_PROVING_KEY=\"$(location @sprout-params//:sprout_proving_key)\"",
        r"-DSPROUT_VERIFYING_KEY=\"$(location @sprout-params//:sprout_verifying_key)\"",
    ],
)

cc_library(
    name = "test_util",
    visibility = ["//src/wallet/test:__pkg__"],
    hdrs = ["test_util.h"],
    srcs = ["test_util.cpp"],
    deps = [
        "//src:main",
        "//src:rpcclient",
        "//src/script:interpreter",
        "//src/univalue",
        "@boost//:algorithm",
        "@boost//:assign",
        "@boost//:foreach",
        "@boost//:test",
    ],
)

cc_test(
    name = "timedata_tests",
    size = "small",
    srcs = ["timedata_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:timedata",
        "@boost//:test",
    ],
)

cc_test(
    name = "torcontrol_tests",
    size = "small",
    srcs = ["torcontrol_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:main",
        "@boost//:test",
    ],
)

cc_test(
    name = "transaction_tests",
    size = "enormous",
    timeout = "moderate",
    srcs = ["transaction_tests.cpp"],
    deps = [
        ":test_bitcoin",
        ":test_util",
        "//src:clientversion",
        "//src:key",
        "//src:keystore",
        "//src:main",
        "//src/consensus:validation",
        "//src/primitives:transaction",
        "//src/script",
        "//src/script:script_error",
        "//src/test/data:tx_invalid",
        "//src/test/data:tx_valid",
        "//src/univalue",
        "//src/zcash:Address",
        "//src/zcash:Note",
        "//src/zcash:Proof",
        "@boost//:test",
        "@libsodium//:sodium",
    ],
)

cc_test(
    name = "uint256_tests",
    size = "small",
    srcs = ["uint256_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:arith_uint256",
        "//src:uint256",
        "//src:version",
        "@boost//:test",
    ],
)

cc_test(
    name = "univalue_tests",
    size = "small",
    srcs = ["univalue_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src/univalue",
        "@boost//:test",
    ],
)

cc_test(
    name = "util_tests",
    size = "small",
    srcs = ["util_tests.cpp"],
    deps = [
        ":test_bitcoin",
        "//src:clientversion",
        "//src:util",
        "//src:utilmoneystr",
        "//src:utilstrencodings",
        "//src/primitives:transaction",
        "@boost//:test",
    ],
)
