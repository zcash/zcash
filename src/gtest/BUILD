cc_library(
    name = "json_test_vectors",
    hdrs = ["json_test_vectors.h"],
    srcs = ["json_test_vectors.cpp"],
    deps = [
        "//src:serialize",
        "//src:streams",
        "//src:utilstrencodings",
        "//src:version",
        "//src/univalue",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "main",
    tags = ["buildcleaner-keep"],
    srcs = ["main.cpp"],
    deps = [
        "//src:hash",
        "//src:util",
        "//src/crypto:common",
        "//src/snark:libsnark",
        "//src/zcash:JoinSplit",
        "@com_google_googletest//:gtest",
    ],
    data = [
        "@sprout-params//:sprout_proving_key",
        "@sprout-params//:sprout_verifying_key",
    ],
    copts = [
        r"-DSPROUT_PROVING_KEY=\"$(location @sprout-params//:sprout_proving_key)\"",
        r"-DSPROUT_VERIFYING_KEY=\"$(location @sprout-params//:sprout_verifying_key)\"",
    ],
)

cc_test(
    name = "test_checkblock",
    size = "small",
    srcs = ["test_checkblock.cpp"],
    deps = [
        ":main",
        "//src:main",
        "//src/consensus:validation",
        "//src/zcash:Proof",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_checktransaction",
    size = "small",
    srcs = ["test_checktransaction.cpp"],
    deps = [
        ":main",
        "//src:main",
        "//src/consensus:validation",
        "//src/primitives:transaction",
        "@com_google_googletest//:gtest",
        "@libsodium//:sodium",
    ],
)

cc_test(
    name = "test_circuit",
    size = "enormous",
    timeout = "moderate",
    srcs = ["test_circuit.cpp"],
    deps = [
        ":main",
        "//src:uint256",
        "//src/snark:libsnark",
        "//src/zcash:IncrementalMerkleTree",
        "//src/zcash:util",
        "//src/zcash/circuit",
        "@boost//:foreach",
        "@boost//:format",
        "@boost//:optional",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_deprecation",
    size = "small",
    srcs = ["test_deprecation.cpp"],
    deps = [
        ":main",
        "//src:clientversion",
        "//src:main",
        "//src:ui_interface",
        "//src:util",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_equihash",
    size = "small",
    srcs = ["test_equihash.cpp"],
    deps = [
        ":main",
        "//src:uint256",
        "//src/config:bitcoin-config",
        "//src/crypto:equihash",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_foundersreward",
    size = "small",
    srcs = ["test_foundersreward.cpp"],
    deps = [
        ":main",
        "//src:amount",
        "//src:main",
        "//src:util",
        "//src:utilmoneystr",
        "//src:utilstrencodings",
        "//src/zcash:Address",
        "@boost//:filesystem",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_httprpc",
    size = "small",
    srcs = ["test_httprpc.cpp"],
    deps = [
        ":main",
        "//src:httpserver",
        "//src:main",
        "@com_google_googletest//:gtest",
    ],
    copts = ["-DBAZEL_BUILD"],
)

cc_test(
    name = "test_joinsplit",
    srcs = ["test_joinsplit.cpp"],
    deps = [
        ":main",
        "//src:util",
        "//src:utilstrencodings",
        "//src/zcash:IncrementalMerkleTree",
        "//src/zcash:JoinSplit",
        "//src/zcash:Note",
        "//src/zcash:NoteEncryption",
        "//src/zcash:prf",
        "@boost//:foreach",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_keystore",
    size = "small",
    srcs = ["test_keystore.cpp"],
    deps = [
        ":main",
        "//src:keystore",
        "//src:util",
        "//src:wallet_crypter",
        "//src/zcash:Address",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_libzcash_utils",
    size = "small",
    srcs = ["test_libzcash_utils.cpp"],
    deps = [
        ":main",
        "//src/zcash:util",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_mempool",
    size = "small",
    srcs = ["test_mempool.cpp"],
    deps = [
        ":main",
        "//src:main",
        "//src:util",
        "//src/consensus:validation",
        "//src/primitives:transaction",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_merkletree",
    srcs = ["test_merkletree.cpp"],
    deps = [
        ":json_test_vectors",
        ":main",
        "//src:serialize",
        "//src:streams",
        "//src:utilstrencodings",
        "//src:version",
        "//src/snark:libsnark",
        "//src/test/data:merkle_commitments",
        "//src/test/data:merkle_path",
        "//src/test/data:merkle_roots",
        "//src/test/data:merkle_roots_empty",
        "//src/test/data:merkle_serialization",
        "//src/test/data:merkle_witness_serialization",
        "//src/zcash:IncrementalMerkleTree",
        "//src/zcash:util",
        "@boost//:foreach",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_metrics",
    size = "small",
    srcs = ["test_metrics.cpp"],
    deps = [
        ":main",
        "//src:main",
        "//src:utiltime",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_miner",
    size = "small",
    srcs = ["test_miner.cpp"],
    deps = [
        ":main",
        "//src:key",
        "//src:main",
        "//src:util",
        "@boost//:optional",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_noteencryption",
    size = "small",
    srcs = ["test_noteencryption.cpp"],
    deps = [
        ":main",
        "//src/crypto:sha256",
        "//src/zcash:NoteEncryption",
        "//src/zcash:prf",
        "@com_google_googletest//:gtest",
        "@libsodium//:sodium",
    ],
)

cc_test(
    name = "test_paymentdisclosure",
    size = "small",
    srcs = ["test_paymentdisclosure.cpp"],
    deps = [
        ":main",
        "//src:amount",
        "//src:main",
        "//src:util",
        "//src:utilmoneystr",
        "//src:utilstrencodings",
        "//src/zcash:Address",
        "@boost//:filesystem",
        "@boost//:uuid",
        "@com_google_googletest//:gtest",
        "@libsodium//:sodium",
    ],
)

cc_test(
    name = "test_pow",
    size = "small",
    srcs = ["test_pow.cpp"],
    deps = [
        ":main",
        "//src:main",
        "//src:util",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_proofs",
    srcs = ["test_proofs.cpp"],
    deps = [
        ":json_test_vectors",
        ":main",
        "//src:streams",
        "//src:utilstrencodings",
        "//src:version",
        "//src/snark:libsnark",
        "//src/test/data:g1_compressed",
        "//src/test/data:g2_compressed",
        "//src/zcash:Proof",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_random",
    size = "small",
    srcs = ["test_random.cpp"],
    deps = [
        ":main",
        ":utils",
        "//src:util",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_rpc",
    size = "small",
    srcs = ["test_rpc.cpp"],
    deps = [
        ":main",
        "//src:clientversion",
        "//src:main",
        "//src:streams",
        "//src:utilstrencodings",
        "//src/primitives:block",
        "//src/univalue",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_tautology",
    size = "small",
    srcs = ["test_tautology.cpp"],
    deps = [
        ":main",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_transaction",
    size = "small",
    srcs = ["test_transaction.cpp"],
    deps = [
        ":main",
        ":utils",
        "//src/primitives:transaction",
        "//src/zcash:Address",
        "//src/zcash:Note",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_txid",
    size = "small",
    srcs = ["test_txid.cpp"],
    deps = [
        ":main",
        "//src:clientversion",
        "//src:serialize",
        "//src:streams",
        "//src:uint256",
        "//src:util",
        "//src:utilstrencodings",
        "//src/primitives:transaction",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_validation",
    size = "small",
    srcs = ["test_validation.cpp"],
    deps = [
        ":main",
        "//src:main",
        "//src/consensus:validation",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "utils",
    hdrs = ["utils.h"],
    srcs = ["utils.cpp"],
)
