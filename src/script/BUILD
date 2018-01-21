cc_library(
    name = "interpreter",
    visibility = ["//visibility:public"],
    hdrs = ["interpreter.h"],
    srcs = ["interpreter.cpp"],
    deps = [
        ":script",
        ":script_error",
        "//src:hash",
        "//src:uint256",
        "//src/crypto:ripemd160",
        "//src/crypto:sha1",
        "//src/crypto:sha256",
        "//src/primitives:transaction",
    ],
)

cc_library(
    name = "script",
    visibility = ["//visibility:public"],
    hdrs = ["script.h"],
    srcs = ["script.cpp"],
    deps = [
        "//src:tinyformat",
        "//src:utilstrencodings",
        "//src/crypto:common",
    ],
)

cc_library(
    name = "script_error",
    visibility = ["//visibility:public"],
    hdrs = ["script_error.h"],
    srcs = ["script_error.cpp"],
)

cc_library(
    name = "sigcache",
    visibility = ["//visibility:public"],
    srcs = ["sigcache.cpp"],
    hdrs = ["sigcache.h"],
    deps = [
        ":interpreter",
        "//src:hash",
        "//src:uint256",
        "//src:util",
        "@boost//:thread",
        "@boost//:tuple",
    ],
)

cc_library(
    name = "sign",
    visibility = ["//visibility:public"],
    srcs = ["sign.cpp"],
    hdrs = ["sign.h"],
    deps = [
        ":interpreter",
        ":standard",
        "//src:key",
        "//src:keystore",
        "//src:uint256",
        "//src/primitives:transaction",
        "@boost//:foreach",
    ],
)

cc_library(
    name = "standard",
    visibility = ["//visibility:public"],
    hdrs = ["standard.h"],
    srcs = ["standard.cpp"],
    deps = [
        ":interpreter",
        ":script",
        "//src:hash",
        "//src:uint256",
        "//src:util",
        "//src:utilstrencodings",
        "@boost//:foreach",
        "@boost//:variant",
    ],
)

cc_library(
    name = "zcashconsensus",
    visibility = ["//visibility:public"],
    hdrs = ["zcashconsensus.h"],
    srcs = ["zcashconsensus.cpp"],
    copts = ["-DBUILD_BITCOIN_INTERNAL"],
    linkopts = [
        # libzcashconsensus.so only exposes a C API: No exceptions are thrown
        # across the API boundary. Because of this it is safe to statically link
        # against libgcc. Statically linking against libgcc is nice because it
        # allows the built shared library to be used on systems that have an
        # older version of GCC than our toolchain. This is important because
        # this build uses a quite recent version of GCC.
        "-static-libgcc",
    ],
    deps = [
        ":interpreter",
        "//src:hash",
        "//src:version",
        "//src/config:bitcoin-config",
        "//src/primitives:transaction",
    ],
)
