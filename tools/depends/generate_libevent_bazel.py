"""Generates a BUILD.bazel file suitable for ZCash given a libevent source
directory. Run this script in the libevent source directory. It creates a
BUILD.bazel file there.
"""
import generator_util
import re
import subprocess

libevent_config_opts = [
    "--disable-shared",
    "--disable-openssl",
    "--disable-libevent-regress",
    "--with-pic",
]

subprocess.call(["./autogen.sh"])
subprocess.call(["./configure"] + libevent_config_opts)

subprocess.call(["make", "include/event2/event-config.h"])
subprocess.call(["make", "evconfig-private.h"])

# TODO(per-gron): This is not very nice; it assumes that the package is
# being imported with a given name.
external_dir = "external/libevent/"

srcs = generator_util.extract_variable_from_makefile("$(libevent_la_SOURCES) $(libevent_pthreads_la_SOURCES)").split()
cflags = ["-I%scompat" % external_dir] + generator_util.extract_variable_from_makefile("$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)").split()
cflags = [flag for flag in cflags if flag != "-g" and not re.match(r"^-O\d$", flag)]

generated_headers = {
    "include/event2/event-config.h": generator_util.read_file("include/event2/event-config.h"),
    "evconfig-private.h": generator_util.read_file("evconfig-private.h"),
}

build_file = generator_util.build_header()
build_file += ("""
cc_library(
    visibility = ["//visibility:public"],
    includes = [
        "include",
    ],
    copts = %s + [
        "-DHAVE_CONFIG_H",
        "-w",  # Silence "warning: comparison between pointer and integer"
    ],
    srcs = glob(["*.h"]) + [
        "include/event2/event-config.h",
        "evconfig-private.h",
        "compat/sys/queue.h",
    ] + %s,
    textual_hdrs = [
        "arc4random.c",
        "epoll_sub.c",
    ],
    hdrs = glob(["include/**/*.h"]),
    name = "event",
)

""" % (cflags, srcs))

for generated_header in generated_headers:
    build_file += generator_util.copy_file_genrule(generated_header, generated_headers[generated_header])

generator_util.write_file("BUILD.bazel", build_file)
