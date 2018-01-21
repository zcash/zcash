py_library(
    name = "authproxy",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["authproxy.py"],
)

py_library(
    name = "bignum",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["bignum.py"],
)

py_library(
    name = "blockstore",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["blockstore.py"],
    deps = [
        ":mininode",
    ],
)

py_library(
    name = "blocktools",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["blocktools.py"],
    deps = [
        ":mininode",
        ":script",
    ],
)

py_library(
    name = "comptool",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["comptool.py"],
    deps = [
        ":blockstore",
        ":mininode",
        ":util",
    ],
)

py_library(
    name = "equihash",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["equihash.py"],
)

py_library(
    name = "mininode",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["comptool.py"],
    deps = [
        ":equihash",
        ":util",
    ],
    data = [
        "@py_pyblake2//:pyblake2.so",
    ],
)

py_library(
    name = "netutil",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["netutil.py"],
)

py_library(
    name = "script",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["comptool.py"],
    deps = [
        ":bignum",
        ":mininode",
    ],
)

py_library(
    name = "socks5",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["socks5.py"],
)

py_library(
    name = "test_framework",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["test_framework.py"],
    deps = [
        ":authproxy",
        ":util",
    ],
)

py_library(
    name = "util",
    visibility = ["//qa/rpc_tests:__pkg__"],
    srcs = ["util.py"],
    deps = [
        ":authproxy",
    ],
)
