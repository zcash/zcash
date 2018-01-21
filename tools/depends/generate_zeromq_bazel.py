"""Generates a BUILD.bazel file suitable for ZCash given a zeromq source
directory. Run this script in the zeromq source directory. It creates a
BUILD.bazel file there.
"""

import generator_util
import os
import re
import shlex
import subprocess

zeromq_config_opts = [
    "--without-documentation",
    "--disable-shared",
    "--disable-curve",
    "--with-pic",
]

subprocess.call(["./configure"] + zeromq_config_opts)

platform_hpp = "src/platform.hpp"

cflags_str = generator_util.extract_variable_from_makefile(
    "$(DEFS) $(src_libzmq_la_CPPFLAGS) $(CPPFLAGS) $(src_libzmq_la_CXXFLAGS) $(CXXFLAGS)")
cflags = shlex.split(cflags_str)
cflags = ["'%s'" % flag for flag in cflags if flag != "-g" and not re.match(r"^-O\d$", flag)]
ldflags = shlex.split(generator_util.extract_variable_from_makefile("$(LDFLAGS)"))
objs = shlex.split(generator_util.extract_variable_from_makefile(
    "$(src_libzmq_la_OBJECTS)"))

# TODO(per-gron): This is not very nice; it assumes that the package is
# being imported with a given name.
external_dir = "external/zeromq/"

extra_cflags = [
    "-I%ssrc" % external_dir,
    "-I$(GENDIR)/%ssrc" % external_dir,
]
extra_ldflags = []

def obj_to_src(obj):
    file = re.sub(r"/src_libzmq_la-(.*)\.lo$", r"/\1", obj) + ".cpp"
    if not os.path.isfile(file):
        raise Exception("Could not find file for %s" % file)
    return file

def process_library():
    rule = ""

    srcs = [obj_to_src(obj) for obj in objs] + [platform_hpp]

    rule += "cc_library(\n"
    rule += '    visibility = ["//visibility:public"],\n'
    rule += "    name = 'zmq',\n"
    rule += "    copts = %s,\n" % (cflags + extra_cflags)
    rule += "    linkopts = %s,\n" % (ldflags + extra_ldflags)
    rule += "    includes = ['include'],\n"
    rule += "    srcs = depset(%s + glob(['src/**/*.h', 'src/**/*.hpp'])),\n" % srcs
    rule += "    hdrs = glob(['include/*.h']),\n"
    rule += ")\n\n"

    return rule

build_file = generator_util.build_header()
build_file += process_library()
build_file += generator_util.copy_file_genrule(
    platform_hpp, generator_util.read_file(platform_hpp))

generator_util.write_file("BUILD.bazel", build_file)
