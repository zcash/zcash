cc_library(
    name = "cleanse",
    visibility = ["//visibility:public"],
    srcs = ["cleanse.cpp"],
    hdrs = ["cleanse.h"],
    deps = [
        "@openssl//:crypto",
    ],
)

cc_library(
    name = "events",
    visibility = ["//visibility:public"],
    hdrs = ["events.h"],
    deps = [
        "@libevent//:event",
    ],
)

cc_library(
    name = "pagelocker",
    visibility = ["//visibility:public"],
    srcs = ["pagelocker.cpp"],
    hdrs = ["pagelocker.h"],
    deps = [
        ":cleanse",
        "//src/config:bitcoin-config",
        "@boost//:thread",
    ],
)
