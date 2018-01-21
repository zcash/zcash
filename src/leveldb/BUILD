licenses(["notice"])

filegroup(
    name = "license",
    srcs = ["LICENSE"],
)

cc_library(
    name = "leveldb",
    visibility = ["//visibility:public"],
    srcs = [
        "db/builder.cc",
        "db/c.cc",
        "db/db_impl.cc",
        "db/db_iter.cc",
        "db/dbformat.cc",
        "db/dumpfile.cc",
        "db/filename.cc",
        "db/log_reader.cc",
        "db/log_writer.cc",
        "db/memtable.cc",
        "db/repair.cc",
        "db/table_cache.cc",
        "db/version_edit.cc",
        "db/version_set.cc",
        "db/write_batch.cc",
        "port/port_posix.cc",
        "table/block.cc",
        "table/block_builder.cc",
        "table/filter_block.cc",
        "table/format.cc",
        "table/iterator.cc",
        "table/merger.cc",
        "table/table.cc",
        "table/table_builder.cc",
        "table/two_level_iterator.cc",
        "util/arena.cc",
        "util/bloom.cc",
        "util/cache.cc",
        "util/coding.cc",
        "util/comparator.cc",
        "util/crc32c.cc",
        "util/env.cc",
        "util/env_posix.cc",
        "util/filter_policy.cc",
        "util/hash.cc",
        "util/histogram.cc",
        "util/logging.cc",
        "util/options.cc",
        "util/status.cc",
    ],
    hdrs = [
        "db/builder.h",
        "db/db_impl.h",
        "db/db_iter.h",
        "db/dbformat.h",
        "db/filename.h",
        "db/log_format.h",
        "db/log_reader.h",
        "db/log_writer.h",
        "db/memtable.h",
        "db/skiplist.h",
        "db/snapshot.h",
        "db/table_cache.h",
        "db/version_edit.h",
        "db/version_set.h",
        "db/write_batch_internal.h",
        "include/leveldb/c.h",
        "include/leveldb/cache.h",
        "include/leveldb/comparator.h",
        "include/leveldb/db.h",
        "include/leveldb/dumpfile.h",
        "include/leveldb/env.h",
        "include/leveldb/filter_policy.h",
        "include/leveldb/iterator.h",
        "include/leveldb/options.h",
        "include/leveldb/slice.h",
        "include/leveldb/status.h",
        "include/leveldb/table.h",
        "include/leveldb/table_builder.h",
        "include/leveldb/write_batch.h",
        "port/atomic_pointer.h",
        "port/port.h",
        "port/port_example.h",
        "port/port_posix.h",
        "port/port_win.h",
        "port/thread_annotations.h",
        "port/win/stdint.h",
        "table/block.h",
        "table/block_builder.h",
        "table/filter_block.h",
        "table/format.h",
        "table/iterator_wrapper.h",
        "table/merger.h",
        "table/two_level_iterator.h",
        "util/arena.h",
        "util/coding.h",
        "util/crc32c.h",
        "util/hash.h",
        "util/histogram.h",
        "util/logging.h",
        "util/mutexlock.h",
        "util/posix_logger.h",
        "util/random.h",
    ],
    includes = ["include"],
    copts = [
        "-Wno-implicit-fallthrough",
        "-Wno-non-virtual-dtor",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-DNDEBUG",
        "-iquote",
        "src/leveldb",
    ],
    defines = [
        "LEVELDB_PLATFORM_POSIX",
    ] + select({
        ":darwin": ["OS_MACOSX"],
        "//conditions:default": [],
    }),
    linkopts = [
        "-lpthread",
    ],
)

cc_library(
    name = "memenv",
    visibility = ["//visibility:public"],
    srcs = ["helpers/memenv/memenv.cc"],
    hdrs = ["helpers/memenv/memenv.h"],
    includes = ["helpers/memenv"],
    copts = [
        "-DNDEBUG",
        "-iquote",
        "src/leveldb",
    ],
    deps = [
        ":leveldb",
    ],
)

cc_library(
    name = "testharness",
    srcs = ["util/testharness.cc"],
    hdrs = ["util/testharness.h"],
    includes = ["."],
    deps = [
        ":leveldb",
    ],
)

cc_library(
    name = "testutil",
    srcs = ["util/testutil.cc"],
    hdrs = ["util/testutil.h"],
    includes = ["."],
    deps = [
        ":leveldb",
    ],
)

cc_test(
    name = "db_autocompact_test",
    size = "small",
    srcs = ["db/autocompact_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
        ":testutil",
    ],
)

cc_test(
    name = "db_c_test",
    size = "small",
    srcs = ["db/c_test.c"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
    ],
)

cc_test(
    name = "db_corruption_test",
    size = "small",
    srcs = ["db/corruption_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
        ":testutil",
    ],
)

cc_test(
    name = "db_db_test",
    srcs = ["db/db_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
        ":testutil",
    ],
)

cc_test(
    name = "db_dbformat_test",
    size = "small",
    srcs = ["db/dbformat_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "db_filename_test",
    size = "small",
    srcs = ["db/filename_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "db_log_test",
    size = "small",
    srcs = ["db/log_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "db_skiplist_test",
    size = "small",
    srcs = ["db/skiplist_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "db_version_edit_test",
    size = "small",
    srcs = ["db/version_edit_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "db_version_set_test",
    size = "small",
    srcs = ["db/version_set_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
        ":testutil",
    ],
)

cc_test(
    name = "db_write_batch_test",
    size = "small",
    srcs = ["db/write_batch_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "helpers/memenv_memenv_test",
    size = "small",
    srcs = ["helpers/memenv/memenv_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":memenv",
        ":testharness",
    ],
)

cc_test(
    name = "issues_issue178_test",
    size = "small",
    srcs = ["issues/issue178_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "issues_issue200_test",
    size = "small",
    srcs = ["issues/issue200_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "table_filter_block_test",
    size = "small",
    srcs = ["table/filter_block_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
        ":testutil",
    ],
)

cc_test(
    name = "table_table_test",
    size = "small",
    srcs = ["table/table_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
        ":testutil",
    ],
)

cc_test(
    name = "util_arena_test",
    size = "small",
    srcs = ["util/arena_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "util_bloom_test",
    size = "small",
    srcs = ["util/bloom_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
        ":testutil",
    ],
)

cc_test(
    name = "util_cache_test",
    size = "small",
    srcs = ["util/cache_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "util_coding_test",
    size = "small",
    srcs = ["util/coding_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "util_crc32c_test",
    size = "small",
    srcs = ["util/crc32c_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "util_env_test",
    size = "small",
    srcs = ["util/env_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

cc_test(
    name = "util_hash_test",
    size = "small",
    srcs = ["util/hash_test.cc"],
    copts = ["-iquote", "src/leveldb"],
    deps = [
        ":leveldb",
        ":testharness",
    ],
)

config_setting(
    name = "darwin",
    values = {"cpu": "darwin"},
    visibility = ["//visibility:public"],
)
