"""Generates a BUILD.bazel file suitable for ZCash given a procps source
directory. Run this script in the procps source directory. It creates a
BUILD.bazel file there.
"""

import generator_util
import os
import shlex
import subprocess

procps_config_opts = [
    "--disable-shared",
    "--disable-nls",
    "--disable-pidof",
    "--disable-kill",
    "--without-ncurses",
]

subprocess.call(["./autogen.sh"])
subprocess.call(["./configure"] + procps_config_opts)

config_h = "config.h"

ldflags = shlex.split(generator_util.extract_variable_from_makefile("$(LIBS)"))
objs = shlex.split(generator_util.extract_variable_from_makefile(
    "$(proc_libprocps_la_OBJECTS)"))

# TODO(per-gron): This is not very nice; it assumes that the package is
# being imported with a given name.
external_dir = "external/procps/"

extra_cflags = [
    "-I%s" % external_dir,
    "-I%sinclude" % external_dir,
]

def obj_to_src(obj):
    file = obj.replace(".lo", ".c")
    if not os.path.isfile(file):
        raise Exception("Could not find file for %s" % file)
    return file

def process_library():
    rule = ""

    srcs = [obj_to_src(obj) for obj in objs] + [config_h]

    rule += "cc_library(\n"
    rule += '    visibility = ["//visibility:public"],\n'
    rule += "    name = 'procps',\n"
    rule += "    copts = %s,\n" % extra_cflags
    rule += "    linkopts = %s,\n" % ldflags
    rule += "    srcs = %s + glob(['include/*.h']),\n" % srcs
    rule += "    hdrs = glob(['proc/*.h']),\n"
    rule += "    includes = ['.'],\n"
    rule += ")\n\n"

    return rule

build_file = generator_util.build_header()
build_file += process_library()
build_file += generator_util.copy_file_genrule(
    config_h, generator_util.read_file(config_h))

generator_util.write_file("BUILD.bazel", build_file)
