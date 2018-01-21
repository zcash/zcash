cc_library(
    name = "byteswap",
    visibility = ["//visibility:public"],
    hdrs = ["byteswap.h"],
    deps = [
        "//src/config:bitcoin-config",
    ],
)

cc_library(
    name = "endian",
    visibility = ["//visibility:public"],
    hdrs = ["endian.h"],
    deps = [
        ":byteswap",
        "//src/config:bitcoin-config",
    ],
)

cc_library(
    name = "sanity",
    visibility = ["//visibility:public"],
    hdrs = ["sanity.h"],
    srcs = [
        "glibc_sanity.cpp",
        "glibcxx_sanity.cpp",
    ],
    deps = [
        "//src/config:bitcoin-config",
    ],
)
