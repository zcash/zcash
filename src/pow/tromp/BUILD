cc_library(
    name = "equi",
    hdrs = ["equi.h"],
    deps = [
        ":osx_barrier",
        "//src/compat:endian",
        "@libsodium//:sodium",
    ],
)

cc_library(
    name = "equi_miner",
    visibility = ["//visibility:public"],
    hdrs = ["equi_miner.h"],
    deps = [
        ":equi",
    ],
)

cc_library(
    name = "osx_barrier",
    hdrs = ["osx_barrier.h"],
)
