# This file is part of the Bazel based build system.

load("//tools:params.bzl", "fetch_sprout_params")
fetch_sprout_params(
    name = "sprout-params",
)

http_archive(
    name = "io_bazel_rules_rust",
    sha256 = "5cf8b372e1c61bc42e7975fe1deb05daea9c2005a29f1dacbe423cb9a709c0a8",
    strip_prefix = "bazelbuild-rules_rust-674dcd9",
    type = "tar.gz",
    urls = ["https://api.github.com/repos/bazelbuild/rules_rust/tarball/674dcd95ac8f7f5c67fbbfaada5ae558cc456b2c"],
)
load("@io_bazel_rules_rust//rust:repositories.bzl", "rust_repositories")
rust_repositories()

new_http_archive(
    name = "bdb",
    strip_prefix = "db-6.2.23",
    urls = ["http://download.oracle.com/berkeley-db/db-6.2.23.tar.gz"],
    sha256 = "47612c8991aa9ac2f6be721267c8d3cdccf5ac83105df8e50809daea24e95dc7",
    build_file = "tools/depends/generated/bdb.bazel",
)

http_archive(
    name = "com_github_nelhage_rules_boost",
    strip_prefix = "rules_boost-34c774f086af0cb67f9ad8d023ce2598abb4b084",
    # TODO(per-gron): When https://github.com/nelhage/rules_boost/pull/43 is
    # merged, point this to the main repo.
    urls = ["https://github.com/per-gron/rules_boost/archive/34c774f086af0cb67f9ad8d023ce2598abb4b084.tar.gz"],
    sha256 = "31b662ec74506bb4aebf19fc5e05faa904ed4ae3c93c32ccaf388f9146c750d5",
)
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()

http_archive(
    name = "com_google_googletest",
    strip_prefix = "googletest-0fe96607d85cf3a25ac40da369db62bbee2939a5",
    urls = ["https://github.com/google/googletest/archive/0fe96607d85cf3a25ac40da369db62bbee2939a5.tar.gz"],
    sha256 = "80532b7a9c62945eed127a9cfa502f4b9f4af2a0c2329146026fd423e539f578",
)

new_http_archive(
    name = "hexdump",
    strip_prefix = "hexdump-rel-20160408",
    urls = ["https://github.com/wahern/hexdump/archive/rel-20160408.tar.gz"],
    sha256 = "84890f042f8c301e0c59f8f229ea30f0659e001ed2ebf6ccc73b0f01e4072747",
    build_file_content = r"""
cc_binary(
    name = "hexdump",
    visibility = ["//visibility:public"],
    srcs = [
        "hexdump.c",
        "hexdump.h",
    ],
    copts = ["-DHEXDUMP_MAIN"],
)
""",
)

new_http_archive(
    name = "libevent",
    strip_prefix = "libevent-release-2.1.8-stable",
    urls = ["https://github.com/libevent/libevent/archive/release-2.1.8-stable.tar.gz"],
    sha256 = "316ddb401745ac5d222d7c529ef1eada12f58f6376a66c1118eee803cb70f83d",
    build_file = "tools/depends/generated/libevent.bazel",
)

new_http_archive(
    name = "libgmp",
    strip_prefix = "gmp-6.1.1",
    urls = ["https://gmplib.org/download/gmp/gmp-6.1.1.tar.bz2"],
    sha256 = "a8109865f2893f1373b0a8ed5ff7429de8db696fc451b1036bd7bdf95bbeffd6",
    build_file = "tools/depends/generated/libgmp.bazel",
)

# TODO(per-gron): Add this (and set ENABLE_RUST). It can't be done until
# https://github.com/bazelbuild/rules_rust/issues/55 is fixed.
'''
new_http_archive(
    name = "librustzcash",
    sha256 = "1c5c1d44b350752b69e242d7aee801b24486a7c8c1e99e2a10952a3abfbdf274",
    strip_prefix = "zcash-librustzcash-9134864",
    type = "tar.gz",
    urls = ["https://api.github.com/repos/zcash/librustzcash/tarball/91348647a86201a9482ad4ad68398152dc3d635e"],
    build_file_content = r"""
load("@io_bazel_rules_rust//rust:rust.bzl", "rust_library", "rust_test")

rust_library(
    name = "rust_rustzcash",
    srcs = ["src/rustzcash.rs"],
    deps = [
        "@rust_libc//:libc",
    ],
)

cc_library(
    name = "rustzcash",
    visibility = ["//visibility:public"],
    includes = ["include"],
    hdrs = ["include/librustzcash.h"],
    srcs = [":rust_rustzcash"],
)

rust_test(
    name = "rustzcash_test",
    size = "small",
    deps = [":rust_rustzcash"],
)
""",
)
'''

