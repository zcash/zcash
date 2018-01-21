cc_library(
    name = "common",
    visibility = ["//visibility:public"],
    hdrs = ["common.h"],
    deps = [
        "//src/compat:endian",
        "//src/config:bitcoin-config",
        "@libsodium//:sodium",
    ],
)

cc_library(
    name = "equihash",
    visibility = ["//visibility:public"],
    srcs = ["equihash.cpp"],
    hdrs = [
        "equihash.h",
        "equihash.tcc",
    ],
    deps = [
        ":sha256",
        "//src:util",
        "//src:utilstrencodings",
        "//src/compat:endian",
        "//src/config:bitcoin-config",
        "@boost//:optional",
        "@boost//:static_assert",
        "@libsodium//:sodium",
    ],
)

cc_library(
    name = "hmac_sha256",
    visibility = ["//visibility:public"],
    srcs = ["hmac_sha256.cpp"],
    hdrs = ["hmac_sha256.h"],
    deps = [
        ":sha256",
    ],
)

cc_library(
    name = "hmac_sha512",
    visibility = ["//visibility:public"],
    srcs = ["hmac_sha512.cpp"],
    hdrs = ["hmac_sha512.h"],
    deps = [
        ":sha512",
    ],
)

cc_library(
    name = "ripemd160",
    visibility = ["//visibility:public"],
    srcs = ["ripemd160.cpp"],
    hdrs = ["ripemd160.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "sha1",
    visibility = ["//visibility:public"],
    srcs = ["sha1.cpp"],
    hdrs = ["sha1.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "sha256",
    visibility = ["//visibility:public"],
    srcs = ["sha256.cpp"],
    hdrs = ["sha256.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "sha512",
    visibility = ["//visibility:public"],
    srcs = ["sha512.cpp"],
    hdrs = ["sha512.h"],
    deps = [
        ":common",
    ],
)
