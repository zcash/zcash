licenses(["notice"])

cc_library(
    name = "univalue",
    visibility = ["//visibility:public"],
    hdrs = [
        "include/univalue.h",
    ],
    srcs = [
        "lib/univalue.cpp",
        "lib/univalue_escapes.h",
        "lib/univalue_read.cpp",
        "lib/univalue_utffilter.h",
        "lib/univalue_write.cpp",
    ],
    includes = ["include"],
    copts = [
        "-Wno-builtin-declaration-mismatch",
        "-Werror",
    ],
)