new_http_archive(
    name = "libsodium",
    strip_prefix = "libsodium-1.0.15",
    urls = ["https://download.libsodium.org/libsodium/releases/libsodium-1.0.15.tar.gz"],
    sha256 = "fb6a9e879a2f674592e4328c5d9f79f082405ee4bb05cb6e679b90afe9e178f4",
    build_file = "tools/depends/generated/libsodium.bazel",
)

new_http_archive(
    name = "m4",
    strip_prefix = "m4-2",
    urls = ["http://haddonthethird.net/m4/m4-2.tar.bz2"],
    sha256 = "e4315fef49b08912b1d1db3774dd98f971397b2751c648512b6c8d852590dc50",
    build_file_content = r"""
cc_binary(
    name = "m4",
    visibility = ["//visibility:public"],
    srcs = glob(["*.c", "*.h"]),
    copts = ["-Wno-unused-function", "-Wno-unused-result"],
)
""",
)

new_http_archive(
    name = "openssl",
    strip_prefix = "openssl-1.1.0d",
    urls = ["https://www.openssl.org/source/openssl-1.1.0d.tar.gz"],
    sha256 = "7d5ebb9e89756545c156ff9c13cf2aa6214193b010a468a3bc789c3c28fe60df",
    build_file = "tools/depends/generated/openssl.bazel",
)

new_http_archive(
    name = "procps",
    strip_prefix = "procps-v3.3.12-e0784ddaed30d095bb1d9a8ad6b5a23d10a212c4",
    urls = ["https://gitlab.com/procps-ng/procps/repository/v3.3.12/archive.tar.bz2"],
    sha256 = "20d6e82eb51ecd8eedf2cd046497cf01908d4dd41988694a8225e38643c6d657",
    build_file = "tools/depends/generated/procps.bazel",
)

new_http_archive(
    name = "py_appdirs",
    strip_prefix = "appdirs-1.4.3",
    urls = ["https://github.com/ActiveState/appdirs/archive/1.4.3.tar.gz"],
    sha256 = "5ce44e43c3fd537ce1aaf72141c525aa67032a5af0a14dcf755621e69d72414b",
    build_file_content = r"""
py_library(
    name = "py_appdirs",
    visibility = ["//visibility:public"],
    srcs = ["appdirs.py"],
)
""",
)

new_http_archive(
    name = "py_baron",
    strip_prefix = "baron-0.6.6",
    urls = ["https://github.com/PyCQA/baron/archive/0.6.6.tar.gz"],
    sha256 = "86ea5a7082138ef4c77564c11728d83f8c622cddfabcd44d0468c93e64318ce9",
    build_file_content = r"""
py_library(
    name = "py_baron",
    visibility = ["//visibility:public"],
    srcs = glob(["baron/*.py"]),
    deps = [
        "@py_rply//:py_rply",
    ],
)
""",
)

new_http_archive(
    name = "py_pyblake2",
    strip_prefix = "pyblake2-1.1.0",
    urls = ["https://github.com/dchest/pyblake2/archive/v1.1.0.tar.gz"],
    sha256 = "d420eb77dfcfd3ed196a6a3c252da00d2a03fe47fbfe60256a7ab2a06eaf57fa",
    build_file_content = r"""
cc_binary(
    name = "pyblake2.so",
    visibility = ["//visibility:public"],
    linkstatic = 1,
    linkshared = 1,
    srcs = [
        "pyblake2module.c",
    ],
    deps = [
        ":blake2",
        "@python2//:python_headers",
    ],
)

cc_library(
    name = "blake2",
    linkstatic = 1,
    srcs = [
        "blake2b_impl.c",
        "blake2s_impl.c",
        "impl/blake2-config.h",
        "impl/blake2-impl.h",
        "impl/blake2.h",
        "impl/blake2b-load-sse2.h",
        "impl/blake2b-load-sse41.h",
        "impl/blake2b-round.h",
        "impl/blake2s-load-sse2.h",
        "impl/blake2s-load-sse41.h",
        "impl/blake2s-load-xop.h",
        "impl/blake2s-round.h",
    ],
    hdrs = [
        "pyblake2_impl_common.h",
    ],
    textual_hdrs = [
        # The .c files here are textual_hdrs because they are #included by
        # blake2b_impl.c and blake2s_impl.c
        "impl/blake2b.c",
        "impl/blake2s.c",
    ],
    defines = ["USE_OPTIMIZED_IMPL"],
)
""",
)

