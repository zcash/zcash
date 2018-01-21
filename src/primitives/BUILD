cc_library(
    name = "block",
    visibility = ["//visibility:public"],
    hdrs = ["block.h"],
    srcs = ["block.cpp"],
    deps = [
        ":transaction",
        "//src:hash",
        "//src:serialize",
        "//src:tinyformat",
        "//src:uint256",
        "//src:utilstrencodings",
        "//src/crypto:common",
    ],
)

cc_library(
    name = "transaction",
    visibility = ["//visibility:public"],
    hdrs = ["transaction.h"],
    srcs = ["transaction.cpp"],
    deps = [
        "//src:amount",
        "//src:hash",
        "//src:serialize",
        "//src:tinyformat",
        "//src:uint256",
        "//src:util",
        "//src:utilstrencodings",
        "//src/consensus",
        "//src/script",
        "//src/zcash:JoinSplit",
        "//src/zcash:NoteEncryption",
        "//src/zcash:Proof",
        "//src/zcash:Zcash",
        "@boost//:array",
    ],
)
