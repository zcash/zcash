cc_library(
    name = "Address",
    visibility = ["//visibility:public"],
    srcs = ["Address.cpp"],
    hdrs = ["Address.hpp"],
    deps = [
        ":NoteEncryption",
        ":prf",
        "//src:hash",
        "//src:serialize",
        "//src:streams",
        "//src:uint252",
        "//src:uint256",
    ],
)

cc_binary(
    name = "CreateJoinSplit",
    visibility = ["//visibility:public"],
    srcs = ["CreateJoinSplit.cpp"],
    deps = [
        ":JoinSplit",
        "//src/primitives:transaction",
        "//src/snark:libsnark",
    ],
)

cc_binary(
    name = "GenerateParams",
    visibility = ["//visibility:public"],
    srcs = ["GenerateParams.cpp"],
    deps = [
        ":JoinSplit",
        "//src/crypto:common",
    ],
)

cc_library(
    name = "IncrementalMerkleTree",
    visibility = ["//visibility:public"],
    srcs = ["IncrementalMerkleTree.cpp"],
    hdrs = ["IncrementalMerkleTree.hpp"],
    deps = [
        ":util",
        ":Zcash",
        "//src:serialize",
        "//src:uint256",
        "//src/crypto:sha256",
        "@boost//:foreach",
        "@boost//:optional",
        "@boost//:static_assert",
    ],
)

cc_library(
    name = "JoinSplit",
    visibility = ["//visibility:public"],
    srcs = ["JoinSplit.cpp"],
    hdrs = ["JoinSplit.hpp"],
    deps = [
        ":Address",
        ":IncrementalMerkleTree",
        ":Note",
        ":NoteEncryption",
        ":prf",
        ":Proof",
        ":util",
        ":Zcash",
        "//src:amount",
        "//src:tinyformat",
        "//src:uint252",
        "//src:uint256",
        "//src:util",
        "//src/snark:libsnark",
        "//src/zcash/circuit",
        "@boost//:array",
        "@boost//:foreach",
        "@boost//:format",
        "@boost//:optional",
        "@libsodium//:sodium",
    ],
)

cc_library(
    name = "Note",
    visibility = ["//visibility:public"],
    srcs = ["Note.cpp"],
    hdrs = ["Note.hpp"],
    deps = [
        ":Address",
        ":NoteEncryption",
        ":prf",
        ":util",
        ":Zcash",
        "//src:streams",
        "//src:uint256",
        "//src:version",
        "//src/crypto:sha256",
    ],
)

cc_library(
    name = "NoteEncryption",
    visibility = ["//visibility:public"],
    srcs = ["NoteEncryption.cpp"],
    hdrs = ["NoteEncryption.hpp"],
    deps = [
        ":prf",
        ":Zcash",
        "//src:uint252",
        "//src:uint256",
        "@boost//:array",
        "@boost//:static_assert",
        "@libsodium//:sodium",
    ],
)

cc_library(
    name = "prf",
    visibility = ["//visibility:public"],
    srcs = ["prf.cpp"],
    hdrs = ["prf.h"],
    deps = [
        "//src:uint252",
        "//src:uint256",
        "//src/crypto:sha256",
    ],
)

cc_library(
    name = "Proof",
    visibility = ["//visibility:public"],
    srcs = ["Proof.cpp"],
    hdrs = ["Proof.hpp"],
    deps = [
        "//src:serialize",
        "//src:uint256",
        "//src/crypto:common",
        "//src/snark:libsnark",
        "@boost//:static_assert",
    ],
)

cc_library(
    name = "util",
    visibility = ["//src/gtest:__pkg__"],
    srcs = ["util.cpp"],
    hdrs = ["util.h"],
)

cc_library(
    name = "Zcash",
    visibility = ["//visibility:public"],
    hdrs = ["Zcash.h"],
)