new_http_archive(
    name = "py_redbaron",
    strip_prefix = "redbaron-0.6.3",
    urls = ["https://github.com/PyCQA/redbaron/archive/0.6.3.tar.gz"],
    sha256 = "314cbcf442af55b06544fa0aec15991a94f88595d2b6b61e8d92bda7005a1400",
    build_file_content = r"""
py_library(
    name = "py_redbaron",
    visibility = ["//visibility:public"],
    srcs = glob(["redbaron/*.py"]),
    deps = [
        "@py_baron//:py_baron",
    ],
)
""",
)

new_http_archive(
    name = "py_rply",
    strip_prefix = "rply-0.7.5",
    urls = ["https://github.com/alex/rply/archive/v0.7.5.tar.gz"],
    sha256 = "a772cddc9cd648961d394749507d67e95498670ef9dd4a461617bb7b51d5eafc",
    build_file_content = r"""
py_library(
    name = "py_rply",
    visibility = ["//visibility:public"],
    srcs = glob(["rply/*.py"]),
    deps = [
        "@py_appdirs//:py_appdirs",
    ],
)
""",
)

new_http_archive(
    name = "python2",
    strip_prefix = "Python-2.7.14",
    urls = ["https://www.python.org/ftp/python/2.7.14/Python-2.7.14.tar.xz"],
    sha256 = "71ffb26e09e78650e424929b2b457b9c912ac216576e6bd9e7d204ed03296a66",
    build_file_content = r"""
cc_library(
    name = "python_headers",
    visibility = ["//visibility:public"],
    hdrs = glob(["Include/*.h"]) + ["Include/pyconfig.h"],
    strip_include_prefix = "Include",
)

pyconfig_contents = r'''
#pragma once
#define PY_LONG_LONG long long
#define SIZEOF_SIZE_T 8
#define SIZEOF_INT 4
#define SIZEOF_LONG 8
#define HAVE_SSIZE_T 1
'''
genrule(
    name = 'pyconfig',
    outs = ["Include/pyconfig.h"],
    cmd = "cat > $@ << 'BAZEL_EOF'\n" + pyconfig_contents.replace('$', '$$') + "\nBAZEL_EOF",
)

"""
)

new_http_archive(
    name = "proton",
    strip_prefix = "qpid-proton-0.17.0",
    urls = ["http://apache.cs.utah.edu/qpid/proton/0.17.0/qpid-proton-0.17.0.tar.gz"],
    sha256 = "6ffd26d3d0e495bfdb5d9fefc5349954e6105ea18cc4bb191161d27742c5a01a",
    build_file = "tools/depends/generated/proton.bazel",
)

new_http_archive(
    name = "rust_libc",
    sha256 = "bb3642a71e03bfc3215e7037a6c82ee8ecb10f5a2c0d442d1a0740a5290a73ca",
    strip_prefix = "libc-0.2.36",
    urls = ["https://github.com/rust-lang/libc/archive/0.2.36.tar.gz"],
    build_file_content = r"""
load("@io_bazel_rules_rust//rust:rust.bzl", "rust_library")

package(default_visibility = ["//visibility:public"])

rust_library(
    name = "libc",
    srcs = glob(["src/**/*.rs"]),
)
""",
)

http_archive(
    name = "zcash_cc_toolchain",
    strip_prefix = "zcash_cc_toolchain",
    urls = ["https://github.com/per-gron/zcash-toolchain/releases/download/v0.1.0/zcash_cc_toolchain.tar.xz"],
    sha256 = "3ed7b545352f2c8a632b673f6f0df90b36af5f4eb2bfc6a1204eab913fc95f63",
)

new_http_archive(
    name = "zeromq",
    strip_prefix = "zeromq-4.2.1",
    urls = ["https://github.com/zeromq/libzmq/releases/download/v4.2.1/zeromq-4.2.1.tar.gz"],
    sha256 = "27d1e82a099228ee85a7ddb2260f40830212402c605a4a10b5e5498a7e0e9d03",
    build_file = "tools/depends/generated/zeromq.bazel",
)
